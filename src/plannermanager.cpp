#include "plannermanager.h"
#include "databasemanager.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QtMath>
#include <QDebug>

PlannerManager::PlannerManager(QObject *parent) : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &PlannerManager::onTick);
    m_timer.start(5000); // каждые 5 секунд — напоминания срабатывают вовремя
}

// Происходит ли событие (с учётом повторения) в день d
static bool eventOccursOn(const QDate &eventDate, const QString &repeat, const QDate &d)
{
    if (!eventDate.isValid() || !d.isValid())
        return false;
    if (eventDate == d)
        return true;
    if (d < eventDate)
        return false;
    if (repeat == QStringLiteral("daily"))
        return true;
    if (repeat == QStringLiteral("weekly"))
        return eventDate.dayOfWeek() == d.dayOfWeek();
    if (repeat == QStringLiteral("monthly"))
        return eventDate.day() == d.day();
    return false;
}

// ── События ──────────────────────────────────────────────────────────

int PlannerManager::addEvent(const QString &title, const QString &date,
                             const QString &time, const QString &description,
                             const QString &repeat)
{
    QSqlQuery q;
    q.prepare("INSERT INTO events(title, date, time, description, repeat) VALUES(?,?,?,?,?)");
    q.addBindValue(title);
    q.addBindValue(date);
    q.addBindValue(time);
    q.addBindValue(description);
    q.addBindValue(repeat);
    if (!q.exec()) {
        qWarning() << "addEvent:" << q.lastError().text();
        return -1;
    }
    emit eventsChanged();
    return q.lastInsertId().toInt();
}

void PlannerManager::updateEvent(int id, const QString &title, const QString &date,
                                 const QString &time, const QString &description,
                                 const QString &repeat)
{
    QSqlQuery q;
    q.prepare("UPDATE events SET title=?, date=?, time=?, description=?, repeat=? "
              "WHERE id=?");
    q.addBindValue(title);
    q.addBindValue(date);
    q.addBindValue(time);
    q.addBindValue(description);
    q.addBindValue(repeat);
    q.addBindValue(id);
    q.exec();
    emit eventsChanged();
}

void PlannerManager::deleteEvent(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM events WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    emit eventsChanged();
}

QVariantList PlannerManager::eventsForDate(const QString &date)
{
    QVariantList list;
    const QDate d = QDate::fromString(date, "yyyy-MM-dd");
    QSqlQuery q("SELECT id, title, time, description, repeat, date FROM events "
                "ORDER BY time ASC");
    while (q.next()) {
        const QDate eventDate = QDate::fromString(q.value(5).toString(), "yyyy-MM-dd");
        if (!eventOccursOn(eventDate, q.value(4).toString(), d))
            continue;
        QVariantMap m;
        m["id"]          = q.value(0).toInt();
        m["title"]       = q.value(1).toString();
        m["time"]        = q.value(2).toString();
        m["description"] = q.value(3).toString();
        m["repeat"]      = q.value(4).toString();
        list << m;
    }
    return list;
}

QVariantList PlannerManager::eventDaysInMonth(int year, int month)
{
    // С учётом повторений: проверяем каждый день месяца по всем событиям
    QVariantList list;
    QList<QPair<QDate, QString>> events;
    QSqlQuery q("SELECT date, repeat FROM events");
    while (q.next())
        events.append({QDate::fromString(q.value(0).toString(), "yyyy-MM-dd"),
                       q.value(1).toString()});

    const int daysInMonth = QDate(year, month, 1).daysInMonth();
    for (int day = 1; day <= daysInMonth; ++day) {
        const QDate d(year, month, day);
        for (const auto &ev : events) {
            if (eventOccursOn(ev.first, ev.second, d)) {
                list << day;
                break;
            }
        }
    }
    return list;
}

// ── Задачи ───────────────────────────────────────────────────────────

