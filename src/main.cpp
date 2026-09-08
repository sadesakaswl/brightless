#include "brightlesscontroller.h"
#include "desktopintegration.h"
#include "singleinstance.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QScreen>
#include <QTranslator>
#include <QTimer>
#include <QWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Brightless"));
    QApplication::setApplicationName(QStringLiteral("Brightless"));
    QApplication::setApplicationDisplayName(QStringLiteral("Brightless"));
    QApplication::setApplicationVersion(QString::fromLatin1(BRIGHTLESS_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/com/brightless/icon.png")));
    const auto autostartLaunch = app.arguments().contains(QStringLiteral("--autostart"));

    SingleInstance instance;
    const auto instanceStatus = instance.start(autostartLaunch);
    if (instanceStatus != 0) return instanceStatus > 0 ? 0 : 1;

    QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("brightless"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        app.installTranslator(&translator);
    }

    DesktopIntegration desktop;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("desktopIntegration"), &desktop);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/com/brightless/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    auto *controller = window ? window->findChild<BrightlessController *>() : nullptr;
    if (!window || !controller) return 1;

    auto windowSize = controller->savedWindowSize();
    if (!windowSize.isEmpty()) {
        windowSize = windowSize.expandedTo(window->minimumSize());
        if (window->screen()) windowSize = windowSize.boundedTo(window->screen()->availableGeometry().size());
        QQmlProperty::write(window, QStringLiteral("width"), windowSize.width(), &engine);
        QQmlProperty::write(window, QStringLiteral("height"), windowSize.height(), &engine);
    }
    QObject::connect(&app, &QApplication::aboutToQuit, controller, [controller, window] {
        controller->saveWindowSize(window->size());
        controller->flushSettings();
    });
    const auto activate = [window](const QString &token) {
        if (!token.isEmpty()) qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
        window->show();
        if (window->visibility() == QWindow::Minimized) window->showNormal();
        window->raise();
        window->requestActivate();
        if (!token.isEmpty()) qunsetenv("XDG_ACTIVATION_TOKEN");
    };
    QObject::connect(&instance, &SingleInstance::activateRequested, window, activate);
    QObject::connect(&desktop, &DesktopIntegration::activateRequested, window, [activate] { activate({}); });
    desktop.attach(controller);
    QTimer rescan;
    rescan.setSingleShot(true);
    rescan.setInterval(500);
    QObject::connect(&app, &QGuiApplication::screenAdded, &rescan, [&] { rescan.start(); });
    QObject::connect(&app, &QGuiApplication::screenRemoved, &rescan, [&] { rescan.start(); });
    QObject::connect(&rescan, &QTimer::timeout, controller, &BrightlessController::initialize);
    window->setVisible(!autostartLaunch || !controller->autostartAsTrayIcon()
                       || controller->hideTrayIcon() || !desktop.trayAvailable());
    return app.exec();
}
