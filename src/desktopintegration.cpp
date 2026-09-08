#include "desktopintegration.h"
#include "brightlesscontroller.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <vector>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QSettings>
#include <qt_windows.h>
#else
#include <KGlobalAccel>
#include <KStatusNotifierItem>
#include <QDBusConnection>
#include <QDBusInterface>
#endif

struct DesktopIntegration::Impl
#ifdef Q_OS_WIN
    : QAbstractNativeEventFilter
#endif
{
    DesktopIntegration *owner;
    BrightlessController *controller = nullptr;
    std::vector<std::unique_ptr<QAction>> actions;
    std::unique_ptr<QMenu> menu;
#ifdef Q_OS_WIN
    std::unique_ptr<QSystemTrayIcon> tray;
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override
    {
        const auto *msg = static_cast<MSG *>(message);
        if (msg->message != WM_HOTKEY || msg->wParam < 1 || msg->wParam > actions.size()) {
            return false;
        }
        actions[msg->wParam - 1]->trigger();
        *result = 0;
        return true;
    }

    void updateShortcuts()
    {
        QSettings settings;
        QStringList errors;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            UnregisterHotKey(nullptr, int(i + 1));
            const auto key = QKeySequence::fromString(settings.value(
                QStringLiteral("shortcuts/") + actions[i]->objectName()).toString(), QKeySequence::PortableText);
            if (key.isEmpty()) {
                continue;
            }
            const auto combination = key[0];
            const auto modifiers = combination.keyboardModifiers();
            UINT nativeModifiers = 0;
            if (modifiers.testFlag(Qt::ControlModifier)) nativeModifiers |= MOD_CONTROL;
            if (modifiers.testFlag(Qt::AltModifier)) nativeModifiers |= MOD_ALT;
            if (modifiers.testFlag(Qt::ShiftModifier)) nativeModifiers |= MOD_SHIFT;
            if (modifiers.testFlag(Qt::MetaModifier)) nativeModifiers |= MOD_WIN;
            const auto qtKey = combination.key();
            UINT vk = 0;
            if ((qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) || (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)) {
                vk = UINT(qtKey);
            } else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
                vk = VK_F1 + qtKey - Qt::Key_F1;
            } else {
                switch (qtKey) {
                case Qt::Key_Up: vk = VK_UP; break;
                case Qt::Key_Down: vk = VK_DOWN; break;
                case Qt::Key_Left: vk = VK_LEFT; break;
                case Qt::Key_Right: vk = VK_RIGHT; break;
                case Qt::Key_PageUp: vk = VK_PRIOR; break;
                case Qt::Key_PageDown: vk = VK_NEXT; break;
                case Qt::Key_Home: vk = VK_HOME; break;
                case Qt::Key_End: vk = VK_END; break;
                case Qt::Key_Insert: vk = VK_INSERT; break;
                case Qt::Key_Delete: vk = VK_DELETE; break;
                case Qt::Key_Space: vk = VK_SPACE; break;
                case Qt::Key_VolumeUp: vk = VK_VOLUME_UP; break;
                case Qt::Key_VolumeDown: vk = VK_VOLUME_DOWN; break;
                default:
                    if (qtKey > 0 && qtKey < 0x10000) {
                        const auto translated = VkKeyScanW(wchar_t(qtKey));
                        if (translated != -1) {
                            vk = LOBYTE(translated);
                            if (HIBYTE(translated) & 1) nativeModifiers |= MOD_SHIFT;
                            if (HIBYTE(translated) & 2) nativeModifiers |= MOD_CONTROL;
                            if (HIBYTE(translated) & 4) nativeModifiers |= MOD_ALT;
                        }
                    }
                }
            }
            if (key.count() != 1 || !vk || modifiers.testFlag(Qt::KeypadModifier)
                || !RegisterHotKey(nullptr, int(i + 1), nativeModifiers, vk)) {
                errors.append(actions[i]->text());
            }
        }
        controller->setOperationError(errors.isEmpty() ? QString()
            : DesktopIntegration::tr("Unavailable shortcuts: %1").arg(errors.join(QStringLiteral(", "))));
    }
