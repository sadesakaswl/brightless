#include "brightlesscontroller.h"

#include "model.h"
#include "themepresets.h"

#include "platform/ddcbackend.h"
#include "platform/autostart.h"
#include "platform/battery.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QThreadPool>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <optional>

namespace {

using brightless::defaultVcpCodes;

int percentFromVcp(const brightless::ddc::Value &value)
{
    if (value.maximum == 0) {
        return 0;
    }
    const auto current = std::min(value.current, value.maximum);
    return static_cast<int>((static_cast<std::uint32_t>(current) * 100) / value.maximum);
}

std::uint16_t vcpFromPercent(int percent, std::uint16_t maximum)
{
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(percent) * maximum) / 100);
}

} // namespace

struct BrightlessController::Monitor
{
    std::shared_ptr<brightless::ddc::Device> device;
    QString name;
    QString id;
    int brightness = 50;
    int contrast = 50;
    int volume = 50;
    int inputSourceCode = 0;
    int powerModeCode = 0;
    bool dynamicContrastEnabled = false;
    double dynamicContrastRatio = 0.7;
    bool supportsContrast = false;
    bool supportsVolume = false;
    bool supportsInputSource = false;
    bool supportsPowerMode = false;
    std::uint16_t maximumBrightness = 0;
    std::uint16_t maximumContrast = 0;
    std::uint16_t maximumVolume = 0;
    std::map<std::uint8_t, std::uint16_t> pendingWrites;
};

struct BrightlessController::DdcWorker
{
    using Values = std::map<std::uint8_t, std::uint16_t>;
    using Writes = std::map<std::shared_ptr<brightless::ddc::Device>, Values>;

    explicit DdcWorker(BrightlessController *owner) : owner_(owner)
    {
        pool_.setMaxThreadCount(1);
    }

    ~DdcWorker()
    {
        wait();
    }

    void submit(Writes writes)
    {
        if (writes.empty()) {
            return;
        }

        bool start = false;
        {
            const std::scoped_lock lock(mutex_);
            for (const auto &[reference, values] : writes) {
                auto &latest = pending_[reference];
                for (const auto &[code, value] : values) {
                    latest.insert_or_assign(code, value);
                }
            }
            if (!running_) {
                running_ = true;
                start = true;
            }
        }

        if (start) {
            pool_.start([this] { run(); });
        }
    }

    void wait()
    {
        pool_.waitForDone();
    }

    template<typename Function> void scan(Function function) { pool_.start(std::move(function)); }

private:
    void run()
    {
        while (true) {
            Writes writes;
            {
                const std::scoped_lock lock(mutex_);
                if (pending_.empty()) {
                    running_ = false;
                    return;
                }
                writes.swap(pending_);
            }

            for (const auto &[device, values] : writes) {
                const auto error = brightless::ddc::write(*device, values);
                if (!error.isEmpty()) {
                    QMetaObject::invokeMethod(owner_, [owner = owner_, name = device->name, error] {
                        owner->setOperationError(tr("%1: %2").arg(name, error));
                    });
                }
            }
        }
    }

    BrightlessController *owner_;
    QThreadPool pool_;
    std::mutex mutex_;
    Writes pending_;
    bool running_ = false;
};

BrightlessController::BrightlessController(QObject *parent)
    : QObject(parent)
    , ddcWorker_(std::make_unique<DdcWorker>(this))
{
    ddcTimer_.setSingleShot(true);
    connect(&ddcTimer_, &QTimer::timeout, this, &BrightlessController::flushDdcWrites);
    settingsTimer_.setSingleShot(true);
    settingsTimer_.setInterval(250);
    connect(&settingsTimer_, &QTimer::timeout, this, &BrightlessController::saveSettings);
    loadSettings();
    connect(&timingTimer_, &QTimer::timeout, this, [this] {
        runTimings(QDateTime::currentDateTime());
    });
    timingTimer_.start(1000);
    connect(&batteryTimer_, &QTimer::timeout, this, &BrightlessController::refreshBattery);
    // shortcut: battery changes are polled every 30s; use platform notifications if instant triggers are needed.
    batteryTimer_.start(30000);
    QTimer::singleShot(0, this, &BrightlessController::refreshBattery);
}

