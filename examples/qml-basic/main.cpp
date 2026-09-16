#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <iostream>

namespace {

int readPositiveInt(const QCommandLineParser &parser,
                    const QCommandLineOption &option,
                    int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote QML Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote Declarative QML API example"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Loopback VNC port."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5900"));
    QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                   QStringLiteral("Explicitly enable remote input. Default is view-only."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    QCommandLineOption transitionOption(
        QStringLiteral("policy-transition-ms"),
        QStringLiteral("After N ms, stop the QML RemoteAccess runtime, enable remote input, and restart it (CI/product-fit helper)."),
        QStringLiteral("milliseconds"),
        QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(secondsOption);
    parser.addOption(transitionOption);
    parser.process(app);

    const int port = readPositiveInt(parser, portOption, 5900);
    if (port > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }

    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    const int policyTransitionMs = readPositiveInt(parser, transitionOption, 0);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_BUILD_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("acceptancePort"), port);
    engine.rootContext()->setContextProperty(QStringLiteral("acceptanceRemoteInput"), parser.isSet(inputOption));
    engine.rootContext()->setContextProperty(QStringLiteral("acceptanceTimeoutMs"), testSeconds * 1000);
    engine.rootContext()->setContextProperty(QStringLiteral("acceptancePolicyTransitionMs"), policyTransitionMs);

    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreationFailed,
                     &app,
                     [] { QCoreApplication::exit(65); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("HyRemoteExample"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 65;

    return app.exec();
}
