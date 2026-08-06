#pragma once

#include <QHash>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class BrightlessController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString startup_error READ startupError NOTIFY startupErrorChanged)
    Q_PROPERTY(QStringList monitor_names READ monitorNames NOTIFY monitorNamesChanged)
    Q_PROPERTY(int monitor_count READ monitorCount NOTIFY monitorCountChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool close_to_tray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged)

public:
    explicit BrightlessController(QObject *parent = nullptr);
    ~BrightlessController() override;

    QString startupError() const;
    QStringList monitorNames() const;
    int monitorCount() const;
    int revision() const;
    bool closeToTray() const;
    void setCloseToTray(bool value);
    QSize savedWindowSize() const;
    void saveWindowSize(const QSize &size);
    int adjustAllBrightness(int direction);

    Q_INVOKABLE void initialize();
    Q_INVOKABLE int brightness(int index) const;
    Q_INVOKABLE void set_brightness(int index, int value);
    Q_INVOKABLE int contrast(int index) const;
    Q_INVOKABLE void set_contrast(int index, int value);
    Q_INVOKABLE int volume(int index) const;
    Q_INVOKABLE void set_volume(int index, int value);
    Q_INVOKABLE int input_source_code(int index) const;
    Q_INVOKABLE void set_input_source(int index, int code);
    Q_INVOKABLE int power_mode_code(int index) const;
    Q_INVOKABLE void set_power_mode(int index, int code);
    Q_INVOKABLE bool supports_contrast(int index) const;
    Q_INVOKABLE bool supports_volume(int index) const;
    Q_INVOKABLE bool supports_input_source(int index) const;
    Q_INVOKABLE bool supports_power_mode(int index) const;

    Q_INVOKABLE int scroll_step() const;
    Q_INVOKABLE void set_scroll_step(int value);
    Q_INVOKABLE bool dynamic_contrast_enabled() const;
    Q_INVOKABLE void set_dynamic_contrast_enabled(bool value);
    Q_INVOKABLE bool dynamic_contrast_global() const;
    Q_INVOKABLE void set_dynamic_contrast_global(bool value);
    Q_INVOKABLE float dynamic_contrast_ratio() const;
    Q_INVOKABLE void set_dynamic_contrast_ratio(float value);
    Q_INVOKABLE bool dynamic_contrast_per_monitor_ratio() const;
    Q_INVOKABLE void set_dynamic_contrast_per_monitor_ratio(bool value);
    Q_INVOKABLE bool monitor_dynamic_contrast_enabled(int index) const;
    Q_INVOKABLE void set_monitor_dynamic_contrast_enabled(int index, bool value);
    Q_INVOKABLE float monitor_ratio(int index) const;
    Q_INVOKABLE void set_monitor_ratio(int index, float value);
    Q_INVOKABLE void set_dynamic_contrast_brightness(int index, int value);

signals:
    void startupErrorChanged();
    void monitorNamesChanged();
    void monitorCountChanged();
    void revisionChanged();
    void closeToTrayChanged();

private:
    struct Monitor;

    Monitor *monitorAt(int index);
    const Monitor *monitorAt(int index) const;
    void bumpRevision();
    void refreshDynamicContrastState();
    void loadSettings();
    void saveSettings() const;

    QString startupError_;
    std::vector<std::unique_ptr<Monitor>> monitors_;
    int revision_ = 0;

    int scrollStep_ = 2;
    bool closeToTray_ = true;
    QSize savedWindowSize_;
    bool dynamicContrastEnabled_ = false;
    bool dynamicContrastGlobal_ = true;
    double dynamicContrastRatio_ = 0.7;
    bool dynamicContrastPerMonitorRatio_ = false;
    QHash<QString, bool> monitorDynamicContrast_;
    QHash<QString, double> monitorRatios_;
};
