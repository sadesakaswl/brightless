#include "brightlesscontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

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
    return controller.autostart() || QFileInfo::exists(path);
}
