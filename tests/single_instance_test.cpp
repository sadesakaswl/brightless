#include "singleinstance.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--child"))) {
        SingleInstance instance;
        const auto status = instance.start(app.arguments().contains(QStringLiteral("--autostart")));
        if (app.arguments().contains(QStringLiteral("--hold")) && status == 0) {
            std::puts("ready");
            std::fflush(stdout);
            return app.exec();
        }
        return status == 1 ? 0 : 1;
    }

    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("BRIGHTLESS_TEST_CONFIG", config.path().toUtf8());
    qputenv("XDG_ACTIVATION_TOKEN", "test-token");
    {
        SingleInstance primary;
        if (primary.start(false) != 0) return 2;
        int activations = 0;
        QObject::connect(&primary, &SingleInstance::activateRequested, &app, [&](const QString &token) {
            if (token == QStringLiteral("test-token")) ++activations;
        });
        int expectedActivations = 0;
        for (int attempt = 0; attempt < 20; ++attempt) {
            const bool autostart = attempt % 5 == 0;
            QProcess child;
            QStringList arguments{QStringLiteral("--child")};
            if (autostart) arguments.append(QStringLiteral("--autostart"));
            else ++expectedActivations;
            child.start(QCoreApplication::applicationFilePath(), arguments);
            if (!child.waitForStarted()) return 3;
            QElapsedTimer timer;
            timer.start();
            while (child.state() != QProcess::NotRunning && timer.elapsed() < 10000) {
                app.processEvents();
                QThread::msleep(1);
            }
            if (child.state() != QProcess::NotRunning || child.exitCode() != 0
                || activations != expectedActivations) return 4;
        }
    }
    // A killed process leaves both a lock file and (on Unix) a socket path behind.
    QProcess crashed;
    crashed.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--child"), QStringLiteral("--hold")});
    if (!crashed.waitForReadyRead(5000) || !crashed.readAllStandardOutput().contains("ready")) return 5;
    crashed.kill();
    if (!crashed.waitForFinished(5000)) return 6;
    SingleInstance recovered;
    return recovered.start(false) == 0 ? 0 : 7;
}
