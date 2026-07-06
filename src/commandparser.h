#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QObject>
#include <QVariantMap>
#include <QDate>
#include <QTime>

// Локальный разбор русских команд без нейросети:
//  «напомни завтра в 15:00 позвонить маме»       → напоминание по времени
//  «напомни купить хлеб, когда буду около магазина» → напоминание по месту
//  «добавь задачу купить продукты на завтра»     → задача
//  «добавь встречу с Иваном в пятницу в 18:00»   → событие календаря
//  «как добраться до библиотеки»                 → маршрут
// Возвращает QVariantMap c полем "type":
//  none | reminder_time | reminder_geo | task | event | route
class CommandParser : public QObject
{
    Q_OBJECT
public:
    explicit CommandParser(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap parse(const QString &text) const;

    // Эвристика: нужен ли интернет-поиск для этого вопроса
    Q_INVOKABLE bool needsSearch(const QString &text) const;

private:
    struct When {
        QDate date;      // может быть невалидной
        QTime time;      // может быть невалидным
        QString cleaned; // текст без временных выражений
    };
    When extractWhen(const QString &text) const;
    static QString tidy(QString s);
};

#endif // COMMANDPARSER_H
