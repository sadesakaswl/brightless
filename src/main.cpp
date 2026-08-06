#include "brightlesscontroller.h"

#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QWindow>

#include <memory>

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

    QDBusInterface brightnessOsd(QStringLiteral("org.kde.plasmashell"),
                                 QStringLiteral("/org/kde/osdService"),
                                 QStringLiteral("org.kde.osdService"),
                                 QDBusConnection::sessionBus());
    std::unique_ptr<KStatusNotifierItem> trayIcon;
    const auto updateTray = [&app, &brightnessOsd, &trayIcon, controller, window, showWindow] {
        const auto enabled = controller->closeToTray();
        if (enabled && !trayIcon) {
            trayIcon = std::make_unique<KStatusNotifierItem>(QStringLiteral("brightless"));
            trayIcon->setCategory(KStatusNotifierItem::Hardware);
            trayIcon->setIconByPixmap(QApplication::windowIcon());
            trayIcon->setToolTipTitle(QStringLiteral("Brightless"));
            trayIcon->setStandardActionsEnabled(false);

            auto *trayMenu = new QMenu;
            auto *showAction = trayMenu->addAction(QStringLiteral("Show Brightless"));
            trayMenu->addSeparator();
            auto *quitAction = trayMenu->addAction(QStringLiteral("Quit"));
            trayIcon->setContextMenu(trayMenu);

            QObject::connect(showAction, &QAction::triggered, window, showWindow);
            QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
            QObject::connect(trayIcon.get(), &KStatusNotifierItem::activateRequested, window,
                             showWindow);
            QObject::connect(trayIcon.get(), &KStatusNotifierItem::scrollRequested, controller,
                             [controller, &brightnessOsd](int delta, Qt::Orientation orientation) {
                                 if (orientation != Qt::Vertical) {
                                     return;
                                 }
                                 const auto percent = controller->adjustAllBrightness(delta);
                                 if (percent >= 0 && brightnessOsd.isValid()) {
                                     brightnessOsd.asyncCall(QStringLiteral("brightnessChanged"),
                                                             percent);
                                 }
                             });
            trayIcon->setStatus(KStatusNotifierItem::Active);
        } else if (!enabled) {
            trayIcon.reset();
        }
        app.setQuitOnLastWindowClosed(!enabled || !QSystemTrayIcon::isSystemTrayAvailable());
    };
    QObject::connect(controller, &BrightlessController::closeToTrayChanged, &app, updateTray);
    updateTray();

    return app.exec();
}
