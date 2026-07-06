#include "navigationmanager.h"
#include "databasemanager.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSqlQuery>
#include <QUrlQuery>
#include <QtMath>
#include <QDebug>

NavigationManager::NavigationManager(QObject *parent) : QObject(parent) {}

// ── Публичный запуск ─────────────────────────────────────────────────

void NavigationManager::buildRoute(const QString &from, const QString &to,
                                   const QString &mode)
{
    m_fromText = from.trimmed();
    m_toText   = to.trimmed();
    m_mode     = mode;

    geocode(m_fromText, [this](double lat, double lon, QString name) {
        m_fromLat = lat;
        m_fromLon = lon;
        if (!name.isEmpty())
            m_fromText = name;
        geocode(m_toText, [this](double lat2, double lon2, QString name2) {
            m_toLat = lat2;
            m_toLon = lon2;
            if (!name2.isEmpty())
                m_toText = name2;
            requestRoute();
        });
    });
}

void NavigationManager::buildRouteFrom(double lat, double lon, const QString &to,
                                       const QString &mode)
{
    m_fromLat  = lat;
    m_fromLon  = lon;
    m_fromText = QStringLiteral("Моё местоположение");
    m_toText   = to.trimmed();
    m_mode     = mode;

    geocode(m_toText, [this](double lat2, double lon2, QString name2) {
        m_toLat = lat2;
        m_toLon = lon2;
        if (!name2.isEmpty())
            m_toText = name2;
        requestRoute();
    });
}

// ── Погода (open-meteo) ──────────────────────────────────────────────

static QString weatherCodeText(int code)
{
    if (code == 0) return QStringLiteral("ясно ☀️");
    if (code <= 2) return QStringLiteral("переменная облачность ⛅");
    if (code == 3) return QStringLiteral("пасмурно ☁️");
    if (code == 45 || code == 48) return QStringLiteral("туман 🌫️");
    if (code >= 51 && code <= 57) return QStringLiteral("морось 🌦️");
    if (code >= 61 && code <= 67) return QStringLiteral("дождь 🌧️");
    if (code >= 71 && code <= 77) return QStringLiteral("снег ❄️");
    if (code >= 80 && code <= 82) return QStringLiteral("ливень 🌧️");
    if (code >= 85 && code <= 86) return QStringLiteral("снегопад 🌨️");
    if (code >= 95) return QStringLiteral("гроза ⛈️");
    return QString();
}

void NavigationManager::fetchWeather()
{
    auto &dbm = DatabaseManager::instance();
    // Координаты — последние с телефона; по умолчанию Новосибирск
    const QString lat = dbm.setting("last_lat", "55.03");
    const QString lon = dbm.setting("last_lon", "82.92");

    QUrl url(QStringLiteral(
        "https://api.open-meteo.com/v1/forecast?latitude=%1&longitude=%2"
        "&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m"
        "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
        "&timezone=auto&forecast_days=1").arg(lat, lon));

    QNetworkRequest req(url);
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Погода недоступна:" << reply->errorString();
            return; // без погоды приветствие всё равно уже показано
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject cur = root["current"].toObject();
        const QJsonObject daily = root["daily"].toObject();
        if (cur.isEmpty())
            return;

        auto deg = [](double v) {
            return QStringLiteral("%1%2°")
                .arg(v > 0 ? "+" : "").arg(qRound(v));
        };
        QString text = QStringLiteral("Сейчас %1 (ощущается как %2), %3.")
                           .arg(deg(cur["temperature_2m"].toDouble()),
                                deg(cur["apparent_temperature"].toDouble()),
                                weatherCodeText(cur["weather_code"].toInt()));
        const QJsonArray tmax = daily["temperature_2m_max"].toArray();
        const QJsonArray tmin = daily["temperature_2m_min"].toArray();
        const QJsonArray prec = daily["precipitation_probability_max"].toArray();
        if (!tmax.isEmpty())
            text += QStringLiteral(" Днём до %1, ночью %2.")
                        .arg(deg(tmax.first().toDouble()),
                             deg(tmin.first().toDouble()));
        if (!prec.isEmpty() && prec.first().toInt() >= 30)
            text += QStringLiteral(" Вероятность осадков %1% — возьми зонт! ☂️")
                        .arg(prec.first().toInt());
        emit weatherReady(text);
    });
}

