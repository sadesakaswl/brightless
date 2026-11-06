#include "desktopintegration.h"
#include "brightlesscontroller.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QKeySequence>
#include <QSettings>
#include <QSystemTrayIcon>
#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
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
    QString shortcutError;
    bool updatingShortcuts = false;
#ifdef Q_OS_WIN
    std::unique_ptr<QSystemTrayIcon> tray;
    std::vector<bool> registered;
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override
    {
        const auto *msg = static_cast<MSG *>(message);
        if (!msg || msg->message != WM_HOTKEY || msg->wParam < 1 || msg->wParam > actions.size()
            || !registered[msg->wParam - 1]) {
            return false;
        }
        actions[msg->wParam - 1]->trigger();
        // Qt can pass a null result for thread-level messages such as WM_HOTKEY.
        if (result) *result = 0;
        return true;
    }

    bool registerShortcut(int id, const QKeySequence &key)
    {
        if (key.isEmpty()) return true;
        if (key.count() != 1) return false;
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
        return vk && !(modifiers & (Qt::KeypadModifier | Qt::GroupSwitchModifier))
            && RegisterHotKey(nullptr, id, nativeModifiers | MOD_NOREPEAT, vk);
    }
#else
    std::unique_ptr<KStatusNotifierItem> tray;
    QDBusInterface osd{QStringLiteral("org.kde.plasmashell"), QStringLiteral("/org/kde/osdService"),
                       QStringLiteral("org.kde.osdService"), QDBusConnection::sessionBus()};
#endif

    QString settingsKey(const QAction *action) const
    {
        return QStringLiteral("shortcuts/") + action->objectName();
    }

    QString conflict(const QAction *action, const QKeySequence &key) const
    {
        if (key.isEmpty()) return {};
        if (key.count() != 1 || key[0].key() == Qt::Key_unknown
            || key[0].key() == 0 || key[0].key() == Qt::Key_Shift || key[0].key() == Qt::Key_Control
            || key[0].key() == Qt::Key_Alt || key[0].key() == Qt::Key_Meta || key[0].key() == Qt::Key_AltGr) {
            return DesktopIntegration::tr("Use a single key combination, not a sequence or modifier alone.");
        }
        for (const auto &other : actions) {
            if (other.get() != action && other->shortcuts().contains(key)) {
                return DesktopIntegration::tr("Shortcut %1 is already assigned to %2.")
                    .arg(key.toString(QKeySequence::NativeText), other->text());
            }
        }
#ifndef Q_OS_WIN
        for (const auto match : {KGlobalAccel::Equal, KGlobalAccel::Shadows, KGlobalAccel::Shadowed}) {
            for (const auto &shortcut : KGlobalAccel::globalShortcutsByKey(key, match)) {
                if (shortcut.componentUniqueName() != QCoreApplication::applicationName()
                    || shortcut.uniqueName() != action->objectName()) {
                    return DesktopIntegration::tr("Shortcut %1 is already assigned to %2 (%3).")
                        .arg(key.toString(QKeySequence::NativeText), shortcut.friendlyName(),
                             shortcut.componentFriendlyName());
                }
            }
        }
#endif
        return {};
    }

    bool applyShortcut(std::size_t index, const QList<QKeySequence> &keys)
    {
#ifdef Q_OS_WIN
        UnregisterHotKey(nullptr, int(index + 1));
        registered[index] = false;
        if (keys.isEmpty()) return true;
        registered[index] = registerShortcut(int(index + 1), keys.first());
        return registered[index];
#else
        auto *action = actions[index].get();
        if (!KGlobalAccel::self()->setShortcut(action, keys, KGlobalAccel::NoAutoloading)) return false;
        const auto actual = KGlobalAccel::self()->shortcut(action);
        return std::is_permutation(keys.cbegin(), keys.cend(), actual.cbegin(), actual.cend());
#endif
    }

    void updateShortcuts()
    {
        updatingShortcuts = true;
        QStringList errors;
#ifdef Q_OS_WIN
        // Release all old registrations first so changing assignments cannot collide with stale IDs.
        for (std::size_t i = 0; i < actions.size(); ++i) {
            UnregisterHotKey(nullptr, int(i + 1));
            registered[i] = false;
        }
#endif
        for (std::size_t i = 0; i < actions.size(); ++i) {
            auto *action = actions[i].get();
            if (!owner->shortcutsEnabled()) {
#ifndef Q_OS_WIN
                KGlobalAccel::self()->removeAllShortcuts(action);
#endif
                continue;
            }
            const auto keys = action->shortcuts();
            bool available = true;
            for (const auto &key : keys) {
                const auto error = conflict(action, key);
                if (!error.isEmpty()) {
                    errors.append(error);
                    available = false;
                }
            }
#ifndef Q_OS_WIN
            if (available) {
                auto plasmaKeys = KGlobalAccel::self()->globalShortcut(QCoreApplication::applicationName(), action->objectName());
                plasmaKeys.removeAll(QKeySequence());
                if (!plasmaKeys.isEmpty()
                    && !std::is_permutation(keys.cbegin(), keys.cend(), plasmaKeys.cbegin(), plasmaKeys.cend())) {
                    errors.append(DesktopIntegration::tr("Plasma has a different shortcut for %1: %2. Set it again in Brightless to resolve.")
                        .arg(action->text(), QKeySequence::listToString(plasmaKeys, QKeySequence::NativeText)));
                    available = false;
                }
            }
#endif
            if (available && !applyShortcut(i, keys)) errors.append(action->text());
        }
        updatingShortcuts = false;
        owner->setShortcutError(errors.isEmpty() ? QString()
            : DesktopIntegration::tr("Unavailable shortcuts: %1").arg(errors.join(QStringLiteral(", "))));
    }

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
                        emit owner->trayActivated();
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
                             owner, &DesktopIntegration::trayActivated);
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

