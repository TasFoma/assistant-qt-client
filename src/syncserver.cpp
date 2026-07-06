#include "syncserver.h"
#include "chatmanager.h"
#include "databasemanager.h"
#include "navigationmanager.h"
#include "plannermanager.h"

#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QUrlQuery>
#include <QUuid>
#include <QDebug>

SyncServer::SyncServer(PlannerManager *planner, ChatManager *chat,
                       NavigationManager *nav, QObject *parent)
    : QObject(parent), m_planner(planner), m_chat(chat), m_nav(nav)
{
    // Маршрут, запрошенный с телефона → результат обратно на телефон
    connect(m_nav, &NavigationManager::routeReady, this, [this](const QString &text) {
        if (!m_routeSock)
            return;
        // Ссылка «открыть в Яндекс Картах» с построенным маршрутом
        const QString rtt = m_nav->mode() == "driving" ? "auto"
                          : m_nav->mode() == "bike"    ? "bc"
                                                       : "pd";
        const QString url = QStringLiteral(
            "https://yandex.ru/maps/?rtext=%1,%2~%3,%4&rtt=%5")
            .arg(m_nav->fromLat()).arg(m_nav->fromLon())
            .arg(m_nav->toLat()).arg(m_nav->toLon()).arg(rtt);
        m_chat->addMessage(text, false);
        emit chatActivity(text, false);
        send(m_routeSock, {{"type", "route_result"}, {"text", text}, {"url", url}});
        m_routeSock.clear();
    });
    connect(m_nav, &NavigationManager::routeError, this, [this](const QString &error) {
        if (!m_routeSock)
            return;
        // Даже при неудаче даём кнопку поиска этого места в Яндекс Картах
        const QString url = QStringLiteral("https://yandex.ru/maps/?text=")
            + QString::fromUtf8(QUrl::toPercentEncoding(m_nav->toText()));
        send(m_routeSock, {{"type", "route_result"},
                           {"text", "😕 " + error}, {"url", url}});
        m_routeSock.clear();
    });
    auto &dbm = DatabaseManager::instance();

    m_token = dbm.setting("sync_token");
    if (m_token.isEmpty()) {
        m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        dbm.setSetting("sync_token", m_token);
    }
    m_port = dbm.setting("sync_port", "8765").toUShort();

    // Адрес llama-server для проксирования чата с телефона
    m_llamaUrl = dbm.setting("llama_url", "http://192.168.0.100:8080");
    dbm.setSetting("llama_url", m_llamaUrl);

    // Адрес SearXNG для поиска в интернете (обычно на этой же машине)
    m_searxngUrl = dbm.setting("searxng_url", "http://localhost:8081");
    dbm.setSetting("searxng_url", m_searxngUrl);

    connect(&m_server, &QTcpServer::newConnection, this, &SyncServer::onNewConnection);

    // Любое изменение данных → уведомить мобильный клиент
    connect(m_planner, &PlannerManager::tasksChanged,     this, &SyncServer::onDataChanged);
    connect(m_planner, &PlannerManager::eventsChanged,    this, &SyncServer::onDataChanged);
    connect(m_planner, &PlannerManager::remindersChanged, this, &SyncServer::onDataChanged);
    connect(m_planner, &PlannerManager::habitsChanged,    this, &SyncServer::onDataChanged);

    // Сработавшие напоминания: в историю чата + всем клиентам
    connect(m_planner, &PlannerManager::reminderDue, this, [this](const QString &title) {
        m_chat->addMessage("⏰ Напоминание: " + title, false);
        emit chatActivity("⏰ Напоминание: " + title, false);
        broadcast({{"type", "reminder_due"}, {"title", title}});
    });

    // Проактивные сообщения (утреннее приветствие, вечерний чек-ин)
    connect(m_planner, &PlannerManager::checkInDue, this, [this](const QString &message) {
        m_chat->addMessage(message, false);
        emit chatActivity(message, false);
        broadcast({{"type", "assistant_message"}, {"text", message}});
    });

    // После утреннего приветствия — погода
    connect(m_planner, &PlannerManager::morningGreeted,
            m_nav, &NavigationManager::fetchWeather);
    connect(m_nav, &NavigationManager::weatherReady, this, [this](const QString &text) {
        m_chat->addMessage("🌤️ " + text, false);
        emit chatActivity("🌤️ " + text, false);
        broadcast({{"type", "assistant_message"}, {"text", "🌤️ " + text}});
    });

    detectTailscaleIp();
}