// ── Геокодинг (Nominatim) ────────────────────────────────────────────

void NavigationManager::geocodePendingReminders()
{
    QSqlQuery q("SELECT id, place FROM reminders "
                "WHERE type = 'geo' AND fired = 0 AND (lat IS NULL OR lat = 0) "
                "AND place != ''");
    QList<QPair<int, QString>> pending;
    while (q.next())
        pending.append({q.value(0).toInt(), q.value(1).toString()});

    for (const auto &p : pending) {
        const int id = p.first;
        geocode(p.second, [id](double lat, double lon, QString) {
            QSqlQuery u;
            u.prepare("UPDATE reminders SET lat = ?, lon = ? WHERE id = ?");
            u.addBindValue(lat);
            u.addBindValue(lon);
            u.addBindValue(id);
            u.exec();
            qInfo() << "Гео-напоминание" << id << "получило координаты" << lat << lon;
        }, /*quiet=*/true);
    }
}

void NavigationManager::geocode(const QString &place,
                                std::function<void(double, double, QString)> done,
                                bool quiet)
{
    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery query;
    query.addQueryItem("q", place);
    query.addQueryItem("format", "json");
    query.addQueryItem("limit", "10"); // берём кандидатов и выбираем ближайшего
    query.addQueryItem("accept-language", "ru");
    url.setQuery(query);

    QNetworkRequest req(url);
    // Nominatim требует осмысленный User-Agent
    req.setRawHeader("User-Agent", "MyCompanionAssistant/1.0 (personal desktop app)");
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, place, done, quiet]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (quiet)
                qWarning() << "Геокодинг не удался:" << place << reply->errorString();
            else
                emit routeError("Не удалось найти «" + place + "»: " + reply->errorString());
            return;
        }
        const QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        if (arr.isEmpty()) {
            if (quiet)
                qWarning() << "Геокодинг: место не найдено:" << place;
            else
                emit routeError("Не нашла место «" + place +
                                "» на карте. Попробуй уточнить адрес или город. 🗺️");
            return;
        }

        // Как в Яндекс Картах: из всех найденных берём ближайшее к пользователю
        auto &dbm = DatabaseManager::instance();
        const double refLat = dbm.setting("last_lat", "55.03").toDouble();
        const double refLon = dbm.setting("last_lon", "82.92").toDouble();

        QJsonObject best;
        double bestDist = 1e18;
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const double lat = o["lat"].toString().toDouble();
            const double lon = o["lon"].toString().toDouble();
            const double dLat = (lat - refLat) * 111320.0;
            const double dLon = (lon - refLon) * 111320.0
                                * qCos(qDegreesToRadians(refLat));
            const double dist = dLat * dLat + dLon * dLon;
            if (dist < bestDist) {
                bestDist = dist;
                best = o;
            }
        }

        const double lat = best["lat"].toString().toDouble();
        const double lon = best["lon"].toString().toDouble();
        QString name = best["display_name"].toString();
        // Оставляем первые 2 части адреса, полный слишком длинный
        const QStringList parts = name.split(", ");
        if (parts.size() > 2)
            name = parts.mid(0, 2).join(", ");
        done(lat, lon, name);
    });
}

// ── Маршрут (OSRM) ───────────────────────────────────────────────────

