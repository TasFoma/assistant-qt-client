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

signals:
    void responseReceived(const QString &response);
    void errorOccurred(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    void logToFile(const QString &text);
};

#endif // LLAMACLIENT_H