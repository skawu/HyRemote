#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>
#include <QWheelEvent>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

#include "detail/component_factories.hpp"
#include "detail/cursor_mapping.hpp"
#include "quick/quick_capture_backend.hpp"
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

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    testQuickRendererCapabilityRouting();
    testQuickFactoryAndOwnedFrame();
    testQuickQueuedRequestsCoalesceWithoutReplayingStalePixels();
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