BrightlessController::~BrightlessController()
{
    flushSettings();
    flushDdcWrites();
    ddcWorker_->wait();
}

void BrightlessController::setOperationError(const QString &error)
{
    if (operationError_ != error) {
        operationError_ = error;
        emit operationErrorChanged();
    }
}

void BrightlessController::flushSettings()
{
    if (settingsTimer_.isActive()) {
        saveSettings();
    }
}

QString BrightlessController::startupError() const
{
    return startupError_;
}

QStringList BrightlessController::monitorNames() const
{
    QStringList names;
    names.reserve(static_cast<qsizetype>(monitors_.size()));
    for (const auto &monitor : monitors_) {
        names.append(monitor->name);
    }
    return names;
}

QVariantList BrightlessController::ddcMonitors() const
{
    return ddcMonitors_;
}

int BrightlessController::monitorCount() const
{
    return static_cast<int>(monitors_.size());
}

int BrightlessController::revision() const
{
    return revision_;
}

bool BrightlessController::closeToTray() const
{
    return closeToTray_;
}

void BrightlessController::setCloseToTray(bool value)
{
    if (closeToTray_ == value) {
        return;
    }
    closeToTray_ = value;
    saveSettings();
    emit closeToTrayChanged();
}

bool BrightlessController::hideOnTrayClick() const
{
    return hideOnTrayClick_;
}

void BrightlessController::setHideOnTrayClick(bool value)
{
    if (hideOnTrayClick_ == value) {
        return;
    }
    hideOnTrayClick_ = value;
    saveSettings();
    emit hideOnTrayClickChanged();
}

void BrightlessController::setThemeMode(const QString &mode)
{
    if (themeMode_ == mode || !brightless::isThemeMode(mode)) return;
    themeMode_ = mode;
    saveSettings();
    emit themeModeChanged();
}

bool BrightlessController::hideBrightness() const
{
    return hideBrightness_;
}

void BrightlessController::setHideBrightness(bool value)
{
    setVisibilitySetting(hideBrightness_, value);
}

bool BrightlessController::hideContrast() const
{
    return hideContrast_;
}

void BrightlessController::setHideContrast(bool value)
{
    setVisibilitySetting(hideContrast_, value);
}

bool BrightlessController::hideVolume() const
{
    return hideVolume_;
}

void BrightlessController::setHideVolume(bool value)
{
    setVisibilitySetting(hideVolume_, value);
}

bool BrightlessController::hideInput() const
{
    return hideInput_;
}

void BrightlessController::setHideInput(bool value)
{
    setVisibilitySetting(hideInput_, value);
}

bool BrightlessController::hideTrayIcon() const
{
    return hideTrayIcon_;
}

void BrightlessController::setHideTrayIcon(bool value)
{
    setVisibilitySetting(hideTrayIcon_, value);
}

bool BrightlessController::autostart() const
{
    return brightless::autostartEnabled();
}

void BrightlessController::setAutostart(bool value)
{
    if (autostart() == value) {
        return;
    }

    setOperationError(brightless::setAutostartEnabled(value));
    emit autostartChanged();
}

bool BrightlessController::autostartAsTrayIcon() const
{
    return autostartAsTrayIcon_;
}

void BrightlessController::setAutostartAsTrayIcon(bool value)
{
    if (autostartAsTrayIcon_ == value) {
        return;
    }
    autostartAsTrayIcon_ = value;
    saveSettings();
    emit autostartAsTrayIconChanged();
}

bool BrightlessController::plasmaGlobalShortcuts() const
{
    return plasmaGlobalShortcuts_;
}