bool SyncServer::start()
{
    if (m_server.isListening())
        return true;
    // Слушаем все интерфейсы: доступ извне предполагается только через Tailscale
    const bool ok = m_server.listen(QHostAddress::Any, m_port);
    if (ok)
        qInfo() << "SyncServer слушает порт" << m_port
                << (m_tailscaleIp.isEmpty() ? "" : "Tailscale IP: " + m_tailscaleIp);
    else
        qWarning() << "SyncServer: не удалось открыть порт" << m_port
                   << m_server.errorString();
    emit stateChanged();
    return ok;
}

void SyncServer::detectTailscaleIp()
{
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                    m_tailscaleIp = out.section('\n', 0, 0).trimmed();
                    if (!m_tailscaleIp.isEmpty()) {
                        DatabaseManager::instance().setSetting("tailscale_ip", m_tailscaleIp);
                        emit stateChanged();
                    }
                }
                proc->deleteLater();
            });
    connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError) {
        proc->deleteLater(); // tailscale не установлен — не страшно
    });
    // CLI может не быть в PATH — пробуем стандартный путь установки
    QString exe = QStringLiteral("tailscale");
    const QString installed = QStringLiteral("C:/Program Files/Tailscale/tailscale.exe");
    if (QFile::exists(installed))
        exe = installed;
    proc->start(exe, {"ip", "-4"});
}

void SyncServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server.nextPendingConnection()) {
        m_pendingAuth.insert(sock);
        m_buffers[sock] = QByteArray();

        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            m_buffers[sock] += sock->readAll();
            int idx;
            while ((idx = m_buffers[sock].indexOf('\n')) != -1) {
                const QByteArray line = m_buffers[sock].left(idx).trimmed();
                m_buffers[sock].remove(0, idx + 1);
                if (!line.isEmpty())
                    handleLine(sock, line);
            }
        });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            m_clients.removeAll(sock);
            m_pendingAuth.remove(sock);
            m_buffers.remove(sock);
            sock->deleteLater();
            emit stateChanged();
        });
    }
}

