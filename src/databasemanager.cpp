#include "databasemanager.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDebug>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

bool DatabaseManager::init(const QString &fixedDir)
{
    const QString dir = fixedDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : fixedDir;
    QDir().mkpath(dir);
    m_path = dir + "/companion.db";

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(m_path);
    if (!db.open()) {
        qCritical() << "Не удалось открыть БД:" << db.lastError().text();
        return false;
    }

    createSchema();
    qInfo() << "БД открыта:" << m_path;
    return true;
}

QSqlDatabase DatabaseManager::db() const
{
    return QSqlDatabase::database();
}

void DatabaseManager::createSchema()
{
    const QStringList ddl = {
        "CREATE TABLE IF NOT EXISTS chats ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL DEFAULT 'Новый чат',"
        " created_at TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS messages ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " chat_id INTEGER NOT NULL REFERENCES chats(id) ON DELETE CASCADE,"
        " role TEXT NOT NULL,"                 // 'user' | 'assistant'
        " text TEXT NOT NULL,"
        " created_at TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS events ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " date TEXT NOT NULL,"                 // yyyy-MM-dd
        " time TEXT,"                          // HH:mm (может быть пустым)
        " description TEXT,"
        " repeat TEXT DEFAULT '')",            // '' | 'daily' | 'weekly' | 'monthly'

        "CREATE TABLE IF NOT EXISTS tasks ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " due_date TEXT,"                      // yyyy-MM-dd
        " priority INTEGER NOT NULL DEFAULT 1,"// 0 низкий, 1 обычный, 2 высокий
        " done INTEGER NOT NULL DEFAULT 0,"
        " created_at TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS reminders ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " type TEXT NOT NULL,"                 // 'time' | 'geo'
        " due_at TEXT,"                        // ISO datetime для type='time'
        " place TEXT,"                         // название места для type='geo'
        " lat REAL, lon REAL,"
        " fired INTEGER NOT NULL DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS habits ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " target REAL NOT NULL DEFAULT 1,"     // целевое значение за период
        " unit TEXT DEFAULT '',"               // 'мин', 'раз', 'стр' ...
        " period TEXT NOT NULL DEFAULT 'day')",// 'day' | 'week'

        "CREATE TABLE IF NOT EXISTS habit_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " habit_id INTEGER NOT NULL REFERENCES habits(id) ON DELETE CASCADE,"
        " date TEXT NOT NULL,"                 // yyyy-MM-dd
        " value REAL NOT NULL)",

        "CREATE TABLE IF NOT EXISTS routes ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " created_at TEXT NOT NULL,"
        " origin TEXT NOT NULL,"
        " destination TEXT NOT NULL,"
        " distance_m REAL NOT NULL,"
        " duration_s REAL NOT NULL,"
        " mode TEXT NOT NULL DEFAULT 'foot')",

        "CREATE TABLE IF NOT EXISTS mood_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " date TEXT NOT NULL,"                 // yyyy-MM-dd HH:mm
        " score INTEGER NOT NULL,"             // -1 негатив, 0 нейтрально, 1 позитив
        " note TEXT)",

        "CREATE TABLE IF NOT EXISTS settings ("
        " key TEXT PRIMARY KEY,"
        " value TEXT)"
    };

    QSqlQuery q;
    for (const QString &stmt : ddl) {
        if (!q.exec(stmt))
            qCritical() << "Ошибка создания схемы:" << q.lastError().text() << stmt;
    }

    // Миграции: молча падают, если колонка уже существует
    QSqlQuery mig;
    mig.exec("ALTER TABLE reminders ADD COLUMN repeat_minutes INTEGER NOT NULL DEFAULT 0");
}

QString DatabaseManager::setting(const QString &key, const QString &defaultValue) const
{
    QSqlQuery q;
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

void DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q;
    q.prepare("INSERT INTO settings(key, value) VALUES(?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        qWarning() << "Ошибка записи настройки:" << q.lastError().text();
}
