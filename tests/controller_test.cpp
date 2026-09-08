#include "brightlesscontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <ddcutil_c_api.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
int monitorIds[]{0, 1};
std::array<std::map<int, int>, 2> reads, writes;
bool reverseMonitors = false;
bool failReads = false;
}

DDCA_Status ddca_get_display_info_list2(bool, DDCA_Display_Info_List **list)
{
    *list = static_cast<DDCA_Display_Info_List *>(
        std::calloc(1, sizeof(DDCA_Display_Info_List) + 2 * sizeof(DDCA_Display_Info)));
    (*list)->ct = 2;
    for (int i = 0; i < 2; ++i) {
        const int index = reverseMonitors ? 1 - i : i;
        auto &info = (*list)->info[i];
        info.dref = &monitorIds[index];
        info.path.io_mode = DDCA_IO_I2C;
        info.path.path.i2c_busno = index + 1;
        std::strcpy(info.model_name, "Same model");
    }
    return 0;
}

void ddca_free_display_info_list(DDCA_Display_Info_List *list)
{
    std::free(list);
}

DDCA_Status ddca_open_display2(DDCA_Display_Ref ref, bool, DDCA_Display_Handle *handle)
{
    *handle = ref;
    return 0;
}

DDCA_Status ddca_close_display(DDCA_Display_Handle)
{
    return 0;
}

DDCA_Status ddca_get_non_table_vcp_value(DDCA_Display_Handle handle,
    DDCA_Vcp_Feature_Code code, DDCA_Non_Table_Vcp_Value *value)
{
    ++reads[*static_cast<int *>(handle)][code];
    *value = {};
    value->ml = 100;
    value->sl = 50;
    return failReads ? -1 : 0;
}

DDCA_Status ddca_set_non_table_vcp_value(DDCA_Display_Handle handle,
    DDCA_Vcp_Feature_Code code, uint8_t high, uint8_t low)
{
    writes[*static_cast<int *>(handle)][code] = (high << 8) | low;
    return 0;
}

int main(int argc, char *argv[])
{
    QTemporaryDir config;
    if (!config.isValid()) {
        return 1;
    }
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
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
    controller.initialize();
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
        restored.initialize();
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
        restored.initialize(); // Flush and wait for the asynchronous writes.
        if (writes[0] != std::map<int, int>{{0xb0, 40}, {0xb1, 30}, {0xb2, 20}, {0xb3, 17}, {0xb4, 1}}
            || writes[1] != std::map<int, int>{{0xc0, 60}, {0xc1, 42}}) {
            return 31;
        }
        restored.set_vcp_per_monitor(false);
        reads = {};
        writes = {};
        restored.initialize();
        restored.adjustAllBrightness(1);
        restored.initialize();
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
        restored.initialize();
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
    return controller.autostart() || QFileInfo::exists(path);
}