void BrightlessController::setPlasmaGlobalShortcuts(bool value)
{
    if (plasmaGlobalShortcuts_ == value) {
        return;
    }
    plasmaGlobalShortcuts_ = value;
    saveSettings();
    emit plasmaGlobalShortcutsChanged();
}

QSize BrightlessController::savedWindowSize() const
{
    return savedWindowSize_;
}

void BrightlessController::saveWindowSize(const QSize &size)
{
    if (size.isEmpty() || savedWindowSize_ == size) {
        return;
    }
    savedWindowSize_ = size;
    saveSettings();
}

int BrightlessController::adjustAllBrightness(int direction)
{
    if (monitors_.empty()) {
        return -1;
    }

    int total = 0;
    for (int index = 0; index < monitorCount(); ++index) {
        auto *monitor = monitorAt(index);
        const auto value = brightless::stepPercent(monitor->brightness, scrollStep_, direction);
        if (value != monitor->brightness) {
            if (monitor->dynamicContrastEnabled) {
                set_dynamic_contrast_brightness(index, value);
            } else {
                set_brightness(index, value);
            }
        }
        total += monitor->brightness;
    }
    return total / monitorCount();
}

void BrightlessController::adjustAllContrast(int direction)
{
    for (int index = 0; index < monitorCount(); ++index) {
        auto *monitor = monitorAt(index);
        if (!monitor->supportsContrast) {
            continue;
        }
        const auto value = brightless::stepPercent(monitor->contrast, scrollStep_, direction);
        if (value != monitor->contrast) {
            set_contrast(index, value);
        }
    }
}

int BrightlessController::adjustAllVolume(int direction)
{
    int total = 0;
    int count = 0;
    for (int index = 0; index < monitorCount(); ++index) {
        auto *monitor = monitorAt(index);
        if (!monitor->supportsVolume) {
            continue;
        }
        const auto value = brightless::stepPercent(monitor->volume, scrollStep_, direction);
        if (value != monitor->volume) {
            set_volume(index, value);
        }
        total += monitor->volume;
        ++count;
    }
    return count > 0 ? total / count : -1;
}

void BrightlessController::changeAllInputSources()
{
    for (int index = 0; index < monitorCount(); ++index) {
        auto *monitor = monitorAt(index);
        if (monitor->supportsInputSource) {
            set_input_source(index, brightless::nextInputSource(monitor->inputSourceCode));
        }
    }
}

std::optional<BrightlessController::Timing> BrightlessController::parseTiming(const QJsonObject &entry)
{
    if (!entry.value("monitor_id").isString() || !entry.value("enabled").isBool()) {
        return std::nullopt;
    }
    Timing timing{entry.value("time").toString(QStringLiteral("00:00")),
                  entry.value("monitor_id").toString(), entry.value("control").toInt(-1),
                  entry.value("value").toInt(-1), entry.value("enabled").toBool(), {}};
    // Existing settings have no trigger field and remain daily time schedules.
    if (entry.contains("trigger")) {
        if (!entry.value("trigger").isString()) return std::nullopt;
        timing.trigger = entry.value("trigger").toString();
    }
    const auto time = QTime::fromString(timing.time, QStringLiteral("HH:mm"));
    if (!time.isValid() || time.toString(QStringLiteral("HH:mm")) != timing.time
        || (timing.trigger == "time" && !entry.value("time").isString())) {
        return std::nullopt;
    }
    if (entry.contains("battery_percent") || timing.trigger == "battery") {
        timing.batteryPercent = entry.value("battery_percent").toInt(-1);
        if (timing.batteryPercent < 0 || timing.batteryPercent > 100) return std::nullopt;
    }
    if (entry.contains("battery_condition") || timing.trigger == "battery") {
        timing.batteryCondition = entry.value("battery_condition").toString();
        if (timing.batteryCondition != "below" && timing.batteryCondition != "above")
            return std::nullopt;
    }
    if (timing.trigger != "time" && timing.trigger != "battery") return std::nullopt;
    switch (timing.control) {
    case 0x10:
    case 0x12:
    case 0x62:
        if (timing.value < 0 || timing.value > 100) return std::nullopt;
        break;
    case 0x60:
        if (std::find(brightless::inputSourceCodes.begin(), brightless::inputSourceCodes.end(),
                      timing.value) == brightless::inputSourceCodes.end()) return std::nullopt;
        break;
    case 0xd6:
        if (timing.value < 1 || timing.value > 5) return std::nullopt;
        break;
    default:
        return std::nullopt;
    }
    return timing;
}