int PlannerManager::addTask(const QString &title, const QString &dueDate, int priority)
{
    QSqlQuery q;
    q.prepare("INSERT INTO tasks(title, due_date, priority, done, created_at) VALUES(?,?,?,0,?)");
    q.addBindValue(title);
    q.addBindValue(dueDate);
    q.addBindValue(priority);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!q.exec()) {
        qWarning() << "addTask:" << q.lastError().text();
        return -1;
    }
    emit tasksChanged();
    return q.lastInsertId().toInt();
}

void PlannerManager::updateTask(int id, const QString &title, const QString &dueDate,
                                int priority)
{
    QSqlQuery q;
    q.prepare("UPDATE tasks SET title=?, due_date=?, priority=? WHERE id=?");
    q.addBindValue(title);
    q.addBindValue(dueDate);
    q.addBindValue(priority);
    q.addBindValue(id);
    q.exec();
    emit tasksChanged();
}

void PlannerManager::setTaskDone(int id, bool done)
{
    QSqlQuery q;
    q.prepare("UPDATE tasks SET done = ? WHERE id = ?");
    q.addBindValue(done ? 1 : 0);
    q.addBindValue(id);
    q.exec();
    emit tasksChanged();
}

void PlannerManager::deleteTask(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM tasks WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    emit tasksChanged();
}

QVariantList PlannerManager::tasks()
{
    QVariantList list;
    QSqlQuery q("SELECT id, title, due_date, priority, done FROM tasks "
                "ORDER BY done ASC, priority DESC, due_date ASC");
    while (q.next()) {
        QVariantMap m;
        m["id"]       = q.value(0).toInt();
        m["title"]    = q.value(1).toString();
        m["dueDate"]  = q.value(2).toString();
        m["priority"] = q.value(3).toInt();
        m["done"]     = q.value(4).toInt() != 0;
        list << m;
    }
    return list;
}

// ── Напоминания ──────────────────────────────────────────────────────

int PlannerManager::addTimeReminder(const QString &title, const QString &dueAtIso)
{
    QSqlQuery q;
    q.prepare("INSERT INTO reminders(title, type, due_at, fired) VALUES(?, 'time', ?, 0)");
    q.addBindValue(title);
    q.addBindValue(dueAtIso);
    if (!q.exec()) {
        qWarning() << "addTimeReminder:" << q.lastError().text();
        return -1;
    }
    emit remindersChanged();
    return q.lastInsertId().toInt();
}

int PlannerManager::addRecurringReminder(const QString &title, int minutes)
{
    if (minutes < 1)
        minutes = 60;
    QSqlQuery q;
    q.prepare("INSERT INTO reminders(title, type, due_at, fired, repeat_minutes) "
              "VALUES(?, 'time', ?, 0, ?)");
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentDateTime().addSecs(minutes * 60)
                       .toString(Qt::ISODate));
    q.addBindValue(minutes);
    if (!q.exec()) {
        qWarning() << "addRecurringReminder:" << q.lastError().text();
        return -1;
    }
    emit remindersChanged();
    return q.lastInsertId().toInt();
}

int PlannerManager::addGeoReminder(const QString &title, const QString &place,
                                   double lat, double lon)
{
    QSqlQuery q;
    q.prepare("INSERT INTO reminders(title, type, place, lat, lon, fired) "
              "VALUES(?, 'geo', ?, ?, ?, 0)");
    q.addBindValue(title);
    q.addBindValue(place);
    q.addBindValue(lat);
    q.addBindValue(lon);
    if (!q.exec()) {
        qWarning() << "addGeoReminder:" << q.lastError().text();
        return -1;
    }
    emit remindersChanged();
    return q.lastInsertId().toInt();
}

void PlannerManager::deleteReminder(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM reminders WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    emit remindersChanged();
}

