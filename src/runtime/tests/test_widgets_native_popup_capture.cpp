// #404 physical follow-up: a native Windows regression for a real, owned, root-overlapping popup.
//
// The physical RealVNC run found that a real QLineEdit standard context menu is admitted, is a real OS-level
// transient window and receives remote input, yet its pixels never reach the remote frame. The existing
// offscreen fixture covers a *synthetic* Qt::Popup placed outside the root, where the canvas grows - so
// "the framebuffer got bigger" could stand in for real popup pixels. This test removes that proxy:
//
//   * a real QMenu from QLineEdit::createStandardContextMenu(), not a synthetic popup widget;
//   * the menu lies completely inside the configured root, so the canvas geometry and the framebuffer size
//     are unchanged while the popup is open;
//   * success is a significant pixel difference in the menu region, and a return to the baseline after close.
//
// It needs a real window system, so it is capability-gated to a native Windows platform and is deliberately not
// registered with QT_QPA_PLATFORM=offscreen. The offscreen fixture stays as it is.

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <functional>
#include <iostream>
#include <optional>

#include "detail/component_factories.hpp"
#include "hyremote/core/frame.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 150)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return predicate();
}

// Mean absolute per-channel difference over a logical rectangle of the frame, in 0..255.
double meanAbsoluteDifference(const hyremote::RemoteFrame &frame,
                             const QRect &canvasRect,
                             qreal dpr,
                             const QImage &reference)
{
    const auto plane = frame.storage ? frame.storage->mapRead(0) : std::nullopt;
    if (!plane || !plane->data)
        return -1.0;

    const int x0 = qBound(0, qFloor(canvasRect.left() * dpr), static_cast<int>(frame.geometry.size.width) - 1);
    const int y0 = qBound(0, qFloor(canvasRect.top() * dpr), static_cast<int>(frame.geometry.size.height) - 1);
    const int x1 = qBound(0, qCeil(canvasRect.right() * dpr + 1.0), static_cast<int>(frame.geometry.size.width));
    const int y1 = qBound(0, qCeil(canvasRect.bottom() * dpr + 1.0), static_cast<int>(frame.geometry.size.height));

    double total = 0.0;
    std::size_t samples = 0;
    for (int y = y0; y < y1; ++y) {
        const auto *row = reinterpret_cast<const unsigned char *>(plane->data)
                          + static_cast<std::size_t>(y) * plane->stride;
        for (int x = x0; x < x1; ++x) {
            const auto *pixel = row + static_cast<std::size_t>(x) * 4U;
            const QColor referencePixel = reference.pixelColor(x, y);
            total += std::abs(static_cast<int>(pixel[0]) - referencePixel.red());
            total += std::abs(static_cast<int>(pixel[1]) - referencePixel.green());
            total += std::abs(static_cast<int>(pixel[2]) - referencePixel.blue());
            samples += 3;
        }
    }
    return samples == 0 ? -1.0 : total / static_cast<double>(samples);
}

QImage frameToImage(const hyremote::RemoteFrame &frame)
{
    const auto plane = frame.storage ? frame.storage->mapRead(0) : std::nullopt;
    if (!plane || !plane->data)
        return {};
    const QImage borrowed(reinterpret_cast<const uchar *>(plane->data),
                          static_cast<int>(frame.geometry.size.width),
                          static_cast<int>(frame.geometry.size.height),
                          static_cast<qsizetype>(plane->stride),
                          QImage::Format_RGBA8888_Premultiplied);
    return borrowed.copy();
}

