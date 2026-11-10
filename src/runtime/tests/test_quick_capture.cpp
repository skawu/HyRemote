#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>
#include <QWheelEvent>
#include <QSysInfo>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#endif
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

#include "detail/component_factories.hpp"
#include "detail/cursor_mapping.hpp"
#include "quick/quick_capture_backend.hpp"
#include "quick/quick_capture_single_flight.hpp"
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

// The backend is data, not permission to try QtPrivate QRhi readback.
// This holds for a software-only Qt Quick plugin, RHI graphics APIs and
// uninitialized/unknown renderers. The route remains the documented Qt
// public grab with its normal bounded retry/failure semantics.
void testQuickRendererCapabilityRouting()
{
    using Api = QSGRendererInterface::GraphicsApi;
    using HyRemote::detail::QuickCaptureBackend;
    using HyRemote::detail::QuickCaptureRoute;
    using HyRemote::detail::selectQuickCaptureRoute;
    const struct {
        Api api;
        QuickCaptureBackend backend;
    } cases[] = {
        {Api::Unknown, QuickCaptureBackend::Unknown},
        {Api::Software, QuickCaptureBackend::Software},
        {Api::OpenGL, QuickCaptureBackend::OpenGL},
        {Api::Direct3D11, QuickCaptureBackend::Direct3D11},
        {Api::Direct3D12, QuickCaptureBackend::Direct3D12},
        {Api::Vulkan, QuickCaptureBackend::Vulkan},
        {Api::Metal, QuickCaptureBackend::Metal},
        {Api::OpenVG, QuickCaptureBackend::Other},
        {Api::Null, QuickCaptureBackend::Other},
    };
    for (const auto &scenario : cases) {
        const auto selection = selectQuickCaptureRoute(scenario.api);
        CHECK(selection.backend == scenario.backend);
        CHECK(selection.route == QuickCaptureRoute::PublicQtGrabToImage);
    }
    if (qEnvironmentVariableIsSet("HYREMOTE_EXPECT_QUICK_OPENGL")) {
        // The dedicated Linux Xvfb/Mesa lane must exercise a REAL OpenGL
        // scenegraph, rather than report success through software fallback.
        QQuickWindow window;
        window.resize(160, 90);
        window.show();
        QCoreApplication::processEvents();
        const auto *renderer = window.rendererInterface();
        CHECK(renderer != nullptr);
        if (renderer) {
            CHECK(renderer->graphicsApi() == Api::OpenGL);
            CHECK(selectQuickCaptureRoute(renderer->graphicsApi()).route
                  == QuickCaptureRoute::PublicQtGrabToImage);
        }
    }
}

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 400)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;

        // processEvents() alone does not wait for a future scene-graph/queued callback. Give the
        // platform event loop a bounded real-time slice on every iteration so Windows and Linux
        // exercise the same asynchronous contract instead of depending on immediate readiness.
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

class PaintedProbeItem final : public QQuickPaintedItem
{
public:
    explicit PaintedProbeItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void paint(QPainter *painter) override
    {
        painter->fillRect(boundingRect(), QColor(20, 120, 220));
    }
};

// #567/H4: a real QQuickPaintedItem can render an intentional intermediate
// A/S/B scene. Track which scene was actually painted, not a timer guess.
class StagedQuickProbeItem final : public QQuickPaintedItem
{
public:
    explicit StagedQuickProbeItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void setStage(int stage)
    {
        m_requestedStage.store(stage, std::memory_order_release);
        update();
    }

    int paintedStage() const { return m_paintedStage.load(std::memory_order_acquire); }

    void paint(QPainter *painter) override
    {
        const int stage = m_requestedStage.load(std::memory_order_acquire);
        const QRectF left(0.0, 0.0, width() / 2.0, height());
        const QRectF right(width() / 2.0, 0.0, width() / 2.0, height());
        const QColor a(20, 60, 160);
        const QColor b(225, 30, 40);
        painter->fillRect(left, stage >= 1 ? b : a);
        painter->fillRect(right, stage >= 2 ? b : a);
        m_paintedStage.store(stage, std::memory_order_release);
    }

private:
    std::atomic<int> m_requestedStage{0};
    std::atomic<int> m_paintedStage{-1};
};

class EventProbe final : public QObject
{
public:
    int mousePresses = 0;
    int mouseMoves = 0;
    int wheels = 0;
    int keyPresses = 0;
    int keyReleases = 0;
    int inputMethods = 0;
    QPointF lastLocal;
    QPoint lastAngleDelta;
    int lastKey = 0;
    Qt::KeyboardModifiers lastModifiers = Qt::NoModifier;
    QString committedText;

    void reset()
    {
        mousePresses = 0;
        mouseMoves = 0;
        wheels = 0;
        keyPresses = 0;
        keyReleases = 0;
        inputMethods = 0;
        lastLocal = {};
        lastAngleDelta = {};
        lastKey = 0;
        lastModifiers = Qt::NoModifier;
        committedText.clear();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++mousePresses;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = mouse->position();
#else
            lastLocal = mouse->localPos();
#endif
            lastModifiers = mouse->modifiers();
            break;
        }
        case QEvent::MouseMove: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++mouseMoves;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = mouse->position();
#else
            lastLocal = mouse->localPos();
#endif
            lastModifiers = mouse->modifiers();
            break;
        }
        case QEvent::Wheel: {
            auto *wheel = static_cast<QWheelEvent *>(event);
            ++wheels;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = wheel->position();
#else
            lastLocal = wheel->posF();
#endif
            lastAngleDelta = wheel->angleDelta();
            lastModifiers = wheel->modifiers();
            break;
        }
        case QEvent::KeyPress: {
            auto *key = static_cast<QKeyEvent *>(event);
            ++keyPresses;
            lastKey = key->key();
            lastModifiers = key->modifiers();
            break;
        }
        case QEvent::KeyRelease: {
            auto *key = static_cast<QKeyEvent *>(event);
            ++keyReleases;
            lastKey = key->key();
            lastModifiers = key->modifiers();
            break;
        }
        case QEvent::InputMethod: {
            auto *input = static_cast<QInputMethodEvent *>(event);
            ++inputMethods;
            committedText += input->commitString();
            break;
        }
        default:
            break;
        }
        return false;
    }
};

