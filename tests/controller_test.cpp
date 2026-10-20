#include "brightlesscontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "platform/ddcbackend.h"
#include "platform/battery.h"
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

namespace brightless::battery {
void readPercentage(QObject *, std::function<void(std::optional<int>)> completion)
{
    completion(std::nullopt);
}
}

void testTimings(const QString &settingsPath)
{
    const auto check = [](bool condition, const char *message) {
        if (!condition) {
            std::cerr << "Timing check failed: " << message << '\n';
            std::exit(58);
        }
    };
    responses = {};
    reverseMonitors = false;
    BrightlessController controller;
    controller.set_dynamic_contrast_enabled(false);
    controller.set_ddc_delay(0);
    initialize(controller);
    check(controller.timings().isEmpty(), "old settings must have no schedules");
    check(!controller.use12HourTime(), "old settings must default to 24-hour time");
    int formatChanges = 0;
    QObject::connect(&controller, &BrightlessController::timeFormatChanged, [&] { ++formatChanges; });
    controller.setUse12HourTime(true);
    controller.setUse12HourTime(true);
    check(BrightlessController().use12HourTime() && formatChanges == 1,
          "12-hour preference was not saved or notified correctly");
    controller.setUse12HourTime(false);
    check(!BrightlessController().use12HourTime() && formatChanges == 2,
          "24-hour preference was not saved or notified correctly");
    int changes = 0;
    QObject::connect(&controller, &BrightlessController::timingsChanged, [&] { ++changes; });
    controller.add_timing();
    check(!controller.timings()[0].toMap()["enabled"].toBool() && changes == 1,
          "new entries must be disabled and notify the UI");

    const auto time = QTime::currentTime().addSecs(3600);
    const QDateTime now(QDate(2000, 1, 1), QTime(time.hour(), time.minute()));
    QVariantMap entry{{"time", now.time().toString("HH:mm")}, {"monitor_id", "monitor-0"},
                      {"control", 0x10}, {"value", 80}, {"enabled", true}};
    for (const auto &invalid : std::array<std::pair<QString, QVariant>, 12>{{
             {"time", "24:00"}, {"time", "8:30"}, {"time", "12:60"}, {"time", "12:30:00"},
             {"monitor_id", 0}, {"enabled", "true"}, {"control", 0x99}, {"control", 16.5},
             {"value", -1}, {"value", 101}, {"value", 50.5}, {"value", "50"}}}) {
        auto bad = entry;
        bad[invalid.first] = invalid.second;
        check(!controller.update_timing(0, bad), "invalid entry accepted");
    }
    for (const auto &[control, value] : {std::pair{0x60, 2}, std::pair{0xd6, 0}, std::pair{0xd6, 6}}) {
        auto bad = entry;
        bad["control"] = control;
        bad["value"] = value;
        check(!controller.update_timing(0, bad), "invalid input/power value accepted");
    }
    check(!controller.update_timing(-1, entry) && !controller.update_timing(1, entry), "bad index accepted");
    check(controller.update_timing(0, entry), "valid entry rejected");
    const auto originalTimings = controller.timings();
    controller.setUse12HourTime(true);
    check(controller.timings() == originalTimings && BrightlessController().timings() == originalTimings,
          "changing the display format changed a scheduled time");
    controller.add_timing(); // This disabled entry must never run.
    writes = {};
    controller.runTimings(now.addSecs(-60));
    initialize(controller);
    check(writes[0].empty() && writes[1].empty(), "entry ran early or while disabled");
    controller.runTimings(now);
    initialize(controller);
    check(writes[0] == std::map<int, int>{{0x10, 80}} && writes[1].empty(), "wrong monitor/value");
    writes = {};
    controller.runTimings(now.addSecs(30));
    initialize(controller);
    check(writes[0].empty() && writes[1].empty(), "entry ran twice in the same minute");
    reverseMonitors = true;
    initialize(controller);
    controller.runTimings(now.addDays(1));
    initialize(controller);
    check(writes[0] == std::map<int, int>{{0x10, 80}} && writes[1].empty(),
          "entry must repeat daily and follow the device ID after reordering");

    controller.set_dynamic_contrast_enabled(true);
    controller.set_dynamic_contrast_global(false);
    controller.set_dynamic_contrast_per_monitor_ratio(false);
    controller.set_dynamic_contrast_ratio(0.5F);
    controller.set_monitor_dynamic_contrast_enabled(0, true); // monitor-1 after reordering
    controller.set_monitor_dynamic_contrast_enabled(1, false);
    entry["monitor_id"] = "";
    entry["value"] = 60;
    controller.update_timing(0, entry);
    writes = {};
    controller.runTimings(now.addDays(2));
    initialize(controller);
    check(writes[0] == std::map<int, int>{{0x10, 60}}
          && writes[1] == std::map<int, int>{{0x10, 60}, {0x12, 30}},
          "brightness must use each monitor's dynamic-contrast setting");

    entry["enabled"] = false;
    controller.update_timing(0, entry);
    responses[0] = {{0x12, {200, 50}}, {0xe0, {200, 50}}, {0x60, {0, 17}}, {0xd6, {0, 1}}};
    responses[1] = {{0x12, {0, -1}}, {0xe0, {0, -1}}, {0x60, {0, -1}}, {0xd6, {0, -1}}};
    controller.set_vcp_code(0x62, 0xe0);
    initialize(controller);
    for (const auto &[control, value] : {std::pair{0x12, 25}, std::pair{0x62, 75},
                                       std::pair{0xd6, 4}, std::pair{0x60, 18}}) {
        controller.add_timing();
        auto action = entry;
        action["control"] = control;
        action["value"] = value;
        action["enabled"] = true;
        check(controller.update_timing(controller.timings().size() - 1, action), "control entry rejected");
    }
    writes = {};
    controller.runTimings(now.addDays(3));
    initialize(controller);
    check(writes[0] == std::map<int, int>{{0x12, 50}, {0xe0, 150}, {0xd6, 4}, {0x60, 18}}
          && writes[1].empty(), "control support, VCP overrides or percent scaling ignored");

    for (int index = 0; index < controller.timings().size(); ++index) {
        auto action = controller.timings()[index].toMap();
        action["enabled"] = false;
        controller.update_timing(index, action);
    }
    entry["enabled"] = true;
    entry["monitor_id"] = "disconnected-monitor";
    controller.update_timing(0, entry);
    writes = {};
    controller.runTimings(now.addDays(4));
    initialize(controller);
    check(writes[0].empty() && writes[1].empty(), "missing monitor must not fall back to all monitors");
    entry["monitor_id"] = "";
    controller.update_timing(0, entry);
    controller.runTimings(now.addDays(5).addSecs(60));
    initialize(controller);
    check(writes[0].empty() && writes[1].empty(), "missed times must not run late");
    controller.initialize();
    controller.runTimings(now.addDays(6));
    initialize(controller);
    check(writes[0].empty() && writes[1].empty(), "entry ran during discovery");
    controller.runTimings(now.addDays(6));
    initialize(controller);
    check(!writes[0].empty() && !writes[1].empty(), "discovery must not consume the due entry");

    // Moving an already-fired entry to a later time must allow it to run today.
    const auto later = now.addDays(6).addSecs(60);
    entry["time"] = later.time().toString("HH:mm");
    controller.update_timing(0, entry);
    writes = {};
    controller.runTimings(later);
    initialize(controller);
    check(!writes[0].empty() && !writes[1].empty(), "edited time did not run on the same day");

    const auto saved = controller.timings();
    check(BrightlessController().timings() == saved, "entries were not persisted");
    controller.remove_timing(-1);
    controller.remove_timing(saved.size());
    check(controller.timings() == saved, "invalid removal changed entries");
    controller.remove_timing(0);
    check(controller.timings().size() == saved.size() - 1
          && BrightlessController().timings() == controller.timings(), "removal not saved");
    while (!controller.timings().isEmpty()) controller.remove_timing(0);
    check(BrightlessController().timings().isEmpty(), "removing the last entry was not saved");

    QJsonArray malformed{QJsonObject::fromVariantMap(entry), QJsonObject{{"time", "12:00"}},
                         QJsonObject{{"time", "24:00"}, {"monitor_id", ""}, {"enabled", true},
                                     {"control", 0xd6}, {"value", 4}}, "not an entry"};
    QFile file(settingsPath);
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot write malformed fixture");
    file.write(QJsonDocument(QJsonObject{{"timings", malformed}, {"use_12_hour_time", "true"}}).toJson());
    file.close();
    auto migratedEntry = entry;
    migratedEntry.insert("trigger", "time");
    migratedEntry.insert("battery_percent", 20);
    migratedEntry.insert("battery_condition", "below");
    check(BrightlessController().timings() == QVariantList{migratedEntry},
          "legacy entry not migrated or invalid saved entries not discarded");
    check(!BrightlessController().use12HourTime(), "malformed time format preference accepted");
}