void BrightlessController::setUse12HourTime(bool value)
{
    if (use12HourTime_ == value) return;
    use12HourTime_ = value;
    saveSettings();
    emit timeFormatChanged();
}

QVariantList BrightlessController::timings() const
{
    QVariantList entries;
    for (const auto &timing : timings_) {
        entries.append(QVariantMap{{"time", timing.time}, {"monitor_id", timing.monitorId},
            {"control", timing.control}, {"value", timing.value}, {"enabled", timing.enabled},
            {"trigger", timing.trigger}, {"battery_percent", timing.batteryPercent},
            {"battery_condition", timing.batteryCondition}});
    }
    return entries;
}

void BrightlessController::add_timing()
{
    timings_.push_back({QTime::currentTime().toString(QStringLiteral("HH:mm")), {}});
    saveSettings();
    emit timingsChanged();
}

bool BrightlessController::update_timing(int index, const QVariantMap &entry)
{
    if (!brightless::validIndex(index, timings_.size())) return false;
    auto timing = parseTiming(QJsonObject::fromVariantMap(entry));
    if (!timing) return false;
    const auto &previous = timings_[index];
    if (timing->trigger == previous.trigger) {
        if (timing->time == previous.time) timing->lastRun = previous.lastRun;
        if (timing->enabled == previous.enabled
            && timing->batteryPercent == previous.batteryPercent
            && timing->batteryCondition == previous.batteryCondition) {
            timing->batteryMatched = previous.batteryMatched;
        }
    }
    timings_[index] = *timing;
    saveSettings();
    emit timingsChanged();
    return true;
}

void BrightlessController::remove_timing(int index)
{
    if (!brightless::validIndex(index, timings_.size())) return;
    timings_.erase(timings_.begin() + index);
    saveSettings();
    emit timingsChanged();
}

void BrightlessController::runTimings(const QDateTime &now)
{
    if (!now.isValid() || loading_ || monitors_.empty()) return;
    const auto time = now.time().toString(QStringLiteral("HH:mm"));
    for (auto &timing : timings_) {
        // shortcut: missed minutes are skipped; add resume catch-up if needed.
        if (!timing.enabled || timing.trigger != "time" || timing.time != time
            || timing.lastRun == now.date()) continue;
        timing.lastRun = now.date();
        applyTiming(timing);
    }
}

void BrightlessController::refreshBattery()
{
    if (batteryReadPending_) return;
    batteryReadPending_ = true;
    brightless::battery::readPercentage(this, [this](std::optional<int> percentage) {
        batteryReadPending_ = false;
        runBatteryTimings(percentage);
    });
}

void BrightlessController::runBatteryTimings(std::optional<int> percentage)
{
    if (percentage && (*percentage < 0 || *percentage > 100)) percentage.reset();
    const auto value = percentage.value_or(-1);
    if (batteryPercent_ != value) {
        batteryPercent_ = value;
        emit batteryPercentChanged();
    }
    // Unknown readings do not re-arm a threshold that has already fired.
    if (!percentage || loading_ || monitors_.empty()) return;
    for (auto &timing : timings_) {
        if (!timing.enabled || timing.trigger != "battery") continue;
        const bool matched = timing.batteryCondition == "below"
            ? *percentage <= timing.batteryPercent : *percentage >= timing.batteryPercent;
        if (matched && !timing.batteryMatched) applyTiming(timing);
        timing.batteryMatched = matched;
    }
}

