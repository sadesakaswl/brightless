#include "nightlightcontroller.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <limits>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Night Light check failed: " << message << '\n';
        std::exit(1);
    }
}

void waitForIdle(NightLightController &controller)
{
    QElapsedTimer timer;
    timer.start();
    while (controller.busy() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    check(!controller.busy(), "D-Bus operation timed out");
}

class LocationReply : public QNetworkReply
{
public:
    LocationReply(const QNetworkRequest &request, const QByteArray &body, NetworkError failure, QObject *parent)
        : QNetworkReply(parent), body_(body)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly);
        if (failure != NoError) setError(failure, QStringLiteral("Lookup failed"));
        QTimer::singleShot(0, this, [this] {
            if (error() == NoError) emit readyRead();
            else emit errorOccurred(error());
            if (!isFinished()) {
                setFinished(true);
                emit finished();
            }
        });
    }
    void abort() override
    {
        if (isFinished()) return;
        setError(OperationCanceledError, QStringLiteral("Aborted"));
        setFinished(true);
        emit errorOccurred(error());
        emit finished();
    }
    qint64 bytesAvailable() const override { return body_.size() - offset_ + QNetworkReply::bytesAvailable(); }
    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *data, qint64 size) override
    {
        const auto count = std::min<qint64>(size, body_.size() - offset_);
        if (count <= 0) return -1;
        std::memcpy(data, body_.constData() + offset_, count);
        offset_ += count;
        return count;
    }

private:
    QByteArray body_;
    qint64 offset_ = 0;
};

class LocationNetwork : public QNetworkAccessManager
{
public:
    QByteArray body = R"({"success":true,"latitude":-33.8688,"longitude":151.2093})";
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    int requests = 0;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *data) override
    {
        check(operation == GetOperation && !data, "location lookup was not a plain GET");
        check(request.url() == QUrl(QStringLiteral("https://ipwho.is/?fields=success,latitude,longitude")),
              "unexpected location URL or privacy-sensitive query fields");
        check(request.transferTimeout() == 10000, "location request has no timeout");
        check(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt() == QNetworkRequest::SameOriginRedirectPolicy,
              "location request permits cross-origin redirects");
        ++requests;
        return new LocationReply(request, body, error, this);
    }
};

class PlasmaService : public QDBusVirtualObject
{
public:
    bool available = true;
    bool failReload = false;
    bool sharedSchedule = false;
    bool failProbe = false;
    int reloads = 0;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.member() == "GetAll") {
            check(message.arguments() == QVariantList{QStringLiteral("org.kde.KWin.NightLight")},
                  "wrong Night Light interface");
            return bus.send(message.createReply(QVariantList{QVariantMap{{"available", available}}}));
        }
        if (message.member() == "Get") {
            check(message.arguments() == QVariantList{QStringLiteral("org.kde.NightTime.Manager"),
                                                       QStringLiteral("Version")}, "wrong schedule probe");
            if (failProbe) {
                return bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                                         QStringLiteral("Probe failed")));
            }
            return bus.send(sharedSchedule
                ? message.createReply(QVariantList{QVariant::fromValue(QDBusVariant(1u))})
                : message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"),
                                           QStringLiteral("Legacy scheduler")));
        }
        check(message.member() == "reconfigure", "unexpected D-Bus call");
        ++reloads;
        return bus.send(failReload
            ? message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"), QStringLiteral("Reload failed"))
            : message.createReply());
    }
};
}

