#include "remotebackend.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>

RemoteBackend::RemoteBackend(QObject *parent) : QObject(parent)
{
    QSettings st("MyCompanion", "client");
    m_host  = st.value("server_host", "192.168.0.100").toString();
    m_token = st.value("token", "5465392c-8308-4fdd-8ae3-177b969e8b39").toString();

    connect(&m_socket, &QTcpSocket::readyRead, this, &RemoteBackend::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &RemoteBackend::onDisconnected);
    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_buffer.clear();
        send({{"type", "auth"}, {"token", m_token}});
        emit stateChanged();
    });
    connect(&m_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit stateChanged();
                m_reconnectTimer.start();
            });

    m_reconnectTimer.setInterval(10000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RemoteBackend::reconnect);

    reconnect();
}

void RemoteBackend::setServerHost(const QString &host)
{
    m_host = host.trimmed();
    QSettings("MyCompanion", "client").setValue("server_host", m_host);
    emit stateChanged();
}

void RemoteBackend::setToken(const QString &token)
{
    m_token = token.trimmed();
    QSettings("MyCompanion", "client").setValue("token", m_token);
}

void RemoteBackend::reconnect()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    m_authorized = false;
    emit stateChanged();
    m_socket.connectToHost(m_host, 8765);
}

void RemoteBackend::onDisconnected()
{
    m_authorized = false;
    emit stateChanged();
    m_reconnectTimer.start();
}

void RemoteBackend::send(const QVariantMap &msg)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;
    m_socket.write(QJsonDocument(QJsonObject::fromVariantMap(msg))
                       .toJson(QJsonDocument::Compact) + "\n");
}

void RemoteBackend::onReadyRead()
{
    m_buffer += m_socket.readAll();
    int idx;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        const QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);
        if (line.isEmpty())
            continue;
        const QJsonObject obj = QJsonDocument::fromJson(line).object();
        if (!obj.isEmpty())
            handle(obj.toVariantMap());
    }
}

void RemoteBackend::appendMessage(const QString &text, bool isUser)
{
    QVariantMap m;
    m["text"]   = text;
    m["isUser"] = isUser;
    m["time"]   = QDateTime::currentDateTime().toString("hh:mm");
    m_messages << m;
    emit messageAdded(text, isUser);
}

void RemoteBackend::handle(const QVariantMap &msg)
{
    const QString type = msg["type"].toString();

    if (type == "auth_ok") {
        m_authorized = true;
        send({{"type", "get_all"}});
        send({{"type", "get_chat"}});
        send({{"type", "get_stats"}});
        emit stateChanged();
    } else if (type == "auth_fail") {
        m_authorized = false;
        emit stateChanged();
        emit chatError("Сервер отклонил токен — проверь настройки (⚙ в шапке)");
    } else if (type == "snapshot") {
        m_tasks     = msg["tasks"].toList();
        m_reminders = msg["reminders"].toList();
        m_habits    = msg["habits"].toList();
        // Данные изменились — кэш календаря устарел
        m_eventsByDate.clear();
        m_daysByMonth.clear();
        m_pendingDay.clear();
        m_pendingMonth.clear();
        emit tasksChanged();
        emit remindersChanged();
        emit habitsChanged();
        emit eventsChanged();
    } else if (type == "chat_history") {
        m_messages = msg["messages"].toList();
        emit historyLoaded();
    } else if (type == "day_events") {
        const QString date = msg["date"].toString();
        m_eventsByDate[date] = msg["events"].toList();
        m_pendingDay.remove(date);
        emit eventsChanged();
    } else if (type == "month_events") {
        const QString key = QStringLiteral("%1-%2").arg(msg["year"].toInt())
                                                   .arg(msg["month"].toInt());
        m_daysByMonth[key] = msg["days"].toList();
        m_pendingMonth.remove(key);
        emit eventsChanged();
    } else if (type == "nav_stats") {
        m_navStats = msg["stats"].toMap();
        emit navStatsChanged();
    } else if (type == "command_result") {
        const qint64 id = msg["id"].toLongLong();
        const QString original = m_pendingCommands.take(id);
        if (msg["handled"].toBool()) {
            appendMessage(msg["reply"].toString(), false);
        } else if (!original.isEmpty()) {
            // Не команда — обычный разговор с нейросетью через сервер
            QVariantMap chat;
            chat["type"]     = "chat";
            chat["id"]       = ++m_seq;
            chat["model"]    = m_model;
            chat["messages"] = historyForLlm();
            send(chat);
        }
    } else if (type == "chat_reply") {
        appendMessage(msg["text"].toString(), false);
    } else if (type == "chat_error") {
        emit chatError(msg["error"].toString());
    } else if (type == "route_result") {
        QString text = msg["text"].toString();
        const QString url = msg["url"].toString();
        if (!url.isEmpty())
            text += "\n\n[🗺️ Открыть в Яндекс Картах](" + url + ")";
        appendMessage(text, false);
        send({{"type", "get_stats"}});
    } else if (type == "reminder_due") {
        appendMessage("⏰ Напоминание: " + msg["title"].toString(), false);
        emit reminderDue(msg["title"].toString());
    } else if (type == "assistant_message") {
        appendMessage(msg["text"].toString(), false);
    }
}