#else
    std::unique_ptr<KStatusNotifierItem> tray;
    bool shortcutsRegistered = false;
    QDBusInterface osd{QStringLiteral("org.kde.plasmashell"), QStringLiteral("/org/kde/osdService"),
                       QStringLiteral("org.kde.osdService"), QDBusConnection::sessionBus()};
    void updateShortcuts()
    {
        const bool enabled = controller->plasmaGlobalShortcuts();
        if (enabled == shortcutsRegistered) return;
        for (const auto &action : actions) {
            if (controller->plasmaGlobalShortcuts()) {
                KGlobalAccel::setGlobalShortcut(action.get(), QList<QKeySequence>{});
            } else {
                KGlobalAccel::self()->removeAllShortcuts(action.get());
            }
        }
        shortcutsRegistered = enabled;
    }
#endif

    explicit Impl(DesktopIntegration *owner) : owner(owner) {}
    ~Impl()
    {
#ifdef Q_OS_WIN
        qApp->removeNativeEventFilter(this);
        for (std::size_t i = 0; i < actions.size(); ++i) UnregisterHotKey(nullptr, int(i + 1));
#endif
    }

    void showOsd(const QString &kind, int percent)
    {
        if (percent < 0) return;
#ifndef Q_OS_WIN
        if (osd.isValid()) {
            osd.asyncCall(kind == QStringLiteral("volume") ? QStringLiteral("volumeChanged")
                                                         : QStringLiteral("brightnessChanged"), percent);
            return;
        }
#endif
        emit owner->osdRequested(kind, percent);
    }

    void updateTray()
    {
        const bool visible = !controller->hideTrayIcon() && owner->trayAvailable();
        if (visible && !tray) {
            menu = std::make_unique<QMenu>();
            QObject::connect(menu->addAction(QCoreApplication::translate("Tray", "Show Brightless")),
                &QAction::triggered, owner, &DesktopIntegration::activateRequested);
            menu->addSeparator();
            QObject::connect(menu->addAction(QCoreApplication::translate("Tray", "Quit")),
                &QAction::triggered, qApp, &QApplication::quit);
#ifdef Q_OS_WIN
            tray = std::make_unique<QSystemTrayIcon>(QApplication::windowIcon());
            tray->setToolTip(QStringLiteral("Brightless"));
            tray->setContextMenu(menu.get());
            QObject::connect(tray.get(), &QSystemTrayIcon::activated, owner,
                [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                        emit owner->activateRequested();
                });
            tray->show();
#else
            tray = std::make_unique<KStatusNotifierItem>(QStringLiteral("brightless"));
            tray->setCategory(KStatusNotifierItem::Hardware);
            tray->setIconByPixmap(QApplication::windowIcon());
            tray->setToolTipTitle(QStringLiteral("Brightless"));
            tray->setStandardActionsEnabled(false);
            tray->setContextMenu(menu.release()); // KStatusNotifierItem owns its menu.
            QObject::connect(tray.get(), &KStatusNotifierItem::activateRequested,
                             owner, &DesktopIntegration::activateRequested);
            QObject::connect(tray.get(), &KStatusNotifierItem::scrollRequested, owner,
                [this](int delta, Qt::Orientation orientation) {
                    if (orientation == Qt::Vertical)
                        showOsd(QStringLiteral("brightness"), controller->adjustAllBrightness(delta));
                });
            tray->setStatus(KStatusNotifierItem::Active);
#endif
        } else if (!visible) {
            tray.reset();
            menu.reset();
        }
        qApp->setQuitOnLastWindowClosed(!visible || !controller->closeToTray());
    }
};

