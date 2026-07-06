#include "mentormanager.h"
#include "databasemanager.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSqlQuery>
#include <QVariantMap>

MentorManager::MentorManager(QObject *parent) : QObject(parent) {}

int MentorManager::analyzeMood(const QString &text)
{
    static const QStringList negative = {
        "груст", "тоск", "плохо", "устал", "устала", "тревог", "боюсь", "страшно",
        "зол", "зла", "злюсь", "злая", "бесит", "ненавижу", "одинок", "плачу",
        "депресс", "тяжело", "обидно", "разочаров", "выгора", "паник", "стресс",
        "не могу больше", "всё надоело", "хочется плакать"
    };
    static const QStringList positive = {
        "отлично", "супер", "рад", "рада", "счастлив", "здорово", "класс",
        "прекрасно", "получилось", "ура", "люблю", "восторг", "горжусь"
    };

    const QString lower = text.toLower();
    int score = 0;
    for (const QString &w : negative)
        if (lower.contains(w)) { score = -1; break; }
    if (score == 0)
        for (const QString &w : positive)
            if (lower.contains(w)) { score = 1; break; }

    if (score != 0)
        logMood(score, text.left(120));
    return score;
}

void MentorManager::logMood(int score, const QString &note)
{
    QSqlQuery q;
    q.prepare("INSERT INTO mood_log(date, score, note) VALUES(?,?,?)");
    q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));
    q.addBindValue(score);
    q.addBindValue(note);
    q.exec();
}

QVariantList MentorManager::moodHistory(int days)
{
    QVariantList list;
    const QString since = QDateTime::currentDateTime()
                              .addDays(-days)
                              .toString("yyyy-MM-dd HH:mm");
    QSqlQuery q;
    q.prepare("SELECT date, score, note FROM mood_log WHERE date >= ? ORDER BY date ASC");
    q.addBindValue(since);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap m;
            m["date"]  = q.value(0).toString();
            m["score"] = q.value(1).toInt();
            m["note"]  = q.value(2).toString();
            list << m;
        }
    }
    return list;
}

QString MentorManager::supportSuggestion() const
{
    static const QStringList suggestions = {
        "Попробуй дыхание «4-7-8»: вдох на 4 счёта, задержка на 7, выдох на 8. "
        "Три-четыре цикла — и станет спокойнее. 🌬️",
        "Короткая прогулка на 10 минут отлично перезагружает голову. "
        "Хочешь, я построю маршрут? 🚶",
        "Попробуй технику «5-4-3-2-1»: назови 5 вещей, которые видишь, 4 — которые "
        "слышишь, 3 — которых касаешься, 2 запаха и 1 вкус. Это возвращает в момент.",
        "Выпей стакан воды и потянись пару минут — телу это помогает не меньше, чем голове. 💧",
        "Иногда помогает просто выговориться. Я рядом — расскажи, что случилось?"
    };
    const int idx = QRandomGenerator::global()->bounded(suggestions.size());
    return suggestions.at(idx);
}