void BrightlessController::applyTiming(const Timing &timing)
{
    for (int index = 0; index < monitorCount(); ++index) {
        const auto *monitor = monitorAt(index);
        if (!timing.monitorId.isEmpty() && timing.monitorId != monitor->id) continue;
        switch (timing.control) {
        case 0x10:
            if (monitor_dynamic_contrast_enabled(index)) {
                set_dynamic_contrast_brightness(index, timing.value);
            } else {
                set_brightness(index, timing.value);
            }
            break;
        case 0x12:
            if (supports_contrast(index)) set_contrast(index, timing.value);
            break;
        case 0x62:
            if (supports_volume(index)) set_volume(index, timing.value);
            break;
        case 0x60:
            if (supports_input_source(index)) set_input_source(index, timing.value);
            break;
        case 0xd6:
            if (supports_power_mode(index)) set_power_mode(index, timing.value);
            break;
        }
    }
}

void BrightlessController::initialize()
{
    if (loading_) {
        rescanRequested_ = true;
        return;
    }
    flushDdcWrites();
    loading_ = true;
    emit loadingChanged();
    // No controls may enqueue writes against a previous discovery while scanning.
    monitors_.clear();
    ddcMonitors_.clear();
    emit monitorNamesChanged();
    emit monitorCountChanged();
    ddcWorker_->scan([this, perMonitor = vcpPerMonitor_, globalCodes = vcpCodes_,
                      monitorCodes = monitorVcpCodes_] {
        struct Result {
            std::vector<std::unique_ptr<Monitor>> monitors;
            QVariantList devices;
            QString error;
        };
        auto result = std::make_shared<Result>();
        auto devices = brightless::ddc::enumerate(result->error);
        for (auto &device : devices) {
            if (device.name.isEmpty()) {
                device.name = tr("Monitor %1").arg(result->devices.size() + 1);
            }
            const auto label = device.connection.isEmpty() ? device.name
                : QStringLiteral("%1 (%2)").arg(device.name, device.connection);
            result->devices.append(QVariantMap{{QStringLiteral("id"), device.id},
                {QStringLiteral("name"), label}});
            const auto overrides = perMonitor ? monitorCodes.value(device.id) : globalCodes;
            brightless::ddc::Codes codes;
            for (std::size_t i = 0; i < codes.size(); ++i) {
                codes[i] = static_cast<std::uint8_t>(overrides.value(defaultVcpCodes[i], defaultVcpCodes[i]));
            }
            const auto values = brightless::ddc::read(device, codes);
            if (!values[0] || values[0]->maximum == 0) {
                continue;
            }
            auto monitor = std::make_unique<Monitor>();
            monitor->name = device.name;
            monitor->id = device.id;
            monitor->device = std::make_shared<brightless::ddc::Device>(std::move(device));
            monitor->maximumBrightness = values[0]->maximum;
            monitor->brightness = percentFromVcp(*values[0]);
            if (const auto value = values[1]) {
                monitor->maximumContrast = value->maximum;
                monitor->supportsContrast = value->maximum > 0;
                monitor->contrast = percentFromVcp(*value);
            }
            if (const auto value = values[2]) {
                monitor->maximumVolume = value->maximum;
                monitor->supportsVolume = value->maximum > 0;
                monitor->volume = percentFromVcp(*value);
            }
            if (const auto value = values[3]) {
                monitor->inputSourceCode = value->current & 0xff;
                monitor->supportsInputSource = monitor->inputSourceCode >= 1 && monitor->inputSourceCode <= 27;
            }
            if (const auto value = values[4]) {
                monitor->powerModeCode = value->current & 0xff;
                monitor->supportsPowerMode = monitor->powerModeCode >= 1 && monitor->powerModeCode <= 5;
            }
            result->monitors.push_back(std::move(monitor));
        }
        if (result->error.isEmpty() && result->monitors.empty()) {
            result->error = tr("No DDC monitors found");
        }
        QMetaObject::invokeMethod(this, [this, result] {
            monitors_ = std::move(result->monitors);
            ddcMonitors_ = std::move(result->devices);
            startupError_ = result->error;
            refreshDynamicContrastState();
            emit startupErrorChanged();
            emit monitorNamesChanged();
            emit monitorCountChanged();
            bumpRevision();
            loading_ = false;
            emit loadingChanged();
            if (std::exchange(rescanRequested_, false)) {
                initialize();
            }
        });
    });
}

