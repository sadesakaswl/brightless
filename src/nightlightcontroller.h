#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#ifdef BRIGHTLESS_WINDOWS_NIGHT_LIGHT
#include "platform/nightlight.h"
class QTimer;
#endif

class QNetworkAccessManager;

class NightLightController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sharedSchedule READ sharedSchedule NOTIFY stateChanged)
    Q_PROPERTY(bool runsLocally READ runsLocally CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    explicit NightLightController(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    ~NightLightController() override;
    bool runsLocally() const {
#ifdef BRIGHTLESS_WINDOWS_NIGHT_LIGHT
        return true;
#else
        return false;
#endif
    }
    bool available() const { return available_; }
    bool busy() const { return busy_; }
    bool sharedSchedule() const { return sharedSchedule_; }
    QVariantMap settings() const { return settings_; }
    QString error() const { return error_; }
    Q_INVOKABLE void initialize();
    Q_INVOKABLE void detectLocation();
    Q_INVOKABLE bool apply(const QVariantMap &settings);

signals:
    void stateChanged();
    void settingsChanged();
    void locationDetected(double latitude, double longitude);

private:
    void readSettings();
    bool validateSettings(const QVariantMap &settings);
#ifdef BRIGHTLESS_WINDOWS_NIGHT_LIGHT
    void initializeWindows();
    bool applyWindows(const QVariantMap &settings);
    bool updateWindows();
    bool restoreWindows();
    void shutdownWindows();
    QTimer *scheduleTimer_ = nullptr;
    bool windowsInitialized_ = false;
    bool effectOwned_ = false;
    brightless::nightlight::ColorEffect originalEffect_{};
    brightless::nightlight::ColorEffect lastEffect_{};
#endif
    QNetworkAccessManager *network_;
    QVariantMap settings_;
    QString error_;
    bool available_ = false;
    bool busy_ = false;
    bool sharedSchedule_ = false;
};
