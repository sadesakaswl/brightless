#include "nightlightcontroller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef Q_OS_LINUX
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#endif

NightLightController::NightLightController(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent)
    , network_(network ? network : new QNetworkAccessManager(this))
    , settings_{{"enabled", false}, {"mode", 0}, {"temperature", 4500},
                {"evening", "18:00"}, {"morning", "06:00"}, {"transition", 30},
                {"automaticLocation", true}, {"latitude", 0.0}, {"longitude", 0.0}}
{
}

void NightLightController::initialize()
{
#ifdef Q_OS_LINUX
    if (busy_) return;
    busy_ = true;
    error_.clear();
    emit stateChanged();
    // shortcut: Flatpak lacks host config access; add permissions and host paths before enabling it there.
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.NightLight"),
        QStringLiteral("/org/kde/KWin/NightLight"), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    message.setArguments({QStringLiteral("org.kde.KWin.NightLight")});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        available_ = !reply.isError() && reply.value().value(QStringLiteral("available")).toBool();
        if (!available_) {
            busy_ = false;
            emit stateChanged();
            return;
        }
        // Plasma 6.5+ delegates scheduling to the shared Night Time service.
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.NightTime"),
            QStringLiteral("/org/kde/NightTime/Manager"), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        message.setArguments({QStringLiteral("org.kde.NightTime.Manager"), QStringLiteral("Version")});
        auto *probe = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(probe, &QDBusPendingCallWatcher::finished, this, [this, probe] {
            const QDBusPendingReply<QDBusVariant> version = *probe;
            probe->deleteLater();
            if (version.isError() && version.error().type() != QDBusError::ServiceUnknown) {
                available_ = false;
                error_ = version.error().message();
            } else {
                sharedSchedule_ = !version.isError();
                readSettings();
            }
            busy_ = false;
            emit stateChanged();
        });
    });
#endif
}

void NightLightController::detectLocation()
{
    if (busy_ || !available_) return;
    busy_ = true;
    error_.clear();
    emit stateChanged();
    QNetworkRequest request{QUrl(QStringLiteral("https://ipwho.is/?fields=success,latitude,longitude"))};
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(10000);
    auto *reply = network_->get(request);
    reply->setReadBufferSize(16 * 1024 + 1);
    QTimer::singleShot(10000, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 16 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        busy_ = false;
        if (reply->error() != QNetworkReply::NoError) {
            error_ = tr("Could not detect location via ipwho.is: %1").arg(reply->errorString());
            emit stateChanged();
            return;
        }
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        const auto latitude = data.value(QStringLiteral("latitude"));
        const auto longitude = data.value(QStringLiteral("longitude"));
        if (!data.value(QStringLiteral("success")).toBool() || !latitude.isDouble() || !longitude.isDouble()
            || !std::isfinite(latitude.toDouble()) || !std::isfinite(longitude.toDouble())
            || latitude.toDouble() < -90 || latitude.toDouble() > 90
            || longitude.toDouble() < -180 || longitude.toDouble() > 180) {
            error_ = tr("ipwho.is did not return a valid location.");
            emit stateChanged();
            return;
        }
        emit stateChanged();
        emit locationDetected(latitude.toDouble(), longitude.toDouble());
    });
}

void NightLightController::readSettings()
{
#ifdef Q_OS_LINUX
    const auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    config->reparseConfiguration();
    const KConfigGroup night(config, QStringLiteral("NightColor"));
    const int nativeMode = night.readEntry("Mode", sharedSchedule_ ? 1 : 0);
    int mode = nativeMode == (sharedSchedule_ ? 0 : 3) ? 2 : 0;
    bool automaticLocation = nativeMode != 1;
    double latitude = night.readEntry("LatitudeFixed", 0.0);
    double longitude = night.readEntry("LongitudeFixed", 0.0);
    QTime morning;
    QTime evening;
    int transition;
    if (sharedSchedule_) {
        const auto schedule = KSharedConfig::openConfig(QStringLiteral("knighttimerc"), KConfig::NoGlobals);
        schedule->reparseConfiguration();
        if (mode != 2 && KConfigGroup(schedule, "General").readEntry("Source", 0) == 1) mode = 1;
        const KConfigGroup location(schedule, "Location");
        automaticLocation = location.readEntry("Automatic", true);
        latitude = location.readEntry("Latitude", 0.0);
        longitude = location.readEntry("Longitude", 0.0);
        const KConfigGroup times(schedule, "Times");
        morning = times.readEntry("SunriseStart", QTime(6, 0));
        evening = times.readEntry("SunsetStart", QTime(18, 0));
        transition = times.readEntry("TransitionDuration", 1800) / 60;
    } else {
        if (nativeMode == 2) mode = 1;
        morning = QTime::fromString(night.readEntry("MorningBeginFixed", QStringLiteral("0600")), "HHmm");
        evening = QTime::fromString(night.readEntry("EveningBeginFixed", QStringLiteral("1800")), "HHmm");
        transition = night.readEntry("TransitionTime", 30);
    }
    settings_ = {{"enabled", night.readEntry("Active", false)}, {"mode", mode},
                 {"temperature", night.readEntry("NightTemperature", 4500)},
                 {"morning", morning.isValid() ? morning.toString("HH:mm") : QStringLiteral("06:00")},
                 {"evening", evening.isValid() ? evening.toString("HH:mm") : QStringLiteral("18:00")},
                 {"transition", transition}, {"automaticLocation", automaticLocation},
                 {"latitude", latitude}, {"longitude", longitude}};
    emit settingsChanged();
#endif
}

