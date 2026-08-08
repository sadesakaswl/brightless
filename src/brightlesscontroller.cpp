#include "brightlesscontroller.h"

#include "model.h"

#include <ddcutil_c_api.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <optional>

namespace {

struct VcpValue
{
    std::uint16_t current;
    std::uint16_t maximum;
};

std::optional<VcpValue> readVcp(DDCA_Display_Handle handle, std::uint8_t code)
{
    DDCA_Non_Table_Vcp_Value value{};
    if (ddca_get_non_table_vcp_value(handle, code, &value) != 0) {
        return std::nullopt;
    }

    return VcpValue{
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(value.sh) << 8) | value.sl),
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(value.mh) << 8) | value.ml),
    };
}

int percentFromVcp(const VcpValue &value)
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

QString displayName(const DDCA_Display_Info &info, int number)
{
    const auto model = QString::fromLatin1(info.model_name).trimmed();
    if (!model.isEmpty()) {
        return model;
    }

    const auto manufacturer = QString::fromLatin1(info.mfg_id).trimmed();
    if (!manufacturer.isEmpty()) {
        return QStringLiteral("%1 %2")
            .arg(manufacturer)
            .arg(info.product_code, 4, 16, QLatin1Char('0'));
    }

    return QCoreApplication::translate("BrightlessController", "Monitor %1").arg(number);
}

QString ddcError(const QString &operation, DDCA_Status status)
{
    const char *description = ddca_rc_desc(status);
    const auto detail = description
        ? QString::fromLocal8Bit(description)
        : QCoreApplication::translate("BrightlessController", "unknown error");
    return QStringLiteral("%1: %2").arg(operation, detail);
}

QString settingsPath()
{
    auto root = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }
    return QDir(root).filePath(QStringLiteral("brightless/settings.json"));
}

QString autostartPath()
{
    const auto root = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return root.isEmpty() ? QString()
                          : QDir(root).filePath(QStringLiteral("autostart/brightless.desktop"));
}

QByteArray autostartEntry()
{
    const auto path = QCoreApplication::applicationFilePath();
    if (path.contains(QLatin1Char('\n')) || path.contains(QLatin1Char('\r'))
        || path.contains(QLatin1Char('\t'))) {
        return {};
    }

    QString executable;
    executable.reserve(path.size());
    for (const auto character : path) {
        if (character == QLatin1Char('%')) {
            executable += QStringLiteral("%%");
        } else if (character == QLatin1Char('\\')) {
            executable += QStringLiteral("\\\\\\\\");
        } else if (character == QLatin1Char('"') || character == QLatin1Char('`')
                   || character == QLatin1Char('$')) {
            executable += QStringLiteral("\\\\");
            executable += character;
        } else {
            executable += character;
        }
    }

    return QStringLiteral("[Desktop Entry]\nType=Application\nName=Brightless\n"
                          "Exec=\"%1\" --autostart\nTerminal=false\n")
        .arg(executable)
        .toUtf8();
}

bool writeAutostartEntry()
{
    const auto path = autostartPath();
    const auto data = autostartEntry();
    if (path.isEmpty() || data.isEmpty()
        || !QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace

struct BrightlessController::Monitor
{
    DDCA_Display_Ref reference = nullptr;
    QString name;
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
    using Writes = std::map<DDCA_Display_Ref, Values>;

    DdcWorker()
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

            for (const auto &[reference, values] : writes) {
                DDCA_Display_Handle handle = nullptr;
                if (ddca_open_display2(reference, false, &handle) != 0) {
                    continue;
                }
                for (const auto &[code, value] : values) {
                    ddca_set_non_table_vcp_value(handle, code,
                                                 static_cast<std::uint8_t>(value >> 8),
                                                 static_cast<std::uint8_t>(value & 0xff));
                }
                ddca_close_display(handle);
            }
        }
    }

    QThreadPool pool_;
    std::mutex mutex_;
    Writes pending_;
    bool running_ = false;
};

BrightlessController::BrightlessController(QObject *parent)
    : QObject(parent)
    , ddcWorker_(std::make_unique<DdcWorker>())
{
    ddcTimer_.setSingleShot(true);
    connect(&ddcTimer_, &QTimer::timeout, this, &BrightlessController::flushDdcWrites);
    loadSettings();
}

BrightlessController::~BrightlessController()
{
    flushDdcWrites();
    ddcWorker_->wait();
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
    const auto path = autostartPath();
    return !path.isEmpty() && QFileInfo(path).isFile();
}

