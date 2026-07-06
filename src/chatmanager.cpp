#include "chatmanager.h"
#include "databasemanager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>
#include <QDebug>

ChatManager::ChatManager(QObject *parent) : QObject(parent)
{
    ensureChat();
}

void ChatManager::ensureChat()
{
    auto &dbm = DatabaseManager::instance();
    int saved = dbm.setting("current_chat", "-1").toInt();

    if (saved > 0) {
        QSqlQuery q;
        q.prepare("SELECT id FROM chats WHERE id = ?");
        q.addBindValue(saved);
        if (q.exec() && q.next()) {
            m_chatId = saved;
            return;
        }
    }

    // Нет активного чата — берём последний или создаём новый
    QSqlQuery q("SELECT id FROM chats ORDER BY id DESC LIMIT 1");
    if (q.next()) {
        m_chatId = q.value(0).toInt();
    } else {
        QSqlQuery ins;
        ins.prepare("INSERT INTO chats(title, created_at) VALUES(?, ?)");
        ins.addBindValue(QStringLiteral("Чат от %1")
                             .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy")));
        ins.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        ins.exec();
        m_chatId = ins.lastInsertId().toInt();
    }
    dbm.setSetting("current_chat", QString::number(m_chatId));
}

QVariantList ChatManager::loadMessages()
{
    QVariantList list;
    QSqlQuery q;
    q.prepare("SELECT role, text, created_at FROM messages "
              "WHERE chat_id = ? ORDER BY id ASC");
    q.addBindValue(m_chatId);
    if (!q.exec()) {
        qWarning() << "loadMessages:" << q.lastError().text();
        return list;
    }
    while (q.next()) {
        QVariantMap m;
        m["isUser"] = (q.value(0).toString() == "user");
        m["text"]   = q.value(1).toString();
        m["time"]   = QDateTime::fromString(q.value(2).toString(), Qt::ISODate)
                          .toString("hh:mm");
        list << m;
    }
    return list;
}

void ChatManager::addMessage(const QString &text, bool isUser)
{
    QSqlQuery q;
    q.prepare("INSERT INTO messages(chat_id, role, text, created_at) VALUES(?, ?, ?, ?)");
    q.addBindValue(m_chatId);
    q.addBindValue(isUser ? "user" : "assistant");
    q.addBindValue(text);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!q.exec())
        qWarning() << "addMessage:" << q.lastError().text();
}

void ChatManager::newChat()
{
    QSqlQuery ins;
    ins.prepare("INSERT INTO chats(title, created_at) VALUES(?, ?)");
    ins.addBindValue(QStringLiteral("Чат от %1")
                         .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm")));
    ins.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    ins.exec();
    m_chatId = ins.lastInsertId().toInt();
    DatabaseManager::instance().setSetting("current_chat", QString::number(m_chatId));
    emit currentChatChanged();
}

QVariantList ChatManager::chatList()
{
    QVariantList list;
    QSqlQuery q("SELECT id, title, created_at FROM chats ORDER BY id DESC");
    while (q.next()) {
        QVariantMap m;
        m["id"]    = q.value(0).toInt();
        m["title"] = q.value(1).toString();
        list << m;
    }
    return list;
}

void ChatManager::switchChat(int chatId)
{
    m_chatId = chatId;
    DatabaseManager::instance().setSetting("current_chat", QString::number(chatId));
    emit currentChatChanged();
}

QVariantList ChatManager::historyMessages(int maxChars)
{
    // Последние сообщения в обратном порядке, набираем пока влезает
    QSqlQuery q;
    q.prepare("SELECT role, text FROM messages WHERE chat_id = ? ORDER BY id DESC LIMIT 40");
    q.addBindValue(m_chatId);
    if (!q.exec())
        return {};

    QVariantList turns;
    int total = 0;
    while (q.next()) {
        const QString role = q.value(0).toString();
        const QString text = q.value(1).toString();
        if (total + text.length() > maxChars)
            break;
        total += text.length();
        QVariantMap m;
        m["role"] = role;
        m["text"] = text;
        turns.prepend(m);
    }
    return turns;
}
