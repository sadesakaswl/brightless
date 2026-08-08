#pragma once

#include <QHash>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>
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
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool autostart_as_tray_icon READ autostartAsTrayIcon WRITE setAutostartAsTrayIcon NOTIFY autostartAsTrayIconChanged)
    Q_PROPERTY(bool hide_brightness READ hideBrightness WRITE setHideBrightness NOTIFY visibilitySettingsChanged)
    Q_PROPERTY(bool hide_contrast READ hideContrast WRITE setHideContrast NOTIFY visibilitySettingsChanged)
    Q_PROPERTY(bool hide_volume READ hideVolume WRITE setHideVolume NOTIFY visibilitySettingsChanged)
    Q_PROPERTY(bool hide_input READ hideInput WRITE setHideInput NOTIFY visibilitySettingsChanged)
    Q_PROPERTY(bool hide_tray_icon READ hideTrayIcon WRITE setHideTrayIcon NOTIFY visibilitySettingsChanged)

public:
    explicit BrightlessController(QObject *parent = nullptr);
    ~BrightlessController() override;

    QString startupError() const;
    QStringList monitorNames() const;
    int monitorCount() const;
    int revision() const;
    bool closeToTray() const;
    void setCloseToTray(bool value);
    bool autostart() const;
    void setAutostart(bool value);
    bool autostartAsTrayIcon() const;
    void setAutostartAsTrayIcon(bool value);
    bool hideBrightness() const;
    void setHideBrightness(bool value);
    bool hideContrast() const;
    void setHideContrast(bool value);
    bool hideVolume() const;
    void setHideVolume(bool value);
    bool hideInput() const;
    void setHideInput(bool value);
    bool hideTrayIcon() const;
    void setHideTrayIcon(bool value);
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
    Q_INVOKABLE int ddc_delay() const;
    Q_INVOKABLE void set_ddc_delay(int value);
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
    void autostartChanged();
    void autostartAsTrayIconChanged();
    void visibilitySettingsChanged();

private:
    struct Monitor;
    struct DdcWorker;

    Monitor *monitorAt(int index);
    const Monitor *monitorAt(int index) const;
    void setVisibilitySetting(bool &setting, bool value);
    void bumpRevision();
    void refreshDynamicContrastState();
    void sendVcp(Monitor &monitor,
                 std::initializer_list<std::pair<std::uint8_t, std::uint16_t>> writes);
    void flushDdcWrites();
    void loadSettings();
    void saveSettings() const;

    QString startupError_;
    std::vector<std::unique_ptr<Monitor>> monitors_;
    int revision_ = 0;
    QTimer ddcTimer_;
    std::unique_ptr<DdcWorker> ddcWorker_;

    int scrollStep_ = 2;
    int ddcDelay_ = 0;
    bool closeToTray_ = true;
    bool autostartAsTrayIcon_ = false;
    bool hideBrightness_ = false;
    bool hideContrast_ = false;
    bool hideVolume_ = false;
    bool hideInput_ = false;
    bool hideTrayIcon_ = false;
    QSize savedWindowSize_;
    bool dynamicContrastEnabled_ = false;
    bool dynamicContrastGlobal_ = true;
    double dynamicContrastRatio_ = 0.7;
    bool dynamicContrastPerMonitorRatio_ = false;
    QHash<QString, bool> monitorDynamicContrast_;
    QHash<QString, double> monitorRatios_;
};