void testQuickFactoryAndOwnedFrame()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(160, 90);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    window.show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
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
    request.id = 21;
    request.requestTime = hyremote::Clock::now();
    CHECK(components.capture->requestFrame(request));
    CHECK(!received.has_value());
    CHECK(pumpUntil([&] { return received.has_value(); }));
    CHECK(events == 0);

    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 21);
        CHECK(received->geometry.size.width > 0);
        CHECK(received->geometry.size.height > 0);

        // #162 criterion 4, Quick half: the same "DPR applied exactly once" contract as the Widgets test.
        // Registered a second time with QT_SCALE_FACTOR=1.5 + HYREMOTE_EXPECT_DPR=1.5; the gate keeps a
        // plugin that silently stays at 1.0 from turning these assertions into decoration.
        const qreal dpr = window.devicePixelRatio();
        const QString expectedDpr = qEnvironmentVariable("HYREMOTE_EXPECT_DPR");
        if (!expectedDpr.isEmpty()) {
            CHECK(qAbs(dpr - expectedDpr.toDouble()) < 0.01);
        }
        CHECK(received->geometry.size.width == qRound(160.0 * dpr));
        CHECK(received->geometry.size.height == qRound(90.0 * dpr));

        // The same physical-to-logical mapping is required for the Quick cursor path.
        const std::uint32_t sourceWidth = static_cast<std::uint32_t>(qRound(160.0 * dpr));
        const std::uint32_t sourceHeight = static_cast<std::uint32_t>(qRound(90.0 * dpr));
        const QPointF mappedCentre = HyRemote::detail::mapFramebufferCursorPoint(
            static_cast<int>(sourceWidth / 2U),
            static_cast<int>(sourceHeight / 2U),
            sourceWidth,
            sourceHeight,
            160.0,
            90.0);
        CHECK(qAbs(mappedCentre.x() - 79.5) <= 0.6);
        CHECK(qAbs(mappedCentre.y() - 44.5) <= 0.6);
        const QPointF mappedEdge = HyRemote::detail::mapFramebufferCursorPoint(
            static_cast<int>(sourceWidth), static_cast<int>(sourceHeight), sourceWidth, sourceHeight, 160.0, 90.0);
        CHECK(qAbs(mappedEdge.x() - 159.0) < 0.01);
        CHECK(qAbs(mappedEdge.y() - 89.0) < 0.01);

        // Resize transition on the DPR in force.
        window.resize(320, 180);
        content.setWidth(320);
        content.setHeight(180);
        QCoreApplication::processEvents();
        received.reset();
        hyremote::CaptureRequest resizedRequest{22, hyremote::Clock::now()};
        CHECK(components.capture->requestFrame(resizedRequest));
        CHECK(pumpUntil([&] { return received.has_value(); }));
        if (received) {
            CHECK(received->requestId.has_value());
            CHECK(received->requestId.value() == 22);
            CHECK(received->geometry.size.width == qRound(320.0 * dpr));
            CHECK(received->geometry.size.height == qRound(180.0 * dpr));
        }
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

    components.capture->stop();
}

// Public Qt 5/6 API only: two Core requests queued before the same GUI event
// turn must share ONE actual offscreen grab and its immutable CPU storage,
// while each accepted request receives its own terminal identity and timing.
// A later page transition must cause a NEW image, never replay the old batch.
void testQuickQueuedRequestsCoalesceWithoutReplayingStalePixels()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++events; }));
    content.setStage(0);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 0; }));

    const auto firstTime = hyremote::Clock::now();
    const auto secondTime = hyremote::Clock::now();
    // Deliberately no processEvents between the two calls.
    CHECK(components.capture->requestFrame({701U, firstTime}));
    CHECK(components.capture->requestFrame({702U, secondTime}));
    CHECK(received.empty());
    CHECK(pumpUntil([&] { return received.size() == 2U; }));
    CHECK(events == 0);
    if (received.size() == 2U) {
        CHECK(received[0].requestId && *received[0].requestId == 701U);
        CHECK(received[1].requestId && *received[1].requestId == 702U);
        CHECK(received[0].timing.requestTime
              && *received[0].timing.requestTime == firstTime);
        CHECK(received[1].timing.requestTime
              && *received[1].timing.requestTime == secondTime);
        CHECK(received[0].storage && received[1].storage);
        CHECK(received[0].storage == received[1].storage);
        CHECK(received[0].timing.completionTime == received[1].timing.completionTime);
        CHECK(received[0].geometry.size.width == received[1].geometry.size.width);
        CHECK(received[0].geometry.size.height == received[1].geometry.size.height);

        if (received[0].storage) {
            const auto plane = received[0].storage->mapRead(0);
            CHECK(plane && plane->data);
            if (plane && plane->data) {
                const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                                   + (received[0].geometry.size.height / 2U) * plane->stride
                                   + (received[0].geometry.size.width / 4U) * 4U;
                CHECK(pixel[0] == 20U && pixel[1] == 60U && pixel[2] == 160U);
            }
        }
    }

    // A new, actually rendered scene after the prior batch has completed
    // must NOT borrow its source image or report stale page A as page B.
    content.setStage(2);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 2; }));
    CHECK(components.capture->requestFrame({703U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 3U; }));
    if (received.size() == 3U) {
        CHECK(received[2].requestId && *received[2].requestId == 703U);
        CHECK(received[2].storage != received[0].storage);
        const auto plane = received[2].storage ? received[2].storage->mapRead(0)
                                               : std::nullopt;
        CHECK(plane && plane->data);
        if (plane && plane->data) {
            const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                               + (received[2].geometry.size.height / 2U) * plane->stride
                               + (received[2].geometry.size.width / 4U) * 4U;
            CHECK(pixel[0] == 225U && pixel[1] == 30U && pixel[2] == 40U);
        }
    }
    CHECK(events == 0);
    components.capture->stop();
}

// The single-flight admission contract is independent of operating system,
// Qt Quick's rendering backend or a particular QQuickItemGrabResult timing.
// This model test fails if an in-flight batch accidentally becomes eligible
// for another grab when a later Core request is admitted.
void testQuickSingleFlightDispatchPolicy()
{
    HyRemote::detail::QuickCaptureSingleFlight gate;
    CHECK(gate.needsDispatch());
    gate.dispatchPosted();
    CHECK(!gate.needsDispatch());
    CHECK(gate.beginDispatchedGrab());
    CHECK(gate.grabInProgress());
    // The newer request may be queued but must NOT submit a second grab.
    CHECK(!gate.needsDispatch());
    gate.grabCompleted();
    CHECK(!gate.grabInProgress());
    CHECK(gate.needsDispatch());
    gate.dispatchPosted();
    CHECK(gate.beginDispatchedGrab());
    CHECK(!gate.needsDispatch());
    // Stop / restart invalidates both the old dispatch and active grab.
    gate.reset();
    CHECK(gate.needsDispatch());
    CHECK(!gate.grabInProgress());
}