void testNativeOverlappingStandardContextMenuCapture()
{
    HyRemote::detail::resetFactories();

    const QString platform = QGuiApplication::platformName();
    if (platform != QStringLiteral("windows")) {
        // Capability gate: a synthetic offscreen platform cannot exercise native popup capture. The offscreen
        // fixture in test_widgets_capture.cpp keeps covering the geometry/input/isolation side.
        std::cout << "SKIP: native overlapping popup capture needs the windows platform (platform="
                  << platform.toStdString() << ")\n";
        return;
    }

    QWidget root;
    root.resize(800, 600);
    root.move(60, 60);
    QPalette rootPalette = root.palette();
    rootPalette.setColor(QPalette::Window, QColor(24, 24, 28));
    root.setPalette(rootPalette);
    root.setAutoFillBackground(true);
    root.show();
    CHECK(pumpUntil([&] { return root.isVisible(); }));

    QLineEdit *field = new QLineEdit(QStringLiteral("Conveyor-01"), &root);
    field->setGeometry(40, 40, 300, 28);
    field->show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;

    std::optional<hyremote::RemoteFrame> received;
    int captureEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++captureEvents; }));

    const qreal dpr = qMax<qreal>(1.0, root.devicePixelRatioF());
    const auto requestFrame = [&](hyremote::CaptureRequestId id) -> bool {
        received.reset();
        hyremote::CaptureRequest request{id, hyremote::Clock::now()};
        if (!components.capture->requestFrame(request))
            return false;
        return pumpUntil([&] { return received.has_value(); });
    };

    // 4. Baseline frame with no popup.
    CHECK(requestFrame(1));
    CHECK(received.has_value());
    if (!received)
        return;
    const QImage baseline = frameToImage(*received);
    CHECK(!baseline.isNull());
    const std::uint32_t baselineWidth = received->geometry.size.width;
    const std::uint32_t baselineHeight = received->geometry.size.height;

    // 5-7. A real standard context menu, popped up completely inside the configured root.
    QMenu *menu = field->createStandardContextMenu();
    CHECK(menu != nullptr);
    if (!menu)
        return;
    // Qt::Popup makes it a real top-level popup window owned by the root: isWindow() true, footprint inside
    // the root, and therefore only the scoped surface model can bring its pixels into the composed frame.
    menu->setParent(&root, Qt::Popup);
    menu->adjustSize();

    QPoint menuGlobal = root.mapToGlobal(QPoint(120, 120));
    QRect menuGeometry(menuGlobal, menu->size());
    // Keep the menu fully inside the root so the canvas cannot grow while the popup is open.
    QRect rootGeometry(root.mapToGlobal(QPoint(0, 0)), root.size());
    if (!rootGeometry.contains(menuGeometry)) {
        menuGeometry.moveTo(qMax(rootGeometry.left(), qMin(menuGeometry.left(),
                                                           rootGeometry.right() - menuGeometry.width())),
                            qMax(rootGeometry.top(), qMin(menuGeometry.top(),
                                                          rootGeometry.bottom() - menuGeometry.height())));
        menuGlobal = menuGeometry.topLeft();
    }
    menu->popup(menuGlobal);
    CHECK(pumpUntil([&] { return menu->isVisible(); }));
    CHECK(menu->isVisible());
    CHECK(menu->isWindow());
    // Ownership is what the scoped surface model actually consumes: the QWidget parent chain when Qt keeps one,
    // otherwise the window-level transient parent that a real popup menu uses.
    const QWindow *menuWindow = menu->windowHandle();
    const QWindow *rootWindow = root.windowHandle();
    const bool parentedToRoot = menu->parentWidget() == &root;
    const bool transientOfRoot = menuWindow && rootWindow && menuWindow->transientParent() == rootWindow;
    const bool ownedByRoot = parentedToRoot || transientOfRoot || root.isAncestorOf(menu);
    std::cout << "     observed[native popup]: parentWidget-root=" << parentedToRoot
              << " transientParent-root=" << transientOfRoot
              << " widgetAncestor=" << root.isAncestorOf(menu) << '\n';
    CHECK(ownedByRoot);
    CHECK(rootGeometry.contains(QRect(menu->mapToGlobal(QPoint(0, 0)), menu->size())));
    std::cout << "     observed[native popup]: menu geometry=" << menu->size().width() << 'x'
              << menu->size().height() << " root=" << root.width() << 'x' << root.height()
              << " isWindow=" << menu->isWindow() << '\n';

    // 8-10. The framebuffer must not need to grow, yet the menu region must change.
    CHECK(requestFrame(2));
    CHECK(received.has_value());
    CHECK(captureEvents == 0);
    if (received) {
        CHECK(received->geometry.size.width == baselineWidth);
        CHECK(received->geometry.size.height == baselineHeight);

        const QRect menuOnCanvas = QRect(menu->mapToGlobal(QPoint(0, 0)), menu->size())
                                       .translated(-rootGeometry.topLeft());
        const QImage menuOpen = frameToImage(*received);
        CHECK(!menuOpen.isNull());
        const double openDifference = meanAbsoluteDifference(*received, menuOnCanvas, dpr, baseline);
        std::cout << "     observed[native popup]: framebuffer " << baselineWidth << 'x' << baselineHeight
                  << " (unchanged) | mean|open-baseline| over menu region = " << openDifference << '\n';
        CHECK(openDifference > 8.0);
    }

    // Closing the popup must restore the covered root pixels.
    menu->hide();
    CHECK(pumpUntil([&] { return !menu->isVisible(); }));
    CHECK(requestFrame(3));
    if (received) {
        const QRect menuOnCanvas = QRect(menuGeometry.topLeft(), menu->size())
                                       .translated(-rootGeometry.topLeft());
        const double closedDifference = meanAbsoluteDifference(*received, menuOnCanvas, dpr, baseline);
        std::cout << "     observed[native popup]: mean|closed-baseline| over menu region = "
                  << closedDifference << '\n';
        CHECK(closedDifference < 8.0);
    }

    menu->deleteLater();
    components.input->shutdown();
    QCoreApplication::processEvents();
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testNativeOverlappingStandardContextMenuCapture();

    std::cout << (failures == 0 ? "PASS" : "FAIL") << ": native overlapping popup capture (checks failed: "
              << failures << ")\n";
    return failures == 0 ? 0 : 1;
}
