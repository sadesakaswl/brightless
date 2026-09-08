#include "brightlesscontroller.h"
#include "model.h"
#include "configpath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <limits>

namespace {
using brightless::defaultVcpCodes;
QHash<int, int> loadVcpCodes(const QJsonObject &object)
{
    QHash<int, int> codes;
    for (const auto defaultCode : defaultVcpCodes) {
        const auto code = object.value(QString::number(defaultCode, 16)).toInt(-1);
        if (code >= 0 && code <= 0xff && code != defaultCode) {
            codes.insert(defaultCode, code);
        }
    }
    return codes;
}

QJsonObject saveVcpCodes(const QHash<int, int> &codes)
{
    QJsonObject object;
    for (auto it = codes.constBegin(); it != codes.constEnd(); ++it) {
        object.insert(QString::number(it.key(), 16), it.value());
    }
    return object;
}

QString settingsPath()
{
    auto root = brightless::configRoot();
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }
    return QDir(root).filePath(QStringLiteral("brightless/settings.json"));
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
    vcpCodes_ = loadVcpCodes(object.value(QStringLiteral("vcp_codes")).toObject());
    vcpPerMonitor_ = object.value(QStringLiteral("vcp_per_monitor")).toBool(false);
    const auto monitorCodes = object.value(QStringLiteral("monitor_vcp_codes")).toObject();
    for (auto it = monitorCodes.constBegin(); it != monitorCodes.constEnd(); ++it) {
        monitorVcpCodes_.insert(it.key(), loadVcpCodes(it.value().toObject()));
    }
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

void BrightlessController::saveSettings()
{
    settingsTimer_.stop();
    QJsonObject monitorContrast;
    for (auto it = monitorDynamicContrast_.constBegin(); it != monitorDynamicContrast_.constEnd();
         ++it) {
        monitorContrast.insert(it.key(), it.value());
    }

    QJsonObject monitorRatios;
    for (auto it = monitorRatios_.constBegin(); it != monitorRatios_.constEnd(); ++it) {
        monitorRatios.insert(it.key(), it.value());
    }

    QJsonObject monitorCodes;
    for (auto it = monitorVcpCodes_.constBegin(); it != monitorVcpCodes_.constEnd(); ++it) {
        monitorCodes.insert(it.key(), saveVcpCodes(it.value()));
    }

    QJsonObject object;
    object.insert(QStringLiteral("vcp_codes"), saveVcpCodes(vcpCodes_));
    object.insert(QStringLiteral("vcp_per_monitor"), vcpPerMonitor_);
    object.insert(QStringLiteral("monitor_vcp_codes"), monitorCodes);
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
        setOperationError(tr("Failed to save settings"));
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setOperationError(tr("Failed to save settings: %1").arg(file.errorString()));
        return;
    }
    const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        setOperationError(tr("Failed to save settings: %1").arg(file.errorString()));
    } else if (!file.commit()) {
        setOperationError(tr("Failed to save settings: %1").arg(file.errorString()));
    }
}
