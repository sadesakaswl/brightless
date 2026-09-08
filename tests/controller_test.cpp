#include "brightlesscontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "platform/ddcbackend.h"
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
int monitorIds[]{0, 1};
std::array<std::map<int, int>, 2> reads, writes;
// Optional maximum/current responses; a negative current simulates an unsupported VCP.
std::array<std::map<int, std::pair<int, int>>, 2> responses;
bool reverseMonitors = false;
bool failReads = false;
bool failWrites = false;
int scanDelay = 0;

void initialize(BrightlessController &controller)
{
    controller.initialize();
    QElapsedTimer timer;
    timer.start();
    while (controller.loading() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    if (controller.loading()) {
        std::cerr << "Discovery timed out\n";
        std::exit(99);
    }
}
}

namespace brightless::ddc {
std::vector<Device> enumerate(QString &)
{
    QThread::msleep(scanDelay);
    std::vector<Device> devices;
    for (int i = 0; i < 2; ++i) {
        const int index = reverseMonitors ? 1 - i : i;
        devices.push_back({QStringLiteral("monitor-%1").arg(index), QStringLiteral("Same model"),
                           {&monitorIds[index], [](void *) {}}});
    }
    return devices;
}

Values read(const Device &device, const Codes &codes)
{
    const auto index = *static_cast<int *>(device.native.get());
    Values values;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        ++reads[index][codes[i]];
        const auto [maximum, current] = responses[index].contains(codes[i])
            ? responses[index].at(codes[i]) : std::pair{100, 50};
        if (!failReads && current >= 0) {
            values[i] = Value{static_cast<std::uint16_t>(current), static_cast<std::uint16_t>(maximum)};
        }
    }
    return values;
}

QString write(const Device &device, const Writes &values)
{
    if (failWrites) return QStringLiteral("Simulated write failure");
    for (const auto &[code, value] : values) {
        writes[*static_cast<int *>(device.native.get())][code] = value;
    }
    return {};
}
}