bool DesktopIntegration::shortcutsEnabled() const
{
    if (!impl_ || !impl_->controller) return false;
#ifdef Q_OS_WIN
    QSettings settings;
    // Existing Windows bindings were always enabled before the switch was added.
    return settings.value(QStringLiteral("shortcuts/enabled"),
        std::any_of(impl_->actions.begin(), impl_->actions.end(), [](const auto &action) {
            return !action->shortcuts().isEmpty();
        })).toBool();
#else
    return impl_->controller->plasmaGlobalShortcuts();
#endif
}

void DesktopIntegration::setShortcutsEnabled(bool enabled)
{
    if (!impl_ || !impl_->controller || enabled == shortcutsEnabled()) return;
#ifdef Q_OS_WIN
    QSettings settings;
    settings.setValue(QStringLiteral("shortcuts/enabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        settings.setValue(QStringLiteral("shortcuts/enabled"), !enabled);
        setShortcutError(tr("Failed to save shortcuts"));
        return;
    }
    impl_->updateShortcuts();
    emit shortcutsEnabledChanged();
#else
    impl_->controller->setPlasmaGlobalShortcuts(enabled);
#endif
}

QVariantList DesktopIntegration::shortcutActions() const
{
    QVariantList result;
    if (!impl_) return result;
    for (const auto &action : impl_->actions) {
        result.append(QVariantMap{{QStringLiteral("id"), action->objectName()},
            {QStringLiteral("text"), action->text()},
            {QStringLiteral("sequence"), QKeySequence::listToString(action->shortcuts(), QKeySequence::PortableText)},
            {QStringLiteral("displaySequence"), QKeySequence::listToString(action->shortcuts(), QKeySequence::NativeText)}});
    }
    return result;
}

QString DesktopIntegration::shortcutError() const { return impl_ ? impl_->shortcutError : QString(); }

void DesktopIntegration::setShortcutError(const QString &error)
{
    if (!impl_ || impl_->shortcutError == error) return;
    impl_->shortcutError = error;
    emit shortcutErrorChanged();
}
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
    QSettings settings;
    for (const auto &action : d.actions) {
        const auto key = d.settingsKey(action.get());
        auto keys = QKeySequence::listFromString(settings.value(key).toString(), QKeySequence::PortableText);
#ifndef Q_OS_WIN
        // Import Plasma only on first use; saved app bindings must pass conflict checks unchanged.
        if (!settings.contains(key)) {
            keys = KGlobalAccel::self()->globalShortcut(QCoreApplication::applicationName(), action->objectName());
            if (!keys.isEmpty()) settings.setValue(key, QKeySequence::listToString(keys, QKeySequence::PortableText));
        }
#endif
        keys.removeAll(QKeySequence());
        action->setShortcuts(keys);
    }
#ifdef Q_OS_WIN
    d.registered.resize(d.actions.size(), false);
    qApp->installNativeEventFilter(&d);
#else
    connect(controller, &BrightlessController::plasmaGlobalShortcutsChanged, this, [&d, this] {
        d.updateShortcuts();
        emit shortcutsEnabledChanged();
    });
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this,
        [this](QAction *action, const QKeySequence &) {
            if (!impl_ || impl_->updatingShortcuts || !shortcutsEnabled()) return;
            auto &d = *impl_;
            for (const auto &ownAction : d.actions) {
                if (ownAction.get() != action) continue;
                const auto keys = KGlobalAccel::self()->shortcut(action);
                action->setShortcuts(keys);
                QSettings settings;
                settings.setValue(d.settingsKey(action), QKeySequence::listToString(keys, QKeySequence::PortableText));
                emit shortcutActionsChanged();
            }
        });