DesktopIntegration::DesktopIntegration(QObject *parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {}
DesktopIntegration::~DesktopIntegration() = default;

bool DesktopIntegration::plasmaShortcutsAvailable() const
{
#ifdef Q_OS_WIN
    return false;
#else
    return true;
#endif
}
bool DesktopIntegration::shortcutEditorAvailable() const { return !plasmaShortcutsAvailable(); }
bool DesktopIntegration::trayAvailable() const
{
#ifdef Q_OS_WIN
    return QSystemTrayIcon::isSystemTrayAvailable();
#else
    return true; // Preserve Plasma's StatusNotifier registration even during shell startup.
#endif
}

void DesktopIntegration::attach(BrightlessController *controller)
{
    auto &d = *impl_;
    d.controller = controller;
    connect(controller, &QObject::destroyed, this, [this] { impl_.reset(); });
    const auto add = [&](const QString &id, const QString &text, auto callback) {
        auto action = std::make_unique<QAction>(text);
        action->setObjectName(id);
        connect(action.get(), &QAction::triggered, controller, callback);
        d.actions.push_back(std::move(action));
    };
    add(QStringLiteral("increase_brightness_dynamic_contrast"),
        QCoreApplication::translate("GlobalShortcuts", "Increase brightness/dynamic contrast"),
        [&d] { d.showOsd(QStringLiteral("brightness"), d.controller->adjustAllBrightness(1)); });
    add(QStringLiteral("decrease_brightness_dynamic_contrast"),
        QCoreApplication::translate("GlobalShortcuts", "Decrease brightness/dynamic contrast"),
        [&d] { d.showOsd(QStringLiteral("brightness"), d.controller->adjustAllBrightness(-1)); });
    add(QStringLiteral("increase_contrast"), QCoreApplication::translate("GlobalShortcuts", "Increase contrast"),
        [controller] { controller->adjustAllContrast(1); });
    add(QStringLiteral("decrease_contrast"), QCoreApplication::translate("GlobalShortcuts", "Decrease contrast"),
        [controller] { controller->adjustAllContrast(-1); });
    add(QStringLiteral("increase_volume"), QCoreApplication::translate("GlobalShortcuts", "Increase volume"),
        [&d] { d.showOsd(QStringLiteral("volume"), d.controller->adjustAllVolume(1)); });
    add(QStringLiteral("decrease_volume"), QCoreApplication::translate("GlobalShortcuts", "Decrease volume"),
        [&d] { d.showOsd(QStringLiteral("volume"), d.controller->adjustAllVolume(-1)); });
    add(QStringLiteral("change_input_device"), QCoreApplication::translate("GlobalShortcuts", "Change input device"),
        [controller] { controller->changeAllInputSources(); });
#ifdef Q_OS_WIN
    qApp->installNativeEventFilter(&d);
#else
    connect(controller, &BrightlessController::plasmaGlobalShortcutsChanged, this, [&d] { d.updateShortcuts(); });
#endif
    d.updateShortcuts();
    connect(controller, &BrightlessController::visibilitySettingsChanged, this, [&d] { d.updateTray(); });
    connect(controller, &BrightlessController::closeToTrayChanged, this, [&d] { d.updateTray(); });
    d.updateTray();
}

void DesktopIntegration::configureShortcuts()
{
#ifdef Q_OS_WIN
    QDialog dialog;
    dialog.setWindowTitle(tr("Global shortcuts"));
    auto *layout = new QFormLayout(&dialog);
    QSettings settings;
    std::vector<QKeySequenceEdit *> editors;
    for (const auto &action : impl_->actions) {
        auto *editor = new QKeySequenceEdit(QKeySequence::fromString(settings.value(
            QStringLiteral("shortcuts/") + action->objectName()).toString(), QKeySequence::PortableText));
        editor->setAccessibleName(action->text());
        layout->addRow(action->text(), editor);
        editors.push_back(editor);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        for (std::size_t i = 0; i < editors.size(); ++i) {
            settings.setValue(QStringLiteral("shortcuts/") + impl_->actions[i]->objectName(),
                              editors[i]->keySequence().toString(QKeySequence::PortableText));
        }
        settings.sync();
        impl_->updateShortcuts();
        if (settings.status() != QSettings::NoError)
            impl_->controller->setOperationError(tr("Failed to save shortcuts"));
    }
#endif
}