// A later request in a different GUI turn must get a NEW Qt scene capture,
// but may not overtake the earlier asynchronous result. The old adapter
// began multiple offscreen grabs concurrently; scene/driver readiness could
// reorder completions and publish an older page after a newer page.
void testQuickDeferredRequestsKeepOrderAndFreshPixels()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();
    content.setStage(0);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 0; }));

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> frames;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    const auto firstTime = hyremote::Clock::now();
    CHECK(components.capture->requestFrame({811U, firstTime}));
    bool queuedSecondAccepted = false;
    const auto secondTime = hyremote::Clock::now();
    // The first GUI-dispatch event was queued BEFORE this callback. Thus the
    // second request is admitted in a distinct GUI turn while the first
    // grab may still be waiting for its asynchronous ready signal.
    CHECK(QMetaObject::invokeMethod(
        QCoreApplication::instance(), [&] {
            content.setStage(2);
            queuedSecondAccepted = components.capture->requestFrame(
                {812U, secondTime});
        }, Qt::QueuedConnection));
    CHECK(pumpUntil([&] { return frames.size() == 2U; }, 600));
    CHECK(queuedSecondAccepted);
    CHECK(events == 0);
    if (frames.size() == 2U) {
        CHECK(frames[0].requestId && *frames[0].requestId == 811U);
        CHECK(frames[1].requestId && *frames[1].requestId == 812U);
        CHECK(frames[0].timing.requestTime
              && *frames[0].timing.requestTime == firstTime);
        CHECK(frames[1].timing.requestTime
              && *frames[1].timing.requestTime == secondTime);
        CHECK(frames[0].storage && frames[1].storage);
        CHECK(frames[0].storage != frames[1].storage);
        const auto leftRgb = [](const hyremote::RemoteFrame &frame) {
            if (!frame.storage)
                return std::uint32_t(0);
            const auto plane = frame.storage->mapRead(0);
            if (!plane || !plane->data)
                return std::uint32_t(0);
            const auto *pixel = reinterpret_cast<const std::uint8_t *>(plane->data)
                                + (frame.geometry.size.height / 2U) * plane->stride
                                + (frame.geometry.size.width / 4U) * 4U;
            return (static_cast<std::uint32_t>(pixel[0]) << 16U)
                   | (static_cast<std::uint32_t>(pixel[1]) << 8U)
                   | static_cast<std::uint32_t>(pixel[2]);
        };
        // Qt grabToImage is asynchronous: submission at stage A does
        // not promise an A pixel snapshot. By the time its offscreen render
        // takes place the application may already have painted stage B.
        // Either is valid for request 811; request 812, whose NEW grab starts
        // strictly after 811 completes, must observe the updated B page.
        const auto firstRgb = leftRgb(frames[0]);
        CHECK(firstRgb == 0x143ca0U || firstRgb == 0xe11e28U);
        CHECK(leftRgb(frames[1]) == 0xe11e28U);
    }
    components.capture->stop();
}

// A hidden target exercises the same single-flight gate across bounded
// retry timers. Requests arriving during the previous retry cannot launch
// an independent Qt grab or hold a Core slot forever; every accepted request
// must have a terminal callback, then a visible target must recover.
void testQuickSingleFlightHiddenRetryQueuesAndDrains()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(96, 64);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(96);
    content.setHeight(64);
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> frames;
    int unavailableEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &event) {
            if (event.code == hyremote::CaptureEventCode::TemporarilyUnavailable)
                ++unavailableEvents;
        }));
    CHECK(components.capture->requestFrame({820U, hyremote::Clock::now()}));
    QCoreApplication::processEvents(); // first GUI batch enters bounded retry
    CHECK(components.capture->requestFrame({821U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return frames.size() == 2U; }, 600));
    CHECK(unavailableEvents == 2); // each independently retried batch
    if (frames.size() == 2U) {
        CHECK(frames[0].requestId && *frames[0].requestId == 820U);
        CHECK(frames[1].requestId && *frames[1].requestId == 821U);
        CHECK(!frames[0].storage && !frames[1].storage);
    }
    window.show();
    CHECK(components.capture->requestFrame({822U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return frames.size() == 3U; }, 500));
    if (frames.size() == 3U) {
        CHECK(frames[2].requestId && *frames[2].requestId == 822U);
        CHECK(frames[2].storage != nullptr);
    }
    components.capture->stop();
}

// A queued grab from a stopped capture lifetime must not complete using the
// handlers or framebuffer identity of a newly started lifetime.
void testQuickQueuedRequestsDoNotCrossStopRestart()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(120, 80);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(120);
    content.setHeight(80);
    window.show();
    QCoreApplication::processEvents();

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    int oldCallbacks = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++oldCallbacks; },
        [&](const hyremote::CaptureEvent &) { ++oldCallbacks; }));
    CHECK(components.capture->requestFrame({710U, hyremote::Clock::now()}));
    CHECK(components.capture->requestFrame({711U, hyremote::Clock::now()}));
    components.capture->stop(); // no GUI pump: both belong to old generation

    std::vector<hyremote::CaptureRequestId> newCallbacks;
    int newEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId)
                newCallbacks.push_back(*frame.requestId);
        },
        [&](const hyremote::CaptureEvent &) { ++newEvents; }));
    CHECK(components.capture->requestFrame({712U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return newCallbacks.size() == 1U; }));
    CHECK(oldCallbacks == 0);
    CHECK(newCallbacks.size() == 1U && newCallbacks[0] == 712U);
    CHECK(newEvents == 0);
    components.capture->stop();
}

// #609/#790: each successful asynchronous Qt Quick grab used to leave a
// two-second singleShot callback alive after the image was delivered. Verify
// the replacement cancels its owned timeout promptly instead of maintaining
// a growing set of dormant Qt timers for high-frequency captures.
void testCompletedQuickGrabsCancelTimeouts()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(128, 72);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(128);
    content.setHeight(72);
    window.show();
    QCoreApplication::processEvents();

    auto *dispatcher = QCoreApplication::instance();
    CHECK(dispatcher != nullptr);
    if (!dispatcher)
        return;
    // The grab deadlines have the dispatcher as their direct QObject parent.
    // Count after warmup to ignore unrelated Qt platform timers.
    const auto deadlineCount = [dispatcher] {
        return dispatcher->findChildren<QTimer *>(QString(),
                                                  Qt::FindDirectChildrenOnly).size();
    };
    const int baseline = deadlineCount();
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported && components.capture != nullptr);
    if (!components.capture)
        return;

    hyremote::CaptureRequestId latest = 0;
    int successes = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId && frame.storage) {
                latest = *frame.requestId;
                ++successes;
            }
        },
        [&](const hyremote::CaptureEvent &) {}));
    constexpr int count = 20;
    for (int index = 0; index < count; ++index) {
        const auto id = static_cast<hyremote::CaptureRequestId>(3000 + index);
        const bool accepted = components.capture->requestFrame(
            {id, hyremote::Clock::now()});
        CHECK(accepted);
        if (!accepted || !pumpUntil([&] { return latest == id; }))
            break;
        // Flush just Qt's deferred deletion, never wait 2s for the watchdog:
        // cancellation is part of the ready callback itself.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK(deadlineCount() <= baseline + 1);
    }
    CHECK(successes == count);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(deadlineCount() <= baseline + 1);
    components.capture->stop();
}