// История для нейросети: последние сообщения, включая свежий текст
// пользователя (он уже в m_messages)
QVariantList RemoteBackend::historyForLlm(int maxChars) const
{
    QVariantList out;
    int total = 0;
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        const QVariantMap m = m_messages[i].toMap();
        const QString text = m["text"].toString();
        if (total + text.length() > maxChars)
            break;
        total += text.length();
        QVariantMap turn;
        turn["role"]    = m["isUser"].toBool() ? "user" : "assistant";
        turn["content"] = text;
        out.prepend(turn);
    }
    return out;
}

void RemoteBackend::sendUserText(const QString &text, const QString &model)
{
    m_model = model;
    appendMessage(text, true); // эхо в ленту (сервер сохранит сам)
    const qint64 id = ++m_seq;
    m_pendingCommands[id] = text;
    send({{"type", "command"}, {"id", id}, {"text", text}});
}

// ── Планировщик ──────────────────────────────────────────────────────

QVariantList RemoteBackend::eventsForDate(const QString &date)
{
    if (!m_eventsByDate.contains(date) && !m_pendingDay.contains(date)
        && m_authorized) {
        m_pendingDay.insert(date);
        send({{"type", "get_day"}, {"date", date}});
    }
    return m_eventsByDate.value(date);
}

QVariantList RemoteBackend::eventDaysInMonth(int year, int month)
{
    const QString key = QStringLiteral("%1-%2").arg(year).arg(month);
    if (!m_daysByMonth.contains(key) && !m_pendingMonth.contains(key)
        && m_authorized) {
        m_pendingMonth.insert(key);
        send({{"type", "get_month"}, {"year", year}, {"month", month}});
    }
    return m_daysByMonth.value(key);
}

void RemoteBackend::addTask(const QString &title, const QString &dueDate, int priority)
{ send({{"type", "add_task"}, {"title", title}, {"dueDate", dueDate}, {"priority", priority}}); }

void RemoteBackend::updateTask(int id, const QString &title, const QString &dueDate, int priority)
{ send({{"type", "update_task"}, {"id", id}, {"title", title}, {"dueDate", dueDate}, {"priority", priority}}); }

void RemoteBackend::setTaskDone(int id, bool done)
{ send({{"type", "set_task_done"}, {"id", id}, {"done", done}}); }

void RemoteBackend::deleteTask(int id)
{ send({{"type", "delete_task"}, {"id", id}}); }

void RemoteBackend::addEvent(const QString &title, const QString &date,
                             const QString &time, const QString &description,
                             const QString &repeat)
{ send({{"type", "add_event"}, {"title", title}, {"date", date}, {"time", time},
        {"description", description}, {"repeat", repeat}}); }

void RemoteBackend::updateEvent(int id, const QString &title, const QString &date,
                                const QString &time, const QString &description,
                                const QString &repeat)
{ send({{"type", "update_event"}, {"id", id}, {"title", title}, {"date", date},
        {"time", time}, {"description", description}, {"repeat", repeat}}); }

void RemoteBackend::deleteEvent(int id)
{ send({{"type", "delete_event"}, {"id", id}}); }

void RemoteBackend::addTimeReminder(const QString &title, const QString &dueAtIso)
{ send({{"type", "add_reminder"}, {"kind", "time"}, {"title", title}, {"dueAt", dueAtIso}}); }

void RemoteBackend::addGeoReminder(const QString &title, const QString &place,
                                   double lat, double lon)
{ send({{"type", "add_reminder"}, {"kind", "geo"}, {"title", title}, {"place", place},
        {"lat", lat}, {"lon", lon}}); }

void RemoteBackend::deleteReminder(int id)
{ send({{"type", "delete_reminder"}, {"id", id}}); }

void RemoteBackend::addHabit(const QString &name, double target,
                             const QString &unit, const QString &period)
{ send({{"type", "add_habit"}, {"name", name}, {"target", target},
        {"unit", unit}, {"period", period}}); }

void RemoteBackend::logHabit(int id, double value)
{ send({{"type", "log_habit"}, {"id", id}, {"value", value}}); }

void RemoteBackend::deleteHabit(int id)
{ send({{"type", "delete_habit"}, {"id", id}}); }

void RemoteBackend::newChat()
{ send({{"type", "new_chat"}}); }

void RemoteBackend::buildRouteText(const QString &from, const QString &to,
                                   const QString &mode)
{ send({{"type", "route_text"}, {"from", from}, {"to", to}, {"mode", mode}}); }
