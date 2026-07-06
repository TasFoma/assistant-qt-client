#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

// Синглтон-обёртка над единой SQLite-базой приложения.
// Хранит всё: чаты, сообщения, события, задачи, напоминания,
// привычки, маршруты, дневник настроения и настройки.
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager &instance();

    // Открыть БД и создать схему. fixedDir — папка для companion.db
    // (сервер передаёт свою папку: база переносима вместе с ним);
    // пусто — стандартная папка данных пользователя (%APPDATA%).
    bool init(const QString &fixedDir = QString());
    QSqlDatabase db() const;

    // Простое key-value хранилище настроек (таблица settings)
    QString setting(const QString &key, const QString &defaultValue = QString()) const;
    void    setSetting(const QString &key, const QString &value);

    QString databasePath() const { return m_path; }

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    void createSchema();

    QString m_path;
};

#endif // DATABASEMANAGER_H