// H4 mechanism check: distinguish a source that really paints A/S/B from an
// adapter which invents mixed pixels. A and B are complete pages, while S is
// deliberately half of each, representing an application-staged transition.
// The Qt rendering signal is a stage barrier; no sleep is used for ordering.
void testStagedQuickSourceCaptureFollowsPaintedScene()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);

    std::atomic<int> completedPaintStage{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        completedPaintStage.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported && components.capture != nullptr);
    if (!components.capture)
        return;

    std::optional<hyremote::RemoteFrame> received;
    int captureEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++captureEvents; }));

    for (int stage = 0; stage <= 2; ++stage) {
        // afterRendering is emitted only after the Quick scene graph has drawn
        // the stage; requestFrame is issued strictly after that confirmation.
        content.setStage(stage);
        const bool rendered = pumpUntil([&] {
            return completedPaintStage.load(std::memory_order_acquire) == stage;
        });
        CHECK(rendered);
        if (!rendered)
            break;

        received.reset();
        hyremote::CaptureRequest request;
        request.id = static_cast<hyremote::CaptureRequestId>(500U + stage);
        request.requestTime = hyremote::Clock::now();
        CHECK(components.capture->requestFrame(request));
        const bool captured = pumpUntil([&] { return received.has_value(); });
        CHECK(captured);
        if (!captured)
            break;

        CHECK(received->requestId.has_value() && *received->requestId == request.id);
        CHECK(received->storage != nullptr);
        if (!received->storage)
            break;
        CHECK(received->geometry.pixelFormat == hyremote::PixelFormat::Rgba8888);
        CHECK(received->geometry.alphaMode == hyremote::AlphaMode::Premultiplied);
        const auto plane = received->storage->mapRead(0);
        CHECK(plane.has_value() && plane->data != nullptr);
        if (!plane || !plane->data)
            break;

        const auto width = static_cast<std::size_t>(received->geometry.size.width);
        const auto height = static_cast<std::size_t>(received->geometry.size.height);
        CHECK(width >= 4U && height >= 4U);
        CHECK(plane->stride >= width * 4U && height <= plane->bytes / plane->stride);
        if (width < 4U || height < 4U || plane->stride < width * 4U
            || height > plane->bytes / plane->stride)
            break;

        const auto sample = [&](std::size_t x) {
            const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                                + (height / 2U) * plane->stride + x * 4U;
            return (static_cast<std::uint32_t>(pixel[0]) << 16U)
                   | (static_cast<std::uint32_t>(pixel[1]) << 8U)
                   | static_cast<std::uint32_t>(pixel[2]);
        };
        constexpr std::uint32_t aRgb = 0x143ca0U;
        constexpr std::uint32_t bRgb = 0xe11e28U;
        CHECK(sample(width / 4U) == (stage >= 1 ? bRgb : aRgb));
        CHECK(sample(3U * width / 4U) == (stage >= 2 ? bRgb : aRgb));
    }
    CHECK(captureEvents == 0);
    components.capture->stop();
}

void testDestroyedQuickTargetReportsTargetLost()
{
    HyRemote::detail::resetFactories();

    auto *window = new QQuickWindow;
    window->resize(80, 40);
    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(window, false);
    CHECK(components.capture != nullptr);

    int frames = 0;
    std::optional<hyremote::CaptureEvent> lastEvent;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++frames; },
        [&](const hyremote::CaptureEvent &event) { lastEvent = event; }));

    hyremote::CaptureRequest request{22, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(request));
    delete window;

    CHECK(pumpUntil([&] { return lastEvent.has_value(); }));
    CHECK(frames == 0);
    CHECK(lastEvent.has_value());
    if (lastEvent) {
        CHECK(lastEvent->code == hyremote::CaptureEventCode::TargetLost);
        CHECK(!lastEvent->recoverable);
    }
    components.capture->stop();
}

void testUnavailableQuickCaptureHasBoundedRetryAndRecovers()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(120, 60);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(120);
    content.setHeight(60);
    // Intentionally leave the window hidden. The adapter accepts the asynchronous request, but a
    // permanently unavailable public Quick grab must not retain Core's in-flight slot forever.

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);

    std::optional<hyremote::RemoteFrame> received;
    std::optional<hyremote::CaptureEvent> lastEvent;
    int eventCount = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &event) {
            lastEvent = event;
            ++eventCount;
        }));

    hyremote::CaptureRequest hiddenRequest{31, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(hiddenRequest));
    CHECK(pumpUntil([&] { return received.has_value() && lastEvent.has_value(); }));
    CHECK(eventCount == 1);
    CHECK(lastEvent.has_value());
    if (lastEvent) {
        CHECK(lastEvent->code == hyremote::CaptureEventCode::TemporarilyUnavailable);
        CHECK(lastEvent->recoverable);
    }
    CHECK(received.has_value());
    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 31);
        CHECK(received->storage == nullptr);
        CHECK(received->geometry.size.width == 0);
        CHECK(received->geometry.size.height == 0);
        CHECK(received->timing.requestTime.has_value());
        CHECK(received->timing.completionTime.has_value());
    }

    // A later request must remain usable. Showing the same target after the bounded unavailable
    // completion proves the adapter did not enter a terminal state or keep retry state across requests.
    received.reset();
    lastEvent.reset();
    eventCount = 0;
    window.show();
    QCoreApplication::processEvents();
    hyremote::CaptureRequest recoveryRequest{32, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(recoveryRequest));
    CHECK(pumpUntil([&] { return received.has_value(); }));
    CHECK(eventCount == 0);
    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 32);
        CHECK(received->storage != nullptr);
        CHECK(received->geometry.size.width > 0);
        CHECK(received->geometry.size.height > 0);
    }

    components.capture->stop();
}