void BrightlessController::setAutostart(bool value)
{
    if (autostart() == value) {
        return;
    }

    if (value) {
        writeAutostartEntry();
    } else {
        const auto path = autostartPath();
        if (!path.isEmpty()) {
            QFile::remove(path);
        }
    }
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

void BrightlessController::adjustAllVolume(int direction)
{
    for (int index = 0; index < monitorCount(); ++index) {
        auto *monitor = monitorAt(index);
        if (!monitor->supportsVolume) {
            continue;
        }
        const auto value = brightless::stepPercent(monitor->volume, scrollStep_, direction);
        if (value != monitor->volume) {
            set_volume(index, value);
        }
    }
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

void BrightlessController::initialize()
{
    flushDdcWrites();
    ddcWorker_->wait();
    monitors_.clear();
    QString error;

    DDCA_Display_Info_List *rawList = nullptr;
    const auto status = ddca_get_display_info_list2(false, &rawList);
    const std::unique_ptr<DDCA_Display_Info_List, decltype(&ddca_free_display_info_list)> displayList(
        rawList, &ddca_free_display_info_list);

    if (status != 0) {
        error = ddcError(tr("Failed to detect displays"), status);
    } else if (displayList) {
        for (int index = 0; index < displayList->ct; ++index) {
            const auto &info = displayList->info[index];
            DDCA_Display_Handle handle = nullptr;
            if (ddca_open_display2(info.dref, false, &handle) != 0) {
                continue;
            }

            auto monitor = std::make_unique<Monitor>();
            monitor->reference = info.dref;
            monitor->name = displayName(info, index + 1);

            const auto brightnessValue = readVcp(handle, 0x10);
            if (!brightnessValue) {
                ddca_close_display(handle);
                continue;
            }
            monitor->maximumBrightness = brightnessValue->maximum;
            monitor->brightness = percentFromVcp(*brightnessValue);

            if (const auto value = readVcp(handle, 0x12)) {
                monitor->maximumContrast = value->maximum;
                monitor->supportsContrast = value->maximum > 0;
                if (monitor->supportsContrast) {
                    monitor->contrast = percentFromVcp(*value);
                }
            }

            if (const auto value = readVcp(handle, 0x62)) {
                monitor->maximumVolume = value->maximum;
                monitor->supportsVolume = value->maximum > 0;
                monitor->volume = percentFromVcp(*value);
            }

            if (const auto value = readVcp(handle, 0x60)) {
                monitor->inputSourceCode = value->current & 0xff;
                monitor->supportsInputSource = monitor->inputSourceCode >= 1
                    && monitor->inputSourceCode <= 27;
            }

            if (const auto value = readVcp(handle, 0xd6)) {
                monitor->powerModeCode = value->current & 0xff;
                monitor->supportsPowerMode = monitor->powerModeCode >= 1
                    && monitor->powerModeCode <= 5;
            }

            ddca_close_display(handle);
            monitors_.push_back(std::move(monitor));
        }
    }

    if (error.isEmpty() && monitors_.empty()) {
        error = tr("No DDC monitors found");
    }

    refreshDynamicContrastState();
    if (startupError_ != error) {
        startupError_ = error;
        emit startupErrorChanged();
    }
    emit monitorNamesChanged();
    emit monitorCountChanged();
    bumpRevision();
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
    scrollStep_ = std::clamp(value, 1, 10);
    saveSettings();
    bumpRevision();
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
    saveSettings();
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
    dynamicContrastRatio_ = brightless::clampRatio(value);
    saveSettings();
    refreshDynamicContrastState();
    bumpRevision();
}

bool BrightlessController::dynamic_contrast_per_monitor_ratio() const
{
    return dynamicContrastPerMonitorRatio_;
}

void BrightlessController::set_dynamic_contrast_per_monitor_ratio(bool value)
{
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

    monitorDynamicContrast_.insert(monitor->name, value);
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

    monitorRatios_.insert(monitor->name, brightless::clampRatio(value));
    saveSettings();
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
        monitor.pendingWrites.insert_or_assign(code, value);
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
            writes.emplace(monitor->reference, std::exchange(monitor->pendingWrites, {}));
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
    for (auto &monitor : monitors_) {
        monitor->dynamicContrastEnabled = dynamicContrastEnabled_
            && (dynamicContrastGlobal_ || monitorDynamicContrast_.value(monitor->name, true));
        monitor->dynamicContrastRatio = dynamicContrastPerMonitorRatio_
            ? monitorRatios_.value(monitor->name, dynamicContrastRatio_)
            : dynamicContrastRatio_;
    }
}

void BrightlessController::loadSettings()
{
    QFile file(settingsPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    const auto object = document.object();
    if (const auto value = object.value(QStringLiteral("scroll_step")); value.isDouble()) {
        scrollStep_ = std::clamp(value.toInt(scrollStep_), 1, 10);
    }
    if (const auto value = object.value(QStringLiteral("ddc_delay")); value.isDouble()) {
        ddcDelay_ = std::clamp(value.toInt(ddcDelay_), 0, 1500);
    }
    if (const auto value = object.value(QStringLiteral("hide_brightness")); value.isBool()) {
        hideBrightness_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("hide_contrast")); value.isBool()) {
        hideContrast_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("hide_volume")); value.isBool()) {
        hideVolume_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("hide_input")); value.isBool()) {
        hideInput_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("hide_tray_icon")); value.isBool()) {
        hideTrayIcon_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("close_to_tray")); value.isBool()) {
        closeToTray_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("autostart_as_tray_icon")); value.isBool()) {
        autostartAsTrayIcon_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("plasma_global_shortcuts")); value.isBool()) {
        plasmaGlobalShortcuts_ = value.toBool();
    }
    const auto windowWidth = object.value(QStringLiteral("window_width"));
    const auto windowHeight = object.value(QStringLiteral("window_height"));
    if (windowWidth.isDouble() && windowHeight.isDouble()) {
        const QSize size(windowWidth.toInt(), windowHeight.toInt());
        if (!size.isEmpty()) {
            savedWindowSize_ = size;
        }
    }
    if (const auto value = object.value(QStringLiteral("dynamic_contrast_enabled")); value.isBool()) {
        dynamicContrastEnabled_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("dynamic_contrast_global")); value.isBool()) {
        dynamicContrastGlobal_ = value.toBool();
    }
    if (const auto value = object.value(QStringLiteral("dynamic_contrast_ratio")); value.isDouble()) {
        const auto ratio = value.toDouble();
        if (std::isfinite(ratio)) {
            dynamicContrastRatio_ = brightless::clampRatio(ratio);
        }
    }
    if (const auto value = object.value(QStringLiteral("dynamic_contrast_per_monitor_ratio"));
        value.isBool()) {
        dynamicContrastPerMonitorRatio_ = value.toBool();
    }

    const auto monitorContrast = object.value(QStringLiteral("monitor_dynamic_contrast"));
    if (monitorContrast.isObject()) {
        const auto contrast = monitorContrast.toObject();
        for (auto it = contrast.constBegin(); it != contrast.constEnd(); ++it) {
            if (it.value().isBool()) {
                monitorDynamicContrast_.insert(it.key(), it.value().toBool());
            }
        }
    }

    const auto monitorRatios = object.value(QStringLiteral("monitor_ratios"));
    if (monitorRatios.isObject()) {
        const auto ratios = monitorRatios.toObject();
        for (auto it = ratios.constBegin(); it != ratios.constEnd(); ++it) {
            const auto ratio = it.value().toDouble(std::numeric_limits<double>::quiet_NaN());
            if (it.value().isDouble() && std::isfinite(ratio)) {
                monitorRatios_.insert(it.key(), brightless::clampRatio(ratio));
            }
        }
    }
}

