#include "singleinstance.h"
#include "configpath.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

SingleInstance::SingleInstance(QObject *parent) : QObject(parent)
{
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = server_.nextPendingConnection()) {
            socket->setReadBufferSize(8193);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(5000, socket, &QLocalSocket::abort);
            const auto receive = [this, socket] {
                if (socket->bytesAvailable() > 8192) {
                    socket->abort();
                } else if (socket->canReadLine()) {
                    const auto request = socket->readLine().trimmed();
                    if (request.startsWith("activate:")) {
                        emit activateRequested(QString::fromUtf8(QByteArray::fromBase64(request.mid(9))));
                        socket->write("ok\n");
                        socket->flush();
                    }
                    socket->disconnectFromServer();
                }
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            receive();
        }
    });
}

int SingleInstance::start(bool autostart)
{
    const auto root = brightless::configRoot()
        + QStringLiteral("/brightless");
    if (!QDir().mkpath(root)) return -1;
    const auto name = QStringLiteral("brightless-") + QString::fromLatin1(
        QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    lock_ = std::make_unique<QLockFile>(root + QStringLiteral("/instance.lock"));
    lock_->setStaleLockTime(0); // A slow/live application must never be treated as stale.
    if (!lock_->tryLock()) {
        if (lock_->error() != QLockFile::LockFailedError) return -1;
        if (autostart) return 1;
#ifdef Q_OS_WIN
        qint64 pid = 0;
        QString host, app;
        if (lock_->getLockInfo(&pid, &host, &app)) AllowSetForegroundWindow(DWORD(pid));
#endif
        QLocalSocket socket;
        // The primary may still be between acquiring the lock and listening.
        for (int attempt = 0; attempt < 20; ++attempt) {
            socket.connectToServer(name);
            if (socket.waitForConnected(100)) break;
            socket.abort();
            QThread::msleep(50);
        }
        if (socket.state() != QLocalSocket::ConnectedState) return -1;
        const auto token = qEnvironmentVariable("XDG_ACTIVATION_TOKEN").toUtf8().toBase64();
        if (token.size() > 8000) return -1;
        socket.write("activate:" + token + '\n');
        if (socket.bytesToWrite() && !socket.waitForBytesWritten(1000)) return -1;
        QElapsedTimer timer;
        timer.start();
        while (!socket.canReadLine()) {
            const auto remaining = 5000 - timer.elapsed();
            if (remaining <= 0 || !socket.waitForReadyRead(int(remaining))) return -1;
        }
        return socket.readLine() == "ok\n" ? 1 : -1;
    }
    QLocalServer::removeServer(name); // Only the lock owner can remove a stale endpoint.
    return server_.listen(name) ? 0 : -1;
}