void testThemeMode(const QString &settingsPath)
{
    const auto check = [](bool condition, const char *message) {
        if (!condition) {
            std::cerr << "Theme preference check failed: " << message << '\n';
            std::exit(60);
        }
    };
    BrightlessController controller;
    check(controller.themeMode() == "auto", "existing settings must default to auto");
    int changes = 0;
    QObject::connect(&controller, &BrightlessController::themeModeChanged, [&] { ++changes; });
    for (const auto &mode : {"light", "dark", "amoled", "light_green", "dark_green",
                             "light_blue", "dark_blue", "purple", "red", "orange", "grey", "brown", "yellow", "auto"}) {
        controller.setThemeMode(QString::fromLatin1(mode));
        check(controller.themeMode() == mode && BrightlessController().themeMode() == mode,
              "theme mode was not persisted");
        controller.setThemeMode(QString::fromLatin1(mode));
    }
    controller.setThemeMode(QStringLiteral("invalid"));
    check(controller.themeMode() == "auto" && changes == 14, "invalid/redundant change notified");
    for (const auto &invalid : {QJsonValue("invalid"), QJsonValue(2), QJsonValue(true)}) {
        QFile file(settingsPath);
        check(file.open(QIODevice::ReadOnly), "cannot read settings fixture");
        auto settings = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        settings.insert("theme_mode", invalid);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot write settings fixture");
        file.write(QJsonDocument(settings).toJson());
        file.close();
        check(BrightlessController().themeMode() == "auto", "malformed theme mode accepted");
    }
    controller.setThemeMode(QStringLiteral("light"));
    controller.setThemeMode(QStringLiteral("auto"));
}

