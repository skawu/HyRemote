#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("HyRemote QPA Quick Existing App"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/hyremote/branding/logo.png")));

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/hyremote/qpa-quick/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
