#include "rfb_test_client.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMenu>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QWidget>

#include <iostream>

namespace {
constexpr quint16 kPort = 5996;

// The native popup window is created and withdrawn asynchronously by the platform plugin, so
// "the popup has appeared / has withdrawn" has to be observed as a condition instead of being
// assumed true after a fixed delay. This bounded, event-driven wait keeps processing Qt events
// (so the very events that make the condition true can still be delivered) and gives up after
// `timeoutMs` rather than blocking for an arbitrary constant.
template <typename Condition>
bool waitForPopupCondition(Condition condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (condition())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return condition();
}

constexpr int kPopupMaterializeTimeoutMs = 5000;
constexpr int kPopupWithdrawTimeoutMs = 5000;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget primary;
    primary.setWindowTitle(QStringLiteral("HyRemote QPA popup primary"));
    primary.resize(320, 180);
    primary.move(100, 100);
    primary.show();

    QMenu menu;
    menu.setTitle(QStringLiteral("HyRemote remote popup"));
    menu.addAction(QStringLiteral("Inspect"));
    menu.addAction(QStringLiteral("Operate"));
    menu.addAction(QStringLiteral("Acknowledge"));

    int result = 1;
    QTimer::singleShot(350, &app, [&] {
        HyRemote::Qpa::Test::RfbTestClient viewer(kPort);
        if (!viewer.connectAndHandshake() || !viewer.requestFramebuffer()) {
            std::cerr << "FAIL: could not establish initial popup-test viewer session\n";
            app.quit();
            return;
        }
        const QSize primarySize = viewer.geometry().size();

        // Position far enough to the right that an independent native QWidget popup must expand
        // the application canvas. The actual post-show geometry is authoritative because native
        // window managers may clamp/reposition popups.
        menu.popup(QPoint(primary.x() + primary.width() + 80, primary.y() + 20));
        if (!waitForPopupCondition([&menu] { return menu.isVisible() && menu.isWindow(); },
                                   kPopupMaterializeTimeoutMs)) {
            std::cerr << "FAIL: QMenu did not materialize as a visible top-level popup within "
                      << kPopupMaterializeTimeoutMs << " ms\n";
            app.quit();
            return;
        }

        const QRect primaryGeometry(primary.mapToGlobal(QPoint(0, 0)), primary.size());
        const QRect popupGeometry(menu.mapToGlobal(QPoint(0, 0)), menu.size());
        const QSize popupCanvas = primaryGeometry.united(popupGeometry).size();
        if (popupCanvas == primarySize) {
            std::cerr << "FAIL: QMenu popup did not enlarge the test canvas; placement was not discriminating\n";
            app.quit();
            return;
        }

        if (!viewer.waitForGeometry(popupCanvas) || !viewer.connected()) {
            std::cerr << "FAIL: same viewer did not observe QWidget popup in composite canvas\n";
            app.quit();
            return;
        }

        menu.hide();
        if (!waitForPopupCondition([&menu] { return !menu.isVisible(); },
                                   kPopupWithdrawTimeoutMs)) {
            std::cerr << "FAIL: QMenu popup did not withdraw within "
                      << kPopupWithdrawTimeoutMs << " ms\n";
            app.quit();
            return;
        }
        if (!viewer.waitForGeometry(primary.size()) || !viewer.connected()) {
            std::cerr << "FAIL: popup removal changed/restarted the remote session\n";
            app.quit();
            return;
        }

        viewer.socket().disconnectFromHost();
        std::cout << "PASS: QWidget QMenu popup entered and left one live remote application session\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(15000, &app, [&] {
        std::cerr << "FAIL: QPA QWidget popup connection smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