#endif
    d.updateShortcuts();
    emit shortcutActionsChanged();
    emit shortcutsEnabledChanged();
    connect(controller, &BrightlessController::visibilitySettingsChanged, this, [&d] { d.updateTray(); });
    connect(controller, &BrightlessController::closeToTrayChanged, this, [&d] { d.updateTray(); });
    d.updateTray();
}

QString DesktopIntegration::keySequence(int key, int modifiers) const
{
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(modifiers), Qt::Key(key)))
        .toString(QKeySequence::PortableText);
}

bool DesktopIntegration::setShortcut(const QString &actionId, const QString &sequence)
{
    if (!impl_ || !shortcutsEnabled()) return false;
    auto &d = *impl_;
    const auto found = std::find_if(d.actions.begin(), d.actions.end(), [&](const auto &action) {
        return action->objectName() == actionId;
    });
    if (found == d.actions.end()) {
        setShortcutError(tr("Unknown shortcut action."));
        return false;
    }
    auto *action = found->get();
    const auto key = QKeySequence::fromString(sequence, QKeySequence::PortableText);
    const auto error = d.conflict(action, key);
    if (!error.isEmpty() || (!sequence.isEmpty() && key.isEmpty())) {
        setShortcutError(error.isEmpty() ? tr("Invalid shortcut.") : error);
        return false;
    }
    const QList<QKeySequence> keys = key.isEmpty() ? QList<QKeySequence>{} : QList<QKeySequence>{key};
    const auto previous = action->shortcuts();
    auto previousRegistration = previous;
#ifndef Q_OS_WIN
    previousRegistration = KGlobalAccel::self()->globalShortcut(QCoreApplication::applicationName(), action->objectName());
#endif
    const auto index = std::size_t(found - d.actions.begin());
    d.updatingShortcuts = true;
    const auto restore = [&] {
        d.applyShortcut(index, previousRegistration);
        d.updatingShortcuts = false;
    };
#ifdef Q_OS_WIN
    // Probe while the old key is still registered; never discard a working binding on conflict.
    if (!key.isEmpty() && !previous.contains(key)) {
        constexpr int probeId = 0x6ffe;
        if (!d.registerShortcut(probeId, key)) {
            d.updatingShortcuts = false;
            setShortcutError(tr("Shortcut %1 is unavailable or reserved by another application.")
                .arg(key.toString(QKeySequence::NativeText)));
            return false;
        }
        UnregisterHotKey(nullptr, probeId);
    }
#endif
    if (!d.applyShortcut(index, keys)) {
        restore();
        setShortcutError(tr("Shortcut %1 is unavailable or reserved by another application.")
            .arg(key.toString(QKeySequence::NativeText)));
        return false;
    }
    QSettings settings;
    settings.setValue(d.settingsKey(action), QKeySequence::listToString(keys, QKeySequence::PortableText));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        settings.setValue(d.settingsKey(action), QKeySequence::listToString(previous, QKeySequence::PortableText));
        restore();
        setShortcutError(tr("Failed to save shortcuts"));
        return false;
    }
    action->setShortcuts(keys);
    d.updatingShortcuts = false;
    setShortcutError({});
    emit shortcutActionsChanged();
    return true;
}
