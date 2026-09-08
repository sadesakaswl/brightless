#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <memory>

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);
    // 0: primary, 1: notified primary, -1: IPC/lock error (do not start another instance).
    int start(bool autostart);
signals:
    void activateRequested(const QString &token);
private:
    QLocalServer server_;
    std::unique_ptr<QLockFile> lock_;
};
