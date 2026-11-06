#pragma once

#include <QDateTime>
#include <QTimeZone>
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <vector>

namespace brightless {
namespace nightLightDetail {

constexpr double pi = 3.14159265358979323846;
constexpr double radians(double degrees) { return degrees * pi / 180.0; }

struct Sun {
    double declination;
    double equationMinutes;
};

// Low-order solar ephemeris; sufficient for minute-scale display scheduling.
inline Sun sunAt(const QDateTime &utc)
{
    const double yearDays = QDate::isLeapYear(utc.date().year()) ? 366.0 : 365.0;
    const double phase = 2 * pi / yearDays * (utc.date().dayOfYear() - 1
        + (utc.time().msecsSinceStartOfDay() / 3600000.0 - 12) / 24);
    return {0.006918 - 0.399912 * std::cos(phase) + 0.070257 * std::sin(phase)
                - 0.006758 * std::cos(2 * phase) + 0.000907 * std::sin(2 * phase)
                - 0.002697 * std::cos(3 * phase) + 0.001480 * std::sin(3 * phase),
            229.18 * (0.000075 + 0.001868 * std::cos(phase) - 0.032077 * std::sin(phase)
                - 0.014615 * std::cos(2 * phase) - 0.040849 * std::sin(2 * phase))};
}

struct Event {
    qint64 milliseconds;
    bool day;
};

inline std::optional<QDateTime> solarEvent(QDate date, double latitude, double longitude, bool rise)
{
    const QDateTime midnight(date, QTime(0, 0), QTimeZone("UTC"));
    auto estimate = midnight.addSecs(qRound64((720 - 4 * longitude) * 60));
    for (int iteration = 0; iteration < 4; ++iteration) {
        const auto sun = sunAt(estimate);
        const double base = std::sin(latitude) * std::sin(sun.declination);
        const double amplitude = std::cos(latitude) * std::cos(sun.declination);
        // The apparent upper limb crosses a level horizon at solar altitude -0.833 degrees.
        const double crossing = std::sin(radians(-0.833)) - base;
        if (amplitude <= 1e-12 || std::abs(crossing) > amplitude) return std::nullopt;
        const double angle = std::acos(std::clamp(crossing / amplitude, -1.0, 1.0));
        const double minutes = 720 - 4 * longitude - sun.equationMinutes
            + (rise ? -1 : 1) * angle * 720 / pi;
        estimate = midnight.addMSecs(qRound64(minutes * 60000));
    }
    return estimate;
}

inline bool sunIsUp(const QDateTime &utc, double latitude, double longitude)
{
    const auto sun = sunAt(utc);
    const double minutes = utc.time().msecsSinceStartOfDay() / 60000.0;
    const double hourAngle = radians((minutes + sun.equationMinutes + 4 * longitude) / 4 - 180);
    return std::sin(latitude) * std::sin(sun.declination)
        + std::cos(latitude) * std::cos(sun.declination) * std::cos(hourAngle)
        >= std::sin(radians(-0.833));
}

} // namespace nightLightDetail

// Settings are validated by the controller. Custom times use now's civil time zone;
// Qt's default transition resolution handles skipped/repeated DST times. Ramps use elapsed time.
inline std::optional<double> nightLightTemperature(const QVariantMap &settings, const QDateTime &now)
{
    if (!settings.value("enabled").toBool() || !now.isValid()) return std::nullopt;
    const double night = settings.value("temperature").toDouble();
    const double day = settings.value("dayTemperature").toDouble();
    const int mode = settings.value("mode").toInt();
    if (mode == 2 || night == day) return night;

    std::vector<nightLightDetail::Event> events;
    bool daylight = false;
    if (mode == 0) {
        const double latitude = nightLightDetail::radians(settings.value("latitude").toDouble());
        const double longitude = settings.value("longitude").toDouble();
        const auto utc = now.toUTC();
        daylight = nightLightDetail::sunIsUp(utc, latitude, longitude);
        // UTC dates, not civil dates, keep antimeridian and unrelated time zones correct.
        for (int offset = -3; offset <= 3; ++offset) {
            for (bool rise : {true, false}) {
                const auto event = nightLightDetail::solarEvent(utc.date().addDays(offset), latitude, longitude, rise);
                if (event) events.push_back({event->toMSecsSinceEpoch(), rise});
            }
        }
    } else if (mode == 1) {
        const auto morning = QTime::fromString(settings.value("morning").toString(), "HH:mm");
        const auto evening = QTime::fromString(settings.value("evening").toString(), "HH:mm");
        if (!morning.isValid() || !evening.isValid() || morning == evening) return std::nullopt;
        for (int offset = -2; offset <= 2; ++offset) {
            const auto date = now.date().addDays(offset);
            for (bool rise : {true, false}) {
                const QDateTime event(date, rise ? morning : evening, now.timeZone());
                if (event.isValid()) events.push_back({event.toMSecsSinceEpoch(), rise});
            }
        }
    } else {
        return std::nullopt;
    }
    std::sort(events.begin(), events.end(), [](const auto &a, const auto &b) {
        return a.milliseconds < b.milliseconds;
    });
    const auto current = now.toMSecsSinceEpoch();
    const auto next = std::upper_bound(events.begin(), events.end(), current,
        [](qint64 time, const auto &event) { return time < event.milliseconds; });
    // No recent crossing means polar day/night, not an arbitrary 06:00/18:00 fallback.
    if (next == events.begin()) return daylight ? day : night;
    const auto latest = std::prev(next);
    qint64 duration = qRound64(settings.value("transition").toDouble() * 60000);
    if (next != events.end()) duration = std::min(duration, next->milliseconds - latest->milliseconds);
    if (latest != events.begin()) duration = std::min(duration, latest->milliseconds - std::prev(latest)->milliseconds);
    const double target = latest->day ? day : night;
    const double source = latest->day ? night : day;
    const double fraction = duration > 0
        ? std::clamp(double(current - latest->milliseconds) / duration, 0.0, 1.0) : 1.0;
    return source + (target - source) * fraction;
}

// Helland's fitted black-body coefficients, independently implemented, normalized at 6500 K:
// https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
// Approximate sRGB attenuation, not calibrated colorimetry; never amplify a channel.
inline std::array<float, 25> nightLightColorEffect(double kelvin)
{
    if (!std::isfinite(kelvin)) kelvin = 6500;
    const double temperature = std::clamp(kelvin, 1000.0, 6500.0) / 100;
    const auto green = [](double t) { return 99.4708025861 * std::log(t) - 161.1195681661; };
    const auto blue = [](double t) {
        return t <= 19 ? 0.0 : std::max(0.0, 138.5177312231 * std::log(t - 10) - 305.0447927307);
    };
    std::array<float, 25> matrix{};
    matrix[0] = matrix[18] = matrix[24] = 1;
    matrix[6] = float(std::clamp(green(temperature) / green(65), 0.0, 1.0));
    matrix[12] = float(std::clamp(blue(temperature) / blue(65), 0.0, 1.0));
    return matrix;
}

} // namespace brightless
