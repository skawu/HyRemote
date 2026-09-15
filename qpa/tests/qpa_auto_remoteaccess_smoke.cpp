#include <QApplication>
#include <QByteArray>
#include <QElapsedTimer>
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

bool waitForListenerClosed()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, kPort);
        if (!socket.waitForConnected(100))
            return true;
        socket.abort();
        QCoreApplication::processEvents();
    }
    return false;
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

        window.hide();
        QCoreApplication::processEvents();
        QTimer::singleShot(250, &app, [&] {
            if (!waitForListenerClosed()) {
                std::cerr << "FAIL: listener remained open after the primary target was hidden\n";
                app.quit();
                return;
            }

            std::cout << "PASS: native QPA target -> shared RemoteAccess -> listener lifecycle\n";
            result = 0;
            app.quit();
        });
    });

    QTimer::singleShot(6000, &app, [&] {
        std::cerr << "FAIL: QPA automatic RemoteAccess smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