// If a same-turn Qt grab cannot complete after the finite retry budget,
// every accepted Core slot must receive exactly one terminal frame, not just
// the first request in the batch. A later visible grab remains recoverable.
void testQuickHiddenBatchCompletesEveryAcceptedRequest()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(96, 64);
    PaintedProbeItem item(window.contentItem());
    item.setWidth(96);
    item.setHeight(64);
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;

    std::vector<hyremote::RemoteFrame> received;
    int unavailable = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &event) {
            if (event.code == hyremote::CaptureEventCode::TemporarilyUnavailable)
                ++unavailable;
        }));
    // No GUI event processing: the hidden requests necessarily share a batch.
    CHECK(components.capture->requestFrame({720U, hyremote::Clock::now()}));
    CHECK(components.capture->requestFrame({721U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 2U; }));
    CHECK(unavailable == 1);
    if (received.size() == 2U) {
        CHECK(received[0].requestId && *received[0].requestId == 720U);
        CHECK(received[1].requestId && *received[1].requestId == 721U);
        CHECK(!received[0].storage && !received[1].storage);
        CHECK(received[0].timing.requestTime && received[1].timing.requestTime);
        CHECK(received[0].timing.completionTime && received[1].timing.completionTime);
    }

    window.show();
    QCoreApplication::processEvents();
    CHECK(components.capture->requestFrame({722U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 3U; }));
    if (received.size() == 3U) {
        CHECK(received[2].requestId && *received[2].requestId == 722U);
        CHECK(received[2].storage != nullptr);
    }
    CHECK(unavailable == 1);
    components.capture->stop();
}

void testQuickInputIsQueuedToWindow()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(200, 100);
    window.show();
    QCoreApplication::processEvents();

    EventProbe probe;
    window.installEventFilter(&probe);
    probe.reset();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input != nullptr);

    hyremote::InputEvent press;
    press.kind = hyremote::InputEventKind::PointerButton;
    press.sourceViewport = {200U, 100U, 1.0F};
    press.x = 50.0F;
    press.y = 25.0F;
    press.button = hyremote::PointerButton::Left;
    press.pressed = true;
    press.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(press);

    CHECK(probe.mousePresses == 0);
    CHECK(pumpUntil([&] { return probe.mousePresses == 1; }));
    CHECK(std::fabs(probe.lastLocal.x() - 50.0) <= 1.0);
    CHECK(std::fabs(probe.lastLocal.y() - 25.0) <= 1.0);
    CHECK(probe.lastModifiers.testFlag(Qt::ShiftModifier));

    hyremote::InputEvent wheel;
    wheel.kind = hyremote::InputEventKind::PointerScroll;
    wheel.sourceViewport = {200U, 100U, 1.0F};
    wheel.x = 50.0F;
    wheel.y = 25.0F;
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
    CHECK(pumpUntil([&] { return probe.inputMethods == 1; }));
    CHECK(probe.committedText == QStringLiteral("A"));
}

void testQueuedQuickInputIsDroppedWhenSinkIsDestroyed()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(100, 50);
    EventProbe probe;
    window.installEventFilter(&probe);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
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

// This is a process CPU clock, not a stopwatch. MSVC std::clock() returns
// elapsed wall time and would mislabel GUI event-loop waits as "CPU". Match
// the rootless CPU probe's OS clocks; unsupported OS builds can run ordinary
// Quick CTest but must not emit synthetic CPU qualification data.
bool quickProfileProcessCpuNs(std::uint64_t &out) noexcept
{
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return false;
    ULARGE_INTEGER kernelTicks{}, userTicks{};
    kernelTicks.LowPart = kernel.dwLowDateTime;
    kernelTicks.HighPart = kernel.dwHighDateTime;
    userTicks.LowPart = user.dwLowDateTime;
    userTicks.HighPart = user.dwHighDateTime;
    out = (kernelTicks.QuadPart + userTicks.QuadPart) * 100ULL;
    return true;
#elif defined(__linux__)
    timespec sample{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &sample) != 0
        || sample.tv_sec < 0 || sample.tv_nsec < 0
        || sample.tv_nsec >= 1000000000L)
        return false;
    out = static_cast<std::uint64_t>(sample.tv_sec) * 1000000000ULL
          + static_cast<std::uint64_t>(sample.tv_nsec);
    return true;
#else
    (void)out;
    return false; // no guessed CPU-time semantics for other OS toolchains
#endif
}

// Test the semantic distinction responsible for review P2 on a real
// Windows/Linux runner, not only that the OS function compiles. A 120ms
// sleep must advance wall time but not consume roughly 120ms of process CPU.
// Do this before creating any scenegraph rendering threads.
void testQuickProfileClockDoesNotMeasureSleepAsCpu()
{
#if defined(_WIN32) || defined(__linux__)
    std::uint64_t before = 0, after = 0;
    const bool okBefore = quickProfileProcessCpuNs(before);
    CHECK(okBefore);
    if (!okBefore)
        return;
    const auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    const auto elapsedWallMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const bool okAfter = quickProfileProcessCpuNs(after);
    CHECK(okAfter && after >= before);
    if (!okAfter || after < before)
        return;
    const double processCpuMs = static_cast<double>(after - before) / 1000000.0;
    CHECK(elapsedWallMs >= 100.0);
    CHECK(processCpuMs < elapsedWallMs * 0.70);
#endif
}

// Advisory, opt-in production PUBLIC Qt Quick capture baseline. Never runs
// as part of ordinary CTest: a hosted Xvfb/Mesa graphics environment cannot
// establish release CPU benefit, especially not for RK3588 EGLFS or D3D11
// private QRhi. Bracket one animated case with equivalent static A1/A2
// workloads so large host drift is visible, rather than averaging it away.
struct PublicQuickProfileCase
{
    QString name;
    int validFrames = 0;
    double processCpuMs = 0.0;
    double wallMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    int observedWidth = 0;
    int observedHeight = 0;
};

double profilePercentile(std::vector<double> times, double fraction)
{
    if (times.empty())
        return 0.0;
    std::sort(times.begin(), times.end());
    const auto position = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(times.size())));
    return times[std::min(times.size() - 1U,
                          position > 0U ? position - 1U : 0U)];
}

