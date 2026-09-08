#include "platform/autostart.h"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace brightless {
namespace {
const QString runKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString appKey = QStringLiteral("Brightless");
}

bool autostartEnabled()
{
    return QSettings(runKey, QSettings::NativeFormat).contains(appKey);
}

QString setAutostartEnabled(bool enabled)
{
    QSettings settings(runKey, QSettings::NativeFormat);
    if (enabled) {
        settings.setValue(appKey, QStringLiteral("\"%1\" --autostart")
            .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        settings.remove(appKey);
    }
    settings.sync();
    return settings.status() == QSettings::NoError ? QString()
        : QCoreApplication::translate("BrightlessController", "Failed to update autostart");
}
}