QVariantList PlannerManager::reminders()
{
    QVariantList list;
    QSqlQuery q("SELECT id, title, type, due_at, place, repeat_minutes FROM reminders "
                "WHERE fired = 0 ORDER BY due_at ASC");
    while (q.next()) {
        QVariantMap m;
        m["id"]    = q.value(0).toInt();
        m["title"] = q.value(1).toString();
        m["type"]  = q.value(2).toString();
        const QDateTime dt = QDateTime::fromString(q.value(3).toString(), Qt::ISODate);
        const int repeat = q.value(5).toInt();
        m["dueAtIso"] = q.value(3).toString(); // для планирования уведомлений на телефоне
        m["repeatMinutes"] = repeat;
        m["dueAt"] = repeat > 0
            ? QStringLiteral("каждые %1 мин (след. %2)")
                  .arg(repeat)
                  .arg(dt.isValid() ? dt.toString("hh:mm") : QString())
            : (dt.isValid() ? dt.toString("dd.MM hh:mm") : QString());
        m["place"] = q.value(4).toString();
        list << m;
    }
    return list;
}

void PlannerManager::checkGeoReminders(double lat, double lon)
{
    // Радиус срабатывания — 200 метров
    QSqlQuery q("SELECT id, title, lat, lon FROM reminders "
                "WHERE fired = 0 AND type = 'geo' AND lat != 0");
    QList<QPair<int, QString>> due;
    while (q.next()) {
        const double dLat = (q.value(2).toDouble() - lat) * 111320.0;
        const double dLon = (q.value(3).toDouble() - lon) * 111320.0
                            * qCos(qDegreesToRadians(lat));
        if (qSqrt(dLat * dLat + dLon * dLon) < 200.0)
            due.append({q.value(0).toInt(), q.value(1).toString()});
    }
    for (const auto &r : due) {
        QSqlQuery upd;
        upd.prepare("UPDATE reminders SET fired = 1 WHERE id = ?");
        upd.addBindValue(r.first);
        upd.exec();
        emit reminderDue(r.second);
    }
    if (!due.isEmpty())
        emit remindersChanged();
}

// ── Привычки ─────────────────────────────────────────────────────────

int PlannerManager::addHabit(const QString &name, double target,
                             const QString &unit, const QString &period)
{
    QSqlQuery q;
    q.prepare("INSERT INTO habits(name, target, unit, period) VALUES(?,?,?,?)");
    q.addBindValue(name);
    q.addBindValue(target);
    q.addBindValue(unit);
    q.addBindValue(period);
    if (!q.exec())
        return -1;
    emit habitsChanged();
    return q.lastInsertId().toInt();
}

void PlannerManager::logHabit(int id, double value)
{
    QSqlQuery q;
    q.prepare("INSERT INTO habit_log(habit_id, date, value) VALUES(?,?,?)");
    q.addBindValue(id);
    q.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    q.addBindValue(value);
    q.exec();
    emit habitsChanged();
}

void PlannerManager::deleteHabit(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM habits WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    QSqlQuery q2;
    q2.prepare("DELETE FROM habit_log WHERE habit_id = ?");
    q2.addBindValue(id);
    q2.exec();
    emit habitsChanged();
}

QVariantList PlannerManager::habits()
{
    QVariantList list;
    const QDate today = QDate::currentDate();
    const QString weekStart =
        today.addDays(1 - today.dayOfWeek()).toString("yyyy-MM-dd");

    QSqlQuery q("SELECT id, name, target, unit, period FROM habits ORDER BY id");
    while (q.next()) {
        QVariantMap m;
        const int id = q.value(0).toInt();
        m["id"]     = id;
        m["name"]   = q.value(1).toString();
        m["target"] = q.value(2).toDouble();
        m["unit"]   = q.value(3).toString();
        m["period"] = q.value(4).toString();

        QSqlQuery p;
        if (m["period"].toString() == "week") {
            p.prepare("SELECT COALESCE(SUM(value),0) FROM habit_log "
                      "WHERE habit_id = ? AND date >= ?");
            p.addBindValue(id);
            p.addBindValue(weekStart);
        } else {
            p.prepare("SELECT COALESCE(SUM(value),0) FROM habit_log "
                      "WHERE habit_id = ? AND date = ?");
            p.addBindValue(id);
            p.addBindValue(today.toString("yyyy-MM-dd"));
        }
        double progress = 0;
        if (p.exec() && p.next())
            progress = p.value(0).toDouble();
        m["progress"] = progress;
        list << m;
    }
    return list;
}

