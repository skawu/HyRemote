#include <QApplication>
#include <QGuiApplication>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        std::fprintf(stderr,
                     "FAIL: Generic Plugin changed native platform identity to '%s'\n",
                     qPrintable(QGuiApplication::platformName()));
        return 2;
    }

    QWidget window;
    window.resize(320, 200);
    window.show();

    QTcpSocket socket;
    int attempts = 0;
    QTimer retry;
    retry.setInterval(50);
    QObject::connect(&retry, &QTimer::timeout, &app, [&] {
        if (socket.state() == QAbstractSocket::ConnectedState) {
            retry.stop();
            app.exit(0);
            return;
        }
        if (++attempts > 60) {
            std::fprintf(stderr, "FAIL: HyRemote Generic Plugin listener did not become reachable\n");
            retry.stop();
            app.exit(3);
            return;
        }
        socket.abort();
        socket.connectToHost(QHostAddress::LocalHost, 5992);
    });
    retry.start();

    QTimer::singleShot(5000, &app, [&] {
        std::fprintf(stderr, "FAIL: Generic Plugin smoke test timed out\n");
        app.exit(4);
    });

    return app.exec();
}