void NavigationManager::requestRoute()
{
    // Здравый смысл: пеший маршрут за сотни км — скорее всего, геокодер
    // нашёл не то место
    {
        const double R = 6371000.0;
        const double dLat = qDegreesToRadians(m_toLat - m_fromLat);
        const double dLon = qDegreesToRadians(m_toLon - m_fromLon);
        const double a = qSin(dLat / 2) * qSin(dLat / 2)
                         + qCos(qDegreesToRadians(m_fromLat))
                           * qCos(qDegreesToRadians(m_toLat))
                           * qSin(dLon / 2) * qSin(dLon / 2);
        const double straight = 2 * R * qAtan2(qSqrt(a), qSqrt(1 - a));
        if (m_mode != QStringLiteral("driving") && straight > 200000.0) {
            emit routeError(QStringLiteral(
                "Ближайшее найденное «%1» — в %2 от тебя. Похоже, рядом такого "
                "места нет на карте OpenStreetMap. Уточни адрес (улица, район) "
                "или поищи в Яндекс Картах.")
                .arg(m_toText, humanDistance(straight)));
            return;
        }
    }

    // Сервер FOSSGIS: отдельные инстансы под каждый профиль
    QString base;
    if (m_mode == "driving")
        base = "https://routing.openstreetmap.de/routed-car";
    else if (m_mode == "bike")
        base = "https://routing.openstreetmap.de/routed-bike";
    else
        base = "https://routing.openstreetmap.de/routed-foot";

    const QString path = QStringLiteral("%1/route/v1/%2/%3,%4;%5,%6")
                             .arg(base)
                             .arg(m_mode == "driving" ? "driving" : m_mode)
                             .arg(m_fromLon, 0, 'f', 6)
                             .arg(m_fromLat, 0, 'f', 6)
                             .arg(m_toLon, 0, 'f', 6)
                             .arg(m_toLat, 0, 'f', 6);
    QUrl url(path);
    QUrlQuery query;
    query.addQueryItem("steps", "true");
    query.addQueryItem("overview", "false");
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MyCompanionAssistant/1.0 (personal desktop app)");
    req.setTransferTimeout(20000);

    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // Запасной вариант: расстояние по прямой + грубая оценка времени
            const double R = 6371000.0;
            const double dLat = qDegreesToRadians(m_toLat - m_fromLat);
            const double dLon = qDegreesToRadians(m_toLon - m_fromLon);
            const double a = qSin(dLat / 2) * qSin(dLat / 2)
                             + qCos(qDegreesToRadians(m_fromLat))
                               * qCos(qDegreesToRadians(m_toLat))
                               * qSin(dLon / 2) * qSin(dLon / 2);
            const double dist = 2 * R * qAtan2(qSqrt(a), qSqrt(1 - a)) * 1.3; // коэф. кривизны улиц
            const double speed = m_mode == "driving" ? 40.0 / 3.6
                               : m_mode == "bike"    ? 15.0 / 3.6
                                                     : 5.0 / 3.6;
            const double dur = dist / speed;
            saveRoute(dist, dur);
            emit routeReady(QStringLiteral(
                "🗺️ Сервис маршрутов недоступен, поэтому оценка приблизительная:\n\n"
                "**%1 → %2**\n"
                "Расстояние: ~%3\n"
                "Время в пути: ~%4")
                .arg(m_fromText, m_toText, humanDistance(dist), humanDuration(dur)));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray routes = root["routes"].toArray();
        if (root["code"].toString() != "Ok" || routes.isEmpty()) {
            emit routeError("Не получилось построить маршрут между этими точками. 😕");
            return;
        }

        const QJsonObject route = routes.first().toObject();
        const double distance = route["distance"].toDouble();
        double duration = route["duration"].toDouble();

        // Персонализация: средняя скорость ходьбы из статистики пользователя
        if (m_mode == "foot") {
            QSqlQuery q("SELECT SUM(distance_m), SUM(duration_s) FROM routes "
                        "WHERE mode = 'foot' AND duration_s > 0");
            if (q.next() && q.value(1).toDouble() > 0) {
                const double userSpeed = q.value(0).toDouble() / q.value(1).toDouble();
                if (userSpeed > 0.5 && userSpeed < 3.0) // разумный диапазон м/с
                    duration = distance / userSpeed;
            }
        }

        QString text = QStringLiteral("🗺️ **Маршрут: %1 → %2**\n\n").arg(m_fromText, m_toText);

        int n = 1;
        const QJsonArray legs = route["legs"].toArray();
        for (const QJsonValue &legVal : legs) {
            const QJsonArray steps = legVal.toObject()["steps"].toArray();
            for (const QJsonValue &stepVal : steps) {
                const QString instr = stepInstruction(stepVal.toObject());
                if (!instr.isEmpty())
                    text += QStringLiteral("%1. %2\n").arg(n++).arg(instr);
            }
        }

        const QString modeName = m_mode == "driving" ? "на машине"
                               : m_mode == "bike"    ? "на велосипеде"
                                                     : "пешком";
        text += QStringLiteral("\n📏 Расстояние: %1\n⏱️ Время %2: ~%3")
                    .arg(humanDistance(distance), modeName, humanDuration(duration));

        saveRoute(distance, duration);
        emit routeReady(text);
    });
}

