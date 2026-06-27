#ifndef LLAMACLIENT_H
#define LLAMACLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class LlamaClient : public QObject
{
    Q_OBJECT
public:
    explicit LlamaClient(QObject *parent = nullptr);

    Q_INVOKABLE void sendMessageWithHistory(const QString &message,
                                            const QString &serverUrl,
                                            const QString &history,
                                            const QString &modelName);

    Q_INVOKABLE void searchAndAnswer(const QString &question,
                                     const QString &serverUrl,
                                     const QString &history,
                                     const QString &modelName);

signals:
    void responseReceived(const QString &response);
    void errorOccurred(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onSearchReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QNetworkAccessManager *m_searchManager;
    QString m_lastQuestion;
    QString m_lastServerUrl;
    QString m_lastHistory;
    QString m_lastModelName;
    void logToFile(const QString &text);
    void sendToLlama(const QString &prompt, const QString &serverUrl, const QString &modelName);
};

#endif // LLAMACLIENT_H