int main(int argc, char *argv[])
{
    QTemporaryDir directory;
    check(directory.isValid(), "cannot create isolated configuration");
    qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    QCoreApplication app(argc, argv);
    auto bus = QDBusConnection::sessionBus();
    PlasmaService plasma;
    check(bus.registerService(QStringLiteral("org.kde.KWin.NightLight")), "cannot register Night Light");
    check(bus.registerService(QStringLiteral("org.kde.KWin")), "cannot register KWin");
    check(bus.registerService(QStringLiteral("org.kde.NightTime")), "cannot register shared scheduler");
    check(bus.registerVirtualObject(QStringLiteral("/org/kde/NightTime/Manager"), &plasma), "cannot register scheduler path");
    check(bus.registerVirtualObject(QStringLiteral("/org/kde/KWin/NightLight"), &plasma), "cannot register Night Light path");
    check(bus.registerVirtualObject(QStringLiteral("/KWin"), &plasma), "cannot register KWin path");

    const auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    KConfigGroup night(config, "NightColor");
    night.writeEntry("Active", true);
    night.writeEntry("Mode", 1);
    night.writeEntry("LatitudeFixed", 51.5);
    night.writeEntry("LongitudeFixed", -0.125);
    night.writeEntry("LatitudeAuto", 50.0);
    night.writeEntry("LongitudeAuto", 1.0);
    night.writeEntry("NightTemperature", 4200);
    night.writeEntry("DayTemperature", 6200);
    KConfigGroup(config, "Unrelated").writeEntry("Keep", "untouched");
    check(config->sync(), "cannot write initial Plasma config");

    LocationNetwork network;
    NightLightController controller(nullptr, &network);
    check(controller.settings().value("dayTemperature").toInt() == 6500, "default day temperature is incorrect");
    controller.initialize();
    waitForIdle(controller);
    check(controller.available() && !controller.sharedSchedule(), "legacy Plasma detection failed");
    check(controller.settings().value("enabled").toBool()
          && controller.settings().value("temperature").toInt() == 4200
          && controller.settings().value("dayTemperature").toInt() == 6200, "existing temperatures were not loaded");
    check(!controller.settings().value("automaticLocation").toBool()
          && controller.settings().value("latitude").toDouble() == 51.5
          && controller.settings().value("longitude").toDouble() == -0.125, "existing legacy location was not loaded");
    check(plasma.reloads == 0, "loading changed Plasma settings");
    auto settings = controller.settings();
    check(controller.apply(settings), "cannot preserve location schedule");
    waitForIdle(controller);
    check(night.readEntry("Mode", -1) == 1 && night.readEntry("LatitudeFixed", 0.0) == 51.5
          && night.readEntry("DayTemperature", 0) == 6200, "manual location or day temperature was overwritten");

    settings["mode"] = 1;
    settings["temperature"] = 3500;
    settings["dayTemperature"] = 5600;
    settings["evening"] = "21:30";
    settings["morning"] = "07:15";
    settings["transition"] = 45;
    check(controller.apply(settings), "valid custom schedule rejected");
    waitForIdle(controller);
    check(controller.error().isEmpty() && plasma.reloads == 2, "KWin was not reconfigured");
    check(night.readEntry("Mode", -1) == 2 && night.readEntry("NightTemperature", 0) == 3500
          && night.readEntry("DayTemperature", 0) == 5600
          && controller.settings().value("dayTemperature").toInt() == 5600,
          "legacy mode or temperatures were not saved");
    check(night.readEntry("MorningBeginFixed", QString()) == "0715"
          && night.readEntry("EveningBeginFixed", QString()) == "2130"
          && night.readEntry("TransitionTime", 0) == 45, "legacy transition/time units are incorrect");
    check(KConfigGroup(config, "Unrelated").readEntry("Keep", QString()) == "untouched",
          "unrelated Plasma settings were lost");

    for (const auto &change : {QVariantMap{{"morning", "25:00"}}, QVariantMap{{"evening", "7:00"}},
                              QVariantMap{{"morning", "21:30"}}, QVariantMap{{"transition", 600}},
                              QVariantMap{{"temperature", 999}}, QVariantMap{{"mode", 3}},
                              QVariantMap{{"dayTemperature", 999}}, QVariantMap{{"dayTemperature", 6501}},
                              QVariantMap{{"dayTemperature", "not a number"}}, QVariantMap{{"dayTemperature", QVariant()}},
                              QVariantMap{{"transition", 0}}}) {
        auto invalid = settings;
        for (auto it = change.begin(); it != change.end(); ++it) invalid[it.key()] = it.value();
        check(!controller.apply(invalid) && !controller.error().isEmpty(), "invalid schedule was accepted");
    }
    for (const auto &change : {QVariantMap{{"latitude", -90.1}}, QVariantMap{{"latitude", 90.1}},
                              QVariantMap{{"longitude", -180.1}}, QVariantMap{{"longitude", 180.1}},
                              QVariantMap{{"latitude", std::numeric_limits<double>::quiet_NaN()}},
                              QVariantMap{{"longitude", std::numeric_limits<double>::infinity()}},
                              QVariantMap{{"latitude", "not a number"}}, QVariantMap{{"longitude", QVariant()}}}) {
        auto invalid = settings;
        invalid["mode"] = 0;
        invalid["automaticLocation"] = false;
        for (auto it = change.begin(); it != change.end(); ++it) invalid[it.key()] = it.value();
        check(!controller.apply(invalid) && !controller.error().isEmpty(), "invalid coordinates were accepted");
    }
    check(plasma.reloads == 2 && night.readEntry("DayTemperature", 0) == 5600,
          "invalid settings changed Plasma temperatures or reached KWin");
    settings["mode"] = 2;
    settings["enabled"] = false;
    settings["dayTemperature"] = 1000;
    check(controller.apply(settings), "cannot disable Night Light");
    waitForIdle(controller);
    check(!night.readEntry("Active", true) && night.readEntry("Mode", -1) == 3
          && night.readEntry("DayTemperature", 0) == 1000,
          "legacy constant mode, disable, or minimum day temperature was not saved");

    settings["mode"] = 0;
    settings["automaticLocation"] = false;
    settings["latitude"] = -33.8688;
    settings["longitude"] = 151.2093;
    check(controller.apply(settings), "legacy manual location rejected");
    waitForIdle(controller);
    check(night.readEntry("Mode", -1) == 1
          && night.readEntry("LatitudeFixed", 0.0) == -33.8688
          && night.readEntry("LongitudeFixed", 0.0) == 151.2093, "legacy manual location was not saved");
    controller.initialize();
    waitForIdle(controller);
    check(controller.settings() == settings, "legacy location did not round-trip");
    settings["automaticLocation"] = true;
    check(controller.apply(settings), "legacy automatic location rejected");
    waitForIdle(controller);
    check(night.readEntry("Mode", -1) == 0
          && night.readEntry("LatitudeFixed", 0.0) == -33.8688
          && night.readEntry("LongitudeFixed", 0.0) == 151.2093
          && night.readEntry("LatitudeAuto", 0.0) == 50.0
          && night.readEntry("LongitudeAuto", 0.0) == 1.0, "automatic mode overwrote legacy coordinates");

    const auto schedule = KSharedConfig::openConfig(QStringLiteral("knighttimerc"), KConfig::NoGlobals);
    KConfigGroup location(schedule, "Location");
    location.writeEntry("Automatic", false);
    location.writeEntry("Latitude", 40.7128);
    location.writeEntry("Longitude", -74.0060);
    check(schedule->sync(), "cannot write shared location fixture");
    plasma.sharedSchedule = true;
    night.writeEntry("Mode", 1);
    check(config->sync(), "cannot set modern mode");
    controller.initialize();
    waitForIdle(controller);
    check(controller.sharedSchedule(), "shared scheduler was not detected");
    check(!controller.settings().value("automaticLocation").toBool()
          && controller.settings().value("latitude").toDouble() == 40.7128
          && controller.settings().value("longitude").toDouble() == -74.0060, "existing shared location was not loaded");
    settings = controller.settings();
    check(settings.value("dayTemperature").toInt() == 1000, "shared scheduler did not load existing day temperature");
    settings["dayTemperature"] = 6500;
    settings["enabled"] = true;
    settings["mode"] = 1;
    settings["morning"] = "07:15";
    settings["evening"] = "21:30";
    settings["transition"] = 45;
    check(controller.apply(settings), "shared custom schedule rejected");
    waitForIdle(controller);
    KConfigGroup times(schedule, "Times");
    check(night.readEntry("Mode", -1) == 1
          && night.readEntry("DayTemperature", 0) == 6500
          && night.readEntry("NightTemperature", 0) == 3500
          && KConfigGroup(schedule, "General").readEntry("Source", -1) == 1,
          "shared custom mode or temperatures incorrect");
    check(times.readEntry("SunriseStart", QTime()) == QTime(7, 15)
          && times.readEntry("SunsetStart", QTime()) == QTime(21, 30)
          && times.readEntry("TransitionDuration", 0) == 2700, "shared time/transition units incorrect");
    controller.initialize();
    waitForIdle(controller);
    check(controller.settings() == settings, "shared settings did not round-trip");
    settings["mode"] = 0;
    settings["latitude"] = 90.0;
    settings["longitude"] = -180.0;
    check(controller.apply(settings), "shared sunset/sunrise schedule rejected");
    waitForIdle(controller);
    check(KConfigGroup(schedule, "General").readEntry("Source", -1) == 0
          && !location.readEntry("Automatic", true)
          && location.readEntry("Latitude", 0.0) == 90.0
          && location.readEntry("Longitude", 0.0) == -180.0, "shared manual location incorrect");
    controller.initialize();
    waitForIdle(controller);
    check(controller.settings() == settings, "shared location did not round-trip");
    settings["automaticLocation"] = true;
    check(controller.apply(settings), "shared automatic location rejected");
    waitForIdle(controller);
    check(location.readEntry("Automatic", false) && location.readEntry("Latitude", 0.0) == 90.0
          && location.readEntry("Longitude", 0.0) == -180.0, "automatic mode overwrote shared coordinates");
    settings["mode"] = 2;
    check(controller.apply(settings), "shared constant mode rejected");
    waitForIdle(controller);
    check(night.readEntry("Mode", -1) == 0, "shared constant mode incorrect");

    check(network.requests == 0, "location was requested without user action");
    int detections = 0;
    double latitude = 0, longitude = 0;
    QObject::connect(&controller, &NightLightController::locationDetected, [&](double lat, double lon) {
        ++detections;
        latitude = lat;
        longitude = lon;
    });
    const auto beforeLookup = controller.settings();
    const int reloadsBeforeLookup = plasma.reloads;
    controller.detectLocation();
    check(controller.busy() && network.requests == 1, "location lookup was not asynchronous");
    controller.detectLocation();
    check(network.requests == 1 && !controller.apply(settings), "busy location lookup allowed concurrent changes");
    waitForIdle(controller);
    check(detections == 1 && latitude == -33.8688 && longitude == 151.2093 && controller.error().isEmpty(),
          "valid IP coordinates were not returned");
    for (const auto &invalid : std::initializer_list<QByteArray>{
             {}, "not JSON", "[]", R"({"success":false,"latitude":1,"longitude":2})",
             R"({"success":"true","latitude":1,"longitude":2})", R"({"success":true,"latitude":1})",
             R"({"success":true,"latitude":"1","longitude":2})", R"({"success":true,"latitude":true,"longitude":2})",
             R"({"success":true,"latitude":91,"longitude":2})", R"({"success":true,"latitude":-91,"longitude":2})",
             R"({"success":true,"latitude":1,"longitude":181})", R"({"success":true,"latitude":1,"longitude":-181})",
             R"({"success":true,"latitude":1e999,"longitude":2})", QByteArray(16 * 1024 + 1, 'x')}) {
        network.body = invalid;
        controller.detectLocation();
        waitForIdle(controller);
        check(detections == 1 && !controller.error().isEmpty(), "invalid IP response was accepted");
    }
    for (auto error : {QNetworkReply::HostNotFoundError, QNetworkReply::TimeoutError,
                       QNetworkReply::SslHandshakeFailedError}) {
        network.error = error;
        controller.detectLocation();
        waitForIdle(controller);
        check(detections == 1 && !controller.error().isEmpty(), "network error changed the location or was hidden");
    }
    network.error = QNetworkReply::NoError;
    for (const auto &valid : {QByteArray(R"({"success":true,"latitude":0,"longitude":0})"),
                             QByteArray(R"({"success":true,"latitude":-90,"longitude":180})")}) {
        network.body = valid;
        controller.detectLocation();
        waitForIdle(controller);
        check(controller.error().isEmpty(), "valid boundary coordinates or retry were rejected");
    }
    check(detections == 3 && latitude == -90 && longitude == 180, "boundary coordinates were not returned");
    check(controller.settings() == beforeLookup && plasma.reloads == reloadsBeforeLookup,
          "location lookup saved or changed Plasma settings without applying");

    night.deleteEntry("DayTemperature");
    check(config->sync(), "cannot remove day temperature for default check");
    controller.initialize();
    waitForIdle(controller);
    check(controller.settings().value("dayTemperature").toInt() == 6500,
          "missing Plasma day temperature did not default to 6500 K");

    plasma.failProbe = true;
    controller.initialize();
    waitForIdle(controller);
    check(!controller.available() && !controller.error().isEmpty() && !controller.apply(settings),
          "a failed shared scheduler probe fell back to incompatible legacy settings");
    plasma.failProbe = false;
    controller.initialize();
    waitForIdle(controller);

    plasma.failReload = true;
    check(controller.apply(settings), "reload failure should occur asynchronously");
    waitForIdle(controller);
    check(!controller.error().isEmpty(), "reload error was hidden");
    plasma.available = false;
    controller.initialize();
    waitForIdle(controller);
    check(!controller.available() && !controller.apply(settings), "unsupported compositor accepted changes");
    const int requests = network.requests;
    controller.detectLocation();
    check(network.requests == requests, "unsupported compositor requested an IP location");
    bus.unregisterService(QStringLiteral("org.kde.KWin.NightLight"));
    controller.initialize();
    waitForIdle(controller);
    check(!controller.available() && !controller.busy(), "missing Plasma service was not handled");
    std::cout << "Verified legacy/shared Plasma scheduling, smooth transitions, validation and D-Bus failures.\n";
}
