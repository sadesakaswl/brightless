#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Brightless"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Brightless"));
    QGuiApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/com/brightless/icon.png")));

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/com/brightless/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