bool profileHasStagePixels(const hyremote::RemoteFrame &frame, int stage)
{
    if (!frame.storage || frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || frame.geometry.size.width < 8U || frame.geometry.size.height < 8U)
        return false;
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return false;
    const std::size_t width = frame.geometry.size.width;
    const std::size_t height = frame.geometry.size.height;
    if (plane->stride < width * 4U || height > plane->bytes / plane->stride)
        return false;

    const auto sample = [&](std::size_t x) {
        const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                            + (height / 2U) * plane->stride + x * 4U;
        return (static_cast<std::uint32_t>(pixel[0]) << 16U)
               | (static_cast<std::uint32_t>(pixel[1]) << 8U)
               | static_cast<std::uint32_t>(pixel[2]);
    };
    constexpr std::uint32_t a = 0x143ca0U;
    constexpr std::uint32_t b = 0xe11e28U;
    return sample(width / 4U) == (stage >= 1 ? b : a)
           && sample(3U * width / 4U) == (stage >= 2 ? b : a);
}

PublicQuickProfileCase runPublicQuickProfileCase(
    hyremote::CaptureSource &source, StagedQuickProbeItem &content,
    const std::atomic<int> &renderedStage, std::optional<hyremote::RemoteFrame> &received,
    QString name, bool animate, int &lastStage, std::uint64_t &nextRequestId)
{
    constexpr int warmup = 8;
    constexpr int measured = 24;
    using Clock = std::chrono::steady_clock;
    PublicQuickProfileCase result;
    result.name = std::move(name);
    std::vector<double> latencies;
    latencies.reserve(measured);
    std::uint64_t startCpuNs = 0;
    bool cpuClockValid = false;
    Clock::time_point startWall{};
    bool started = false;

    for (int index = -warmup; index < measured; ++index) {
        if (index == 0) {
            cpuClockValid = quickProfileProcessCpuNs(startCpuNs);
            CHECK(cpuClockValid);
            startWall = Clock::now();
            started = true;
        }
        // Public A1 and A2 are truly static after the first paint. Dynamic
        // switches A -> split -> B -> A and waits for the actual scene paint.
        const int stage = animate ? ((index + warmup) % 3) : 0;
        if (stage != lastStage) {
            content.setStage(stage);
            lastStage = stage;
            const bool painted = pumpUntil([&] {
                return renderedStage.load(std::memory_order_acquire) == stage;
            });
            CHECK(painted);
            if (!painted)
                break;
        }

        received.reset();
        const auto issuedAt = Clock::now();
        const auto id = ++nextRequestId;
        const bool accepted = source.requestFrame({id, hyremote::Clock::now()});
        CHECK(accepted);
        if (!accepted)
            break;
        const bool completed = pumpUntil([&] { return received.has_value(); }, 500);
        CHECK(completed);
        if (!completed)
            break;
        const auto completedAt = Clock::now();
        CHECK(received->requestId && *received->requestId == id);
        CHECK(profileHasStagePixels(*received, stage));
        if (!received->requestId || *received->requestId != id
            || !profileHasStagePixels(*received, stage))
            break;

        if (index >= 0) {
            ++result.validFrames;
            result.observedWidth = static_cast<int>(received->geometry.size.width);
            result.observedHeight = static_cast<int>(received->geometry.size.height);
            latencies.push_back(std::chrono::duration<double, std::milli>(
                                    completedAt - issuedAt).count());
        }
    }

    if (started) {
        std::uint64_t endCpuNs = 0;
        const bool cpuValid = cpuClockValid && quickProfileProcessCpuNs(endCpuNs)
                              && endCpuNs >= startCpuNs;
        CHECK(cpuValid);
        result.processCpuMs = cpuValid
                                  ? static_cast<double>(endCpuNs - startCpuNs) / 1000000.0
                                  : 0.0;
        result.wallMs = std::chrono::duration<double, std::milli>(
                            Clock::now() - startWall).count();
    }
    result.p50Ms = profilePercentile(latencies, 0.50);
    result.p95Ms = profilePercentile(latencies, 0.95);
    CHECK(result.validFrames == measured);
    return result;
}

QJsonObject publicProfileToJson(const PublicQuickProfileCase &sample)
{
    QJsonObject result;
    result.insert(QStringLiteral("name"), sample.name);
    result.insert(QStringLiteral("valid_frames"), sample.validFrames);
    result.insert(QStringLiteral("observed_width"), sample.observedWidth);
    result.insert(QStringLiteral("observed_height"), sample.observedHeight);
    result.insert(QStringLiteral("process_cpu_ms"), sample.processCpuMs);
    result.insert(QStringLiteral("wall_ms"), sample.wallMs);
    result.insert(QStringLiteral("request_p50_ms"), sample.p50Ms);
    result.insert(QStringLiteral("request_p95_ms"), sample.p95Ms);
    return result;
}

