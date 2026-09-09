#include "singleinstance.h"
#include "configpath.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDebug>
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
    const auto fail = [](const QString &error) {
        qWarning().noquote() << "Single-instance IPC:" << error;
        return -1;
    };
    const auto root = brightless::configRoot()
        + QStringLiteral("/brightless");
    if (!QDir().mkpath(root)) return fail(QStringLiteral("Cannot create configuration directory"));
    const auto name = QStringLiteral("brightless-") + QString::fromLatin1(
        QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    lock_ = std::make_unique<QLockFile>(root + QStringLiteral("/instance.lock"));
    lock_->setStaleLockTime(0); // A slow/live application must never be treated as stale.
    if (!lock_->tryLock()) {
        if (lock_->error() != QLockFile::LockFailedError) return fail(QStringLiteral("Cannot lock configuration directory"));
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
        if (socket.state() != QLocalSocket::ConnectedState) return fail(socket.errorString());
        const auto token = qEnvironmentVariable("XDG_ACTIVATION_TOKEN").toUtf8().toBase64();
        if (token.size() > 8000) return fail(QStringLiteral("Activation token too long"));
        socket.write("activate:" + token + '\n');
        socket.flush();
        QElapsedTimer timer;
        timer.start();
        // Windows named-pipe waits can report false before the asynchronous operation completes.
        while (!socket.canReadLine() && socket.state() == QLocalSocket::ConnectedState
               && timer.elapsed() < 5000) {
            socket.waitForReadyRead(100);
        }
        return socket.canReadLine() && socket.readLine() == "ok\n" ? 1
            : fail(QStringLiteral("Activation reply: %1").arg(socket.errorString()));
    }
    QLocalServer::removeServer(name); // Only the lock owner can remove a stale endpoint.
    return server_.listen(name) ? 0 : fail(server_.errorString());
}
