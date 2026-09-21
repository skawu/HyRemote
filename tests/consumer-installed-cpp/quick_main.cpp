// A real Qt Quick application consuming the installed C++ product surface.
//
// A Quick user window is a C++ Quick window: this consumer bootstraps QQuickWindow directly and deliberately does
// not link the declarative HyRemote frontend and does not import the HyRemote QML URI. Quick-as-a-UI-family and the
// QML-as-an-integration-frontend are different things, and the product surface a Quick application is promised is
// the same public C++ facade a Widgets application uses.
//
// The scene graph runs on the software backend so the consumer is deterministic on a headless reference runner; the
// rendering backend is a Qt choice and does not affect the platform identity, the runtime lifecycle or the product
// contract under test.

#include <HyRemote/RemoteAccess.h>

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QString>
#include <QTimer>

#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

    QQuickWindow window;
    window.resize(480, 320);
    window.setTitle(QStringLiteral("HyRemote C++ Quick consumer"));
    window.show();

    HyRemote::RemoteAccess remote(&window);

    bool portConfigured = false;
    const int configuredPort = qEnvironmentVariableIntValue("HYREMOTE_CPP_CONSUMER_PORT", &portConfigured);
    if (portConfigured) {
        if (!remote.setPort(static_cast<quint16>(configuredPort))) {
            std::fprintf(stderr, "FAIL: installed SDK refused setPort(%d)\n", configuredPort);
            return 2;
        }
    }

    if (remote.target() != &window) {
        std::fprintf(stderr, "FAIL: installed SDK did not retain the Quick window as the capture target\n");
        return 3;
    }

    if (!remote.start()) {
        const auto error = remote.lastError();
        std::fprintf(stderr,
                     "FAIL: installed SDK refused to start on a Quick target (code=%d)\n",
                     error ? static_cast<int>(error->code) : -1);
        return 4;
    }

    if (remote.state() != HyRemote::RemoteAccessState::Running) {
        std::fprintf(stderr, "FAIL: start() did not leave the runtime Running\n");
        remote.stop();
        return 5;
    }
    std::printf("HYREMOTE_CPP_QUICK_START=Running\n");
    std::printf("HYREMOTE_CPP_QUICK_PLATFORM=%s\n", qPrintable(QGuiApplication::platformName()));
    std::printf("HYREMOTE_CPP_QUICK_PORT=%u\n", static_cast<unsigned>(remote.port()));

    QTimer::singleShot(1200, &app, &QCoreApplication::quit);
    app.exec();

    if (remote.state() != HyRemote::RemoteAccessState::Running) {
        std::fprintf(stderr, "FAIL: runtime left Running while the event loop was pumped\n");
        remote.stop();
        return 6;
    }

    remote.stop();
    if (remote.state() != HyRemote::RemoteAccessState::Stopped) {
        std::fprintf(stderr, "FAIL: stop() did not return the runtime to Stopped\n");
        return 7;
    }
    std::printf("HYREMOTE_CPP_QUICK_STOP=Stopped\n");
    return 0;
}
