#include "commandparser.h"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>

CommandParser::CommandParser(QObject *parent) : QObject(parent) {}

static const QRegularExpression::PatternOptions kOpts =
    QRegularExpression::CaseInsensitiveOption |
    QRegularExpression::UseUnicodePropertiesOption;

QString CommandParser::tidy(QString s)
{
    s.replace(QRegularExpression("\\s{2,}"), " ");
    s = s.trimmed();
    while (!s.isEmpty() && (s.endsWith(',') || s.endsWith('.') || s.endsWith('?')))
        s.chop(1);
    // убрать служебные начала: «что», «о том, что», «мне»
    static const QRegularExpression lead(
        "^(?:мне\\s+|о\\s+том,?\\s+что\\s+|что\\s+(?:надо|нужно)\\s+)", kOpts);
    s.remove(lead);
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s.trimmed();
}

CommandParser::When CommandParser::extractWhen(const QString &text) const
{
    When w;
    w.cleaned = text;
    const QDate today = QDate::currentDate();

    // «через N минут», «через минуту», «через полчаса», «через пару/пять минут»
    static const QRegularExpression relRe(
        "через\\s+(?:(\\d+)|(одну|две|пару|три|четыре|пять|шесть|семь|восемь|девять|"
        "десять|пятнадцать|двадцать|тридцать|сорок|пятьдесят))?\\s*"
        "(полчаса|минут\\w*|час\\w*|дн(?:я|ей)|день)", kOpts);
    auto rel = relRe.match(w.cleaned);
    if (rel.hasMatch()) {
        int n = 1; // «через минуту», «через час» — без числа
        if (!rel.captured(1).isEmpty()) {
            n = rel.captured(1).toInt();
        } else if (!rel.captured(2).isEmpty()) {
            static const QHash<QString, int> words = {
                {"одну", 1}, {"две", 2}, {"пару", 2}, {"три", 3}, {"четыре", 4},
                {"пять", 5}, {"шесть", 6}, {"семь", 7}, {"восемь", 8}, {"девять", 9},
                {"десять", 10}, {"пятнадцать", 15}, {"двадцать", 20},
                {"тридцать", 30}, {"сорок", 40}, {"пятьдесят", 50}};
            n = words.value(rel.captured(2).toLower(), 1);
        }
        const QString unit = rel.captured(3).toLower();
        QDateTime dt = QDateTime::currentDateTime();
        if (unit == QStringLiteral("полчаса"))
            dt = dt.addSecs(30 * 60);
        else if (unit.startsWith(QStringLiteral("минут")))
            dt = dt.addSecs(n * 60);
        else if (unit.startsWith(QStringLiteral("час")))
            dt = dt.addSecs(n * 3600);
        else
            dt = dt.addDays(n);
        w.date = dt.date();
        w.time = dt.time();
        w.cleaned.remove(rel.capturedStart(), rel.capturedLength());
        return w;
    }

    // Дни недели: «в пятницу», «во вторник» (ближайший будущий)
    static const QRegularExpression wdRe(
        "\\b(?:в|во)\\s+(понедельник|вторник|среду|четверг|пятницу|субботу|воскресенье)\\b",
        kOpts);
    auto wd = wdRe.match(w.cleaned);
    if (wd.hasMatch()) {
        static const QStringList names = {"понедельник", "вторник", "среду", "четверг",
                                          "пятницу", "субботу", "воскресенье"};
        const int target = names.indexOf(wd.captured(1).toLower()) + 1;
        int diff = target - today.dayOfWeek();
        if (diff < 0)
            diff += 7; // прошедший день недели → на следующей неделе
        w.date = today.addDays(diff);
        w.cleaned.remove(wd.capturedStart(), wd.capturedLength());
    }

    // «сегодня», «завтра», «послезавтра» (возможно с «на»)
    static const QRegularExpression dayRe(
        "\\b(?:на\\s+)?(послезавтра|завтра|сегодня)\\b", kOpts);
    auto day = dayRe.match(w.cleaned);
    if (day.hasMatch()) {
        const QString word = day.captured(1).toLower();
        if (word == QStringLiteral("сегодня"))
            w.date = today;
        else if (word == QStringLiteral("завтра"))
            w.date = today.addDays(1);
        else
            w.date = today.addDays(2);
        w.cleaned.remove(day.capturedStart(), day.capturedLength());
    }

    // Время: «в 15:00», «в 15.30», «в 12 51» (голосовой ввод), «в 15 часов», «к 9»
    static const QRegularExpression timeRe(
        "\\b(?:в|к)\\s*(\\d{1,2})(?:[:.](\\d{2})|\\s+(\\d{2})(?!\\d))?\\s*(?:час(?:а|ов)?)?\\b",
        kOpts);
    auto tm = timeRe.match(w.cleaned);
    if (tm.hasMatch()) {
        const int h = tm.captured(1).toInt();
        const QString minStr = !tm.captured(2).isEmpty() ? tm.captured(2)
                             : !tm.captured(3).isEmpty() ? tm.captured(3)
                                                         : QString();
        const int m = minStr.isEmpty() ? 0 : minStr.toInt();
        if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            w.time = QTime(h, m);
            w.cleaned.remove(tm.capturedStart(), tm.capturedLength());
        }
    }

    return w;
}