QString NavigationManager::stepInstruction(const QJsonObject &step)
{
    const QJsonObject man = step["maneuver"].toObject();
    const QString type = man["type"].toString();
    const QString modifier = man["modifier"].toString();
    const QString street = step["name"].toString();
    const double dist = step["distance"].toDouble();

    auto dir = [&]() -> QString {
        if (modifier == "left")          return "налево";
        if (modifier == "right")         return "направо";
        if (modifier == "slight left")   return "плавно налево";
        if (modifier == "slight right")  return "плавно направо";
        if (modifier == "sharp left")    return "резко налево";
        if (modifier == "sharp right")   return "резко направо";
        if (modifier == "uturn")         return "в обратную сторону (разворот)";
        return QString();
    };

    QString s;
    if (type == "depart") {
        s = street.isEmpty() ? "Начните движение"
                             : "Начните движение по " + street;
    } else if (type == "arrive") {
        return "🏁 Вы прибыли в пункт назначения" +
               QString(modifier == "left" ? " (слева)"
                       : modifier == "right" ? " (справа)" : "");
    } else if (type == "turn" || type == "end of road" || type == "fork") {
        const QString d = dir();
        s = d.isEmpty() ? "Продолжайте движение" : "Поверните " + d;
        if (!street.isEmpty())
            s += " на " + street;
    } else if (type == "continue" || type == "new name") {
        s = street.isEmpty() ? "Продолжайте движение прямо"
                             : "Продолжайте по " + street;
    } else if (type == "roundabout" || type == "rotary") {
        const int exit = man["exit"].toInt();
        s = QStringLiteral("На круговом движении сверните на %1-й съезд").arg(exit > 0 ? exit : 1);
        if (!street.isEmpty())
            s += " на " + street;
    } else {
        s = street.isEmpty() ? "Продолжайте движение" : "Двигайтесь по " + street;
    }

    if (dist >= 10)
        s += QStringLiteral(" и пройдите %1").arg(humanDistance(dist));
    return s;
}

QString NavigationManager::humanDistance(double meters)
{
    if (meters >= 1000)
        return QStringLiteral("%1 км").arg(meters / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1 м").arg(qRound(meters / 10.0) * 10);
}

QString NavigationManager::humanDuration(double seconds)
{
    const int mins = qMax(1, qRound(seconds / 60.0));
    if (mins < 60)
        return QStringLiteral("%1 мин").arg(mins);
    return QStringLiteral("%1 ч %2 мин").arg(mins / 60).arg(mins % 60);
}

// ── Статистика ───────────────────────────────────────────────────────

void NavigationManager::saveRoute(double distanceM, double durationS)
{
    QSqlQuery q;
    q.prepare("INSERT INTO routes(created_at, origin, destination, distance_m, duration_s, mode) "
              "VALUES(?,?,?,?,?,?)");
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(m_fromText);
    q.addBindValue(m_toText);
    q.addBindValue(distanceM);
    q.addBindValue(durationS);
    q.addBindValue(m_mode);
    q.exec();
}

QVariantMap NavigationManager::stats()
{
    QVariantMap m;
    QSqlQuery q("SELECT COUNT(*), COALESCE(SUM(distance_m),0), COALESCE(SUM(duration_s),0) "
                "FROM routes");
    if (q.next()) {
        m["count"]   = q.value(0).toInt();
        m["totalKm"] = QString::number(q.value(1).toDouble() / 1000.0, 'f', 1);
        const double dur = q.value(2).toDouble();
        m["avgSpeed"] = dur > 0
            ? QString::number(q.value(1).toDouble() / dur * 3.6, 'f', 1)
            : "0";
    }

    QVariantList recent;
    QSqlQuery r("SELECT origin, destination, distance_m, duration_s, created_at "
                "FROM routes ORDER BY id DESC LIMIT 5");
    while (r.next()) {
        QVariantMap rm;
        rm["origin"]      = r.value(0).toString();
        rm["destination"] = r.value(1).toString();
        rm["distance"]    = humanDistance(r.value(2).toDouble());
        rm["duration"]    = humanDuration(r.value(3).toDouble());
        rm["date"] = QDateTime::fromString(r.value(4).toString(), Qt::ISODate)
                         .toString("dd.MM hh:mm");
        recent << rm;
    }
    m["recent"] = recent;
    return m;
}
