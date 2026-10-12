#include "platform/battery.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantMap>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Battery reader check failed: " << message << '\n';
        std::exit(1);
    }
}

class PowerService : public QDBusVirtualObject
{
public:
    QVariantMap properties;
    bool fail = false;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        check(message.member() == "GetAll"
              && message.arguments() == QVariantList{QStringLiteral("org.freedesktop.UPower.Device")},
              "wrong UPower request");
        return connection.send(fail
            ? message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                       QStringLiteral("Unavailable battery"))
            : message.createReply(QVariantList{properties}));
    }
};
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // Use the isolated test bus, never the host's UPower service.
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", qgetenv("DBUS_SESSION_BUS_ADDRESS"));
    auto bus = QDBusConnection::systemBus();
    PowerService service;
    check(bus.registerService(QStringLiteral("org.freedesktop.UPower")), "cannot register fake service");
    check(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
                                    &service), "cannot register fake device");
    const QVariantMap valid{{"Type", 2u}, {"IsPresent", true}, {"PowerSupply", true}, {"Percentage", 48.6}};
    const auto read = [&] {
        bool finished = false;
        std::optional<int> percentage;
        brightless::battery::readPercentage(&app, [&](std::optional<int> result) {
            percentage = result;
            finished = true;
        });
        QElapsedTimer timer;
        timer.start();
        while (!finished && timer.elapsed() < 5000) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        check(finished, "asynchronous read did not complete");
        return percentage;
    };
    service.properties = valid;
    check(read() == 49, "valid battery percentage was not rounded correctly");
    for (const auto percentage : {0.0, 100.0}) {
        service.properties["Percentage"] = percentage;
        check(read() == static_cast<int>(percentage), "valid endpoint rejected");
    }
    for (const auto &invalid : std::initializer_list<std::pair<QString, QVariant>>{
             {"Type", 0u}, {"Type", 1u}, {"Type", 5u}, {"IsPresent", false}, {"PowerSupply", false},
             {"Percentage", -1.0}, {"Percentage", 101.0}, {"Percentage", "unknown"},
             {"Percentage", std::numeric_limits<double>::quiet_NaN()}}) {
        service.properties = valid;
        service.properties[invalid.first] = invalid.second;
        check(!read(), "invalid or non-system battery accepted");
    }
    for (const auto &key : {"Type", "IsPresent", "PowerSupply", "Percentage"}) {
        service.properties = valid;
        service.properties.remove(key);
        check(!read(), "incomplete battery response accepted");
    }
    service.fail = true;
    check(!read(), "D-Bus error returned a battery reading");
    bus.unregisterService(QStringLiteral("org.freedesktop.UPower"));
    check(!read(), "missing UPower service returned a battery reading");
}
