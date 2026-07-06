#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>
#include <QVariantList>

// История чатов: сообщения хранятся в SQLite, при запуске
// загружается последний активный чат. Умеет собирать ChatML-историю
// для передачи в llama.cpp.
class ChatManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentChatId READ currentChatId NOTIFY currentChatChanged)
public:
    explicit ChatManager(QObject *parent = nullptr);

    int currentChatId() const { return m_chatId; }

    Q_INVOKABLE QVariantList loadMessages();          // сообщения текущего чата
    Q_INVOKABLE void addMessage(const QString &text, bool isUser);
    Q_INVOKABLE void newChat();
    Q_INVOKABLE QVariantList chatList();
    Q_INVOKABLE void switchChat(int chatId);

    // История последних сообщений для /v1/chat/completions:
    // список [{role: "user"|"assistant", text: "..."}], суммарно
    // не длиннее maxChars. Вызывать ДО записи текущего сообщения.
    Q_INVOKABLE QVariantList historyMessages(int maxChars = 4000);

signals:
    void currentChatChanged();

private:
    void ensureChat();
    int m_chatId = -1;
};

#endif // CHATMANAGER_H