void SyncServer::handleLine(QTcpSocket *sock, const QByteArray &line)
{
    const QJsonObject msg = QJsonDocument::fromJson(line).object();
    const QString type = msg["type"].toString();

    // ── Аутентификация ───────────────────────────────────────────────
    if (m_pendingAuth.contains(sock)) {
        if (type == "auth" && msg["token"].toString() == m_token) {
            m_pendingAuth.remove(sock);
            m_clients.append(sock);
            send(sock, {{"type", "auth_ok"}});
            emit stateChanged();
        } else {
            send(sock, {{"type", "auth_fail"}});
            sock->disconnectFromHost();
        }
        return;
    }

    // ── Команды авторизованного клиента ─────────────────────────────
    if (type == "get_all") {
        send(sock, snapshot());
    } else if (type == "add_task") {
        m_planner->addTask(msg["title"].toString(),
                           msg["dueDate"].toString(),
                           msg["priority"].toInt(1));
    } else if (type == "set_task_done") {
        m_planner->setTaskDone(msg["id"].toInt(), msg["done"].toBool());
    } else if (type == "delete_task") {
        m_planner->deleteTask(msg["id"].toInt());
    } else if (type == "delete_reminder") {
        m_planner->deleteReminder(msg["id"].toInt());
    } else if (type == "delete_event") {
        m_planner->deleteEvent(msg["id"].toInt());
    } else if (type == "add_event") {
        m_planner->addEvent(msg["title"].toString(), msg["date"].toString(),
                            msg["time"].toString(), msg["description"].toString(),
                            msg["repeat"].toString());
    } else if (type == "add_habit") {
        m_planner->addHabit(msg["name"].toString(), msg["target"].toDouble(),
                            msg["unit"].toString(), msg["period"].toString());
    } else if (type == "delete_habit") {
        m_planner->deleteHabit(msg["id"].toInt());
    } else if (type == "get_stats") {
        send(sock, {{"type", "nav_stats"}, {"stats", m_nav->stats()}});
    } else if (type == "new_chat") {
        m_chat->newChat();
        send(sock, {{"type", "chat_history"}, {"messages", m_chat->loadMessages()}});
    } else if (type == "route_text") {
        // Маршрут с текстовой точкой старта (десктоп: «откуда выходишь?»)
        m_routeSock = sock;
        m_nav->buildRoute(msg["from"].toString(), msg["to"].toString(),
                          msg["mode"].toString());
    } else if (type == "add_reminder") {
        if (msg["kind"].toString() == "geo")
            m_planner->addGeoReminder(msg["title"].toString(), msg["place"].toString(),
                                      msg["lat"].toDouble(), msg["lon"].toDouble());
        else
            m_planner->addTimeReminder(msg["title"].toString(), msg["dueAt"].toString());
    } else if (type == "location") {
        // Телефон сообщает координаты → гео-напоминания, маршруты, погода
        const double lat = msg["lat"].toDouble();
        const double lon = msg["lon"].toDouble();
        m_phoneLat = lat;
        m_phoneLon = lon;
        auto &dbm = DatabaseManager::instance();
        dbm.setSetting("last_lat", QString::number(lat, 'f', 5));
        dbm.setSetting("last_lon", QString::number(lon, 'f', 5));
        m_planner->checkGeoReminders(lat, lon);
        emit locationReceived(lat, lon);
    } else if (type == "update_task") {
        m_planner->updateTask(msg["id"].toInt(), msg["title"].toString(),
                              msg["dueDate"].toString(), msg["priority"].toInt(1));
    } else if (type == "update_event") {
        m_planner->updateEvent(msg["id"].toInt(), msg["title"].toString(),
                               msg["date"].toString(), msg["time"].toString(),
                               msg["description"].toString(),
                               msg["repeat"].toString());
    } else if (type == "get_chat") {
        // История переписки — телефон показывает ту же ленту, что и ПК
        send(sock, {{"type", "chat_history"},
                    {"messages", m_chat->loadMessages()}});
    } else if (type == "get_month") {
        send(sock, {{"type", "month_events"},
                    {"year", msg["year"].toInt()},
                    {"month", msg["month"].toInt()},
                    {"days", m_planner->eventDaysInMonth(msg["year"].toInt(),
                                                         msg["month"].toInt())}});
    } else if (type == "get_day") {
        send(sock, {{"type", "day_events"},
                    {"date", msg["date"].toString()},
                    {"events", m_planner->eventsForDate(msg["date"].toString())}});
    } else if (type == "log_habit") {
        m_planner->logHabit(msg["id"].toInt(), msg["value"].toDouble());
    } else if (type == "chat") {
        // Проксируем запрос телефона в llama-server
        const qint64 id = msg["id"].toVariant().toLongLong();

        // Сообщение пользователя — в общую историю (и в чат на ПК)
        const QJsonArray msgs = msg["messages"].toArray();
        QString userText;
        if (!msgs.isEmpty())
            userText = msgs.last().toObject()["content"].toString();
        if (!userText.isEmpty()) {
            m_chat->addMessage(userText, true);
            emit chatActivity(userText, true);
        }

        QJsonObject req;
        req["messages"]       = msg["messages"];
        req["max_tokens"]     = 600;
        req["temperature"]    = 0.6;
        req["top_p"]          = 0.9;
        req["top_k"]          = 40;
        req["repeat_penalty"] = 1.1;
        req["stop"] = QJsonArray::fromStringList({"<|im_end|>", "<|im_start|>"});
        req["stream"]         = false;
        if (msg.contains("model"))
            req["model"] = msg["model"];

        QPointer<QTcpSocket> psock(sock);

        // Фактические вопросы («какая погода…», «кто такой…») — сначала
        // поиск через SearXNG, результаты добавляются в системный промпт
        if (m_parser.needsSearch(userText)) {
            QUrl surl(m_searxngUrl + "/search");
            QUrlQuery squery;
            squery.addQueryItem("q", userText);
            squery.addQueryItem("format", "json");
            surl.setQuery(squery);
            QNetworkRequest sreq(surl);
            sreq.setRawHeader("User-Agent", "Mozilla/5.0");
            sreq.setTransferTimeout(10000);
            QNetworkReply *sr = m_net.get(sreq);
            connect(sr, &QNetworkReply::finished, this,
                    [this, sr, req, psock, id]() mutable {
                sr->deleteLater();
                QString results;
                if (sr->error() == QNetworkReply::NoError) {
                    const QJsonArray arr = QJsonDocument::fromJson(sr->readAll())
                                               .object()["results"].toArray();
                    int count = 0;
                    for (const QJsonValue &v : arr) {
                        if (count >= 3) break;
                        const QJsonObject res = v.toObject();
                        if (res["title"].toString().isEmpty()) continue;
                        QString content = res["content"].toString();
                        if (content.length() > 300)
                            content = content.left(300) + "...";
                        results += QStringLiteral("--- Результат %1 ---\n%2\n%3\n\n")
                                       .arg(count + 1)
                                       .arg(res["title"].toString(), content);
                        count++;
                    }
                }
                if (!results.isEmpty()) {
                    QJsonArray msgs2 = req["messages"].toArray();
                    msgs2.prepend(QJsonObject{
                        {"role", "system"},
                        {"content", "Свежие результаты поиска в интернете "
                                    "(используй их для ответа):\n" + results}});
                    req["messages"] = msgs2;
                }
                postChatToLlama(req, psock, id);
            });
        } else {
            postChatToLlama(req, psock, id);
        }
    } else if (type == "command") {
        // Телефон присылает текст пользователя: если это команда
        // (задача/напоминание/событие) — выполняем здесь, иначе
        // телефон отправит его в нейросеть
        const QString text = msg["text"].toString();

        // Маршрут — асинхронный: сначала подтверждение, потом route_result
        const QVariantMap probe = m_parser.parse(text);
        if (probe["type"].toString() == "route") {
            QVariantMap result;
            result["type"] = "command_result";
            result["id"]   = msg["id"].toVariant().toLongLong();
            result["handled"] = true;
            if (m_phoneLat != 0.0 || m_phoneLon != 0.0) {
                m_routeSock = sock;
                m_nav->buildRouteFrom(m_phoneLat, m_phoneLon,
                                      probe["place"].toString(),
                                      probe["mode"].toString());
                result["reply"] = "🗺️ Строю маршрут от тебя до: "
                                  + probe["place"].toString() + "…";
            } else {
                result["reply"] = "📍 Пока не знаю, где ты. Включи «Передавать "
                                  "геолокацию» в настройках и подожди минуту.";
            }
            m_chat->addMessage(text, true);
            emit chatActivity(text, true);
            m_chat->addMessage(result["reply"].toString(), false);
            emit chatActivity(result["reply"].toString(), false);
            send(sock, result);
            return;
        }

        QVariantMap result = runCommand(text);
        result["type"] = "command_result";
        result["id"]   = msg["id"].toVariant().toLongLong();
        if (result["handled"].toBool()) {
            // Выполненные команды тоже часть переписки
            m_chat->addMessage(text, true);
            emit chatActivity(text, true);
            m_chat->addMessage(result["reply"].toString(), false);
            emit chatActivity(result["reply"].toString(), false);
        }
        send(sock, result);
    } else if (type == "ping") {
        send(sock, {{"type", "pong"}});
    }
}

