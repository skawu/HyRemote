#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QWidget>
#include <QWheelEvent>

#include <functional>
#include <iostream>
#include <optional>

#include "detail/component_factories.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 100)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return predicate();
}

class InputProbeWidget final : public QWidget
{
public:
    explicit InputProbeWidget(QWidget *parent = nullptr, Qt::WindowFlags flags = {})
        : QWidget(parent, flags)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_InputMethodEnabled, true);
        setMouseTracking(true);
    }

    int mousePresses = 0;
    int mouseMoves = 0;
    int wheels = 0;
    int keyPresses = 0;
    int keyReleases = 0;
    QPointF lastMousePosition;
    QPoint lastAngleDelta;
    int lastKey = 0;
    Qt::KeyboardModifiers lastModifiers = Qt::NoModifier;
    QString committedText;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        ++mousePresses;
        lastMousePosition = event->position();
        lastModifiers = event->modifiers();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++mouseMoves;
        lastMousePosition = event->position();
        event->accept();
    }

    void wheelEvent(QWheelEvent *event) override
    {
        ++wheels;
        lastAngleDelta = event->angleDelta();
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPresses;
        lastKey = event->key();
        lastModifiers = event->modifiers();
        event->accept();
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        ++keyReleases;
        lastKey = event->key();
        lastModifiers = event->modifiers();
        event->accept();
    }

    void inputMethodEvent(QInputMethodEvent *event) override
    {
        committedText += event->commitString();
        event->accept();
    }
};

void setSolidBackground(QWidget &widget, const QColor &color)
{
    QPalette palette = widget.palette();
    palette.setColor(QPalette::Window, color);
    widget.setPalette(palette);
    widget.setAutoFillBackground(true);
}

void testWidgetsFactoryAndOwnedFrame()
{
    HyRemote::detail::resetFactories();

    QWidget target;
    target.resize(160, 90);
    target.show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&target, false);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input == nullptr);

    const hyremote::CaptureCapabilities caps = components.capture->capabilities();
    CHECK(caps.asynchronous);
    CHECK(caps.cpuReadable);
    CHECK(caps.cpuFormats.size() == 1);
    CHECK(caps.cpuFormats.front() == hyremote::PixelFormat::Rgba8888);

    std::optional<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    hyremote::CaptureRequest request;
    request.id = 7;
    request.requestTime = hyremote::Clock::now();
    CHECK(components.capture->requestFrame(request));

    // requestFrame() is asynchronous even when the caller happens to be the GUI thread.
    CHECK(!received.has_value());
    CHECK(pumpUntil([&] { return received.has_value(); }));
    CHECK(events == 0);

    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 7);
        CHECK(received->geometry.size.width > 0);
        CHECK(received->geometry.size.height > 0);
        CHECK(received->geometry.pixelFormat == hyremote::PixelFormat::Rgba8888);
        CHECK(received->geometry.alphaMode == hyremote::AlphaMode::Premultiplied);
        CHECK(received->geometry.planeCount == 1);
        CHECK(received->damage.kind == hyremote::DamageKind::FullFrame);
        CHECK(received->timing.requestTime.has_value());
        CHECK(received->timing.completionTime.has_value());
        CHECK(received->storage != nullptr);
        if (received->storage) {
            const auto plane = received->storage->mapRead(0);
            CHECK(plane.has_value());
            if (plane) {
                CHECK(plane->data != nullptr);
                CHECK(plane->bytes > 0);
                CHECK(plane->stride == static_cast<std::size_t>(received->geometry.size.width) * 4U);
            }
        }
    }

    // #162 criterion 4: DPR must be applied to the captured pixel dimensions exactly once, and the logical
    // geometry must stay the authoritative mapping input. This binary is registered a second time in
    // CMakeLists.txt with QT_SCALE_FACTOR=1.5, so a double application of DPR or a silently ignored DPR
    // fails here deterministically instead of only being visible on a scaled physical display.
    const qreal dpr = target.devicePixelRatioF();
    const QString expectedDpr = qEnvironmentVariable("HYREMOTE_EXPECT_DPR");
    if (!expectedDpr.isEmpty()) {
        CHECK(qAbs(dpr - expectedDpr.toDouble()) < 0.01);
    }
    if (received) {
        CHECK(received->geometry.size.width == qRound(160.0 * dpr));
        CHECK(received->geometry.size.height == qRound(90.0 * dpr));
    }

    // Resize transition: the same relation must hold for the new logical size, on the DPR in force.
    target.resize(320, 180);
    QCoreApplication::processEvents();
    received.reset();
    hyremote::CaptureRequest resized{11, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(resized));
    CHECK(pumpUntil([&] { return received.has_value(); }));
    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 11);
        CHECK(received->geometry.size.width == qRound(320.0 * dpr));
        CHECK(received->geometry.size.height == qRound(180.0 * dpr));
    }

    // Pointer delivery must stay in logical coordinates: a remote point expressed at the logical centre is
    // delivered as that logical point, never multiplied by dpr a second time.
    InputProbeWidget probe;
    probe.resize(200, 100);
    probe.show();
    QCoreApplication::processEvents();
    const QPointF logicalCentre(100.0, 50.0);
    const QPointF globalCentre = probe.mapToGlobal(logicalCentre);
    QMouseEvent move(QEvent::MouseMove, logicalCentre, globalCentre, Qt::NoButton, Qt::NoButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&probe, &move);
    CHECK(qAbs(probe.lastMousePosition.x() - logicalCentre.x()) < 0.5);
    CHECK(qAbs(probe.lastMousePosition.y() - logicalCentre.y()) < 0.5);
    CHECK(qAbs(probe.devicePixelRatioF() - dpr) < 0.01);
    probe.close();

    components.capture->stop();
}

