#include <QApplication>
#include <QByteArray>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>

#include <iostream>

namespace {

constexpr quint16 kPort = 5998;

bool waitForRfbBanner()
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, kPort);
    if (!socket.waitForConnected(2000))
        return false;
    if (!socket.waitForReadyRead(2000))
        return false;
    const QByteArray banner = socket.read(12);
    return banner.startsWith("RFB 003.008");
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote QPA automatic RemoteAccess smoke"));
    window.resize(320, 180);
    window.show();

    int result = 1;
    QTimer::singleShot(250, &app, [&] {
        if (!waitForRfbBanner()) {
            std::cerr << "FAIL: transparent QPA mode did not auto-compose RemoteAccess\n";
            app.quit();
            return;
        }

        // QPA-04 intentionally supersedes the old single-primary behavior where hiding this one
        // target stopped the listener. Multi-surface persistence is qualified by the dedicated
        // connection-continuity E2E; this predecessor smoke remains focused on transparent start.
        std::cout << "PASS: qualified native QPA application auto-started shared RemoteAccess\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(6000, &app, [&] {
        std::cerr << "FAIL: QPA automatic RemoteAccess smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
