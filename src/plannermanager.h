#ifndef PLANNERMANAGER_H
#define PLANNERMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Календарь, задачи, напоминания и привычки.
// Таймер каждые 30 секунд проверяет напоминания по времени
// и инициирует проактивные сообщения (утреннее приветствие,
// вечерний чек-ин настроения в 20:00).
class PlannerManager : public QObject
{
    Q_OBJECT
public:
    explicit PlannerManager(QObject *parent = nullptr);

    // ── События календаря ────────────────────────────────────────────
    Q_INVOKABLE int  addEvent(const QString &title, const QString &date,
                              const QString &time, const QString &description = QString(),
                              const QString &repeat = QString());
    Q_INVOKABLE void updateEvent(int id, const QString &title, const QString &date,
                                 const QString &time, const QString &description,
                                 const QString &repeat);
    Q_INVOKABLE void deleteEvent(int id);
    Q_INVOKABLE QVariantList eventsForDate(const QString &date);   // yyyy-MM-dd
    Q_INVOKABLE QVariantList eventDaysInMonth(int year, int month); // список чисел месяца с событиями

    // ── Задачи ───────────────────────────────────────────────────────
    Q_INVOKABLE int  addTask(const QString &title, const QString &dueDate, int priority = 1);
    Q_INVOKABLE void updateTask(int id, const QString &title, const QString &dueDate,
                                int priority);
    Q_INVOKABLE void setTaskDone(int id, bool done);
    Q_INVOKABLE void deleteTask(int id);
    Q_INVOKABLE QVariantList tasks();

    // ── Напоминания ──────────────────────────────────────────────────
    Q_INVOKABLE int  addTimeReminder(const QString &title, const QString &dueAtIso);
    // Повторяющееся: «пить воду каждые 45 минут» — срабатывает и переносится
    Q_INVOKABLE int  addRecurringReminder(const QString &title, int minutes);
    Q_INVOKABLE int  addGeoReminder(const QString &title, const QString &place,
                                    double lat = 0, double lon = 0);
    Q_INVOKABLE void deleteReminder(int id);
    Q_INVOKABLE QVariantList reminders();
    // Вызывается при получении координат с телефона (через SyncServer)
    Q_INVOKABLE void checkGeoReminders(double lat, double lon);

    // ── Привычки / цели ──────────────────────────────────────────────
    Q_INVOKABLE int  addHabit(const QString &name, double target,
                              const QString &unit, const QString &period);
    Q_INVOKABLE void logHabit(int id, double value);
    Q_INVOKABLE void deleteHabit(int id);
    Q_INVOKABLE QVariantList habits(); // с прогрессом за текущий период

    // ── Профиль пользователя (память ассистента) ─────────────────────
    // «Запомни: меня зовут Настя» → факт сохраняется и попадает
    // в системный промпт каждого запроса
    Q_INVOKABLE void rememberFact(const QString &fact);
    Q_INVOKABLE QString profileInfo() const;

    // ── Контекст и проактивность ────────────────────────────────────
    // Краткая сводка (дата, события, задачи, привычки) для системного промпта
    Q_INVOKABLE QString dailyContext();
    // Утреннее приветствие: непустая строка один раз в день
    Q_INVOKABLE QString morningGreetingIfNeeded();

signals:
    void eventsChanged();
    void tasksChanged();
    void remindersChanged();
    void habitsChanged();
    void reminderDue(const QString &title);   // сработало напоминание
    void checkInDue(const QString &message);  // проактивное сообщение ассистента
    void morningGreeted();                    // утреннее приветствие отправлено

private slots:
    void onTick();

private:
    QTimer m_timer;
};

#endif // PLANNERMANAGER_H