void testOwnedTransientSurfaceCaptureInputAndIsolation()
{
    HyRemote::detail::resetFactories();

    InputProbeWidget root;
    root.resize(200, 120);
    root.move(100, 100);
    setSolidBackground(root, QColor(16, 16, 16));
    root.show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input != nullptr);

    std::optional<hyremote::RemoteFrame> received;
    int captureEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++captureEvents; }));

    const qreal dpr = qMax<qreal>(1.0, root.devicePixelRatioF());
    const auto requestFrame = [&](hyremote::CaptureRequestId id) {
        received.reset();
        hyremote::CaptureRequest request{id, hyremote::Clock::now()};
        CHECK(components.capture->requestFrame(request));
        CHECK(pumpUntil([&] { return received.has_value(); }));
    };

    requestFrame(20);
    CHECK(received.has_value());
    if (received) {
        CHECK(received->geometry.size.width == qRound(root.width() * dpr));
        CHECK(received->geometry.size.height == qRound(root.height() * dpr));
    }

    // Same-process is not ownership. This unrelated top-level is deliberately larger and far away;
    // if broad QApplication::topLevelWidgets() enumeration leaked into an explicit target, the
    // framebuffer would grow immediately.
    InputProbeWidget unrelated;
    unrelated.resize(260, 180);
    unrelated.move(500, 420);
    setSolidBackground(unrelated, QColor(210, 20, 20));
    unrelated.show();
    QCoreApplication::processEvents();

    requestFrame(21);
    if (received) {
        CHECK(received->geometry.size.width == qRound(root.width() * dpr));
        CHECK(received->geometry.size.height == qRound(root.height() * dpr));
    }

    // A real top-level Qt::Popup whose QWidget parent is the configured root is a bounded owned
    // transient. Place it outside the root so both capture and coordinate routing are discriminating.
    InputProbeWidget popup(&root, Qt::Popup);
    popup.resize(80, 50);
    popup.move(root.mapToGlobal(QPoint(root.width() + 30, 20)));
    setSolidBackground(popup, QColor(20, 210, 40));
    popup.show();
    QCoreApplication::processEvents();
    CHECK(popup.isWindow());
    CHECK(popup.parentWidget() == &root);
    CHECK(popup.isVisible());

    const QRect rootGeometry(root.mapToGlobal(QPoint(0, 0)), root.size());
    const QRect popupGeometry(popup.mapToGlobal(QPoint(0, 0)), popup.size());
    const QRect expectedCanvas = rootGeometry.united(popupGeometry);
    CHECK(expectedCanvas.width() > root.width() || expectedCanvas.height() > root.height());

    requestFrame(22);
    CHECK(captureEvents == 0);
    if (received) {
        CHECK(received->geometry.size.width == static_cast<std::uint32_t>(qCeil(expectedCanvas.width() * dpr)));
        CHECK(received->geometry.size.height == static_cast<std::uint32_t>(qCeil(expectedCanvas.height() * dpr)));

        // Prove pixels, not only geometry: sample the popup-only green surface from the composed
        // framebuffer. The unrelated red window is outside expectedCanvas and therefore unreachable.
        const auto plane = received->storage ? received->storage->mapRead(0) : std::nullopt;
        CHECK(plane.has_value());
        if (plane && plane->data) {
            const QPoint popupCentre = popupGeometry.center() - expectedCanvas.topLeft();
            const int px = qBound(0, qFloor((popupCentre.x() + 0.5) * dpr),
                                  static_cast<int>(received->geometry.size.width) - 1);
            const int py = qBound(0, qFloor((popupCentre.y() + 0.5) * dpr),
                                  static_cast<int>(received->geometry.size.height) - 1);
            const auto *row = reinterpret_cast<const unsigned char *>(plane->data)
                              + static_cast<std::size_t>(py) * plane->stride;
            const auto *pixel = row + static_cast<std::size_t>(px) * 4U;
            CHECK(pixel[1] > pixel[0] + 80U);
            CHECK(pixel[1] > pixel[2] + 80U);
        }

        const QPoint canvasPoint = popupGeometry.center() - expectedCanvas.topLeft();
        hyremote::InputEvent press;
        press.kind = hyremote::InputEventKind::PointerButton;
        press.sourceViewport = {received->geometry.size.width, received->geometry.size.height, 1.0F};
        press.x = static_cast<float>((canvasPoint.x() + 0.5) * dpr);
        press.y = static_cast<float>((canvasPoint.y() + 0.5) * dpr);
        press.button = hyremote::PointerButton::Left;
        press.pressed = true;
        components.input->post(press);
        CHECK(pumpUntil([&] { return popup.mousePresses == 1; }));
        CHECK(unrelated.mousePresses == 0);

        press.pressed = false;
        components.input->post(press);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    popup.hide();
    QCoreApplication::processEvents();
    requestFrame(23);
    if (received) {
        CHECK(received->geometry.size.width == qRound(root.width() * dpr));
        CHECK(received->geometry.size.height == qRound(root.height() * dpr));
    }

    components.input->shutdown();
    components.capture->stop();
    unrelated.close();
    root.close();
}

