#include "platform/autostart.h"
#include "configpath.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString autostartPath()
{
    const auto root = brightless::configRoot();
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

}

namespace brightless {
bool autostartEnabled()
{
    const auto path = autostartPath();
    return !path.isEmpty() && QFileInfo(path).isFile();
}

QString setAutostartEnabled(bool enabled)
{
    const bool ok = enabled ? writeAutostartEntry() : QFile::remove(autostartPath());
    return ok ? QString() : QCoreApplication::translate("BrightlessController", "Failed to update autostart");
}
}
