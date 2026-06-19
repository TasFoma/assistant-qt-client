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

LlamaClient::LlamaClient(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
    connect(m_manager, &QNetworkAccessManager::finished, this, &LlamaClient::onReplyFinished);
}

void LlamaClient::sendMessageWithHistory(const QString &message,
                                         const QString &serverUrl,
                                         const QString &history,
                                         const QString &modelName)
{
    QJsonObject request;

    // Системный промпт
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

    // ДОБАВЛЯЕМ ВЫБОР МОДЕЛИ
    if (!modelName.isEmpty()) {
        request["model"] = modelName;
    }

    QNetworkRequest req(QUrl(serverUrl + "/completion"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonDocument doc(request);
    QByteArray data = doc.toJson();

    logToFile("=== ЗАПРОС (модель: " + modelName + ") ===");
    logToFile(QString::fromUtf8(data));
    logToFile("");

    m_manager->post(req, data);
}

void LlamaClient::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QString error = reply->errorString();
        logToFile("=== ОШИБКА ===");
        logToFile(error);
        logToFile("");
        emit errorOccurred(error);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();

    logToFile("=== ОТВЕТ ===");
    logToFile(QString::fromUtf8(responseData));
    logToFile("");

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString content = obj["content"].toString();

        // Парсим токены и скорость
        int tokensPredicted = obj["tokens_predicted"].toInt();
        int tokensEvaluated = obj["tokens_evaluated"].toInt();

        // Время генерации (если есть)
        QJsonObject timings = obj["timings"].toObject();
        double predictedMs = timings["predicted_ms"].toDouble();
        double predictedPerSecond = (predictedMs > 0) ? (tokensPredicted / (predictedMs / 1000.0)) : 0;

        // Сохраняем метаданные в лог
        QString meta = QString("Токенов предсказано: %1, Оценено: %2, Скорость: %3 т/с")
                       .arg(tokensPredicted)
                       .arg(tokensEvaluated)
                       .arg(predictedPerSecond, 0, 'f', 2);
        logToFile("=== МЕТА ===");
        logToFile(meta);
        logToFile("");

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