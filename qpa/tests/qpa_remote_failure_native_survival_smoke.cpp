#include <QApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTimer>
#include <QWidget>

#include <cstdio>

int main(int argc, char **argv)
{
    // Reserve an ephemeral loopback port before QPA initialization, then ask HyRemote to use the
    // exact occupied port. This deterministically forces RemoteAccess::start() to fail without
    // relying on a fixed-port collision or any external process.
    QTcpServer blocker;
    if (!blocker.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "FAIL: could not reserve loopback port for QPA remote-failure test\n");
        return 1;
    }

    const quint16 blockedPort = blocker.serverPort();
    const QByteArray platform = QByteArrayLiteral("hyremote:hyremote-port=")
                                + QByteArray::number(blockedPort);
    qputenv("QT_QPA_PLATFORM", platform);

    QApplication app(argc, argv);
    if (QApplication::platformName().compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0) {
        std::fprintf(stderr, "FAIL: HyRemote QPA plugin was not selected\n");
        return 2;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote QPA remote-failure native survival"));
    window.resize(320, 180);
    window.show();

    if (!window.windowHandle() || !window.windowHandle()->handle()) {
        std::fprintf(stderr,
                     "FAIL: native delegate did not create a platform window while remote port was occupied\n");
        return 3;
    }

    bool eventLoopTicked = false;
    QTimer::singleShot(0, &app, [&eventLoopTicked] { eventLoopTicked = true; });

    int result = 1;
    QTimer::singleShot(300, &app, [&] {
        if (!eventLoopTicked) {
            std::fprintf(stderr, "FAIL: native event loop stopped after remote listener failure\n");
            app.quit();
            return;
        }
        if (!blocker.isListening() || blocker.serverPort() != blockedPort) {
            std::fprintf(stderr, "FAIL: occupied port unexpectedly changed ownership\n");
            app.quit();
            return;
        }
        if (!window.isVisible() || !window.windowHandle() || !window.windowHandle()->handle()) {
            std::fprintf(stderr, "FAIL: native application surface did not survive remote bind failure\n");
            app.quit();
            return;
        }

        std::printf("PASS: native QPA application remains live when HyRemote remote listener cannot bind\n");
        result = 0;
        window.close();
        app.quit();
    });

    QTimer::singleShot(5000, &app, [&] {
        std::fprintf(stderr, "FAIL: QPA remote-failure native survival smoke timed out\n");
        app.quit();
    });

    app.exec();
    return result;
}