// ── Профиль пользователя (память ассистента) ─────────────────────────

void PlannerManager::rememberFact(const QString &fact)
{
    auto &dbm = DatabaseManager::instance();
    QString info = dbm.setting("profile_info");
    if (!info.isEmpty())
        info += "\n";
    info += "• " + fact.trimmed();
    // Не даём профилю разрастись бесконечно — храним последние 30 фактов
    QStringList lines = info.split('\n');
    while (lines.size() > 30)
        lines.removeFirst();
    dbm.setSetting("profile_info", lines.join('\n'));
}

QString PlannerManager::profileInfo() const
{
    return DatabaseManager::instance().setting("profile_info");
}

// ── Контекст и проактивность ─────────────────────────────────────────

static QString ruDate(const QDate &d)
{
    static const char *days[] = {"понедельник", "вторник", "среда", "четверг",
                                 "пятница", "суббота", "воскресенье"};
    static const char *months[] = {"января", "февраля", "марта", "апреля", "мая",
                                   "июня", "июля", "августа", "сентября",
                                   "октября", "ноября", "декабря"};
    return QStringLiteral("%1, %2 %3 %4")
        .arg(days[d.dayOfWeek() - 1])
        .arg(d.day())
        .arg(months[d.month() - 1])
        .arg(d.year());
}

QString PlannerManager::dailyContext()
{
    const QDate today = QDate::currentDate();
    QString ctx = "Сегодня " + ruDate(today) + ", время "
                  + QTime::currentTime().toString("hh:mm") + ".\n";

    const QString profile = profileInfo();
    if (!profile.isEmpty())
        ctx += "Что ты знаешь о пользователе:\n" + profile + "\n";

    const QVariantList evs = eventsForDate(today.toString("yyyy-MM-dd"));
    if (!evs.isEmpty()) {
        QStringList parts;
        for (const QVariant &v : evs) {
            const QVariantMap m = v.toMap();
            parts << (m["time"].toString().isEmpty()
                          ? m["title"].toString()
                          : m["time"].toString() + " — " + m["title"].toString());
        }
        ctx += "События сегодня: " + parts.join("; ") + ".\n";
    }

    QStringList open;
    for (const QVariant &v : tasks()) {
        const QVariantMap m = v.toMap();
        if (!m["done"].toBool())
            open << m["title"].toString();
        if (open.size() >= 5) break;
    }
    if (!open.isEmpty())
        ctx += "Открытые задачи: " + open.join("; ") + ".\n";

    QStringList hs;
    for (const QVariant &v : habits()) {
        const QVariantMap m = v.toMap();
        hs << QStringLiteral("%1: %2/%3 %4")
                  .arg(m["name"].toString())
                  .arg(m["progress"].toDouble())
                  .arg(m["target"].toDouble())
                  .arg(m["unit"].toString());
    }
    if (!hs.isEmpty())
        ctx += "Цели пользователя: " + hs.join("; ") + ".\n";

    return ctx;
}

