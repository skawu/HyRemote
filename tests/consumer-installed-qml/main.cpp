#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QStringList>
#include <QTimer>
#include <QVariant>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    int testSeconds = 0;
    const QStringList arguments = QCoreApplication::arguments();
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        if (arguments.at(index) != QStringLiteral("--test-seconds"))
            continue;
        if (index + 1 >= arguments.size())
            return 4;
        bool ok = false;
        testSeconds = arguments.at(++index).toInt(&ok);
        if (!ok || testSeconds <= 0 || testSeconds > 60)
            return 4;
    }

    QQmlApplicationEngine engine;
    engine.loadFromModule("HyRemoteInstalledConsumer", "Main");
    if (engine.rootObjects().isEmpty())
        return 2;

    QObject *root = engine.rootObjects().constFirst();
    if (!root || !root->property("contractOk").toBool())
        return 3;

    // Normal clean-QML consumption only needs to prove import/instantiation/deployment and exits
    // immediately. The combined QML+QPA GA configuration asks the exact same application to stay
    // alive long enough for the transparent platform runtime to accept/reaccept RFB viewers.
    if (testSeconds == 0)
        return 0;

    QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);
    return app.exec();
}
