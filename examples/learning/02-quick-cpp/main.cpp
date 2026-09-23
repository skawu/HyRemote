// HyRemote V0.1 learning example 02: the same Embedded C++ path as example 01, on a Qt Quick window.
//
// The point of this example is one sentence:
//
//     A Qt Quick UI does NOT require HyRemote's QML frontend.
//
// Quick is a UI family; the HyRemote QML module is a separate integration frontend. A Quick application that wants
// remote access links exactly what a Widgets application links - Qt plus HyRemote::RemoteAccess - and never writes
// `import HyRemote`. Compare this file with example 01: the integration lines are the same, and only the window and
// the UI language differ.
//
// A QQuickView is a QQuickWindow that loads QML, so the structure is:
//
//     QML UI -> QQuickView (a QQuickWindow) -> this C++ bootstrap -> HyRemote::RemoteAccess

#include <HyRemote/RemoteAccess.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlError>
#include <QQuickView>
#include <QTimer>
#include <QUrl>

#include <iostream>

namespace {

int positiveInt(const QCommandLineParser &parser, const QCommandLineOption &option, int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Learning 02 - Quick + C++"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote Embedded C++ API over a Qt Quick window"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Listener port the viewer connects to."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5921"));
    QCommandLineOption inputOption(
        QStringLiteral("remote-input"),
        QStringLiteral("Enable remote keyboard/pointer input. Without it the application is view-only."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit by itself after N seconds (used by the smoke test)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const int port = positiveInt(parser, portOption, 5921);
    if (port > 65535) {
        std::cerr << "invalid port: " << port << std::endl;
        return 64;
    }
    const int testSeconds = positiveInt(parser, secondsOption, 0);
    const bool remoteInput = parser.isSet(inputOption);

    QQuickView view;
    view.setTitle(QStringLiteral("HyRemote Learning 02 - Quick + C++"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(420, 180);
    // The QML is plain Qt Quick: it has no HyRemote import and no HyRemote types.
    view.setInitialProperties({{QStringLiteral("remoteControlEnabled"), remoteInput},
                               {QStringLiteral("listenPort"), port}});
    view.setSource(QUrl(QStringLiteral("qrc:/hyremote/learning/02-quick-cpp/Main.qml")));
    if (view.status() == QQuickView::Error) {
        for (const QQmlError &error : view.errors())
            std::cerr << error.toString().toStdString() << std::endl;
        return 65;
    }
    if (!view.rootObject()) {
        std::cerr << "the Quick root object is unavailable" << std::endl;
        return 66;
    }
    view.show();

    HyRemote::RemoteAccess remote(&view);
    remote.setPort(static_cast<quint16>(port));
    remote.setRemoteInputEnabled(remoteInput);
    if (!remote.start()) {
        const auto error = remote.lastError();
        std::cerr << "START_FAILED";
        if (error)
            std::cerr << " " << error->message.toStdString();
        std::cerr << std::endl;
        return 2;
    }

    std::cout << "APP_READY" << std::endl;
    std::cout << "PLATFORM_NAME=" << QGuiApplication::platformName().toStdString() << std::endl;
    if (remote.state() != HyRemote::RemoteAccessState::Running) {
        std::cerr << "unexpected state after start()" << std::endl;
        remote.stop();
        return 3;
    }
    std::cout << "HYREMOTE_START=Running" << std::endl;

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    const int result = app.exec();

    remote.stop();
    if (remote.state() != HyRemote::RemoteAccessState::Stopped)
        std::cerr << "unexpected state after stop()" << std::endl;
    std::cout << "HYREMOTE_STOP=Stopped" << std::endl;
    return result;
}
