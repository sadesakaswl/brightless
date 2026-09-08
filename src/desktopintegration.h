#pragma once

#include <QObject>
#include <memory>

class BrightlessController;

class DesktopIntegration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool plasmaShortcutsAvailable READ plasmaShortcutsAvailable CONSTANT)
    Q_PROPERTY(bool shortcutEditorAvailable READ shortcutEditorAvailable CONSTANT)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
public:
    explicit DesktopIntegration(QObject *parent = nullptr);
    ~DesktopIntegration() override;
    void attach(BrightlessController *controller);
    bool plasmaShortcutsAvailable() const;
    bool shortcutEditorAvailable() const;
    bool trayAvailable() const;
    Q_INVOKABLE void configureShortcuts();

signals:
    void activateRequested();
    void osdRequested(const QString &kind, int percent);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