void SyncServer::postChatToLlama(const QJsonObject &req_,
                                 QPointer<QTcpSocket> psock, qint64 id)
{
    // Сервер сам добавляет системный промпт и живой контекст:
    // дата/время, профиль пользователя, планы дня, цели
    QJsonObject req = req_;
    QJsonArray msgs = req["messages"].toArray();
    msgs.prepend(QJsonObject{
        {"role", "system"},
        {"content",
         QStringLiteral(
             "Ты — «Мой спутник», личный ИИ-ассистент и друг. Отвечай умно, "
             "точно и по-русски.\n"
             "• Давай конкретные ответы — без воды.\n"
             "• Отвечай ТОЛЬКО за себя, не пиши реплики за пользователя.\n"
             "• Если не знаешь — честно скажи.\n"
             "• Уместные эмодзи — можно, но без перебора.\n\nКонтекст:\n")
             + m_planner->dailyContext()}});
    req["messages"] = msgs;

    QNetworkRequest r(QUrl(m_llamaUrl + "/v1/chat/completions"));
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    r.setTransferTimeout(300000); // модель может грузиться с диска

    QNetworkReply *reply = m_net.post(r, QJsonDocument(req).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, psock, id]() {
        reply->deleteLater();
        if (!psock)
            return; // телефон уже отключился
        if (reply->error() != QNetworkReply::NoError) {
            send(psock, {{"type", "chat_error"}, {"id", id},
                         {"error", reply->errorString()}});
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString content;
        const QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty())
            content = choices.first().toObject()["message"].toObject()["content"]
                          .toString().trimmed();
        if (content.isEmpty())
            content = QStringLiteral("(нет ответа)");
        // Ответ ассистента — в общую историю и в чат на ПК
        m_chat->addMessage(content, false);
        emit chatActivity(content, false);
        send(psock, {{"type", "chat_reply"}, {"id", id}, {"text", content}});
    });
}