void testBatteryTimings()
{
    const auto check = [](bool condition, const char *message) {
        if (!condition) {
            std::cerr << "Battery schedule check failed: " << message << '\n';
            std::exit(59);
        }
    };
    responses = {};
    reverseMonitors = false;
    BrightlessController controller;
    while (!controller.timings().isEmpty()) controller.remove_timing(0);
    controller.set_dynamic_contrast_enabled(false);
    controller.set_ddc_delay(0);
    controller.add_timing();
    QVariantMap entry{{"trigger", "battery"}, {"battery_percent", 20},
                      {"battery_condition", "below"}, {"monitor_id", "monitor-0"},
                      {"control", 0x10}, {"value", 35}, {"enabled", true}};
    for (const auto &invalid : std::initializer_list<std::pair<QString, QVariant>>{
             {"trigger", "unknown"}, {"trigger", 1}, {"battery_percent", -1},
             {"battery_percent", 101}, {"battery_percent", 20.5}, {"battery_percent", "20"},
             {"battery_condition", "equal"}, {"battery_condition", 1}}) {
        auto bad = entry;
        bad[invalid.first] = invalid.second;
        check(!controller.update_timing(0, bad), "invalid trigger accepted");
    }
    for (const auto &key : {"battery_percent", "battery_condition"}) {
        auto bad = entry;
        bad.remove(key);
        check(!controller.update_timing(0, bad), "incomplete battery trigger accepted");
    }
    check(controller.update_timing(0, entry), "battery trigger rejected");
    entry = controller.timings()[0].toMap();
    check(BrightlessController().timings() == controller.timings(), "battery trigger not persisted");
    controller.runBatteryTimings(20); // No monitors must not consume this condition.
    initialize(controller);
    const auto run = [&](std::optional<int> percentage) {
        writes = {};
        controller.runBatteryTimings(percentage);
        initialize(controller); // Wait for the async fake DDC worker.
    };
    run(20);
    check(writes[0] == std::map<int, int>{{0x10, 35}} && writes[1].empty(),
          "first matching reading did not run at the inclusive threshold");
    run(19);
    check(writes[0].empty(), "condition repeatedly fired while true");
    run(std::nullopt);
    check(controller.batteryPercent() == -1 && writes[0].empty(), "unknown reading fired");
    run(19);
    check(writes[0].empty(), "unknown reading re-armed the trigger");
    run(101);
    check(controller.batteryPercent() == -1 && writes[0].empty(), "invalid reading accepted");
    run(21);
    check(writes[0].empty(), "nonmatching reading fired");
    run(18);
    check(writes[0] == std::map<int, int>{{0x10, 35}}, "crossing back did not re-arm");
    entry["value"] = 45;
    controller.update_timing(0, entry);
    run(18);
    check(writes[0].empty(), "editing an action re-armed the trigger");
    entry["enabled"] = false;
    controller.update_timing(0, entry);
    run(18);
    check(writes[0].empty(), "disabled trigger fired");
    entry["enabled"] = true;
    controller.update_timing(0, entry);
    controller.initialize();
    controller.runBatteryTimings(18);
    initialize(controller);
    check(writes[0].empty(), "trigger ran during discovery");
    run(18);
    check(writes[0] == std::map<int, int>{{0x10, 45}}, "enabling did not re-arm");
    entry["battery_condition"] = "above";
    entry["battery_percent"] = 80;
    entry["monitor_id"] = "";
    controller.update_timing(0, entry);
    run(79);
    check(writes[0].empty() && writes[1].empty(), "above trigger fired early");
    run(80);
    check(writes[0] == std::map<int, int>{{0x10, 45}} && writes[1] == writes[0],
          "above trigger did not apply to all monitors");
    run(90);
    check(writes[0].empty(), "above trigger repeatedly fired");
    entry["battery_percent"] = 100;
    controller.update_timing(0, entry);
    controller.add_timing();
    auto last = entry;
    last["value"] = 60;
    check(controller.update_timing(1, last), "second battery entry rejected");
    run(100);
    check(writes[0] == std::map<int, int>{{0x10, 60}} && writes[1] == writes[0],
          "threshold edit did not re-arm or list order was ignored");
    entry["trigger"] = "time";
    entry["time"] = "12:34";
    check(controller.update_timing(0, entry), "switch to time rejected");
    run(0);
    check(writes[0].empty(), "battery evaluation ran a time entry");
    writes = {};
    controller.runTimings(QDateTime(QDate(2000, 1, 1), QTime(12, 34)));
    initialize(controller);
    check(writes[0] == std::map<int, int>{{0x10, 45}}, "switched time trigger did not run");
    entry["trigger"] = "battery";
    entry["battery_condition"] = "below";
    entry["battery_percent"] = 0;
    check(controller.update_timing(0, entry), "zero threshold rejected");
    run(0);
    check(writes[0] == std::map<int, int>{{0x10, 45}}, "zero threshold did not run");
    check(BrightlessController().timings() == controller.timings(), "edited triggers not persisted");
    controller.set_dynamic_contrast_enabled(true);
    controller.set_dynamic_contrast_global(true);
    controller.set_dynamic_contrast_per_monitor_ratio(false);
    controller.set_dynamic_contrast_ratio(0.5F);
    run(1);
    run(0);
    check(writes[0] == std::map<int, int>{{0x10, 45}, {0x12, 23}} && writes[1] == writes[0],
          "battery brightness did not use dynamic contrast");
    controller.flushSettings();
    {
        BrightlessController restored;
        initialize(restored);
        writes = {};
        restored.runBatteryTimings(0);
        initialize(restored);
        check(writes[0] == std::map<int, int>{{0x10, 45}, {0x12, 23}},
              "matching condition did not fire after restart");
    }
    while (!controller.timings().isEmpty()) controller.remove_timing(0);
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

    int trayClickChanges = 0;
    QObject::connect(&controller, &BrightlessController::hideOnTrayClickChanged,
                     &app, [&] { ++trayClickChanges; });
    if (!controller.hideOnTrayClick()) {
        std::cerr << "Hide on tray click should default to enabled\n";
        return 1;
    }
    controller.setHideOnTrayClick(true);
    controller.setHideOnTrayClick(false);
    controller.setHideOnTrayClick(false);
    if (BrightlessController restored; restored.hideOnTrayClick() || trayClickChanges != 1) {
        std::cerr << "Disabling hide on tray click was not saved or notified correctly\n";
        return 1;
    }
    controller.setHideOnTrayClick(true);
    if (BrightlessController restored; !restored.hideOnTrayClick() || trayClickChanges != 2) {
        std::cerr << "Hide on tray click was not saved or notified correctly\n";
        return 1;
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

    file.close();
    updatedFile.close(); // Windows cannot remove files while these readers are open.
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
    testTimings(settings.fileName());
    testThemeMode(settings.fileName());
    testBatteryTimings();
    return controller.autostart() || QFileInfo::exists(path) ? 57 : 0;
}

int main(int argc, char *argv[])
{
    const auto result = runTests(argc, argv);
    if (result) std::cerr << "Controller check failed: " << result << '\n';
    return result;
}
