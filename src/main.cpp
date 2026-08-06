#include "brightlesscontroller.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Brightless"));
    QApplication::setApplicationDisplayName(QStringLiteral("Brightless"));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/com/brightless/icon.png")));

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/com/brightless/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    auto *controller = window ? window->findChild<BrightlessController *>() : nullptr;
    if (!window || !controller) {
        return -1;
    }

    auto windowSize = controller->savedWindowSize();
    if (!windowSize.isEmpty()) {
        windowSize = windowSize.expandedTo(window->minimumSize());
        if (window->screen()) {
            windowSize = windowSize.boundedTo(window->screen()->availableGeometry().size());
        }
        QQmlProperty::write(window, QStringLiteral("width"), windowSize.width(), &engine);
        QQmlProperty::write(window, QStringLiteral("height"), windowSize.height(), &engine);
    }
    QObject::connect(&app, &QApplication::aboutToQuit, controller, [controller, window] {
        controller->saveWindowSize(window->size());
    });

    const auto showWindow = [window] {
        window->show();
        window->raise();
        window->requestActivate();
    };

    QMenu trayMenu;
    auto *showAction = trayMenu.addAction(QStringLiteral("Show Brightless"));
    trayMenu.addSeparator();
    auto *quitAction = trayMenu.addAction(QStringLiteral("Quit"));

    QSystemTrayIcon trayIcon(QApplication::windowIcon());
    trayIcon.setToolTip(QStringLiteral("Brightless"));
    trayIcon.setContextMenu(&trayMenu);

    const auto updateTray = [&app, &trayIcon, controller] {
        const auto enabled = controller->closeToTray();
        trayIcon.setVisible(enabled);
        app.setQuitOnLastWindowClosed(!enabled || !QSystemTrayIcon::isSystemTrayAvailable());
    };
    updateTray();

    QObject::connect(controller, &BrightlessController::closeToTrayChanged, &trayIcon, updateTray);
    QObject::connect(showAction, &QAction::triggered, window, showWindow);
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, window,
                     [showWindow](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger
                             || reason == QSystemTrayIcon::DoubleClick) {
                             showWindow();
                         }
                     });

    return app.exec();
}
