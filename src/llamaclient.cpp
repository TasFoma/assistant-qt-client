#include "llamaclient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

LlamaClient::LlamaClient(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
    connect(m_manager, &QNetworkAccessManager::finished, this, &LlamaClient::onReplyFinished);

    m_searchManager = new QNetworkAccessManager(this);
    connect(m_searchManager, &QNetworkAccessManager::finished, this, &LlamaClient::onSearchReplyFinished);
}

void LlamaClient::setSystemContext(const QString &context)
{
    m_systemContext = context;
}

void LlamaClient::sendMessageWithHistory(const QString &message,
                                         const QString &serverUrl,
                                         const QVariantList &history,
                                         const QString &modelName)
{
    sendChat(message, serverUrl, history, modelName, 0.6);
}

void LlamaClient::sendChat(const QString &userContent,
                           const QString &serverUrl,
                           const QVariantList &history,
                           const QString &modelName,
                           double temperature,
                           const QString &extraSystem)
{
    QString systemPrompt =
        "Ты — «Мой спутник», личный ИИ-ассистент и друг. Отвечай умно, точно и по-русски.\n"
        "Правила:\n"
        "• Давай конкретные ответы — без воды и лишних оговорок.\n"
        "• Если вопрос фактический — отвечай фактами; если личный — с теплом и заботой.\n"
        "• Отвечай ТОЛЬКО за себя. Никогда не пиши реплики за пользователя.\n"
        "• Если не знаешь — честно скажи, не выдумывай.\n"
        "• Можно использовать уместные эмодзи, но не перебарщивай.";
    if (!m_systemContext.isEmpty())
        systemPrompt += "\n\nКонтекст:\n" + m_systemContext;
    if (!extraSystem.isEmpty())
        systemPrompt += "\n\n" + extraSystem;

    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    for (const QVariant &v : history) {
        const QVariantMap m = v.toMap();
        const QString role = m.value("role").toString();
        const QString text = m.value("text").toString();
        if (text.isEmpty() || (role != "user" && role != "assistant"))
            continue;
        messages.append(QJsonObject{{"role", role}, {"content", text}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", userContent}});

    QJsonObject request;
    request["messages"]       = messages;
    request["max_tokens"]     = 600;
    request["temperature"]    = temperature;
    request["top_p"]          = 0.9;
    request["top_k"]          = 40;
    request["repeat_penalty"] = 1.1; // без него слабые модели зацикливаются
    // Страховка для моделей со сломанным EOS: режем по ChatML-маркерам
    request["stop"] = QJsonArray::fromStringList({"<|im_end|>", "<|im_start|>"});
    request["stream"]         = false;
    if (!modelName.isEmpty())
        request["model"] = modelName;

    QNetworkRequest req(QUrl(serverUrl + "/v1/chat/completions"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 5 мин: при переключении 7B↔14B серверу нужно время перечитать модель с диска
    req.setTransferTimeout(300000);

    const QByteArray data = QJsonDocument(request).toJson();

    logToFile("=== ЗАПРОС (модель: " + modelName + ") ===");
    logToFile(QString::fromUtf8(data));
    logToFile("");

    m_manager->post(req, data);
}

void LlamaClient::searchAndAnswer(const QString &question,
                                  const QString &serverUrl,
                                  const QVariantList &history,
                                  const QString &modelName)
{
    m_lastQuestion = question;
    m_lastServerUrl = serverUrl;
    m_lastHistory = history;
    m_lastModelName = modelName;

    logToFile("=== ПОИСК В ИНТЕРНЕТЕ ===");
    logToFile("Запрос: " + question);

    QUrl url("http://localhost:8081/search");
    QUrlQuery query;
    query.addQueryItem("q", question);
    query.addQueryItem("format", "json");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10000); // 10 s — SearXNG локальный, должен отвечать мгновенно

    m_searchManager->get(request);
}

void LlamaClient::onSearchReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    // При любой проблеме с поиском — отвечаем без него
    auto fallback = [this](const QString &why) {
        logToFile("=== ПОИСК НЕ УДАЛСЯ: " + why + " ===");
        sendChat(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName, 0.6);
    };

    if (reply->error() != QNetworkReply::NoError)
        return fallback(reply->errorString());

    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fallback("ошибка разбора JSON");

    const QJsonArray results = doc.object()["results"].toArray();
    if (results.isEmpty())
        return fallback("нет результатов");

    QString searchResults;
    int count = 0;
    for (const QJsonValue &val : results) {
        if (count >= 3) break;
        const QJsonObject result = val.toObject();
        const QString title = result["title"].toString();
        QString content = result["content"].toString();
        const QString url = result["url"].toString();
        if (title.isEmpty()) continue;
        if (content.length() > 300)
            content = content.left(300) + "...";

        searchResults += QString("--- Результат %1 ---\n").arg(count + 1);
        searchResults += QString("Заголовок: %1\n").arg(title);
        if (!content.isEmpty())
            searchResults += QString("Содержание: %1\n").arg(content);
        searchResults += QString("Источник: %1\n\n").arg(url);
        count++;
    }
    if (searchResults.isEmpty())
        return fallback("нет подходящих результатов");

    const QString extraSystem =
        "Свежие результаты поиска в интернете (используй их для ответа, "
        "не выдумывай факты вне этих источников):\n" + searchResults;

    sendChat(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName,
             0.2, extraSystem);
}

void LlamaClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = reply->errorString();
        logToFile("=== ОШИБКА LLAMA ===");
        logToFile(error);
        logToFile("");
        emit errorOccurred(error);
        return;
    }

    const QByteArray responseData = reply->readAll();
    logToFile("=== ОТВЕТ ОТ LLAMA ===");
    logToFile(QString::fromUtf8(responseData));
    logToFile("");

    const QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        emit errorOccurred("Ошибка разбора ответа");
        return;
    }

    const QJsonObject obj = doc.object();
    QString content;
    const QJsonArray choices = obj["choices"].toArray();
    if (!choices.isEmpty())
        content = choices.first().toObject()["message"].toObject()["content"]
                      .toString().trimmed();

    // Страховка: если модель всё же выдала служебную разметку — обрезаем
    for (const QString &marker : {
             QStringLiteral("<|im_end|>"),
             QStringLiteral("<|im_start|>"),
             QStringLiteral("Пользователь:")}) {
        const int idx = content.indexOf(marker);
        if (idx != -1)
            content = content.left(idx).trimmed();
    }

    if (content.isEmpty())
        content = "(нет ответа)";

    emit responseReceived(content);
}

void LlamaClient::logToFile(const QString &text)
{
    QFile file("assistant_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " | ";
        out << text << "\n";
        file.close();
    }
}