bool NightLightController::apply(const QVariantMap &settings)
{
    if (busy_ || !available_) return false;
    bool modeOk = false, temperatureOk = false, transitionOk = false;
    const int mode = settings.value("mode").toInt(&modeOk);
    const int temperature = settings.value("temperature").toInt(&temperatureOk);
    const int transition = settings.value("transition").toInt(&transitionOk);
    const auto morningText = settings.value("morning").toString();
    const auto eveningText = settings.value("evening").toString();
    const auto morning = QTime::fromString(morningText, "HH:mm");
    const auto evening = QTime::fromString(eveningText, "HH:mm");
    const int gap = std::abs(morning.secsTo(evening));
    if (!modeOk || mode < 0 || mode > 2 || !temperatureOk || temperature < 1000 || temperature > 6500
        || !transitionOk || transition < 1 || transition > 720
        || !morning.isValid() || morning.toString("HH:mm") != morningText
        || !evening.isValid() || evening.toString("HH:mm") != eveningText
        || (mode == 1 && transition * 60 >= std::min(gap, 86400 - gap))) {
        error_ = tr("Use a temperature from 1000–6500 K, valid times, and a transition shorter than both the day and the night.");
        emit stateChanged();
        return false;
    }
    const bool automaticLocation = settings.value("automaticLocation").toBool();
    bool latitudeOk = false, longitudeOk = false;
    const double latitude = settings.value("latitude").toDouble(&latitudeOk);
    const double longitude = settings.value("longitude").toDouble(&longitudeOk);
    if (mode == 0 && !automaticLocation
        && (!latitudeOk || !longitudeOk || !std::isfinite(latitude) || !std::isfinite(longitude)
            || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)) {
        error_ = tr("Use a latitude from -90 to 90 and a longitude from -180 to 180.");
        emit stateChanged();
        return false;
    }
#ifdef Q_OS_LINUX
    const auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    config->reparseConfiguration();
    KConfigGroup night(config, "NightColor");
    const auto flags = KConfig::Persistent | KConfig::Notify;
    if (sharedSchedule_ && mode != 2) {
        const auto schedule = KSharedConfig::openConfig(QStringLiteral("knighttimerc"), KConfig::NoGlobals);
        schedule->reparseConfiguration();
        KConfigGroup(schedule, "General").writeEntry("Source", mode == 1 ? 1 : 0, flags);
        if (mode == 0) {
            KConfigGroup location(schedule, "Location");
            location.writeEntry("Automatic", automaticLocation, flags);
            if (!automaticLocation) {
                location.writeEntry("Latitude", latitude, flags);
                location.writeEntry("Longitude", longitude, flags);
            }
        }
        if (mode == 1) {
            KConfigGroup times(schedule, "Times");
            times.writeEntry("SunriseStart", morning, flags);
            times.writeEntry("SunsetStart", evening, flags);
            times.writeEntry("TransitionDuration", transition * 60, flags);
        }
        if (!schedule->sync()) {
            error_ = tr("Could not save Plasma Night Light settings.");
            emit stateChanged();
            return false;
        }
    }
    const int nativeMode = sharedSchedule_ ? (mode == 2 ? 0 : 1)
        : (mode == 2 ? 3 : mode == 1 ? 2 : automaticLocation ? 0 : 1);
    night.writeEntry("Active", settings.value("enabled").toBool(), flags);
    night.writeEntry("Mode", nativeMode, flags);
    night.writeEntry("NightTemperature", temperature, flags);
    if (!sharedSchedule_ && mode == 0 && !automaticLocation) {
        night.writeEntry("LatitudeFixed", latitude, flags);
        night.writeEntry("LongitudeFixed", longitude, flags);
    }
    if (!sharedSchedule_ && mode == 1) {
        night.writeEntry("MorningBeginFixed", morning.toString("HHmm"), flags);
        night.writeEntry("EveningBeginFixed", evening.toString("HHmm"), flags);
        night.writeEntry("TransitionTime", transition, flags);
    }
    if (!config->sync()) {
        error_ = tr("Could not save Plasma Night Light settings.");
        emit stateChanged();
        return false;
    }
    readSettings();
    error_.clear();
    busy_ = true;
    emit stateChanged();
    const auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            error_ = tr("Settings were saved, but Plasma could not reload them: %1").arg(reply.error().message());
        }
        busy_ = false;
        emit stateChanged();
    });
    return true;
#else
    return false;
#endif
}