int BrightlessController::brightness(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? monitor->brightness : 0;
}

void BrightlessController::set_brightness(int index, int value)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->brightness = brightless::clampPercent(value);
    if (monitor->maximumBrightness > 0) {
        sendVcp(*monitor,
                {{0x10, vcpFromPercent(monitor->brightness, monitor->maximumBrightness)}});
    }
    bumpRevision();
}

int BrightlessController::contrast(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? monitor->contrast : 0;
}

void BrightlessController::set_contrast(int index, int value)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->contrast = brightless::clampPercent(value);
    if (monitor->maximumContrast > 0) {
        sendVcp(*monitor,
                {{0x12, vcpFromPercent(monitor->contrast, monitor->maximumContrast)}});
    }
    bumpRevision();
}

int BrightlessController::volume(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? monitor->volume : 0;
}

void BrightlessController::set_volume(int index, int value)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->volume = brightless::clampPercent(value);
    if (monitor->maximumVolume > 0) {
        sendVcp(*monitor,
                {{0x62, vcpFromPercent(monitor->volume, monitor->maximumVolume)}});
    }
    bumpRevision();
}

int BrightlessController::input_source_code(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? monitor->inputSourceCode : 0;
}

void BrightlessController::set_input_source(int index, int code)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->inputSourceCode = std::clamp(code, 0, 255);
    sendVcp(*monitor, {{0x60, static_cast<std::uint16_t>(monitor->inputSourceCode)}});
    bumpRevision();
}

int BrightlessController::power_mode_code(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? monitor->powerModeCode : 0;
}

void BrightlessController::set_power_mode(int index, int code)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->powerModeCode = std::clamp(code, 0, 255);
    sendVcp(*monitor, {{0xd6, static_cast<std::uint16_t>(monitor->powerModeCode)}});
    bumpRevision();
}

bool BrightlessController::supports_contrast(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor && monitor->supportsContrast;
}

bool BrightlessController::supports_volume(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor && monitor->supportsVolume;
}

bool BrightlessController::supports_input_source(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor && monitor->supportsInputSource;
}

bool BrightlessController::supports_power_mode(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor && monitor->supportsPowerMode;
}

int BrightlessController::scroll_step() const
{
    return scrollStep_;
}

void BrightlessController::set_scroll_step(int value)
{
    value = std::clamp(value, 1, 10);
    if (scrollStep_ == value) {
        return;
    }
    scrollStep_ = value;
    settingsTimer_.start();
    bumpRevision();
}

bool BrightlessController::vcp_per_monitor() const
{
    return vcpPerMonitor_;
}

void BrightlessController::set_vcp_per_monitor(bool value)
{
    if (vcpPerMonitor_ == value) {
        return;
    }
    vcpPerMonitor_ = value;
    saveSettings();
    bumpRevision();
}

int BrightlessController::vcp_code(int defaultCode, const QString &monitorId) const
{
    const auto codes = monitorId.isEmpty() ? vcpCodes_ : monitorVcpCodes_.value(monitorId);
    return codes.value(defaultCode, defaultCode);
}

