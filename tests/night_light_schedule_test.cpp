#include "nightlightschedule.h"

#include <QCoreApplication>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Night Light schedule check failed: " << message << '\n';
        std::exit(1);
    }
}

QDateTime at(const char *text, const QTimeZone &zone = QTimeZone("UTC"))
{
    const auto parsed = QDateTime::fromString(QString::fromLatin1(text), "yyyy-MM-dd HH:mm:ss");
    return QDateTime(parsed.date(), parsed.time(), zone);
}

QVariantMap settings(int mode = 1)
{
    return {{"enabled", true}, {"mode", mode}, {"temperature", 3500}, {"dayTemperature", 6500},
            {"morning", "06:00"}, {"evening", "18:00"}, {"transition", 30},
            {"latitude", 0.0}, {"longitude", 0.0}};
}

void temperature(const QVariantMap &map, const QDateTime &time, double expected, const char *message)
{
    const auto result = brightless::nightLightTemperature(map, time);
    check(result && std::abs(*result - expected) < 0.01, message);
}

void solarCity(double latitude, double longitude, const QTimeZone &zone,
               const char *beforeDawn, const char *day, const char *beforeDusk, const char *night)
{
    auto map = settings(0);
    map["latitude"] = latitude;
    map["longitude"] = longitude;
    temperature(map, at(beforeDawn, zone), 3500, "solar city before sunrise");
    temperature(map, at(day, zone), 6500, "solar city after sunrise ramp");
    temperature(map, at(beforeDusk, zone), 6500, "solar city before sunset");
    temperature(map, at(night, zone), 3500, "solar city after sunset ramp");
    const auto local = at(day, zone);
    check(brightless::nightLightTemperature(map, local)
          == brightless::nightLightTemperature(map, local.toUTC()), "solar time-zone invariance");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto map = settings();
    map["enabled"] = false;
    check(!brightless::nightLightTemperature(map, at("2024-06-21 12:00:00")), "disabled returns no effect");
    map["enabled"] = true;
    check(!brightless::nightLightTemperature(map, {}), "invalid clock returns no effect");
    temperature(map, at("2024-06-21 00:00:00"), 3500, "custom midnight wraps");
    temperature(map, at("2024-06-21 06:00:00"), 3500, "morning ramp begins at schedule time");
    temperature(map, at("2024-06-21 06:15:00"), 5000, "morning ramp midpoint");
    temperature(map, at("2024-06-21 06:30:00"), 6500, "morning ramp ends");
    temperature(map, at("2024-06-21 18:00:00"), 6500, "evening ramp begins at schedule time");
    temperature(map, at("2024-06-21 18:15:00"), 5000, "evening ramp midpoint");
    temperature(map, at("2024-06-21 18:30:00"), 3500, "evening ramp ends");
    map["evening"] = "23:50";
    temperature(map, at("2024-06-22 00:05:00"), 5000, "ramp crosses date boundary");
    map["morning"] = "20:00";
    map["evening"] = "08:00";
    temperature(map, at("2024-06-21 00:00:00"), 6500, "reversed custom times wrap");
    temperature(map, at("2024-06-21 12:00:00"), 3500, "reversed custom night interval");
    map["mode"] = 2;
    temperature(map, at("2024-06-21 12:00:00"), 3500, "always on");
    map["mode"] = 1;
    map["temperature"] = 6500;
    temperature(map, at("2024-06-21 08:15:00"), 6500, "equal day/night temperature");

    const QTimeZone newYork("America/New_York");
    check(newYork.isValid(), "New York time-zone data required");
    map = settings();
    map["morning"] = "01:30";
    map["transition"] = 120;
    temperature(map, at("2024-03-10 03:30:00", newYork), 5000, "spring DST ramp uses elapsed time");
    temperature(map, at("2024-03-10 04:30:00", newYork), 6500, "spring DST ramp finishes");
    map["morning"] = "00:30";
    const auto fallStart = at("2024-11-03 00:30:00", newYork);
    temperature(map, fallStart.addSecs(3600), 5000, "fall DST first repeated hour");
    temperature(map, fallStart.addSecs(7200), 6500, "fall DST second repeated hour");
    map = settings();
    const QTimeZone nepal(20700);
    temperature(map, at("2024-06-21 06:15:00", nepal), 5000, "fractional UTC offset custom time");

    // Known city sunrise/sunset fixtures, allowing several minutes of ephemeris/refraction error.
    // London June solstice ~04:43/21:21 BST; Sydney December ~05:41/20:05 AEDT.
    solarCity(51.5074, -0.1278, QTimeZone("Europe/London"),
              "2024-06-21 04:20:00", "2024-06-21 05:30:00",
              "2024-06-21 21:05:00", "2024-06-21 22:10:00");
    solarCity(-33.8688, 151.2093, QTimeZone("Australia/Sydney"),
              "2024-12-21 05:20:00", "2024-12-21 06:30:00",
              "2024-12-21 19:45:00", "2024-12-21 20:55:00");
    solarCity(-33.8688, 151.2093, QTimeZone("Australia/Sydney"),
              "2024-06-21 06:40:00", "2024-06-21 07:50:00",
              "2024-06-21 16:35:00", "2024-06-21 17:45:00");
    // New York's sunrise on the DST change date ~07:15 EDT, sunset ~18:58 EDT.
    solarCity(40.7128, -74.0060, newYork,
              "2024-03-10 06:55:00", "2024-03-10 08:05:00",
              "2024-03-10 18:40:00", "2024-03-10 19:45:00");
    // Kiritimati's civil date leads its UTC date; Suva lies on the other longitude side.
    solarCity(1.8721, -157.4278, QTimeZone(14 * 3600),
              "2024-06-21 06:00:00", "2024-06-21 07:30:00",
              "2024-06-21 18:10:00", "2024-06-21 19:30:00");
    solarCity(-18.1416, 178.4419, QTimeZone(12 * 3600),
              "2024-06-21 06:10:00", "2024-06-21 07:30:00",
              "2024-06-21 17:15:00", "2024-06-21 18:30:00");

    map = settings(0);
    map["latitude"] = 69.6492;
    map["longitude"] = 18.9553;
    for (const auto *time : {"2024-06-21 00:00:00", "2024-06-21 12:00:00"})
        temperature(map, at(time), 6500, "Tromso polar day");
    for (const auto *time : {"2024-12-21 00:00:00", "2024-12-21 12:00:00"})
        temperature(map, at(time), 3500, "Tromso polar night");
    for (double latitude : {-90.0, 90.0}) {
        map["latitude"] = latitude;
        temperature(map, at("2024-06-21 12:00:00"), latitude > 0 ? 6500 : 3500, "exact pole June");
        temperature(map, at("2024-12-21 12:00:00"), latitude > 0 ? 3500 : 6500, "exact pole December");
    }

    // A long solar ramp must finish before the next event, even with a short Arctic night.
    map["latitude"] = 65.0;
    map["longitude"] = 0.0;
    map["transition"] = 720;
    const auto sunrise = brightless::nightLightDetail::solarEvent(QDate(2024, 6, 21),
        brightless::nightLightDetail::radians(65), 0, true);
    check(sunrise.has_value(), "Arctic sunrise exists");
    const auto beforeSunrise = brightless::nightLightTemperature(map, sunrise->addSecs(-1));
    check(beforeSunrise && *beforeSunrise >= 3500 && *beforeSunrise < 3501, "long solar night ramp capped");
    temperature(map, *sunrise, 3500, "solar morning ramp starts at night temperature");
    temperature(map, sunrise->addSecs(6 * 3600), 6500, "solar day ramp capped to shorter night");

    const auto identity = brightless::nightLightColorEffect(6500);
    for (std::size_t i = 0; i < identity.size(); ++i)
        check(identity[i] == (i % 6 == 0 ? 1.0f : 0.0f), "6500 K exact identity");
    for (double kelvin : {0.0, 1000.0, 3500.0, 6500.0, 10000.0,
                          std::numeric_limits<double>::quiet_NaN()}) {
        const auto matrix = brightless::nightLightColorEffect(kelvin);
        for (std::size_t i = 0; i < matrix.size(); ++i)
            check(std::isfinite(matrix[i]) && matrix[i] >= 0 && matrix[i] <= 1
                  && (i % 6 == 0 || matrix[i] == 0), "finite diagonal-only bounded matrix");
        check(matrix[0] == 1 && matrix[18] == 1 && matrix[24] == 1, "red/alpha/homogeneous preserved");
    }
    const auto warm = brightless::nightLightColorEffect(3500);
    check(warm[12] < warm[6] && warm[6] < warm[0], "warmth reduces blue more than green");
    const auto reference = brightless::nightLightColorEffect(4500);
    check(std::abs(reference[6] - 0.856055f) < 0.00001f
          && std::abs(reference[12] - 0.749610f) < 0.00001f, "4500 K normalized black-body gains");
    auto previous = brightless::nightLightColorEffect(1000);
    check(previous[12] == 0, "1000 K suppresses blue");
    for (int kelvin = 1001; kelvin <= 6500; ++kelvin) {
        const auto current = brightless::nightLightColorEffect(kelvin);
        check(current[6] >= previous[6] && current[12] >= previous[12], "RGB gains increase monotonically");
        previous = current;
    }
}
