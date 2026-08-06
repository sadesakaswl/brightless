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
    const auto path = QDir(config.path()).filePath(QStringLiteral("autostart/brightless.desktop"));
    if (controller.autostart() || QFileInfo::exists(path)) {
        return 2;
    }

    controller.setAutostart(true);
    QFile file(path);
    if (!controller.autostart() || !file.open(QIODevice::ReadOnly)) {
        return 3;
    }
    const auto entry = file.readAll();
    if (!entry.startsWith("[Desktop Entry]\n")
        || !entry.contains(QCoreApplication::applicationFilePath().toUtf8())) {
        return 4;
    }

    controller.setAutostart(false);
    return controller.autostart() || QFileInfo::exists(path);
}
