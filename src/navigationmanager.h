#ifndef NAVIGATIONMANAGER_H
#define NAVIGATIONMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantMap>
#include <QJsonObject>
#include <functional>

// Навигация: геокодинг через Nominatim (OpenStreetMap),
// построение маршрута через OSRM (сервер FOSSGIS с профилями
// пешком/велосипед/машина). Выдаёт пошаговые инструкции на русском,
// сохраняет маршруты в SQLite и ведёт статистику.
class NavigationManager : public QObject
{
    Q_OBJECT
public:
    explicit NavigationManager(QObject *parent = nullptr);

    // mode: "foot" | "bike" | "driving"
    Q_INVOKABLE void buildRoute(const QString &from, const QString &to,
                                const QString &mode = QStringLiteral("foot"));

    // Маршрут от известных координат (GPS телефона) до места по названию
    Q_INVOKABLE void buildRouteFrom(double lat, double lon, const QString &to,
                                    const QString &mode = QStringLiteral("foot"));

    // Координаты последнего маршрута — для ссылки «открыть в картах»
    double fromLat() const { return m_fromLat; }
    double fromLon() const { return m_fromLon; }
    double toLat() const { return m_toLat; }
    double toLon() const { return m_toLon; }
    QString mode() const { return m_mode; }
    QString toText() const { return m_toText; }

    // Погода (open-meteo, без ключа) по последним координатам телефона
    Q_INVOKABLE void fetchWeather();

    // Статистика: количество маршрутов, суммарные км, средняя скорость, последние маршруты
    Q_INVOKABLE QVariantMap stats();

    // Гео-напоминания без координат: находим место через Nominatim
    // и записываем lat/lon в БД, чтобы напоминание могло сработать
    Q_INVOKABLE void geocodePendingReminders();

signals:
    void routeReady(const QString &text);   // готовый текст с шагами
    void routeError(const QString &error);
    void weatherReady(const QString &text);

private:
    // quiet: при неудаче не сигналить routeError (для фоновых задач).
    // Из всех найденных кандидатов выбирается ближайший к пользователю.
    void geocode(const QString &place, std::function<void(double lat, double lon, QString name)> done,
                 bool quiet = false);
    void requestRoute();
    void saveRoute(double distanceM, double durationS);
    static QString stepInstruction(const QJsonObject &step);
    static QString humanDistance(double meters);
    static QString humanDuration(double seconds);

    QNetworkAccessManager m_net;
    QString m_fromText, m_toText, m_mode;
    double m_fromLat = 0, m_fromLon = 0, m_toLat = 0, m_toLon = 0;
};

#endif // NAVIGATIONMANAGER_H
