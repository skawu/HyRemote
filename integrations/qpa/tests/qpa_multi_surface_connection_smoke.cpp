#include "rfb_test_client.hpp"

#include <QApplication>
#include <QDialog>
#include <QTimer>
#include <QWidget>

#include <iostream>

namespace {
constexpr quint16 kPort = 5997;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget primary;
    primary.setWindowTitle(QStringLiteral("HyRemote QPA multi-surface primary"));
    primary.resize(320, 180);
    primary.move(100, 100);
    primary.show();

    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("HyRemote QPA independent dialog"));
    dialog.resize(160, 100);
    dialog.move(470, 100);

    int result = 1;
    QTimer::singleShot(350, &app, [&] {
        HyRemote::Qpa::Test::RfbTestClient viewer(kPort);
        if (!viewer.connectAndHandshake() || !viewer.requestFramebuffer()) {
            std::cerr << "FAIL: could not establish initial RFB viewer session\n";
            app.quit();
            return;
        }
        const QSize primarySize = viewer.geometry().size();

        dialog.show();
        dialog.raise();
        dialog.activateWindow();
        HyRemote::Qpa::Test::pumpEvents(350);

        const QRect primaryGeometry(primary.mapToGlobal(QPoint(0, 0)), primary.size());
        const QRect dialogGeometry(dialog.mapToGlobal(QPoint(0, 0)), dialog.size());
        const QSize expandedSize = primaryGeometry.united(dialogGeometry).size();
        if (expandedSize == primarySize) {
            std::cerr << "FAIL: test dialog did not enlarge the application canvas\n";
            app.quit();
            return;
        }
        if (!viewer.waitForGeometry(expandedSize) || !viewer.connected()) {
            std::cerr << "FAIL: existing viewer connection did not survive dialog creation\n";
            app.quit();
            return;
        }

        primary.hide();
        HyRemote::Qpa::Test::pumpEvents(350);
        if (!viewer.waitForGeometry(dialog.size()) || !viewer.connected()) {
            std::cerr << "FAIL: existing viewer connection did not survive primary-window hide\n";
            app.quit();
            return;
        }

        primary.show();
        primary.raise();
        primary.activateWindow();
        dialog.hide();
        HyRemote::Qpa::Test::pumpEvents(350);
        if (!viewer.waitForGeometry(primary.size()) || !viewer.connected()) {
            std::cerr << "FAIL: existing viewer connection did not survive surface replacement\n";
            app.quit();
            return;
        }

        viewer.socket().disconnectFromHost();
        std::cout << "PASS: one RFB viewer connection survived primary/dialog surface churn\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(15000, &app, [&] {
        std::cerr << "FAIL: QPA multi-surface connection smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
