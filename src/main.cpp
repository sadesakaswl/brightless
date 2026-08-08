#include "brightlesscontroller.h"

#include <KGlobalAccel>
#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QIcon>
#include <QKeySequence>
#include <QLocale>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QScreen>
#include <QTranslator>
#include <QUrl>
#include <QWindow>

#include <memory>
#include <vector>

namespace {
class ApplicationActivation final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.brightless.Application")

public:
    void setWindow(QWindow *window) { window_ = window; }
    void activate() { activate({}); }

public slots:
    void activate(const QString &token)
    {
        if (window_) {
            if (!token.isEmpty()) {
                qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
            }
            window_->show();
            window_->raise();
            window_->requestActivate();
            if (!token.isEmpty()) {
                qunsetenv("XDG_ACTIVATION_TOKEN");
            }
        }
    }

private:
    QWindow *window_ = nullptr;
};
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Brightless"));
    QApplication::setApplicationDisplayName(QStringLiteral("Brightless"));
    QApplication::setApplicationVersion(QString::fromLatin1(BRIGHTLESS_VERSION));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/com/brightless/icon.png")));
    const auto autostartLaunch = app.arguments().contains(QStringLiteral("--autostart"));

    const auto serviceName = QStringLiteral("com.brightless.Application");
    const auto objectPath = QStringLiteral("/com/brightless/Application");
    auto sessionBus = QDBusConnection::sessionBus();
    ApplicationActivation activation;
    if (sessionBus.isConnected()
        && sessionBus.registerObject(objectPath, &activation, QDBusConnection::ExportAllSlots)
        && !sessionBus.registerService(serviceName)) {
        if (autostartLaunch) {
            return 0;
        }
        QDBusInterface runningApplication(serviceName, objectPath, serviceName, sessionBus);
        const auto reply = runningApplication.call(QStringLiteral("activate"),
                                                   qEnvironmentVariable("XDG_ACTIVATION_TOKEN"));
        return reply.type() == QDBusMessage::ErrorMessage ? 1 : 0;
    }

    QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("brightless"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        app.installTranslator(&translator);
    }

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

    activation.setWindow(window);
    window->setVisible(!autostartLaunch || !controller->autostartAsTrayIcon()
                       || controller->hideTrayIcon());
    const auto showWindow = [&activation] { activation.activate(); };

    QDBusInterface brightnessOsd(QStringLiteral("org.kde.plasmashell"),
                                 QStringLiteral("/org/kde/osdService"),
                                 QStringLiteral("org.kde.osdService"),
                                 QDBusConnection::sessionBus());
    const auto adjustBrightness = [controller, &brightnessOsd](int direction) {
        const auto percent = controller->adjustAllBrightness(direction);
        if (percent >= 0 && brightnessOsd.isValid()) {
            brightnessOsd.asyncCall(QStringLiteral("brightnessChanged"), percent);
        }
    };

    std::vector<std::unique_ptr<QAction>> globalShortcutActions;
    const auto addGlobalShortcut = [&](const QString &id, const QString &text, auto callback) {
        auto action = std::make_unique<QAction>(text);
        action->setObjectName(id);
        QObject::connect(action.get(), &QAction::triggered, controller, callback);
        KGlobalAccel::setGlobalShortcut(action.get(), QList<QKeySequence>{});
        globalShortcutActions.push_back(std::move(action));
    };
    const auto updateGlobalShortcuts = [&] {
        if (!controller->plasmaGlobalShortcuts()) {
            for (const auto &action : globalShortcutActions) {
                KGlobalAccel::self()->removeAllShortcuts(action.get());
            }
            globalShortcutActions.clear();
            return;
        }
        if (!globalShortcutActions.empty()) {
            return;
        }

        addGlobalShortcut(
            QStringLiteral("increase_brightness_dynamic_contrast"),
            QCoreApplication::translate("GlobalShortcuts",
                                        "Increase brightness/dynamic contrast"),
            [adjustBrightness] { adjustBrightness(1); });
        addGlobalShortcut(
            QStringLiteral("decrease_brightness_dynamic_contrast"),
            QCoreApplication::translate("GlobalShortcuts",
                                        "Decrease brightness/dynamic contrast"),
            [adjustBrightness] { adjustBrightness(-1); });
        addGlobalShortcut(QStringLiteral("increase_contrast"),
                          QCoreApplication::translate("GlobalShortcuts", "Increase contrast"),
                          [controller] { controller->adjustAllContrast(1); });
        addGlobalShortcut(QStringLiteral("decrease_contrast"),
                          QCoreApplication::translate("GlobalShortcuts", "Decrease contrast"),
                          [controller] { controller->adjustAllContrast(-1); });
        addGlobalShortcut(QStringLiteral("increase_volume"),
                          QCoreApplication::translate("GlobalShortcuts", "Increase volume"),
                          [controller] { controller->adjustAllVolume(1); });
        addGlobalShortcut(QStringLiteral("decrease_volume"),
                          QCoreApplication::translate("GlobalShortcuts", "Decrease volume"),
                          [controller] { controller->adjustAllVolume(-1); });
        addGlobalShortcut(QStringLiteral("change_input_device"),
                          QCoreApplication::translate("GlobalShortcuts", "Change input device"),
                          [controller] { controller->changeAllInputSources(); });
    };
    QObject::connect(controller, &BrightlessController::plasmaGlobalShortcutsChanged, &app,
                     updateGlobalShortcuts);
    updateGlobalShortcuts();

    std::unique_ptr<KStatusNotifierItem> trayIcon;
    const auto updateTray = [&app, &trayIcon, controller, window, showWindow, adjustBrightness] {
        const auto visible = !controller->hideTrayIcon();
        if (visible && !trayIcon) {
            trayIcon = std::make_unique<KStatusNotifierItem>(QStringLiteral("brightless"));
            trayIcon->setCategory(KStatusNotifierItem::Hardware);
            trayIcon->setIconByPixmap(QApplication::windowIcon());
            trayIcon->setToolTipTitle(QStringLiteral("Brightless"));
            trayIcon->setStandardActionsEnabled(false);

            auto *trayMenu = new QMenu;
            auto *showAction = trayMenu->addAction(
                QCoreApplication::translate("Tray", "Show Brightless"));
            trayMenu->addSeparator();
            auto *quitAction = trayMenu->addAction(QCoreApplication::translate("Tray", "Quit"));
            trayIcon->setContextMenu(trayMenu);

            QObject::connect(showAction, &QAction::triggered, window, showWindow);
            QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
            QObject::connect(trayIcon.get(), &KStatusNotifierItem::activateRequested, window,
                             showWindow);
            QObject::connect(trayIcon.get(), &KStatusNotifierItem::scrollRequested, controller,
                             [adjustBrightness](int delta, Qt::Orientation orientation) {
                                 if (orientation == Qt::Vertical) {
                                     adjustBrightness(delta);
                                 }
                             });
            trayIcon->setStatus(KStatusNotifierItem::Active);
        } else if (!visible) {
            trayIcon.reset();
        }
        app.setQuitOnLastWindowClosed(!controller->closeToTray());
    };
    QObject::connect(controller, &BrightlessController::closeToTrayChanged, &app, updateTray);
    QObject::connect(controller, &BrightlessController::visibilitySettingsChanged, &app, updateTray);
    updateTray();

    return app.exec();
}

#include "main.moc"