void BrightlessController::saveSettings() const
{
    QJsonObject monitorContrast;
    for (auto it = monitorDynamicContrast_.constBegin(); it != monitorDynamicContrast_.constEnd();
         ++it) {
        monitorContrast.insert(it.key(), it.value());
    }

    QJsonObject monitorRatios;
    for (auto it = monitorRatios_.constBegin(); it != monitorRatios_.constEnd(); ++it) {
        monitorRatios.insert(it.key(), it.value());
    }

    QJsonObject object;
    object.insert(QStringLiteral("scroll_step"), scrollStep_);
    object.insert(QStringLiteral("ddc_delay"), ddcDelay_);
    object.insert(QStringLiteral("close_to_tray"), closeToTray_);
    object.insert(QStringLiteral("autostart_as_tray_icon"), autostartAsTrayIcon_);
    object.insert(QStringLiteral("plasma_global_shortcuts"), plasmaGlobalShortcuts_);
    object.insert(QStringLiteral("hide_brightness"), hideBrightness_);
    object.insert(QStringLiteral("hide_contrast"), hideContrast_);
    object.insert(QStringLiteral("hide_volume"), hideVolume_);
    object.insert(QStringLiteral("hide_input"), hideInput_);
    object.insert(QStringLiteral("hide_tray_icon"), hideTrayIcon_);
    if (!savedWindowSize_.isEmpty()) {
        object.insert(QStringLiteral("window_width"), savedWindowSize_.width());
        object.insert(QStringLiteral("window_height"), savedWindowSize_.height());
    }
    object.insert(QStringLiteral("dynamic_contrast_enabled"), dynamicContrastEnabled_);
    object.insert(QStringLiteral("dynamic_contrast_global"), dynamicContrastGlobal_);
    object.insert(QStringLiteral("dynamic_contrast_ratio"), dynamicContrastRatio_);
    object.insert(QStringLiteral("dynamic_contrast_per_monitor_ratio"),
                  dynamicContrastPerMonitorRatio_);
    object.insert(QStringLiteral("monitor_dynamic_contrast"), monitorContrast);
    object.insert(QStringLiteral("monitor_ratios"), monitorRatios);

    const auto path = settingsPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) == data.size()) {
        file.commit();
    } else {
        file.cancelWriting();
    }
}
