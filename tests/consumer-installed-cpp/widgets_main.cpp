// A real Widgets application consuming the installed C++ product surface.
//
// This is deliberately not the lightweight package/link smoke in tests/consumer-installed-sdk: it builds an actual
// QMainWindow, drives the public RemoteAccess lifecycle on that window, proves the runtime reached Running while
// the event loop was pumped, and proves stop() returned the runtime to Stopped. What is under test is the installed
// SDK leading to a real target adapter and a real runtime startup - protocol qualification belongs to the runtime
// evidence that already exists.

#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.resize(480, 320);
    window.setWindowTitle(QStringLiteral("HyRemote C++ Widgets consumer"));
    window.show();

    HyRemote::RemoteAccess remote(&window);

    // The port is chosen by the evidence runner so a clean external consumer never has to know about the
    // repository's own test fixtures, and the application still only uses the public setter.
    bool portConfigured = false;
    const int configuredPort = qEnvironmentVariableIntValue("HYREMOTE_CPP_CONSUMER_PORT", &portConfigured);
    if (portConfigured) {
        if (!remote.setPort(static_cast<quint16>(configuredPort))) {
            std::fprintf(stderr, "FAIL: installed SDK refused setPort(%d)\n", configuredPort);
            return 2;
        }
    }

    if (remote.target() != &window) {
        std::fprintf(stderr, "FAIL: installed SDK did not retain the Widgets window as the capture target\n");
        return 3;
    }

    if (!remote.start()) {
        const auto error = remote.lastError();
        std::fprintf(stderr,
                     "FAIL: installed SDK refused to start on a Widgets target (code=%d)\n",
                     error ? static_cast<int>(error->code) : -1);
        return 4;
    }

    if (remote.state() != HyRemote::RemoteAccessState::Running) {
        std::fprintf(stderr, "FAIL: start() did not leave the runtime Running\n");
        remote.stop();
        return 5;
    }
    std::printf("HYREMOTE_CPP_WIDGETS_START=Running\n");
    std::printf("HYREMOTE_CPP_WIDGETS_PLATFORM=%s\n", qPrintable(QGuiApplication::platformName()));
    std::printf("HYREMOTE_CPP_WIDGETS_PORT=%u\n", static_cast<unsigned>(remote.port()));

    // Pump the real event loop while the session is Running: the Widgets adapter and the capture path are built on
    // this target, not asserted from configuration.
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
    std::printf("HYREMOTE_CPP_WIDGETS_STOP=Stopped\n");
    return 0;
}