bool BrightlessController::set_vcp_code(int defaultCode, int code, const QString &monitorId)
{
    if (std::find(defaultVcpCodes.begin(), defaultVcpCodes.end(), defaultCode)
            == defaultVcpCodes.end()
        || code < 0 || code > 0xff || vcp_code(defaultCode, monitorId) == code) {
        return false;
    }

    auto &codes = monitorId.isEmpty() ? vcpCodes_ : monitorVcpCodes_[monitorId];
    if (code == defaultCode) {
        codes.remove(defaultCode);
    } else {
        codes.insert(defaultCode, code);
    }
    saveSettings();
    bumpRevision();
    return true;
}

int BrightlessController::ddc_delay() const
{
    return ddcDelay_;
}

void BrightlessController::set_ddc_delay(int value)
{
    const auto delay = std::clamp(value, 0, 1500);
    if (ddcDelay_ == delay) {
        return;
    }

    ddcDelay_ = delay;
    settingsTimer_.start();
    if (ddcDelay_ == 0) {
        flushDdcWrites();
    } else if (ddcTimer_.isActive()) {
        ddcTimer_.start(ddcDelay_);
    }
    bumpRevision();
}

bool BrightlessController::dynamic_contrast_enabled() const
{
    return dynamicContrastEnabled_;
}

void BrightlessController::set_dynamic_contrast_enabled(bool value)
{
    if (dynamicContrastEnabled_ == value) {
        return;
    }
    dynamicContrastEnabled_ = value;
    saveSettings();
    refreshDynamicContrastState();
    bumpRevision();
}

bool BrightlessController::dynamic_contrast_global() const
{
    return dynamicContrastGlobal_;
}

void BrightlessController::set_dynamic_contrast_global(bool value)
{
    if (dynamicContrastGlobal_ == value) {
        return;
    }
    dynamicContrastGlobal_ = value;
    saveSettings();
    refreshDynamicContrastState();
    bumpRevision();
}

float BrightlessController::dynamic_contrast_ratio() const
{
    return static_cast<float>(dynamicContrastRatio_);
}

void BrightlessController::set_dynamic_contrast_ratio(float value)
{
    if (!std::isfinite(value)) {
        return;
    }
    const auto ratio = brightless::clampRatio(value);
    if (dynamicContrastRatio_ == ratio) {
        return;
    }
    dynamicContrastRatio_ = ratio;
    settingsTimer_.start();
    refreshDynamicContrastState();
    bumpRevision();
}

bool BrightlessController::dynamic_contrast_per_monitor_ratio() const
{
    return dynamicContrastPerMonitorRatio_;
}

void BrightlessController::set_dynamic_contrast_per_monitor_ratio(bool value)
{
    if (dynamicContrastPerMonitorRatio_ == value) {
        return;
    }
    dynamicContrastPerMonitorRatio_ = value;
    saveSettings();
    refreshDynamicContrastState();
    bumpRevision();
}

bool BrightlessController::monitor_dynamic_contrast_enabled(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor && monitor->dynamicContrastEnabled;
}

void BrightlessController::set_monitor_dynamic_contrast_enabled(int index, bool value)
{
    const auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitorDynamicContrast_.insert(monitor->id, value);
    saveSettings();
    refreshDynamicContrastState();
    bumpRevision();
}

float BrightlessController::monitor_ratio(int index) const
{
    const auto *monitor = monitorAt(index);
    return monitor ? static_cast<float>(monitor->dynamicContrastRatio) : 0.7F;
}

void BrightlessController::set_monitor_ratio(int index, float value)
{
    const auto *monitor = monitorAt(index);
    if (!monitor || !std::isfinite(value)) {
        return;
    }

    const auto ratio = brightless::clampRatio(value);
    if (monitorRatios_.contains(monitor->id) && monitorRatios_.value(monitor->id) == ratio) {
        return;
    }
    monitorRatios_.insert(monitor->id, ratio);
    settingsTimer_.start();
    refreshDynamicContrastState();
    bumpRevision();
}

