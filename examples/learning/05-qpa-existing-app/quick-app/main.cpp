#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("HyRemote QPA Existing Quick App"));

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/hyremote/examples/qpa-existing-quick/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
