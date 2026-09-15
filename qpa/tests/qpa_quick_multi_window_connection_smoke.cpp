#include "rfb_test_client.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QRect>
#include <QTimer>
#include <QtQuick/QQuickWindow>

#include <iostream>

namespace {
constexpr quint16 kPort = 5995;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QQuickWindow primary;
    primary.setTitle(QStringLiteral("HyRemote QPA Quick primary"));
    primary.setColor(QColor(30, 90, 160));
    primary.setGeometry(100, 100, 300, 180);
    primary.show();

    QQuickWindow secondary;
    secondary.setTitle(QStringLiteral("HyRemote QPA Quick secondary"));
    secondary.setColor(QColor(180, 90, 30));
    secondary.setGeometry(440, 100, 160, 100);

    int result = 1;
    QTimer::singleShot(500, &app, [&] {
        HyRemote::Qpa::Test::RfbTestClient viewer(kPort);
        if (!viewer.connectAndHandshake() || !viewer.requestFramebuffer()) {
            std::cerr << "FAIL: could not establish initial Quick viewer session\n";
            app.quit();
            return;
        }
        if (viewer.geometry().size() != primary.size()) {
            std::cerr << "FAIL: primary QQuickWindow did not define the initial remote canvas\n";
            app.quit();
            return;
        }

        secondary.show();
        secondary.raise();
        secondary.requestActivate();
        HyRemote::Qpa::Test::pumpEvents(500);

        const QSize expanded = primary.geometry().united(secondary.geometry()).size();
        if (!viewer.waitForGeometry(expanded) || !viewer.connected()) {
            std::cerr << "FAIL: same viewer did not survive second QQuickWindow creation\n";
            app.quit();
            return;
        }

        primary.hide();
        HyRemote::Qpa::Test::pumpEvents(400);
        if (!viewer.waitForGeometry(secondary.size()) || !viewer.connected()) {
            std::cerr << "FAIL: same viewer did not survive Quick primary hide\n";
            app.quit();
            return;
        }

        primary.show();
        primary.raise();
        primary.requestActivate();
        secondary.hide();
        HyRemote::Qpa::Test::pumpEvents(400);
        if (!viewer.waitForGeometry(primary.size()) || !viewer.connected()) {
            std::cerr << "FAIL: same viewer did not survive Quick surface replacement\n";
            app.quit();
            return;
        }

        viewer.socket().disconnectFromHost();
        std::cout << "PASS: multiple QQuickWindows shared one live remote application session\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(18000, &app, [&] {
        std::cerr << "FAIL: QPA Quick multi-window connection smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
