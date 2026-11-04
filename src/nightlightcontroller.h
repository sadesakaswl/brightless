#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

class NightLightController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sharedSchedule READ sharedSchedule NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    explicit NightLightController(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
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
    QNetworkAccessManager *network_;
    QVariantMap settings_;
    QString error_;
    bool available_ = false;
    bool busy_ = false;
    bool sharedSchedule_ = false;
};
