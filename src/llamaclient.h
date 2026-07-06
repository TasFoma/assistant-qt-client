#ifndef LLAMACLIENT_H
#define LLAMACLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>

// Клиент llama-server. Использует OpenAI-совместимый эндпоинт
// /v1/chat/completions: сервер сам применяет шаблон чата модели
// с настоящими спецтокенами, поэтому модель не «дописывает» диалог
// за пользователя и не выводит служебную разметку.
class LlamaClient : public QObject
{
    Q_OBJECT
public:
    explicit LlamaClient(QObject *parent = nullptr);

    // Дополнительный контекст для системного промпта: дата, планы дня,
    // цели, подсказка о настроении. Устанавливается перед каждой отправкой.
    Q_INVOKABLE void setSystemContext(const QString &context);

    // history — список сообщений [{role: "user"|"assistant", text: "..."}]
    Q_INVOKABLE void sendMessageWithHistory(const QString &message,
                                            const QString &serverUrl,
                                            const QVariantList &history,
                                            const QString &modelName);

    Q_INVOKABLE void searchAndAnswer(const QString &question,
                                     const QString &serverUrl,
                                     const QVariantList &history,
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
    QVariantList m_lastHistory;
    QString m_lastModelName;
    QString m_systemContext;

    void logToFile(const QString &text);
    // Отправка в /v1/chat/completions; extraSystem — добавка к системному
    // промпту (например, результаты поиска)
    void sendChat(const QString &userContent,
                  const QString &serverUrl,
                  const QVariantList &history,
                  const QString &modelName,
                  double temperature,
                  const QString &extraSystem = QString());
};

#endif // LLAMACLIENT_H