void testStopCancelsQueuedPublication()
{
    HyRemote::detail::resetFactories();

    QWidget target;
    target.resize(80, 40);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&target, false);
    CHECK(components.capture != nullptr);

    int frames = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++frames; },
        [](const hyremote::CaptureEvent &) {}));

    hyremote::CaptureRequest request{9, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(request));
    components.capture->stop();
    QCoreApplication::processEvents();
    CHECK(frames == 0);
}

void testDestroyedTargetReportsTargetLost()
{
    HyRemote::detail::resetFactories();

    auto *target = new QWidget;
    target->resize(80, 40);
    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(target, false);
    CHECK(components.capture != nullptr);

    int frames = 0;
    std::optional<hyremote::CaptureEvent> lastEvent;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++frames; },
        [&](const hyremote::CaptureEvent &event) { lastEvent = event; }));

    hyremote::CaptureRequest request{11, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(request));
    delete target;

    CHECK(pumpUntil([&] { return lastEvent.has_value(); }));
    CHECK(frames == 0);
    CHECK(lastEvent.has_value());
    if (lastEvent) {
        CHECK(lastEvent->code == hyremote::CaptureEventCode::TargetLost);
        CHECK(!lastEvent->recoverable);
    }
    components.capture->stop();
}

void testNormalizedInputIsQueuedAndDeliveredToChild()
{
    HyRemote::detail::resetFactories();

    QWidget root;
    root.resize(200, 100);
    InputProbeWidget probe(&root);
    probe.setGeometry(20, 10, 120, 60);
    root.show();
    probe.show();
    probe.setFocus();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input != nullptr);
    CHECK(components.error.isEmpty());

    hyremote::InputEvent press;
    press.kind = hyremote::InputEventKind::PointerButton;
    press.sourceViewport = {200U, 100U, 1.0F};
    press.x = 30.0F;
    press.y = 20.0F;
    press.button = hyremote::PointerButton::Left;
    press.pressed = true;
    press.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(press);

    // InputSink::post() must not synchronously execute GUI work.
    CHECK(probe.mousePresses == 0);
    CHECK(pumpUntil([&] { return probe.mousePresses == 1; }));
    CHECK(qAbs(probe.lastMousePosition.x() - 10.0) <= 1.0);
    CHECK(qAbs(probe.lastMousePosition.y() - 10.0) <= 1.0);
    CHECK(probe.lastModifiers.testFlag(Qt::ShiftModifier));

    hyremote::InputEvent wheel;
    wheel.kind = hyremote::InputEventKind::PointerScroll;
    wheel.sourceViewport = {200U, 100U, 1.0F};
    wheel.x = 30.0F;
    wheel.y = 20.0F;
    wheel.scrollY = -1.0F;
    components.input->post(wheel);
    CHECK(pumpUntil([&] { return probe.wheels == 1; }));
    CHECK(probe.lastAngleDelta.y() == -120);

    hyremote::InputEvent key;
    key.kind = hyremote::InputEventKind::Key;
    key.key = hyremote::KeyCode::A;
    key.pressed = true;
    key.modifiers = hyremote::modifierMask(hyremote::InputModifier::Control);
    components.input->post(key);
    CHECK(pumpUntil([&] { return probe.keyPresses == 1; }));
    CHECK(probe.lastKey == Qt::Key_A);
    CHECK(probe.lastModifiers.testFlag(Qt::ControlModifier));

    key.pressed = false;
    components.input->post(key);
    CHECK(pumpUntil([&] { return probe.keyReleases == 1; }));

    hyremote::InputEvent text;
    text.kind = hyremote::InputEventKind::Text;
    text.textUtf8 = "A";
    components.input->post(text);
    CHECK(pumpUntil([&] { return probe.committedText == QStringLiteral("A"); }));
}

void testQueuedInputIsDroppedWhenSinkIsDestroyed()
{
    HyRemote::detail::resetFactories();

    QWidget root;
    root.resize(100, 50);
    InputProbeWidget probe(&root);
    probe.setGeometry(0, 0, 100, 50);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    CHECK(components.input != nullptr);

    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::PointerMove;
    event.sourceViewport = {100U, 50U, 1.0F};
    event.x = 10.0F;
    event.y = 10.0F;
    components.input->post(event);
    components.input.reset();

    QCoreApplication::processEvents();
    CHECK(probe.mouseMoves == 0);
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testWidgetsFactoryAndOwnedFrame();
    testOwnedTransientSurfaceCaptureInputAndIsolation();
    testStopCancelsQueuedPublication();
    testDestroyedTargetReportsTargetLost();
    testNormalizedInputIsQueuedAndDeliveredToChild();
    testQueuedInputIsDroppedWhenSinkIsDestroyed();

    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Widgets adapter checks failed\n";
    return failures == 0 ? 0 : 1;
}