QVariantMap CommandParser::parse(const QString &text) const
{
    QVariantMap r;
    r["type"] = "none";
    QString t = text.trimmed();

    // Срезаем вводные слова: «привет, …», «слушай, …», «пожалуйста, …»
    static const QRegularExpression lead(
        "^(?:привет|здравствуй(?:те)?|добро(?:е|го)\\s+утр\\w+|добрый\\s+(?:день|вечер)|"
        "слушай|пожалуйста|будь\\s+добра?|спутник|эй|окей|ладно|кстати|а)[,!.\\s]+",
        kOpts);
    for (;;) {
        const auto m = lead.match(t);
        if (!m.hasMatch() || m.capturedLength() == 0)
            break;
        t.remove(0, m.capturedLength());
    }
    // «можешь напомнить…», «не забудь напомнить…»
    static const QRegularExpression polite(
        "^(?:можешь|могла\\s+бы|не\\s+забудь|давай)\\s+", kOpts);
    t.remove(polite);
    t = t.trimmed();

    // ── Память: «запомни: меня зовут Настя» ──────────────────────────
    static const QRegularExpression rememberRe(
        "^запомни(?:те)?[:,]?\\s+(?:что\\s+)?(.+)$", kOpts);
    auto rem0 = rememberRe.match(t);
    if (rem0.hasMatch()) {
        QString fact = rem0.captured(1).trimmed();
        while (fact.endsWith('.') || fact.endsWith('!'))
            fact.chop(1);
        r["type"] = "remember";
        r["fact"] = fact;
        return r;
    }

    // ── Повторяющееся напоминание: «пить воду каждые 45 минут»,
    //    «каждый час напоминай размяться» ────────────────────────────
    static const QRegularExpression recurRe(
        "кажд(?:ый|ую|ые|ое)\\s*(\\d+)?\\s*(минут\\w*|полчаса|час\\w*|день|дня)",
        kOpts);
    static const QRegularExpression remindWord("напомин\\w*|напомни\\w*", kOpts);
    auto recur = recurRe.match(t);
    if (recur.hasMatch()) {
        const QString firstWord = t.section(' ', 0, 0).toLower();
        const bool imperative = remindWord.match(t).hasMatch()
                                || firstWord.endsWith(QStringLiteral("ть"))
                                || firstWord.endsWith(QStringLiteral("ай"));
        const QString unit = recur.captured(2).toLower();
        int minutes = 0;
        const int n = recur.captured(1).toInt(); // 0, если числа нет
        if (unit == QStringLiteral("полчаса"))
            minutes = 30;
        else if (unit.startsWith(QStringLiteral("минут")))
            minutes = n > 0 ? n : 0; // «каждые минут» без числа — не команда
        else if (unit.startsWith(QStringLiteral("час")))
            minutes = (n > 0 ? n : 1) * 60;
        else
            minutes = (n > 0 ? n : 1) * 1440; // день
        if (imperative && minutes > 0) {
            QString title = t;
            title.remove(recur.capturedStart(), recur.capturedLength());
            static const QRegularExpression clean(
                "\\b(?:напомин\\w*|напомни\\w*|мне|пожалуйста|меня)\\b", kOpts);
            title.remove(clean);
            title = tidy(title.simplified());
            if (title.length() >= 3) {
                r["type"]    = "reminder_recurring";
                r["title"]   = title;
                r["minutes"] = minutes;
                return r;
            }
        }
    }

    // ── Список задач: «вот задачи, которые надо сделать: 1.… 2.… 3.…» ──
    static const QRegularExpression taskListHint(
        "(задач|надо\\s+сделать|нужно\\s+сделать|список\\s+дел)", kOpts);
    static const QRegularExpression numItem("(?:^|\\s)(\\d{1,2})[.)]\\s*");
    if (taskListHint.match(t).hasMatch()) {
        QList<int> itemStarts, numStarts;
        auto it = numItem.globalMatch(t);
        while (it.hasNext()) {
            const auto m = it.next();
            numStarts << m.capturedStart();
            itemStarts << m.capturedEnd();
        }
        QStringList items;
        for (int i = 0; i < itemStarts.size(); ++i) {
            const int end = (i + 1 < numStarts.size()) ? numStarts[i + 1] : t.length();
            const QString item = tidy(t.mid(itemStarts[i], end - itemStarts[i]));
            if (item.length() >= 3)
                items << item;
        }
        if (items.size() >= 2) { // одиночные «задачи» обрабатываются ниже
            r["type"]  = "task_list";
            r["items"] = items;
            return r;
        }
    }

    // ── Маршрут ──────────────────────────────────────────────────────
    static const QRegularExpression routeRe(
        "^(?:как\\s+(?:мне\\s+)?(?:добраться|дойти|доехать)\\s+до|"
        "построй(?:те)?\\s+маршрут\\s+(?:до|к)|маршрут\\s+до)\\s+(.+?)\\??$",
        kOpts);
    auto route = routeRe.match(t);
    if (route.hasMatch()) {
        QString place = route.captured(1);
        QString mode = "foot";
        static const QRegularExpression carRe("на\\s+(машине|авто|такси)", kOpts);
        static const QRegularExpression bikeRe("на\\s+велосипеде", kOpts);
        if (carRe.match(place).hasMatch()) { place.remove(carRe); mode = "driving"; }
        else if (bikeRe.match(place).hasMatch()) { place.remove(bikeRe); mode = "bike"; }
        r["type"]  = "route";
        r["place"] = tidy(place);
        r["mode"]  = mode;
        return r;
    }

    // ── Напоминание по месту ─────────────────────────────────────────
    static const QRegularExpression geoRe(
        "^напомни(?:ть)?\\s+(?:мне\\s+)?(.+?),?\\s+когда\\s+(?:я\\s+)?"
        "(?:буду|окажусь|выйду|приду)\\s+(?:около|возле|у|рядом\\s+с|в|на)\\s+(.+?)\\.?$",
        kOpts);
    auto geo = geoRe.match(t);
    if (geo.hasMatch()) {
        r["type"]  = "reminder_geo";
        r["title"] = tidy(geo.captured(1));
        r["place"] = tidy(geo.captured(2));
        return r;
    }

    // ── Напоминание по времени ───────────────────────────────────────
    static const QRegularExpression remRe("^напомни(?:ть)?\\s+(.+)$", kOpts);
    auto rem = remRe.match(t);
    if (rem.hasMatch()) {
        When w = extractWhen(rem.captured(1));
        QDate d = w.date.isValid() ? w.date : QDate::currentDate();
        QTime tt = w.time;
        if (!tt.isValid()) {
            if (w.date.isValid())
                tt = QTime(9, 0); // дата без времени → утро
            else
                tt = QTime::currentTime().addSecs(3600); // ничего → через час
        }
        // время уже прошло сегодня → завтра
        if (!w.date.isValid() && w.time.isValid()
            && QDateTime(d, tt) <= QDateTime::currentDateTime())
            d = d.addDays(1);

        r["type"]  = "reminder_time";
        r["title"] = tidy(w.cleaned);
        r["dueAt"] = QDateTime(d, tt).toString(Qt::ISODate);
        r["human"] = QDateTime(d, tt).toString("dd.MM в hh:mm");
        return r;
    }

    // ── Задача ───────────────────────────────────────────────────────
    static const QRegularExpression taskRe(
        "^(?:добавь|создай|запиши)\\s+задачу:?\\s+(.+)$", kOpts);
    auto task = taskRe.match(t);
    if (task.hasMatch()) {
        When w = extractWhen(task.captured(1));
        r["type"]    = "task";
        r["title"]   = tidy(w.cleaned);
        r["dueDate"] = w.date.isValid() ? w.date.toString("yyyy-MM-dd") : QString();
        return r;
    }

    // ── Событие в свободной форме ────────────────────────────────────
    // «завтра тренировка в 18:00, добавь в календарь»,
    // «добавь событие на завтра испечь пирог»
    static const QRegularExpression addWord("добавь|создай|запиши", kOpts);
    static const QRegularExpression eventWord("календар|событ|встреч|мероприят", kOpts);
    static const QRegularExpression eventAnchored(
        "^(?:добавь|создай|запиши)\\s+(?:в\\s+календарь|встречу|событие|мероприятие)",
        kOpts);
    if (addWord.match(t).hasMatch() && eventWord.match(t).hasMatch()
        && !eventAnchored.match(t).hasMatch()) {
        QString s = t;
        static const QRegularExpression evTail(
            "[,.]?\\s*(?:добавь|создай|запиши)(?:те)?(?:\\s+(?:это|её|его))?"
            "(?:\\s+пожалуйста)?(?:\\s+(?:в\\s+календарь|событие|как\\s+событие))?"
            "[.!]?\\s*$", kOpts);
        s.remove(evTail);
        static const QRegularExpression evHead(
            "^(?:добавь|создай|запиши)(?:те)?\\s+(?:пожалуйста\\s+)?"
            "(?:в\\s+календарь[\\s:]*)?(?:событие|встречу|мероприятие|запись)?[\\s:]*",
            kOpts);
        s.remove(evHead);

        When w = extractWhen(s);
        QString title = w.cleaned.trimmed();
        static const QRegularExpression evIntro(
            "^(?:у\\s+меня\\s+(?:будет\\s+)?|будет\\s+|запланируй\\s+)", kOpts);
        title.remove(evIntro);
        title = tidy(title);
        static const QRegularExpression evQuotes("^[«\"']+|[»\"']+$");
        title.remove(evQuotes);
        if (title.length() >= 3) {
            r["type"]  = "event";
            r["title"] = title;
            r["date"]  = (w.date.isValid() ? w.date : QDate::currentDate())
                             .toString("yyyy-MM-dd");
            r["time"]  = w.time.isValid() ? w.time.toString("HH:mm") : QString();
            return r;
        }
    }

    // ── Задача в свободной форме ─────────────────────────────────────
    // «…прочитать книгу, добавь в задачи», «добавь в задачи "X"»,
    // «у меня появилась задача X, добавь пожалуйста»
    static const QRegularExpression taskFree("добавь", kOpts);
    static const QRegularExpression taskWord("задач", kOpts);
    if (taskFree.match(t).hasMatch() && taskWord.match(t).hasMatch()) {
        QString s = t;
        // команда в конце: «, добавь (пожалуйста) (в задачи)»
        static const QRegularExpression tail(
            "[,.]?\\s*добавь(?:те)?(?:\\s+(?:это|её|его))?(?:\\s+пожалуйста)?"
            "(?:\\s+в\\s+(?:задачи|список\\s+задач))?[.!]?\\s*$", kOpts);
        s.remove(tail);
        // команда в начале: «добавь (пожалуйста) в задачи: …»
        static const QRegularExpression head(
            "^добавь(?:те)?\\s+(?:пожалуйста\\s+)?в\\s+(?:задачи|список\\s+задач):?\\s*",
            kOpts);
        s.remove(head);
        // вводные: «у меня появилась задача…», «сегодня надо…»
        static const QRegularExpression intro(
            "^(?:у\\s+меня\\s+(?:появилась|есть)\\s+(?:новая\\s+)?задача[\\s:—-]+|"
            "сегодня\\s+(?:мне\\s+)?(?:надо|нужно)\\s+|"
            "мне\\s+(?:надо|нужно)\\s+|надо\\s+|нужно\\s+)", kOpts);
        // «сегодня» до удаления — учитываем как срок
        When w = extractWhen(s);
        QString title = w.cleaned.trimmed();
        title.remove(intro);
        // кавычки по краям
        title = title.trimmed();
        static const QRegularExpression quotes("^[«\"']+|[»\"']+$");
        title.remove(quotes);
        title = tidy(title);
        if (title.length() >= 3) {
            r["type"]    = "task";
            r["title"]   = title;
            r["dueDate"] = w.date.isValid() ? w.date.toString("yyyy-MM-dd")
                                            : QString();
            return r;
        }
    }

    // ── Событие календаря ────────────────────────────────────────────
    static const QRegularExpression eventRe(
        "^(?:добавь|создай|запиши)\\s+(?:в\\s+календарь\\s+)?"
        "(встречу|событие|мероприятие|запись)?\\s*(.+)$", kOpts);
    static const QRegularExpression eventHint(
        "^(?:добавь|создай|запиши)\\s+(?:в\\s+календарь|встречу|событие|мероприятие)",
        kOpts);
    if (eventHint.match(t).hasMatch()) {
        auto ev = eventRe.match(t);
        When w = extractWhen(ev.captured(2));
        QString title = tidy(w.cleaned);
        // «встречу с Иваном» → «Встреча с Иваном»
        if (ev.captured(1).toLower() == QStringLiteral("встречу") && !title.isEmpty()) {
            title[0] = title[0].toLower();
            title.prepend(QStringLiteral("Встреча "));
        }
        r["type"]  = "event";
        r["title"] = title;
        r["date"]  = (w.date.isValid() ? w.date : QDate::currentDate())
                         .toString("yyyy-MM-dd");
        r["time"]  = w.time.isValid() ? w.time.toString("HH:mm") : QString();
        return r;
    }

    return r;
}

bool CommandParser::needsSearch(const QString &text) const
{
    const QString lower = text.toLower();

    static const QStringList triggers = {
        "погод", "новост", "курс доллара", "курс евро", "курс валют",
        "сколько стоит", "цена ", "что такое", "кто такой", "кто такая",
        "где находится", "столица", "население", "расписание", "во сколько",
        "какого числа", "результат матча", "счёт матча"
    };
    for (const QString &tr : triggers)
        if (lower.contains(tr))
            return true;

    // Фактические вопросы: «когда…», «сколько…», «где…» + вопросительный знак
    static const QRegularExpression factRe(
        "^(когда|сколько|где|почему|какова|каково)\\b", kOpts);
    if (factRe.match(lower).hasMatch() && text.contains('?'))
        return true;

    return false;
}
