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

void LlamaClient::sendMessageWithHistory(const QString &message,
                                         const QString &serverUrl,
                                         const QString &history,
                                         const QString &modelName)
{
    QJsonObject request;
    QString systemPrompt =
        "Ты — мой личный ментор и друг. Ты помогаешь мне становиться лучше, спокойнее и организованнее. "
        "Ты помнишь, что я рассказываю о себе, и учитываешь это в ответах. "
        "Ты поддерживаешь меня в трудные моменты, помогаешь справляться с негативом и самобичеванием. "
        "Ты напоминаешь мне о важном, подбадриваешь и направляешь. "
        "Если я злюсь или расстроена — ты помогаешь мне успокоиться и посмотреть на ситуацию иначе. "
        "Ты не даёшь советов без спроса, но всегда готова помочь. "
        "Ты общаешься тепло, но без слащавости. "
        "Ты не придумываешь диалог за меня. Ты отвечаешь только от своего имени.";

    QString fullPrompt = systemPrompt + "\n\n" + history + "Пользователь: " + message + "\nАссистент:";

    request["prompt"] = fullPrompt;
    request["n_predict"] = 300;
    request["temperature"] = 0.7;
    request["repeat_penalty"] = 1.15;
    request["top_p"] = 0.9;
    request["stop"] = QJsonArray::fromStringList({"<|im_end|>", "Пользователь:"});

    if (!modelName.isEmpty()) {
        request["model"] = modelName;
    }

    QNetworkRequest req(QUrl(serverUrl + "/completion"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(120000); // 2 min — give the model time to load and generate

    QJsonDocument doc(request);
    QByteArray data = doc.toJson();

    logToFile("=== ЗАПРОС (модель: " + modelName + ") ===");
    logToFile(QString::fromUtf8(data));
    logToFile("");

    m_manager->post(req, data);
}

void LlamaClient::searchAndAnswer(const QString &question,
                                  const QString &serverUrl,
                                  const QString &history,
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
    request.setTransferTimeout(10000); // 10 s — SearXNG is local, should be instant

    m_searchManager->get(request);
}

void LlamaClient::onSearchReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QString error = "Ошибка поиска: " + reply->errorString();
        logToFile("=== ОШИБКА ПОИСКА ===");
        logToFile(error);
        logToFile("");
        sendMessageWithHistory(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    logToFile("=== СЫРОЙ ОТВЕТ ОТ SEARXNG ===");
    logToFile(QString::fromUtf8(data));
    logToFile("");

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        logToFile("=== ОШИБКА ПАРСИНГА JSON ===");
        logToFile("Ошибка: " + parseError.errorString());
        logToFile("Сырые данные: " + QString::fromUtf8(data));
        logToFile("");
        sendMessageWithHistory(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName);
        reply->deleteLater();
        return;
    }

    if (!doc.isObject()) {
        logToFile("=== ОШИБКА: Документ не объект ===");
        sendMessageWithHistory(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName);
        reply->deleteLater();
        return;
    }

    QJsonObject obj = doc.object();
    QJsonArray results = obj["results"].toArray();

    if (results.isEmpty()) {
        logToFile("=== НЕТ РЕЗУЛЬТАТОВ ===");
        sendMessageWithHistory(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName);
        reply->deleteLater();
        return;
    }

    // Формируем результаты с реальным содержанием
    QString searchResults;
    int count = 0;
    for (const QJsonValue &val : results) {
        if (count >= 3) break;
        QJsonObject result = val.toObject();
        QString title = result["title"].toString();
        QString content = result["content"].toString();
        QString url = result["url"].toString();

        if (title.isEmpty()) continue;

        if (content.length() > 300) {
            content = content.left(300) + "...";
        }

        searchResults += QString("--- Результат %1 ---\n")
                         .arg(count + 1);
        searchResults += QString("Заголовок: %1\n").arg(title);
        if (!content.isEmpty()) {
            searchResults += QString("Содержание: %1\n").arg(content);
        }
        searchResults += QString("Источник: %1\n").arg(url);
        searchResults += "\n";
        count++;
    }
 if (searchResults.isEmpty()) {
        logToFile("=== НЕТ ПОДХОДЯЩИХ РЕЗУЛЬТАТОВ ===");
        sendMessageWithHistory(m_lastQuestion, m_lastServerUrl, m_lastHistory, m_lastModelName);
        reply->deleteLater();
        return;
    }

    // ✅ КОРОТКИЙ И ЧЁТКИЙ ПРОМПТ
    QString systemPrompt =
        "Используй информацию из результатов поиска, чтобы ответить на вопрос.\n\n"
        "Результаты поиска:\n" + searchResults +
        "\nВопрос: " + m_lastQuestion +
        "\nОтвет (кратко, по фактам):";

    sendToLlama(systemPrompt, m_lastServerUrl, m_lastModelName);
    reply->deleteLater();
}


void LlamaClient::sendToLlama(const QString &prompt, const QString &serverUrl, const QString &modelName)
{
    QJsonObject request;
    request["prompt"] = prompt;
    request["n_predict"] = 300;
    request["temperature"] = 0.2;
    request["repeat_penalty"] = 1.2;
    request["top_p"] = 0.9;
    // Убираем stop, чтобы модель не обрывала ответ на "Пользователь:"
    // request["stop"] = QJsonArray::fromStringList({"<|im_end|>", "Пользователь:"});

    if (!modelName.isEmpty()) {
        request["model"] = modelName;
    }

    QNetworkRequest req(QUrl(serverUrl + "/completion"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(120000); // 2 min

    QJsonDocument doc(request);
    QByteArray data = doc.toJson();

    logToFile("=== ЗАПРОС В LLAMA (с результатами поиска) ===");
    logToFile(QString::fromUtf8(data));
    logToFile("");

    m_manager->post(req, data);
}

void LlamaClient::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QString error = reply->errorString();
        logToFile("=== ОШИБКА LLAMA ===");
        logToFile(error);
        logToFile("");
        emit errorOccurred(error);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    logToFile("=== ОТВЕТ ОТ LLAMA ===");
    logToFile(QString::fromUtf8(responseData));
    logToFile("");

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString content = obj["content"].toString();
        emit responseReceived(content);
    } else {
        emit errorOccurred("Ошибка разбора ответа");
    }

    reply->deleteLater();
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