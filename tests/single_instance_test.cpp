#include "singleinstance.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <array>
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
    const bool parallel = app.arguments().contains(QStringLiteral("--parallel"));
    constexpr int concurrentLaunches = 5;
    const int batchSize = parallel ? concurrentLaunches : 1;
    {
        QStringList activations;
        SingleInstance primary;
        if (primary.start(false) != 0) return 2;
        QObject::connect(&primary, &SingleInstance::activateRequested, &app, [&](const QString &token) {
            activations.append(token);
        });
        for (int first = 0; first < 20; first += batchSize) {
            std::array<QProcess, concurrentLaunches> children;
            QStringList expected;
            for (int i = 0; i < batchSize; ++i) {
                const int attempt = first + i;
                const bool autostart = attempt % 5 == 0;
                const auto token = attempt % 5 == 1 ? QString{} : QStringLiteral("test-\u00E7-%1").arg(attempt);
                auto environment = QProcessEnvironment::systemEnvironment();
                if (token.isEmpty()) environment.remove(QStringLiteral("XDG_ACTIVATION_TOKEN"));
                else environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);
                QStringList arguments{QStringLiteral("--child")};
                if (autostart) arguments.append(QStringLiteral("--autostart"));
                else expected.append(token);
                children[i].setProcessEnvironment(environment);
                children[i].start(QCoreApplication::applicationFilePath(), arguments);
                if (!children[i].waitForStarted()) return 3;
            }
            QElapsedTimer timer;
            timer.start();
            const auto running = [](const QProcess &child) { return child.state() != QProcess::NotRunning; };
            while (std::any_of(children.begin(), children.begin() + batchSize, running)
                   && timer.elapsed() < 10000) {
                app.processEvents();
                QThread::msleep(1);
            }
            for (int i = 0; i < batchSize; ++i) {
                auto &child = children[i];
                if (running(child) || child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0) {
                    qCritical().noquote() << "Secondary launch failed:" << child.readAllStandardError();
                    return 4;
                }
            }
            activations.sort();
            expected.sort();
            if (activations != expected) {
                qCritical() << "Unexpected activation tokens:" << activations << "expected:" << expected;
                return 4;
            }
            activations.clear();
        }
    }
    if (parallel) return 0;

    // A killed process leaves both a lock file and (on Unix) a socket path behind.
    QProcess crashed;
    crashed.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--child"), QStringLiteral("--hold")});
    if (!crashed.waitForReadyRead(5000) || !crashed.readAllStandardOutput().contains("ready")) return 5;
    crashed.kill();
    if (!crashed.waitForFinished(5000)) return 6;
    SingleInstance recovered;
    return recovered.start(false) == 0 ? 0 : 7;
}
