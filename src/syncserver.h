#ifndef SYNCSERVER_H
#define SYNCSERVER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVariantMap>
#include <QSet>
#include <QHash>
#include <QPointer>

#include "commandparser.h"

class PlannerManager;
class ChatManager;
class NavigationManager;

// Сервер синхронизации с мобильным клиентом.
// Протокол: JSON-объекты, разделённые переводом строки, поверх TCP.
// Первым сообщением клиент обязан прислать {"type":"auth","token":"..."} —
// токен генерируется при первом запуске и хранится в настройках БД.
//
// Транспортное шифрование обеспечивает Tailscale (WireGuard):
// сервер предполагается доступным только через tailnet, поэтому
// подключаться следует по Tailscale-IP машины из любой точки мира.
class SyncServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(quint16 port READ port CONSTANT)
    Q_PROPERTY(QString token READ token CONSTANT)
    Q_PROPERTY(QString tailscaleIp READ tailscaleIp NOTIFY stateChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY stateChanged)
public:
    explicit SyncServer(PlannerManager *planner, ChatManager *chat,
                        NavigationManager *nav, QObject *parent = nullptr);

    Q_INVOKABLE bool start();

    bool running() const { return m_server.isListening(); }
    quint16 port() const { return m_port; }
    QString token() const { return m_token; }
    QString tailscaleIp() const { return m_tailscaleIp; }
    int clientCount() const { return m_clients.size(); }

signals:
    void stateChanged();
    void locationReceived(double lat, double lon);
    // Сообщение из переписки с телефона (уже сохранено в БД) —
    // чтобы чат на ПК показал его сразу
    void chatActivity(const QString &text, bool isUser);

private slots:
    void onNewConnection();
    void onDataChanged();

private:
    void handleLine(QTcpSocket *sock, const QByteArray &line);
    void send(QTcpSocket *sock, const QVariantMap &msg);
    void broadcast(const QVariantMap &msg);
    QVariantMap snapshot() const;
    void detectTailscaleIp();

    QTcpServer m_server;
    PlannerManager *m_planner;
    ChatManager *m_chat;
    NavigationManager *m_nav;
    // Маршрут, запрошенный с телефона: кому отправить результат
    QPointer<QTcpSocket> m_routeSock;
    double m_phoneLat = 0, m_phoneLon = 0;
    // Прокси чата: телефон шлёт "chat" → пересылаем в llama-server
    // (настройка llama_url) и возвращаем "chat_reply". Так чат работает
    // из любой точки мира через Tailscale, даже если llama на другой машине.
    QNetworkAccessManager m_net;
    QString m_llamaUrl;
    QString m_searxngUrl;
    // Команды с телефона («добавь задачу…») разбираются тем же парсером,
    // что и в чате на ПК — телефону не нужна своя копия логики
    CommandParser m_parser;
    QVariantMap runCommand(const QString &text);
    // Отправка чат-запроса в llama-server, ответ — на телефон и в историю
    void postChatToLlama(const QJsonObject &req, QPointer<QTcpSocket> psock,
                         qint64 id);
    QList<QTcpSocket *> m_clients;      // прошедшие аутентификацию
    QSet<QTcpSocket *> m_pendingAuth;   // подключились, но ещё не авторизовались
    QHash<QTcpSocket *, QByteArray> m_buffers;
    quint16 m_port = 8765;
    QString m_token;
    QString m_tailscaleIp;
};

#endif // SYNCSERVER_H