void runPublicQuickProfile()
{
    constexpr int requestedWidth = 1920;
    constexpr int requestedHeight = 1080;
    constexpr int measuredFrames = 24;
    constexpr double maxStableSpreadPercent = 15.0;
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.resize(requestedWidth, requestedHeight);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(requestedWidth);
    content.setHeight(requestedHeight);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();
    const auto visible = pumpUntil([&] {
        return window.isVisible() && painted.load(std::memory_order_acquire) >= 0;
    });
    CHECK(visible);
    if (!visible)
        return;

    const auto *renderer = window.rendererInterface();
    const auto selected = HyRemote::detail::selectQuickCaptureRoute(
        renderer ? renderer->graphicsApi() : QSGRendererInterface::Unknown);
    CHECK(selected.route == HyRemote::detail::QuickCaptureRoute::PublicQtGrabToImage);
    const QString backend = [&] {
        using B = HyRemote::detail::QuickCaptureBackend;
        switch (selected.backend) {
        case B::Software: return QStringLiteral("software");
        case B::OpenGL: return QStringLiteral("opengl");
        case B::Direct3D11: return QStringLiteral("d3d11");
        case B::Direct3D12: return QStringLiteral("d3d12");
        case B::Vulkan: return QStringLiteral("vulkan");
        case B::Metal: return QStringLiteral("metal");
        case B::Other: return QStringLiteral("other");
        default: return QStringLiteral("unknown");
        }
    }();
    const QString expectedBackend =
        qEnvironmentVariable("HYREMOTE_PUBLIC_PROFILE_EXPECT_BACKEND");
    if (!expectedBackend.isEmpty())
        CHECK(backend == expectedBackend);

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::optional<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    int lastStage = content.paintedStage();
    std::uint64_t nextRequestId = 50000U;
    const auto a1 = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_static_a1"), false, lastStage, nextRequestId);
    const auto dynamic = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_a_split_b_a"), true, lastStage, nextRequestId);
    const auto a2 = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_static_a2"), false, lastStage, nextRequestId);
    CHECK(events == 0);
    components.capture->stop();

    const double meanCpu = (a1.processCpuMs + a2.processCpuMs) / 2.0;
    const double cpuSpread = meanCpu > 0.0
        ? 100.0 * std::abs(a1.processCpuMs - a2.processCpuMs) / meanCpu
        : -1.0;
    const bool fullResolution = a1.observedWidth == requestedWidth
        && a1.observedHeight == requestedHeight
        && dynamic.observedWidth == requestedWidth
        && dynamic.observedHeight == requestedHeight
        && a2.observedWidth == requestedWidth
        && a2.observedHeight == requestedHeight;
    if (qEnvironmentVariableIsSet("HYREMOTE_EXPECT_PHYSICAL_1080P"))
        CHECK(fullResolution);
    CHECK(a1.validFrames == measuredFrames
          && dynamic.validFrames == measuredFrames
          && a2.validFrames == measuredFrames);
    CHECK(a1.processCpuMs > 0.0 && a2.processCpuMs > 0.0);
    const bool stable = cpuSpread >= 0.0 && cpuSpread <= maxStableSpreadPercent;

    // A failed pixel, stage, backend, source or clock assertion must never
    // leave an apparently qualified JSON artifact behind.
    if (failures != 0) {
        std::cerr << "QUICK_PUBLIC_PROFILE_INVALID=YES (no JSON evidence)\n";
        return;
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema"),
                    QStringLiteral("hyremote-quick-public-profile/1"));
    document.insert(QStringLiteral("context"),
                    QStringLiteral("HOSTED_SYNTHETIC_NOT_DEVICE_OR_NATIVE_GAIN"));
    document.insert(QStringLiteral("route"), QStringLiteral("public_grabToImage"));
    document.insert(QStringLiteral("qt"), QString::fromLatin1(qVersion()));
    document.insert(QStringLiteral("platform"), QSysInfo::prettyProductName());
    document.insert(QStringLiteral("renderer"), backend);
    document.insert(QStringLiteral("probe_mode"),
                    qEnvironmentVariable("HYREMOTE_PUBLIC_PROFILE_PROBE_MODE",
                                         QStringLiteral("unspecified")));
    document.insert(QStringLiteral("requested_width"), requestedWidth);
    document.insert(QStringLiteral("requested_height"), requestedHeight);
    document.insert(QStringLiteral("actual_1080p"), fullResolution);
    document.insert(QStringLiteral("warmup_per_case"), 8);
    document.insert(QStringLiteral("measured_per_case"), measuredFrames);
    document.insert(QStringLiteral("whole_process_cpu_not_gpu_time"), true);
    document.insert(QStringLiteral("source_sha"),
                    qEnvironmentVariable("GITHUB_SHA", QStringLiteral("unstamped")));
    document.insert(QStringLiteral("public_a1_a2_cpu_spread_percent"), cpuSpread);
    document.insert(QStringLiteral("stable_reference_max_spread_percent"),
                    maxStableSpreadPercent);
    document.insert(QStringLiteral("public_reference_stability"),
                    stable ? QStringLiteral("STABLE")
                           : QStringLiteral("UNSTABLE_BASELINE"));
    document.insert(QStringLiteral("native_performance_qualification"),
                    QStringLiteral("NOT_MEASURED_DO_NOT_PROMOTE"));
    QJsonArray cases;
    cases.append(publicProfileToJson(a1));
    cases.append(publicProfileToJson(dynamic));
    cases.append(publicProfileToJson(a2));
    document.insert(QStringLiteral("cases"), cases);
    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Compact);
    std::cout << "QUICK_PUBLIC_PROFILE_JSON=" << bytes.constData() << '\n';
    const QString destination =
        qEnvironmentVariable("HYREMOTE_QUICK_PUBLIC_PROFILE_FILE");
    if (!destination.isEmpty()) {
        QFile file(destination);
        const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        CHECK(opened);
        if (opened) {
            CHECK(file.write(bytes) == bytes.size());
            file.write("\n", 1);
        }
    }
    // An unstable hosted baseline is useful evidence, not a failing
    // correctness test and NEVER permission to cherry-pick a better run.
}

}  // namespace

// R3: actual Runtime CaptureSource, not a standalone glReadPixels benchmark.
// Four independent colored quadrants prove orientation, physical dimensions,
// source epoch and immutable storage; test normal-render tap, static fallback,
// coalesced Core request IDs, subsequent fresh frame and stop/restart.
class GlQuadrantProbeItem final : public QQuickPaintedItem
{
public:
    explicit GlQuadrantProbeItem(QQuickItem *parent) : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void setPhase(int phase)
    {
        m_requested.store(phase, std::memory_order_release);
        update();
    }

    static QColor expected(int phase, int quadrant)
    {
        const QColor colors[] = {
            QColor(224, 34, 45), QColor(30, 198, 64),
            QColor(36, 67, 218), QColor(236, 205, 24)
        };
        return colors[(quadrant + phase) % 4];
    }

    int paintedPhase() const { return m_painted.load(std::memory_order_acquire); }

    void paint(QPainter *painter) override
    {
        const int phase = m_requested.load(std::memory_order_acquire);
        const qreal halfW = width() / 2.0;
        const qreal halfH = height() / 2.0;
        for (int q = 0; q < 4; ++q) {
            const qreal x = (q & 1) ? halfW : 0.0;
            const qreal y = (q & 2) ? halfH : 0.0;
            painter->fillRect(QRectF(x, y, halfW, halfH),
                              expected(phase, q));
        }
        m_painted.store(phase, std::memory_order_release);
    }

private:
    std::atomic<int> m_requested{0};
    std::atomic<int> m_painted{-1};
};

