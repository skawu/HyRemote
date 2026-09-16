#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.loadFromModule("HyRemoteInstalledConsumer", "Main");
    if (engine.rootObjects().isEmpty())
        return 2;

    return 0;
}
