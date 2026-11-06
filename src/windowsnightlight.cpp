#include "nightlightcontroller.h"
#include "nightlightschedule.h"
#include "configpath.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

namespace {
QString settingsPath()
{
    return brightless::configRoot() + QStringLiteral("/brightless/night-light.json");
}
}

void NightLightController::initializeWindows()
{
    if (windowsInitialized_ || busy_) return;
    busy_ = true;
    emit stateChanged();
    if (!brightless::nightlight::initialize()) {
        error_ = tr("Could not initialize Windows Night Light (error %1).")
            .arg(brightless::nightlight::errorCode());
        busy_ = false;
        emit stateChanged();
        return;
    }
    brightless::nightlight::ColorEffect current;
    if (!brightless::nightlight::getColorEffect(current)) {
        error_ = tr("Could not read the Windows screen color filter (error %1).")
            .arg(brightless::nightlight::errorCode());
        brightless::nightlight::uninitialize();
        busy_ = false;
        emit stateChanged();
        return;
    }
    windowsInitialized_ = true;
    available_ = true;
    error_.clear();
    QFile file(settingsPath());
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) {
            error_ = tr("Could not load Windows Night Light settings.");
        } else {
            const auto document = QJsonDocument::fromJson(file.readAll());
            const auto loaded = document.object().toVariantMap();
            if (!document.isObject() || !validateSettings(loaded)) {
                error_ = tr("Could not load Windows Night Light settings.");
            } else {
                settings_ = loaded;
            }
        }
    }
    emit settingsChanged();
    scheduleTimer_ = new QTimer(this);
    scheduleTimer_->setObjectName(QStringLiteral("nightLightScheduleTimer"));
    scheduleTimer_->setInterval(1000);
    connect(scheduleTimer_, &QTimer::timeout, this, [this] {
        if (!updateWindows()) scheduleTimer_->stop();
    });
    if (updateWindows()) scheduleTimer_->start();
    busy_ = false;
    emit stateChanged();
}

bool NightLightController::applyWindows(const QVariantMap &settings)
{
    busy_ = true;
    QSaveFile file(settingsPath());
    const auto data = QJsonDocument(QJsonObject::fromVariantMap(settings)).toJson();
    if (!QDir().mkpath(brightless::configRoot() + QStringLiteral("/brightless"))
        || !file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error_ = tr("Could not save Windows Night Light settings.");
        busy_ = false;
        emit stateChanged();
        return false;
    }
    settings_ = settings;
    error_.clear();
    emit settingsChanged();
    const bool success = updateWindows();
    if (success) scheduleTimer_->start();
    else scheduleTimer_->stop();
    busy_ = false;
    emit stateChanged();
    return success;
}

bool NightLightController::restoreWindows()
{
    if (!effectOwned_) return true;
    brightless::nightlight::ColorEffect current;
    if (!brightless::nightlight::getColorEffect(current)) {
        error_ = tr("Could not read the Windows screen color filter (error %1).")
            .arg(brightless::nightlight::errorCode());
        return false;
    }
    // Do not overwrite a filter another application installed after ours.
    if (current != lastEffect_) {
        effectOwned_ = false;
        return true;
    }
    if (!brightless::nightlight::setColorEffect(originalEffect_)) {
        error_ = tr("Could not restore the previous Windows screen color filter (error %1).")
            .arg(brightless::nightlight::errorCode());
        return false;
    }
    effectOwned_ = false;
    return true;
}

bool NightLightController::updateWindows()
{
    const auto temperature = brightless::nightLightTemperature(settings_, QDateTime::currentDateTime());
    if (!temperature) {
        const bool restored = restoreWindows();
        if (!restored) emit stateChanged();
        return restored;
    }
    brightless::nightlight::ColorEffect current;
    if (!brightless::nightlight::getColorEffect(current)) {
        error_ = tr("Could not read the Windows screen color filter (error %1).")
            .arg(brightless::nightlight::errorCode());
        emit stateChanged();
        return false;
    }
    if (effectOwned_ && current != lastEffect_) {
        effectOwned_ = false;
        error_ = tr("Another application changed the screen color filter. Apply Night Light again to resume.");
        emit stateChanged();
        return false;
    }
    const auto effect = brightless::nightLightColorEffect(*temperature);
    if (effectOwned_ && effect == lastEffect_) return true;
    if (!effectOwned_) originalEffect_ = current;
    if (!brightless::nightlight::setColorEffect(effect)) {
        error_ = tr("Windows Night Light could not update the screen color filter (error %1).")
            .arg(brightless::nightlight::errorCode());
        emit stateChanged();
        return false;
    }
    lastEffect_ = effect;
    effectOwned_ = true;
    return true;
}

void NightLightController::shutdownWindows()
{
    if (scheduleTimer_) scheduleTimer_->stop();
    if (!windowsInitialized_) return;
    if (!restoreWindows()) qWarning().noquote() << error_;
    brightless::nightlight::uninitialize();
    windowsInitialized_ = false;
    available_ = false;
}