QString PlannerManager::morningGreetingIfNeeded()
{
    auto &dbm = DatabaseManager::instance();
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (dbm.setting("last_greeting_date") == today)
        return QString();
    dbm.setSetting("last_greeting_date", today);

    QString msg = "Доброе утро! ☀️ Сегодня " + ruDate(QDate::currentDate()) + ".";

    const QVariantList evs = eventsForDate(today);
    if (!evs.isEmpty()) {
        msg += "\n\n📅 Планы на сегодня:";
        for (const QVariant &v : evs) {
            const QVariantMap m = v.toMap();
            msg += "\n• " + (m["time"].toString().isEmpty()
                                 ? m["title"].toString()
                                 : m["time"].toString() + " — " + m["title"].toString());
        }
    }

    QStringList open;
    for (const QVariant &v : tasks()) {
        const QVariantMap m = v.toMap();
        if (!m["done"].toBool())
            open << m["title"].toString();
        if (open.size() >= 5) break;
    }
    if (!open.isEmpty()) {
        msg += "\n\n✅ Задачи:";
        for (const QString &t : open)
            msg += "\n• " + t;
    }

    if (evs.isEmpty() && open.isEmpty())
        msg += " Планов пока нет — отличный день, чтобы что-то задумать. Чем займёмся?";

    return msg;
}

void PlannerManager::onTick()
{
    const QDateTime now = QDateTime::currentDateTime();

    // 1. Напоминания по времени (повторяющиеся переносятся на следующий раз)
    QSqlQuery q("SELECT id, title, due_at, repeat_minutes FROM reminders "
                "WHERE fired = 0 AND type = 'time'");
    struct Due { int id; QString title; int repeat; };
    QList<Due> due;
    while (q.next()) {
        const QDateTime dt = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        if (dt.isValid() && dt <= now)
            due.append({q.value(0).toInt(), q.value(1).toString(),
                        q.value(3).toInt()});
    }
    for (const auto &r : due) {
        QSqlQuery upd;
        if (r.repeat > 0) {
            upd.prepare("UPDATE reminders SET due_at = ? WHERE id = ?");
            upd.addBindValue(now.addSecs(r.repeat * 60).toString(Qt::ISODate));
            upd.addBindValue(r.id);
        } else {
            upd.prepare("UPDATE reminders SET fired = 1 WHERE id = ?");
            upd.addBindValue(r.id);
        }
        upd.exec();
        qInfo() << "Сработало напоминание:" << r.title;
        emit reminderDue(r.title);
    }
    if (!due.isEmpty())
        emit remindersChanged();

    // 2. Напоминания о событиях за 30 минут до начала (с учётом повторов)
    {
        auto &dbm = DatabaseManager::instance();
        const QVariantList todays = eventsForDate(now.date().toString("yyyy-MM-dd"));
        for (const QVariant &v : todays) {
            const QVariantMap ev = v.toMap();
            const QTime t = QTime::fromString(ev["time"].toString(), "HH:mm");
            if (!t.isValid())
                continue;
            const int mins = now.time().secsTo(t) / 60;
            if (mins > 0 && mins <= 30) {
                const QString key = "event_notified_" + ev["id"].toString()
                                    + "_" + now.date().toString("yyyyMMdd");
                if (dbm.setting(key).isEmpty()) {
                    dbm.setSetting(key, "1");
                    qInfo() << "Скоро событие:" << ev["title"].toString();
                    emit reminderDue(QStringLiteral("Через %1 мин: %2")
                                         .arg(mins)
                                         .arg(ev["title"].toString()));
                }
            }
        }
    }

    // 3. Утреннее приветствие после 8:00 (один раз в день; сервер
    //    рассылает его всем клиентам)
    if (now.time().hour() >= 8) {
        const QString greeting = morningGreetingIfNeeded();
        if (!greeting.isEmpty()) {
            emit checkInDue(greeting);
            emit morningGreeted();
        }
    }

    // 4. Вечерний чек-ин в 20:00
    if (now.time().hour() == 20) {
        auto &dbm = DatabaseManager::instance();
        const QString today = now.date().toString("yyyy-MM-dd");
        if (dbm.setting("last_checkin_date") != today) {
            dbm.setSetting("last_checkin_date", today);
            emit checkInDue("Как прошёл день? 🌙 Расскажи, какое настроение "
                            "и что удалось сделать — я запишу прогресс.");
        }
    }
}