static void checkGlQuadrantFrame(const hyremote::RemoteFrame &frame,
                                 std::uint64_t requestId, int phase,
                                 int width, int height)
{
    CHECK(frame.requestId && *frame.requestId == requestId);
    CHECK(frame.geometry.size.width == static_cast<std::uint32_t>(width));
    CHECK(frame.geometry.size.height == static_cast<std::uint32_t>(height));
    CHECK(frame.geometry.pixelFormat == hyremote::PixelFormat::Rgba8888);
    CHECK(frame.geometry.alphaMode == hyremote::AlphaMode::Premultiplied);
    CHECK(frame.damage.kind == hyremote::DamageKind::FullFrame);
    CHECK(frame.timing.ptsSource == hyremote::PtsSource::Completion);
    const auto plane = frame.storage ? frame.storage->mapRead(0) : std::nullopt;
    CHECK(plane && plane->data);
    if (!plane || !plane->data)
        return;
    CHECK(plane->stride >= static_cast<std::size_t>(width) * 4U);
    for (int q = 0; q < 4; ++q) {
        const int x = (q & 1) ? 3 * width / 4 : width / 4;
        const int y = (q & 2) ? 3 * height / 4 : height / 4;
        const std::size_t offset = static_cast<std::size_t>(y) * plane->stride
                                   + static_cast<std::size_t>(x) * 4U;
        CHECK(offset + 3 < plane->bytes);
        if (offset + 3 >= plane->bytes)
            continue;
        const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data + offset);
        const QColor expected = GlQuadrantProbeItem::expected(phase, q);
        const bool match =
            std::abs(int(pixel[0]) - expected.red()) <= 12
            && std::abs(int(pixel[1]) - expected.green()) <= 12
            && std::abs(int(pixel[2]) - expected.blue()) <= 12;
        if (!match) {
            std::cerr << "R3_FRAME_PIXEL_MISMATCH request=" << requestId
                      << " expected_phase=" << phase << " quadrant=" << q
                      << " actual_rgb=(" << int(pixel[0]) << ","
                      << int(pixel[1]) << "," << int(pixel[2]) << ")"
                      << " expected_rgb=(" << expected.red() << ","
                      << expected.green() << "," << expected.blue() << ")"
                      << " size=" << width << "x" << height << '\n';
        }
        CHECK(match);
    }
}

void testExperimentalGlNormalFrameSourceAndStaticFallback()
{
    HyRemote::detail::resetFactories();
    const int width = qEnvironmentVariable("HYREMOTE_GL_CAPTURE_1080P") == QLatin1String("1")
                          ? 1920 : 320;
    const int height = width == 1920 ? 1080 : 180;
    QQuickWindow window;
    window.resize(width, height);
    GlQuadrantProbeItem content(window.contentItem());
    content.setWidth(width);
    content.setHeight(height);
    std::atomic<int> rendered{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        rendered.store(content.paintedPhase(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();
    content.setPhase(0);
    CHECK(pumpUntil([&] { return rendered.load(std::memory_order_acquire) == 0; }));

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported && components.capture);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> frames;
    int backendEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++backendEvents; }));

    // Two accepted Core IDs in one GUI batch must be fulfilled from exactly
    // one *actual* normal render frame and share the same storage anchor.
    CHECK(components.capture->requestFrame({9001, hyremote::Clock::now()}));
    CHECK(components.capture->requestFrame({9002, hyremote::Clock::now()}));
    QTimer::singleShot(12, &window, [&] { content.setPhase(1); });
    CHECK(pumpUntil([&] { return frames.size() == 2U; }));
    CHECK(rendered.load(std::memory_order_acquire) == 1);
    if (frames.size() >= 2U) {
        CHECK(frames[0].storage && frames[0].storage == frames[1].storage);
        checkGlQuadrantFrame(frames[0], 9001, 1, width, height);
        checkGlQuadrantFrame(frames[1], 9002, 1, width, height);
    }

    // Static: no new afterFrameEnd. Accepted request must complete through
    // bounded public grab without inventing a new Render content timestamp.
    QEventLoop settle;
    QTimer::singleShot(180, &settle, &QEventLoop::quit);
    settle.exec();
    CHECK(components.capture->requestFrame({9003, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return frames.size() == 3U; }));
    if (frames.size() >= 3U) {
        CHECK(frames[2].storage != frames[0].storage);
        checkGlQuadrantFrame(frames[2], 9003, 1, width, height);
    }

    // A fresh source phase on a later request must never reuse previous
    // pixels, even if the Core changes its request ID.
    CHECK(components.capture->requestFrame({9004, hyremote::Clock::now()}));
    QTimer::singleShot(12, &window, [&] { content.setPhase(2); });
    CHECK(pumpUntil([&] { return frames.size() == 4U; }));
    if (frames.size() >= 4U) {
        CHECK(frames[3].storage != frames[2].storage);
        checkGlQuadrantFrame(frames[3], 9004, 2, width, height);
    }
    CHECK(backendEvents == 0);

    components.capture->stop();
    CHECK(!components.capture->requestFrame({9005, hyremote::Clock::now()}));
    frames.clear();
    // Start a fresh generation on the SAME source. No callback or request
    // identity from the prior generation may be published.
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++backendEvents; }));
    CHECK(components.capture->requestFrame({9006, hyremote::Clock::now()}));
    QTimer::singleShot(12, &window, [&] { content.setPhase(3); });
    CHECK(pumpUntil([&] { return frames.size() == 1U; }));
    if (frames.size() == 1U)
        checkGlQuadrantFrame(frames[0], 9006, 3, width, height);
    CHECK(backendEvents == 0);
    components.capture->stop();
    std::cout << "QUICK_GL_PRODUCT_PATH_TEST=PASS_IF_LOG_HAS_TAP_AND_FALLBACK"
              << " SIZE=" << width << "x" << height
              << " STAGES=4 ORIENTATION=4_QUADRANTS\n";
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    testQuickProfileClockDoesNotMeasureSleepAsCpu();
    if (qEnvironmentVariableIsSet("HYREMOTE_EXPECT_GL_NORMAL_FRAME_TAP")) {
        testExperimentalGlNormalFrameSourceAndStaticFallback();
        HyRemote::detail::resetFactories();
        return failures == 0 ? 0 : 1;
    }
    if (qEnvironmentVariableIsSet("HYREMOTE_QUICK_PUBLIC_PROFILE")) {
        runPublicQuickProfile();
        HyRemote::detail::resetFactories();
        return failures == 0 ? 0 : 1;
    }

    testQuickRendererCapabilityRouting();
    testQuickFactoryAndOwnedFrame();
    testQuickQueuedRequestsCoalesceWithoutReplayingStalePixels();
    testQuickSingleFlightDispatchPolicy();
    testQuickDeferredRequestsKeepOrderAndFreshPixels();
    testQuickSingleFlightHiddenRetryQueuesAndDrains();
    testQuickQueuedRequestsDoNotCrossStopRestart();
    testCompletedQuickGrabsCancelTimeouts();
    testStagedQuickSourceCaptureFollowsPaintedScene();
    testDestroyedQuickTargetReportsTargetLost();
    testUnavailableQuickCaptureHasBoundedRetryAndRecovers();
    testQuickHiddenBatchCompletesEveryAcceptedRequest();
    testQuickInputIsQueuedToWindow();
    testQueuedQuickInputIsDroppedWhenSinkIsDestroyed();

    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Quick adapter checks failed\n";
    return failures == 0 ? 0 : 1;
}
