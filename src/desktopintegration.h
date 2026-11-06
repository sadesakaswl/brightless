#pragma once

#include <QObject>
#include <QVariantList>
#include <memory>

class BrightlessController;

class DesktopIntegration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool shortcutsEnabled READ shortcutsEnabled WRITE setShortcutsEnabled NOTIFY shortcutsEnabledChanged)
    Q_PROPERTY(QVariantList shortcutActions READ shortcutActions NOTIFY shortcutActionsChanged)
    Q_PROPERTY(QString shortcutError READ shortcutError NOTIFY shortcutErrorChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
public:
    explicit DesktopIntegration(QObject *parent = nullptr);
    ~DesktopIntegration() override;
    void attach(BrightlessController *controller);
    bool shortcutsEnabled() const;
    void setShortcutsEnabled(bool enabled);
    QVariantList shortcutActions() const;
    QString shortcutError() const;
    bool trayAvailable() const;
    Q_INVOKABLE bool setShortcut(const QString &actionId, const QString &sequence);
    Q_INVOKABLE QString keySequence(int key, int modifiers) const;

signals:
    void shortcutsEnabledChanged();
    void shortcutActionsChanged();
    void shortcutErrorChanged();
    void activateRequested();
    void trayActivated();
    void osdRequested(const QString &kind, int percent);

private:
    void setShortcutError(const QString &error);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