void BrightlessController::set_dynamic_contrast_brightness(int index, int value)
{
    auto *monitor = monitorAt(index);
    if (!monitor) {
        return;
    }

    monitor->brightness = brightless::clampPercent(value);
    monitor->contrast = brightless::contrastForDynamicBrightness(
        monitor->brightness, monitor->dynamicContrastRatio);

    if (monitor->maximumBrightness > 0 && monitor->maximumContrast > 0) {
        sendVcp(*monitor,
                {
                    {0x10, vcpFromPercent(monitor->brightness, monitor->maximumBrightness)},
                    {0x12, vcpFromPercent(monitor->contrast, monitor->maximumContrast)},
                });
    } else if (monitor->maximumBrightness > 0) {
        sendVcp(*monitor,
                {{0x10, vcpFromPercent(monitor->brightness, monitor->maximumBrightness)}});
    } else if (monitor->maximumContrast > 0) {
        sendVcp(*monitor,
                {{0x12, vcpFromPercent(monitor->contrast, monitor->maximumContrast)}});
    }
    bumpRevision();
}

void BrightlessController::sendVcp(
    Monitor &monitor,
    std::initializer_list<std::pair<std::uint8_t, std::uint16_t>> writes)
{
    for (const auto &[code, value] : writes) {
        monitor.pendingWrites.insert_or_assign(
            vcp_code(code, vcpPerMonitor_ ? monitor.id : QString()), value);
    }

    if (ddcDelay_ == 0) {
        flushDdcWrites();
    } else if (writes.size() > 0) {
        // ponytail: one timer debounces all monitors; split timers if simultaneous use matters.
        ddcTimer_.start(ddcDelay_);
    }
}

void BrightlessController::flushDdcWrites()
{
    ddcTimer_.stop();
    DdcWorker::Writes writes;
    for (const auto &monitor : monitors_) {
        if (!monitor->pendingWrites.empty()) {
            writes.emplace(monitor->device, std::exchange(monitor->pendingWrites, {}));
        }
    }
    ddcWorker_->submit(std::move(writes));
}

BrightlessController::Monitor *BrightlessController::monitorAt(int index)
{
    return brightless::validIndex(index, monitors_.size())
        ? monitors_[static_cast<std::size_t>(index)].get()
        : nullptr;
}

const BrightlessController::Monitor *BrightlessController::monitorAt(int index) const
{
    return brightless::validIndex(index, monitors_.size())
        ? monitors_[static_cast<std::size_t>(index)].get()
        : nullptr;
}

void BrightlessController::setVisibilitySetting(bool &setting, bool value)
{
    if (setting == value) {
        return;
    }
    setting = value;
    saveSettings();
    emit visibilitySettingsChanged();
}

void BrightlessController::bumpRevision()
{
    revision_ = revision_ == std::numeric_limits<int>::max() ? 0 : revision_ + 1;
    emit revisionChanged();
}

void BrightlessController::refreshDynamicContrastState()
{
    bool migrated = false;
    for (auto &monitor : monitors_) {
        // Migrate name-based preferences once each attached monitor has an ID.
        if (!monitorDynamicContrast_.contains(monitor->id) && monitorDynamicContrast_.contains(monitor->name)) {
            monitorDynamicContrast_.insert(monitor->id, monitorDynamicContrast_.value(monitor->name));
            migrated = true;
        }
        if (!monitorRatios_.contains(monitor->id) && monitorRatios_.contains(monitor->name)) {
            monitorRatios_.insert(monitor->id, monitorRatios_.value(monitor->name));
            migrated = true;
        }
        monitor->dynamicContrastEnabled = dynamicContrastEnabled_ && monitor->supportsContrast
            && (dynamicContrastGlobal_ || monitorDynamicContrast_.value(monitor->id, true));
        monitor->dynamicContrastRatio = dynamicContrastPerMonitorRatio_
            ? monitorRatios_.value(monitor->id, dynamicContrastRatio_)
            : dynamicContrastRatio_;
    }
    if (migrated) {
        settingsTimer_.start();
    }
}
