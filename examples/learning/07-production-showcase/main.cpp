#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtLogging>

#include <cstdio>
#include <cstdlib>
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

void acceptanceMessageHandler(QtMsgType type,
                              const QMessageLogContext &,
                              const QString &message)
{
    const QByteArray bytes = message.toLocal8Bit();
    if (!bytes.isEmpty())
        std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);

    if (type == QtFatalMsg)
        std::abort();
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Production Showcase"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("HyRemote Qt Quick/QML production showcase"));
    parser.addHelpOption();

    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Loopback VNC port."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5921"));
    QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                   QStringLiteral("Enable remote input when remote access starts."));
    QCommandLineOption autoStartOption(
        QStringLiteral("auto-start"),
        QStringLiteral("Explicit test/acceptance helper: start after the local window is shown."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(autoStartOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const int port = readPositiveInt(parser, portOption, 5921);
    if (port > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }
    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    const bool acceptanceMode = parser.isSet(autoStartOption) || testSeconds > 0;
    if (acceptanceMode)
        qInstallMessageHandler(acceptanceMessageHandler);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_BUILD_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("showcasePort"), port);
    engine.rootContext()->setContextProperty(QStringLiteral("showcaseRemoteInput"),
                                             parser.isSet(inputOption));
    engine.rootContext()->setContextProperty(QStringLiteral("showcaseAutoStart"),
                                             parser.isSet(autoStartOption));
    engine.rootContext()->setContextProperty(QStringLiteral("showcaseTimeoutMs"),
                                             testSeconds * 1000);
    engine.rootContext()->setContextProperty(QStringLiteral("showcaseAcceptanceMode"),
                                             acceptanceMode);

    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreationFailed,
                     &app,
                     [] { QCoreApplication::exit(65); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("HyRemoteShowcase"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 65;

    return app.exec();
}
