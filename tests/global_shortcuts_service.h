#pragma once

#include <KGlobalAccel>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QMap>
#include <algorithm>

// An isolated Plasma shortcut registry; no host shortcuts are read or changed.
class GlobalShortcutsService : public QDBusVirtualObject
{
public:
    struct Shortcut {
        QStringList id;
        QList<QKeySequence> keys;
    };
    QMap<QString, Shortcut> shortcuts;
    QStringList registrations;
    int queries = 0;
    bool failNextRegistration = false;

    void add(const QString &component, const QString &action, const QString &sequence)
    {
        shortcuts[component + '/' + action] = {{component, action, component, action},
            QKeySequence::listFromString(sequence, QKeySequence::PortableText)};
    }

    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        const auto method = message.member();
        const auto arguments = message.arguments();
        QVariantList result;
        if (method == "getComponent") {
            result.append(QVariant::fromValue(QDBusObjectPath(QStringLiteral("/component/ShortcutTests"))));
        } else if (method == "globalShortcutsByKey") {
            ++queries;
            const auto key = qdbus_cast<QKeySequence>(arguments.at(0));
            QDBusArgument reply;
            reply.beginArray(QMetaType::fromType<KGlobalShortcutInfo>());
            for (const auto &shortcut : shortcuts) {
                if (!shortcut.keys.contains(key)) continue;
                reply.beginStructure();
                reply << shortcut.id.at(1) << shortcut.id.at(3)
                      << shortcut.id.at(0) << shortcut.id.at(2) << QString() << QString()
                      << QList<int>() << QList<int>();
                reply.endStructure();
            }
            reply.endArray();
            result.append(QVariant::fromValue(reply));
        } else if (method == "doRegister" || method == "shortcutKeys" || method == "setShortcutKeys") {
            const auto id = qdbus_cast<QStringList>(arguments.at(0));
            const auto name = id.at(0) + '/' + id.at(1);
            if (method == "doRegister") {
                shortcuts[name].id = id;
            } else if (method == "shortcutKeys") {
                result.append(QVariant::fromValue(shortcuts.value(name).keys));
            } else {
                if (failNextRegistration) {
                    failNextRegistration = false;
                    return connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                                                     QStringLiteral("Registration failed")));
                }
                auto keys = qdbus_cast<QList<QKeySequence>>(arguments.at(1));
                std::reverse(keys.begin(), keys.end()); // The native registry stores unordered key sets.
                registrations.append(name);
                shortcuts[name] = {id, keys};
                result.append(QVariant::fromValue(keys));
            }
        } else if (method == "unregister") {
            result.append(shortcuts.remove(arguments.at(0).toString() + '/' + arguments.at(1).toString()) > 0);
        } else if (method != "setInactive") {
            return connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"), method));
        }
        return connection.send(message.createReply(result));
    }
};