QVariantMap SyncServer::runCommand(const QString &text)
{
    const QVariantMap cmd = m_parser.parse(text);
    const QString ctype = cmd["type"].toString();
    QString reply;

    if (ctype == "remember") {
        m_planner->rememberFact(cmd["fact"].toString());
        reply = "🧠 Запомнила: " + cmd["fact"].toString();
    } else if (ctype == "reminder_recurring") {
        m_planner->addRecurringReminder(cmd["title"].toString(),
                                        cmd["minutes"].toInt());
        reply = QStringLiteral("⏰ Буду напоминать каждые %1 мин: «%2». "
                               "Скажи «удали напоминание», когда надоест 😉")
                    .arg(cmd["minutes"].toInt())
                    .arg(cmd["title"].toString());
    } else if (ctype == "reminder_time") {
        m_planner->addTimeReminder(cmd["title"].toString(), cmd["dueAt"].toString());
        reply = "⏰ Хорошо, напомню " + cmd["human"].toString()
                + ": «" + cmd["title"].toString() + "»";
    } else if (ctype == "reminder_geo") {
        m_planner->addGeoReminder(cmd["title"].toString(), cmd["place"].toString());
        reply = "📍 Запомнила! Напомню «" + cmd["title"].toString()
                + "», когда будешь рядом с: " + cmd["place"].toString();
    } else if (ctype == "task") {
        m_planner->addTask(cmd["title"].toString(), cmd["dueDate"].toString(), 1);
        reply = "✅ Задача добавлена: «" + cmd["title"].toString() + "»"
                + (cmd["dueDate"].toString().isEmpty()
                       ? QString()
                       : " (срок: " + cmd["dueDate"].toString() + ")");
    } else if (ctype == "task_list") {
        const QStringList items = cmd["items"].toStringList();
        for (const QString &item : items)
            m_planner->addTask(item, QString(), 1);
        reply = QStringLiteral("✅ Добавила %1 задач(и):\n• %2")
                    .arg(items.size())
                    .arg(items.join("\n• "));
    } else if (ctype == "event") {
        m_planner->addEvent(cmd["title"].toString(), cmd["date"].toString(),
                            cmd["time"].toString());
        reply = "📅 Добавила в календарь: «" + cmd["title"].toString() + "» — "
                + cmd["date"].toString()
                + (cmd["time"].toString().isEmpty()
                       ? QString()
                       : " в " + cmd["time"].toString());
    }

    return {{"handled", !reply.isEmpty()}, {"reply", reply}};
}

QVariantMap SyncServer::snapshot() const
{
    return {
        {"type", "snapshot"},
        {"tasks", m_planner->tasks()},
        {"reminders", m_planner->reminders()},
        {"habits", m_planner->habits()},
        {"events_today", m_planner->eventsForDate(
                             QDate::currentDate().toString("yyyy-MM-dd"))}
    };
}

void SyncServer::send(QTcpSocket *sock, const QVariantMap &msg)
{
    sock->write(QJsonDocument(QJsonObject::fromVariantMap(msg))
                    .toJson(QJsonDocument::Compact) + "\n");
}

void SyncServer::broadcast(const QVariantMap &msg)
{
    for (QTcpSocket *sock : std::as_const(m_clients))
        send(sock, msg);
}

void SyncServer::onDataChanged()
{
    // Шлём свежий снимок всем подключённым клиентам
    if (!m_clients.isEmpty())
        broadcast(snapshot());
}
