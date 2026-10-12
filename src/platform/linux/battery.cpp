#include "platform/battery.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>
#include <cmath>

namespace brightless::battery {
void readPercentage(QObject *context, std::function<void(std::optional<int>)> completion)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << QStringLiteral("org.freedesktop.UPower.Device");
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message, 3000), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
        [completion = std::move(completion)](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            std::optional<int> percentage;
            if (!reply.isError()) {
                const auto properties = reply.value();
                bool valid = false;
                const auto value = properties.value(QStringLiteral("Percentage")).toDouble(&valid);
                if (properties.value(QStringLiteral("Type")).toUInt() == 2
                    && properties.value(QStringLiteral("IsPresent")).toBool()
                    && properties.value(QStringLiteral("PowerSupply")).toBool()
                    && valid
                    && std::isfinite(value) && value >= 0 && value <= 100) {
                    percentage = static_cast<int>(std::lround(value));
                }
            }
            watcher->deleteLater();
            completion(percentage);
        });
}
}
