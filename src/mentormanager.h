#ifndef MENTORMANAGER_H
#define MENTORMANAGER_H

#include <QObject>
#include <QVariantList>

// Менторство и эмоциональная поддержка: простой анализ тона сообщений
// по словарю, дневник настроения в SQLite, подсказки-упражнения.
class MentorManager : public QObject
{
    Q_OBJECT
public:
    explicit MentorManager(QObject *parent = nullptr);

    // -1 негатив, 0 нейтрально, +1 позитив. Негатив и позитив пишутся в дневник.
    Q_INVOKABLE int analyzeMood(const QString &text);

    Q_INVOKABLE void logMood(int score, const QString &note = QString());
    Q_INVOKABLE QVariantList moodHistory(int days = 14);

    // Случайное поддерживающее упражнение (дыхание, прогулка и т.п.)
    Q_INVOKABLE QString supportSuggestion() const;
};

#endif // MENTORMANAGER_H