int runTests(int argc, char *argv[])
{
    QTemporaryDir config;
    if (!config.isValid()) {
        return 1;
    }
    qputenv("BRIGHTLESS_TEST_CONFIG", config.path().toUtf8());
    QCoreApplication app(argc, argv);

    BrightlessController controller;
    if (controller.ddc_delay() != 0) {
        return 2;
    }
    controller.set_ddc_delay(1600);
    if (controller.ddc_delay() != 1500) {
        return 3;
    }
    controller.set_ddc_delay(-1);
    if (controller.ddc_delay() != 0) {
        return 4;
    }
    controller.set_ddc_delay(750);
    controller.flushSettings();
    if (BrightlessController restored; restored.ddc_delay() != 750) {
        return 5;
    }

    for (const int defaultCode : {0x10, 0x12, 0x62, 0x60, 0xd6}) {
        if (controller.vcp_code(defaultCode) != defaultCode
            || !controller.set_vcp_code(defaultCode, 0xe0)
            || controller.vcp_code(defaultCode) != 0xe0) {
            return 19;
        }
        if (BrightlessController restored; restored.vcp_code(defaultCode) != 0xe0) {
            return 20;
        }
        if (controller.set_vcp_code(defaultCode, -1)
            || controller.set_vcp_code(defaultCode, 256)
            || controller.set_vcp_code(defaultCode, 0xe0)
            || controller.vcp_code(defaultCode) != 0xe0) {
            return 21;
        }
        if (!controller.set_vcp_code(defaultCode, 0)
            || !controller.set_vcp_code(defaultCode, 255)
            || !controller.set_vcp_code(defaultCode, defaultCode)) {
            return 22;
        }
        if (BrightlessController restored; restored.vcp_code(defaultCode) != defaultCode) {
            return 23;
        }
    }
    if (controller.set_vcp_code(0x99, 0xe0)) {
        return 24;
    }

    if (controller.hideBrightness() || controller.hideContrast() || controller.hideVolume()
        || controller.hideInput() || controller.hideTrayIcon()) {
        return 9;
    }
    controller.setHideBrightness(true);
    controller.setHideContrast(true);
    controller.setHideVolume(true);
    controller.setHideInput(true);
    controller.setHideTrayIcon(true);
    if (!controller.hideBrightness() || !controller.hideContrast() || !controller.hideVolume()
        || !controller.hideInput() || !controller.hideTrayIcon() || !controller.closeToTray()) {
        return 10;
    }
    controller.setCloseToTray(false);
    controller.setHideTrayIcon(false);
    if (controller.closeToTray() || controller.hideTrayIcon()) {
        return 11;
    }
    controller.setHideTrayIcon(true);
    if (BrightlessController restored;
        !restored.hideBrightness() || !restored.hideContrast() || !restored.hideVolume()
        || !restored.hideInput() || !restored.hideTrayIcon() || restored.closeToTray()) {
        return 12;
    }

    if (controller.autostartAsTrayIcon()) {
        return 13;
    }
    if (controller.plasmaGlobalShortcuts()) {
        return 16;
    }
    controller.setPlasmaGlobalShortcuts(true);
    if (BrightlessController restored; !restored.plasmaGlobalShortcuts()) {
        return 17;
    }
    controller.setPlasmaGlobalShortcuts(false);
    if (BrightlessController restored; restored.plasmaGlobalShortcuts()) {
        return 18;
    }
    controller.setAutostartAsTrayIcon(true);
    if (BrightlessController restored; !restored.autostartAsTrayIcon()) {
        return 14;
    }

    const auto path = QDir(config.path()).filePath(QStringLiteral("autostart/brightless.desktop"));
    if (controller.autostart() || QFileInfo::exists(path)) {
        return 6;
    }

    controller.setAutostart(true);
    QFile file(path);
    if (!controller.autostart() || !file.open(QIODevice::ReadOnly)) {
        return 7;
    }
    const auto entry = file.readAll();
    if (!entry.startsWith("[Desktop Entry]\n")
        || !entry.contains(QCoreApplication::applicationFilePath().toUtf8())
        || !entry.contains(" --autostart\n")) {
        return 8;
    }

    controller.setAutostartAsTrayIcon(false);
    QFile updatedFile(path);
    if (!updatedFile.open(QIODevice::ReadOnly) || updatedFile.readAll() != entry) {
        return 15;
    }

    controller.setAutostart(false);

    if (controller.vcp_per_monitor()) {
        return 27;
    }
    initialize(controller);
    const auto displays = controller.ddcMonitors();
    const auto first = displays[0].toMap().value("id").toString();
    const auto second = displays[1].toMap().value("id").toString();
    if (first == second || controller.monitorCount() != 2) {
        return 28;
    }
    const std::array defaults{0x10, 0x12, 0x62, 0x60, 0xd6};
    for (int i = 0; i < 5; ++i) {
        controller.set_vcp_code(defaults[i], 0xa0 + i);
        controller.set_vcp_code(defaults[i], 0xb0 + i, first);
        controller.set_vcp_code(defaults[i], 0xc0 + i, second);
    }
    controller.set_vcp_per_monitor(true);
    {
        BrightlessController restored;
        if (!restored.vcp_per_monitor()) {
            return 29;
        }
        reverseMonitors = true;
        reads = {};
        initialize(restored);
        for (int i = 0; i < 5; ++i) {
            if (restored.vcp_code(defaults[i], first) != 0xb0 + i
                || restored.vcp_code(defaults[i], second) != 0xc0 + i
                || restored.vcp_code(defaults[i]) != 0xa0 + i
                || !reads[0].contains(0xb0 + i) || !reads[1].contains(0xc0 + i)) {
                return 30;
            }
        }
        restored.set_brightness(1, 40);
        restored.set_contrast(1, 30);
        restored.set_volume(1, 20);
        restored.set_input_source(1, 17);
        restored.set_power_mode(1, 1);
        restored.set_dynamic_contrast_brightness(0, 60);
        initialize(restored); // Flush and wait for the asynchronous writes.
        if (writes[0] != std::map<int, int>{{0xb0, 40}, {0xb1, 30}, {0xb2, 20}, {0xb3, 17}, {0xb4, 1}}
            || writes[1] != std::map<int, int>{{0xc0, 60}, {0xc1, 42}}) {
            return 31;
        }
        restored.set_vcp_per_monitor(false);
        reads = {};
        writes = {};
        initialize(restored);
        restored.adjustAllBrightness(1);
        initialize(restored);
        if (writes[0] != std::map<int, int>{{0xa0, 52}}
            || writes[1] != std::map<int, int>{{0xa0, 52}}) {
            return 32;
        }
        for (int i = 0; i < 5; ++i) {
            if (!reads[0].contains(0xa0 + i) || !reads[1].contains(0xa0 + i)) {
                return 33;
            }
        }
        restored.set_vcp_code(0x10, 0x10, first);
        if (restored.vcp_code(0x10, first) != 0x10 || restored.vcp_code(0x10, second) != 0xc0
            || restored.set_vcp_code(0x10, 256, second)) {
            return 34;
        }
        failReads = true;
        initialize(restored);
        if (restored.monitorCount() != 0 || restored.ddcMonitors().size() != 2) {
            return 35; // Failed VCP reads must not prevent fixing the monitor's codes.
        }
        failReads = false;
    }
    if (BrightlessController restored; restored.vcp_per_monitor()
        || restored.vcp_code(0x10, first) != 0x10 || restored.vcp_code(0x10, second) != 0xc0) {
        return 36;
    }

    QFile settings(QDir(config.path()).filePath(QStringLiteral("brightless/settings.json")));
    if (!settings.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return 25;
    }
    settings.write(R"({"vcp_codes":{"10":-1,"12":256,"62":"e0","60":1.5,"d6":224,"99":225},)"
                   R"("vcp_per_monitor":"true","monitor_vcp_codes":{"test":{"10":-1,"12":256,"62":"e0","60":1.5,"d6":225,"99":226}}})");
    settings.close();
    BrightlessController restored;
    if (restored.vcp_code(0x10) != 0x10 || restored.vcp_code(0x12) != 0x12
        || restored.vcp_code(0x62) != 0x62 || restored.vcp_code(0x60) != 0x60
        || restored.vcp_code(0xd6) != 0xe0 || restored.vcp_code(0x99) != 0x99
        || restored.vcp_per_monitor() || restored.vcp_code(0x10, "test") != 0x10
        || restored.vcp_code(0x12, "test") != 0x12 || restored.vcp_code(0x62, "test") != 0x62
        || restored.vcp_code(0x60, "test") != 0x60 || restored.vcp_code(0xd6, "test") != 0xe1
        || restored.vcp_code(0x99, "test") != 0x99) {
        return 26;
    }
    reverseMonitors = false;
    restored.set_scroll_step(2);
    responses[0] = {{0x62, {100, 40}}, {0x60, {0, 27}}};
    responses[1] = {{0x62, {200, 120}}, {0x60, {0, 17}}};
    initialize(restored);
    writes = {};
    if (restored.adjustAllVolume(1) != 52 || restored.volume(0) != 42
        || restored.volume(1) != 62 || restored.adjustAllVolume(-1) != 50) {
        return 37;
    }
    restored.adjustAllContrast(1);
    restored.changeAllInputSources();
    initialize(restored);
    if (writes[0] != std::map<int, int>{{0x62, 40}, {0x12, 52}, {0x60, 1}}
        || writes[1] != std::map<int, int>{{0x62, 120}, {0x12, 52}, {0x60, 18}}) {
        return 38;
    }
    writes = {};
    restored.adjustAllContrast(-1);
    initialize(restored);
    if (writes[0] != std::map<int, int>{{0x12, 48}}
        || writes[1] != std::map<int, int>{{0x12, 48}}) {
        return 39;
    }

    // Only supported monitors contribute to the volume OSD; clamp at both ends.
    responses[0] = {{0x62, {100, 99}}};
    responses[1] = {{0x62, {0, -1}}, {0x12, {0, -1}}, {0x60, {0, -1}}};
    initialize(restored);
    writes = {};
    if (restored.adjustAllVolume(1) != 100 || restored.adjustAllVolume(1) != 100
        || restored.adjustAllVolume(0) != 100) {
        return 40;
    }
    restored.adjustAllContrast(1);
    restored.changeAllInputSources();
    initialize(restored);
    if (writes[0] != std::map<int, int>{{0x62, 100}, {0x12, 52}} || !writes[1].empty()) {
        return 41;
    }
    responses[0][0x62] = {100, 1};
    initialize(restored);
    if (restored.adjustAllVolume(-1) != 0 || restored.adjustAllVolume(-1) != 0) {
        return 42;
    }
    initialize(restored);
    responses[0][0x62] = {0, -1};
    initialize(restored);
    writes = {};
    if (restored.adjustAllVolume(1) != -1) {
        return 43;
    }
    initialize(restored);
    if (!writes[0].empty() || !writes[1].empty()) {
        return 44;
    }
    failReads = true;
    initialize(restored);
    if (restored.adjustAllVolume(1) != -1 || restored.adjustAllBrightness(1) != -1) {
        return 45;
    }
    failReads = false;
    responses = {};
    scanDelay = 30;
    bool responsive = false;
    QTimer::singleShot(0, &app, [&] { responsive = restored.loading(); });
    initialize(restored);
    scanDelay = 0;
    if (!responsive) return 46; // Discovery must leave the GUI event loop responsive.

    restored.set_dynamic_contrast_enabled(true);
    responses[1][0x12] = {0, -1};
    initialize(restored);
    if (!restored.monitor_dynamic_contrast_enabled(0) || restored.monitor_dynamic_contrast_enabled(1)) return 47;
    writes = {};
    restored.adjustAllBrightness(1);
    initialize(restored);
    if (writes[0] != std::map<int, int>{{0x10, 52}, {0x12, 36}}
        || writes[1] != std::map<int, int>{{0x10, 52}}) return 48;

    failWrites = true;
    restored.adjustAllVolume(1);
    initialize(restored);
    if (!restored.operationError().contains(QStringLiteral("Simulated write failure"))) return 49;
    failWrites = false;

    restored.set_dynamic_contrast_global(false);
    restored.set_dynamic_contrast_per_monitor_ratio(true);
    restored.set_monitor_ratio(0, 0.5F);
    restored.set_monitor_ratio(1, 1.5F);
    restored.set_monitor_dynamic_contrast_enabled(0, false);
    restored.set_monitor_dynamic_contrast_enabled(1, true);
    responses = {};
    reverseMonitors = true;
    initialize(restored);
    if (restored.monitor_ratio(0) != 1.5F || restored.monitor_ratio(1) != 0.5F
        || !restored.monitor_dynamic_contrast_enabled(0) || restored.monitor_dynamic_contrast_enabled(1)) return 50;

    restored.set_scroll_step(7);
    if (BrightlessController before; before.scroll_step() != 2) return 51;
    restored.flushSettings();
    if (BrightlessController after; after.scroll_step() != 7) return 52;
    {
        BrightlessController deferred;
        deferred.set_scroll_step(9);
    }
    if (BrightlessController after; after.scroll_step() != 9) return 53;

    if (!settings.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 54;
    settings.write(R"({"dynamic_contrast_enabled":true,"dynamic_contrast_global":false,)"
                   R"("dynamic_contrast_per_monitor_ratio":true,"monitor_ratios":{"Same model":1.2},)"
                   R"("monitor_dynamic_contrast":{"Same model":false}})");
    settings.close();
    {
        BrightlessController legacy;
        initialize(legacy);
        if (!qFuzzyCompare(legacy.monitor_ratio(0), 1.2F) || legacy.monitor_dynamic_contrast_enabled(0)) return 55;
        legacy.set_monitor_ratio(0, 0.8F);
        legacy.set_monitor_dynamic_contrast_enabled(0, true);
        legacy.flushSettings();
    }
    reverseMonitors = false;
    {
        BrightlessController migrated;
        initialize(migrated);
        if (!qFuzzyCompare(migrated.monitor_ratio(0), 1.2F) || migrated.monitor_dynamic_contrast_enabled(0)
            || !qFuzzyCompare(migrated.monitor_ratio(1), 0.8F) || !migrated.monitor_dynamic_contrast_enabled(1)) return 56;
    }
    return controller.autostart() || QFileInfo::exists(path);
}

int main(int argc, char *argv[])
{
    const auto result = runTests(argc, argv);
    if (result) std::cerr << "Controller check failed: " << result << '\n';
    return result;
}
