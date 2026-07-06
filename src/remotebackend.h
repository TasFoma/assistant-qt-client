#ifndef REMOTEBACKEND_H
#define REMOTEBACKEND_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Тонкий клиент companion_server: единственный источник данных для
// десктопного интерфейса. Повторяет сигнатуры прежних менеджеров
// (plannerManager/chatManager/navigationManager), чтобы QML почти
// не менялся: чтение — из локального кэша (обновляется по снапшотам
// сервера), операции — команды по TCP-протоколу.
class RemoteBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool authorized READ authorized NOTIFY stateChanged)
    Q_PROPERTY(QString serverHost READ serverHost WRITE setServerHost NOTIFY stateChanged)
public:
    explicit RemoteBackend(QObject *parent = nullptr);

    bool connected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }
    bool authorized() const { return m_authorized; }
    QString serverHost() const { return m_host; }
    void setServerHost(const QString &host);
    Q_INVOKABLE void setToken(const QString &token);
    Q_INVOKABLE QString token() const { return m_token; }
    Q_INVOKABLE void reconnect();

    // ── Планировщик (сигнатуры как у PlannerManager) ─────────────────
    Q_INVOKABLE QVariantList tasks() { return m_tasks; }
    Q_INVOKABLE QVariantList reminders() { return m_reminders; }
    Q_INVOKABLE QVariantList habits() { return m_habits; }
    Q_INVOKABLE QVariantList eventsForDate(const QString &date);
    Q_INVOKABLE QVariantList eventDaysInMonth(int year, int month);

    Q_INVOKABLE void addTask(const QString &title, const QString &dueDate, int priority = 1);
    Q_INVOKABLE void updateTask(int id, const QString &title, const QString &dueDate, int priority);
    Q_INVOKABLE void setTaskDone(int id, bool done);
    Q_INVOKABLE void deleteTask(int id);

    Q_INVOKABLE void addEvent(const QString &title, const QString &date,
                              const QString &time, const QString &description = QString(),
                              const QString &repeat = QString());
    Q_INVOKABLE void updateEvent(int id, const QString &title, const QString &date,
                                 const QString &time, const QString &description,
                                 const QString &repeat);
    Q_INVOKABLE void deleteEvent(int id);

    Q_INVOKABLE void addTimeReminder(const QString &title, const QString &dueAtIso);
    Q_INVOKABLE void addGeoReminder(const QString &title, const QString &place,
                                    double lat = 0, double lon = 0);
    Q_INVOKABLE void deleteReminder(int id);

    Q_INVOKABLE void addHabit(const QString &name, double target,
                              const QString &unit, const QString &period);
    Q_INVOKABLE void logHabit(int id, double value);
    Q_INVOKABLE void deleteHabit(int id);

    // ── Чат (сигнатуры как у ChatManager) ────────────────────────────
    Q_INVOKABLE QVariantList loadMessages() { return m_messages; }
    Q_INVOKABLE void newChat();
    // Единая точка отправки: команда → (если не команда) → нейросеть.
    // Эхо пользователя и ответы приходят сигналом messageAdded.
    Q_INVOKABLE void sendUserText(const QString &text, const QString &model);

    // ── Навигация ────────────────────────────────────────────────────
    Q_INVOKABLE QVariantMap stats() { return m_navStats; }
    Q_INVOKABLE void buildRouteText(const QString &from, const QString &to,
                                    const QString &mode);

signals:
    void stateChanged();
    void tasksChanged();
    void remindersChanged();
    void eventsChanged();
    void habitsChanged();
    void navStatsChanged();
    void historyLoaded();                       // пришла история чата
    void messageAdded(const QString &text, bool isUser); // новая реплика в ленту
    void reminderDue(const QString &title);     // для трея/звука/озвучки
    void chatError(const QString &error);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void send(const QVariantMap &msg);
    void handle(const QVariantMap &msg);
    void appendMessage(const QString &text, bool isUser);
    QVariantList historyForLlm(int maxChars = 4000) const;

    QTcpSocket m_socket;
    QTimer m_reconnectTimer;
    QByteArray m_buffer;
    QString m_host;
    QString m_token;
    bool m_authorized = false;

    // Кэш данных сервера
    QVariantList m_tasks, m_reminders, m_habits, m_messages;
    QVariantMap m_navStats;
    QHash<QString, QVariantList> m_eventsByDate;  // "yyyy-MM-dd" → события
    QHash<QString, QVariantList> m_daysByMonth;   // "yyyy-M" → дни с событиями
    QSet<QString> m_pendingDay, m_pendingMonth;   // уже запрошенные

    // Ожидание ответа на команду: id → исходный текст (для чата-фолбэка)
    qint64 m_seq = 0;
    QHash<qint64, QString> m_pendingCommands;
    QString m_model; // модель для чата-фолбэка
};

#endif // REMOTEBACKEND_H
