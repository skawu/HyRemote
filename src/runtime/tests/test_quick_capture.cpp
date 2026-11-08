#include <QColor>
#include <QDebug>
#include <QFile>
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QTextStream>
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
#include <QQuickItemGrabResult>
#include <QSharedPointer>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <optional>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "detail/component_factories.hpp"
#include "detail/cursor_mapping.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

// Only the exact-Kit pilot uses this diagnostic check. The ordinary Quick
// regression neither installs a Qt message hook nor imposes native hits.
// A successful public-Qt grabToImage fallback MUST NOT be counted as a
// successful passive QRhi source optimization.
std::atomic<int> pilotNativeHits{0};
QtMessageHandler previousQtMessageHandler = nullptr;
void pilotQtMessageHandler(QtMsgType type, const QMessageLogContext &context,
                           const QString &message)
{
    constexpr const char *needle = "native_frames_published=";
    const int index = message.indexOf(QString::fromLatin1(needle));
    if (index >= 0) {
        bool ok = false;
        const int hits = message.mid(index + static_cast<int>(std::strlen(needle))).toInt(&ok);
        if (ok)
            pilotNativeHits.fetch_add(hits, std::memory_order_relaxed);
    }
    if (message.contains(QStringLiteral("HyRemote P3 native pilot:")))
        std::cerr << "P3_TRACE=" << message.toStdString() << '\n';
    if (previousQtMessageHandler)
        previousQtMessageHandler(type, context, message);
}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

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
        // Verify uncovered and translucent item pixels. A swapchain's
        // background is not automatically equivalent to a Quick item layer;
        // the pilot must match this reference before it may be promoted.
        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Clear);
        painter->fillRect(QRectF(2, 2, 7, 7), Qt::transparent);
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->fillRect(QRectF(12, 12, 8, 8), QColor(90, 150, 210, 96));
        painter->restore();
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
    if (qEnvironmentVariable("HYREMOTE_QUICK_NATIVE_FRAME") == "1"
        && qEnvironmentVariable("HYREMOTE_FORCE_QUICK_OPAQUE") != "1") {
        // The native pilot may only claim contentItem-equivalent images when
        // both item and swapchain clear to transparent premultiplied pixels.
        QSurfaceFormat format = window.format();
        format.setAlphaBufferSize(8);
        window.setFormat(format);
        window.setColor(Qt::transparent);
    }
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

    // Native Kit experiment only: keep actual scenegraph rendering active
    // while the Core capture request is outstanding. This forces an honest
    // afterRendering readback opportunity and still checks A/S/B pixels.
    // The normal product test remains unchanged without both opt-in gates.
    const int stagePreviousNativeHits = pilotNativeHits.load(std::memory_order_relaxed);
    int activeStage = 0;
    QTimer nativePaintPulse(&window);
    if (qEnvironmentVariable("HYREMOTE_QUICK_NATIVE_FRAME") == "1") {
        nativePaintPulse.setInterval(8);
        QObject::connect(&nativePaintPulse, &QTimer::timeout, &window,
                         [&] { content.setStage(activeStage); });
        nativePaintPulse.start();
    }

    for (int stage = 0; stage <= 2; ++stage) {
        activeStage = stage;
        // afterRendering is emitted only after the Quick scene graph has drawn
        // the stage; requestFrame is issued strictly after that confirmation.
        content.setStage(stage);
        const bool rendered = pumpUntil([&] {
            return completedPaintStage.load(std::memory_order_acquire) == stage;
        });
        CHECK(rendered);
        if (!rendered)
            break;

        QImage referenceImage;
        if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_HIT") == "1") {
            // A separate public Qt content-item grab supplies the full-pixel
            // golden reference for this stage. It is completed BEFORE the
            // candidate request; comparing to itself can never validate P3.
            const auto reference = window.contentItem()->grabToImage(
                QSize(window.width(), window.height()));
            CHECK(!reference.isNull());
            if (reference) {
                bool ready = false;
                QObject::connect(reference.data(), &QQuickItemGrabResult::ready,
                                 &window, [&] { ready = true; });
                CHECK(pumpUntil([&] { return ready; }));
                if (ready)
                    referenceImage = reference->image().convertToFormat(
                        QImage::Format_RGBA8888_Premultiplied);
            }
            CHECK(!referenceImage.isNull());
        }

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

        if (!referenceImage.isNull()) {
            CHECK(referenceImage.width() == static_cast<int>(width));
            CHECK(referenceImage.height() == static_cast<int>(height));
            if (referenceImage.width() == static_cast<int>(width)
                && referenceImage.height() == static_cast<int>(height)) {
                std::size_t pixelChannelDifferences = 0;
                unsigned int largestDelta = 0;
                for (std::size_t y = 0; y < height; ++y) {
                    const auto *actual = reinterpret_cast<const std::uint8_t *>(
                        plane->data + y * plane->stride);
                    const auto *reference = referenceImage.constScanLine(static_cast<int>(y));
                    for (std::size_t channel = 0; channel < width * 4U; ++channel) {
                        const unsigned int diff = static_cast<unsigned int>(
                            std::abs(static_cast<int>(actual[channel])
                                     - static_cast<int>(reference[channel])));
                        largestDelta = std::max(largestDelta, diff);
                        // GPU vs Qt QPainter may differ by up to two levels
                        // on blended pixels; larger changes (including alpha,
                        // upside-down rows and wrong clear colours) are real.
                        if (diff > 2U)
                            ++pixelChannelDifferences;
                    }
                }
                std::cout << "P3_PIXEL_PARITY_STAGE=" << stage
                          << " DIFFERENT_CHANNELS=" << pixelChannelDifferences
                          << " MAX_CHANNEL_DELTA=" << largestDelta << '\n';
                CHECK(pixelChannelDifferences == 0);
            }
        }

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
    nativePaintPulse.stop();
    components.capture->stop();
    if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_HIT") == "1") {
        const int stageHits = pilotNativeHits.load(std::memory_order_relaxed)
                              - stagePreviousNativeHits;
        std::cout << "P3_NATIVE_STAGE_PIXELS_DELIVERED=" << stageHits << '\n';
        CHECK(stageHits > 0); // no substituted public fallback may fake P3 PASS
    } else if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_FALLBACK") == "1") {
        const int stageHits = pilotNativeHits.load(std::memory_order_relaxed)
                              - stagePreviousNativeHits;
        std::cout << "P3_OPAQUE_FALLBACK_STAGE_NATIVE_HITS=" << stageHits << '\n';
        CHECK(stageHits == 0); // opaque Qt Quick window MUST preserve item-grab alpha
    }
}

// Native readbacks are asynchronous; Core may accept a second frame request
// while the first is waiting. The production adapter is not allowed to replace
// the first request's pending slot and silently consume an in-flight quota.
void testConcurrentQuickNativeRequestsKeepBothTerminals()
{
    if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_HIT") != "1")
        return;

    HyRemote::detail::resetFactories();
    QQuickWindow window;
    QSurfaceFormat format = window.format();
    format.setAlphaBufferSize(8);
    window.setFormat(format);
    window.setColor(Qt::transparent);
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);

    std::atomic<int> rendered{0};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window,
                     [&] { rendered.fetch_add(1, std::memory_order_relaxed); },
                     Qt::DirectConnection);
    QTimer pulse(&window);
    pulse.setInterval(8);
    QObject::connect(&pulse, &QTimer::timeout, &window, [&] { content.update(); });
    window.show();
    pulse.start();
    CHECK(pumpUntil([&] { return rendered.load(std::memory_order_relaxed) >= 3; }));

    auto parts = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(parts.supported && parts.capture);
    if (!parts.capture) {
        pulse.stop();
        return;
    }
    std::vector<hyremote::CaptureRequestId> completions;
    CHECK(parts.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId)
                completions.push_back(*frame.requestId);
        },
        [&](const hyremote::CaptureEvent &) {}));
    CHECK(parts.capture->requestFrame({910, hyremote::Clock::now()}));
    CHECK(parts.capture->requestFrame({911, hyremote::Clock::now()}));
    const bool complete = pumpUntil([&] { return completions.size() >= 2; });
    CHECK(complete);
    CHECK(completions.size() == 2U);
    CHECK(std::count(completions.begin(), completions.end(), 910U) == 1);
    CHECK(std::count(completions.begin(), completions.end(), 911U) == 1);
    pulse.stop();
    parts.capture->stop();
}

// #789/#609: paired A / Native / A CPU-cost diagnostic on exactly the
// same hosted Qt Kit. Uses a tiny scenegraph geometry update rather than
// QQuickPaintedItem's full-frame CPU painting, so the captured 1080p readback
// and additional offscreen renders are the dominant variable.
// The sample is advisory, NOT a target-board performance qualification.
class LightweightBenchItem final : public QQuickItem
{
public:
    explicit LightweightBenchItem(QQuickItem *parent) : QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
    }

    void tick()
    {
        m_colorEpoch.fetch_add(1, std::memory_order_release);
        update();
    }

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override
    {
        auto *node = static_cast<QSGGeometryNode *>(old);
        if (!node) {
            node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
            geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geometry);
            node->setFlag(QSGNode::OwnsGeometry);
            auto *material = new QSGFlatColorMaterial;
            node->setMaterial(material);
            node->setFlag(QSGNode::OwnsMaterial);
        }
        QSGGeometry::updateRectGeometry(node->geometry(), boundingRect());
        node->markDirty(QSGNode::DirtyGeometry);
        const auto epoch = m_colorEpoch.load(std::memory_order_acquire);
        auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
        material->setColor((epoch & 1U) ? QColor(20, 60, 160) : QColor(225, 30, 40));
        node->markDirty(QSGNode::DirtyMaterial);
        return node;
    }

private:
    std::atomic<unsigned int> m_colorEpoch{0};
};

double benchmarkProcessCpuMs()
{
#ifdef Q_OS_WIN
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        return -1.0;
    const auto ticks = [](FILETIME f) {
        return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32U)
               | static_cast<std::uint64_t>(f.dwLowDateTime);
    };
    return static_cast<double>(ticks(kernel) + ticks(user)) / 10000.0;
#else
    // This evidence run is Windows-only; do not label std::clock() as CPU
    // time on MSVC, where it is elapsed wall time.
    return -1.0;
#endif
}

struct PilotBenchResult
{
    bool ok = false;
    double cpuMs = 0.0;
    double wallMs = 0.0;
    // Reported on capture->stop(), includes 12 warmup + 48 measured frames.
    int actualNativeFrames = 0;
    int frames = 0;
    std::uint32_t physicalWidth = 0;
    std::uint32_t physicalHeight = 0;
    double requestP50Ms = 0.0;
    double requestP95Ms = 0.0;
};

PilotBenchResult runPilotBenchCase(bool nativeOptIn)
{
    qputenv("HYREMOTE_QUICK_NATIVE_FRAME", nativeOptIn ? "1" : "0");
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    QSurfaceFormat surface = window.format();
    surface.setAlphaBufferSize(8);
    window.setFormat(surface);
    window.setColor(Qt::transparent);
    window.resize(1920, 1080);
    LightweightBenchItem content(window.contentItem());
    content.setWidth(1920);
    content.setHeight(1080);

    std::atomic<int> rendered{0};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window,
                     [&] { rendered.fetch_add(1, std::memory_order_relaxed); },
                     Qt::DirectConnection);
    QTimer timer(&window);
    timer.setInterval(8);
    QObject::connect(&timer, &QTimer::timeout, &window, [&] { content.tick(); });
    window.show();
    timer.start();

    PilotBenchResult result;
    if (!pumpUntil([&] { return rendered.load(std::memory_order_acquire) >= 4; }, 250)) {
        CHECK(false && "benchmark scenegraph did not render");
        timer.stop();
        return result;
    }

    auto parts = HyRemote::detail::createTargetComponents(&window, false);
    if (!parts.capture) {
        CHECK(false && "benchmark capture factory unavailable");
        timer.stop();
        return result;
    }
    std::uint64_t servedRequest = 0;
    const std::uint32_t expectedWidth = static_cast<std::uint32_t>(
        qRound(window.width() * window.devicePixelRatio()));
    const std::uint32_t expectedHeight = static_cast<std::uint32_t>(
        qRound(window.height() * window.devicePixelRatio()));
    std::uint32_t observedWidth = 0;
    std::uint32_t observedHeight = 0;
    bool usablePixels = true;
    const bool started = parts.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId)
                servedRequest = static_cast<std::uint64_t>(*frame.requestId);
            observedWidth = frame.geometry.size.width;
            observedHeight = frame.geometry.size.height;
            usablePixels = usablePixels && frame.storage != nullptr
                && observedWidth == expectedWidth
                && observedHeight == expectedHeight;
        },
        [&](const hyremote::CaptureEvent &) {});
    CHECK(started);
    if (!started) {
        timer.stop();
        return result;
    }

    // Wait until the Runtime's native observer has had a real render pulse,
    // without requesting extra renders or warming up by calling grabToImage.
    const int beforeRender = rendered.load(std::memory_order_relaxed);
    CHECK(pumpUntil([&] {
        return rendered.load(std::memory_order_relaxed) >= beforeRender + 3;
    }, 250));

    // Warm both paths before recording CPU. A cold first public grab creates
    // Qt's offscreen layer and shader/storage caches; without warm-up a short
    // A/B/A run can report a false 4x A1/A2 spread.
    constexpr int warmupFrames = 12;
    for (int i = 0; i < warmupFrames; ++i) {
        const std::uint64_t id = static_cast<std::uint64_t>(11000 + i);
        CHECK(parts.capture->requestFrame({id, hyremote::Clock::now()}));
        if (!pumpUntil([&] { return servedRequest == id; }, 160)) {
            CHECK(false && "benchmark warmup capture did not complete");
            timer.stop();
            parts.capture->stop();
            return result;
        }
    }

    // 48 frames rather than 12 reduces the effect of Windows process-CPU
    // 15.625 ms clock quantization. Still an advisory hosted-renderer sample.
    const int beforeHits = pilotNativeHits.load(std::memory_order_acquire);
    const double cpuStart = benchmarkProcessCpuMs();
    const auto wallStart = std::chrono::steady_clock::now();
    constexpr int requestedFrames = 48;
    std::vector<double> requestLatencyMs;
    requestLatencyMs.reserve(requestedFrames);
    for (int i = 0; i < requestedFrames; ++i) {
        const std::uint64_t id = static_cast<std::uint64_t>(12000 + i);
        const auto admittedAt = std::chrono::steady_clock::now();
        CHECK(parts.capture->requestFrame({id, hyremote::Clock::now()}));
        if (!pumpUntil([&] { return servedRequest == id; }, 160)) {
            CHECK(false && "benchmark capture exceeded bounded completion");
            break;
        }
        const auto doneAt = std::chrono::steady_clock::now();
        requestLatencyMs.push_back(
            std::chrono::duration<double, std::milli>(doneAt - admittedAt).count());
        ++result.frames;
    }
    if (!requestLatencyMs.empty()) {
        std::sort(requestLatencyMs.begin(), requestLatencyMs.end());
        result.requestP50Ms = requestLatencyMs[(requestLatencyMs.size() - 1U) / 2U];
        // Nearest-rank P95, including a slow public-grab fallback if one occurs.
        result.requestP95Ms = requestLatencyMs[
            (requestLatencyMs.size() * 95U + 99U) / 100U - 1U];
    }
    const auto wallEnd = std::chrono::steady_clock::now();
    const double cpuEnd = benchmarkProcessCpuMs();
    result.physicalWidth = observedWidth;
    result.physicalHeight = observedHeight;
    result.wallMs = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();
    result.cpuMs = cpuStart < 0.0 || cpuEnd < cpuStart ? -1.0 : cpuEnd - cpuStart;
    timer.stop();
    parts.capture->stop(); // emits the exact native publication count
    result.actualNativeFrames =
        pilotNativeHits.load(std::memory_order_acquire) - beforeHits;
    result.ok = usablePixels && result.frames == requestedFrames
        && result.cpuMs >= 0.0 && result.wallMs > 0.0;
    std::cout << "BENCH_DIAGNOSTIC case=" << (nativeOptIn ? "native" : "public")
              << " requested=1920x1080"
              << " logical=" << window.width() << 'x' << window.height()
              << " dpr=" << window.devicePixelRatio()
              << " expected=" << expectedWidth << 'x' << expectedHeight
              << " observed=" << observedWidth << 'x' << observedHeight
              << " usable=" << (usablePixels ? 1 : 0)
              << " frames=" << result.frames
              << " cpu_ms=" << result.cpuMs << " wall_ms=" << result.wallMs
              << " native_hits=" << result.actualNativeFrames
              << " p50_latency_ms=" << result.requestP50Ms
              << " p95_latency_ms=" << result.requestP95Ms << '\n';
    CHECK(result.ok);
    if (nativeOptIn)
        CHECK(result.actualNativeFrames == result.frames + warmupFrames);
    else
        CHECK(result.actualNativeFrames == 0);
    return result;
}

void testQuickCaptureProcessCostAba()
{
    if (qEnvironmentVariable("HYREMOTE_P3_BENCH") != "1")
        return;

    // The previous one-off 48-frame run showed an apparent 10.9% CPU benefit
    // while A1 and A2 differed by 17.4%. That is not enough evidence to
    // promote an expensive private-QRhi path. Repeat three bracketed samples
    // with identical warmed Qt scenes and report both variability and P95
    // capture latency. Do not use a hosted Windows result to qualify EGLFS,
    // Qt5, a customer's GPU, or any production default.
    constexpr int repetitions = 3;
    constexpr int framesPerCase = 48;
    const QByteArray oldNative = qgetenv("HYREMOTE_QUICK_NATIVE_FRAME");
    std::vector<PilotBenchResult> publicA1;
    std::vector<PilotBenchResult> nativeResults;
    std::vector<PilotBenchResult> publicA2;
    publicA1.reserve(repetitions);
    nativeResults.reserve(repetitions);
    publicA2.reserve(repetitions);

    for (int cycle = 0; cycle < repetitions; ++cycle) {
        publicA1.push_back(runPilotBenchCase(false));
        nativeResults.push_back(runPilotBenchCase(true));
        publicA2.push_back(runPilotBenchCase(false));
        if (!publicA1.back().ok || !nativeResults.back().ok
            || !publicA2.back().ok) {
            qputenv("HYREMOTE_QUICK_NATIVE_FRAME", oldNative);
            return;
        }
    }
    qputenv("HYREMOTE_QUICK_NATIVE_FRAME", oldNative);

    double sumPublicA1Cpu = 0.0;
    double sumNativeCpu = 0.0;
    double sumPublicA2Cpu = 0.0;
    double sumPublicA1Wall = 0.0;
    double sumNativeWall = 0.0;
    double sumPublicA2Wall = 0.0;
    double maxBaselineSpread = 0.0;
    int nativeHitsIncludingWarmup = 0;
    bool resolutionMatched = true;
    bool exactRequestedResolution = true;
    const std::uint32_t commonWidth = publicA1.front().physicalWidth;
    const std::uint32_t commonHeight = publicA1.front().physicalHeight;
    std::vector<double> cpuRatios;
    cpuRatios.reserve(repetitions);
    std::ostringstream output;
    output << std::fixed << std::setprecision(4);
    output << "BENCH_CONTEXT=HOSTED_WINDOWS_QT6.8.3_D3D11_SYNTHETIC\n"
           << "BENCH_REQUESTED_FRAMEBUFFER=1920x1080\n"
           << "BENCH_OBSERVED_FRAMEBUFFER=" << commonWidth << 'x' << commonHeight << '\n'
           << "BENCH_DESIGN=3x_BRACKETED_PUBLIC_NATIVE_PUBLIC\n"
           << "BENCH_REPETITIONS=" << repetitions << '\n'
           << "BENCH_FRAME_COUNT_PER_CASE=" << framesPerCase << '\n'
           << "BENCH_WARMUP_PER_CASE=12\n";
    for (int i = 0; i < repetitions; ++i) {
        const auto &a = publicA1[i];
        const auto &n = nativeResults[i];
        const auto &z = publicA2[i];
        const double publicCpu = (a.cpuMs + z.cpuMs) / (2.0 * framesPerCase);
        const double nativeCpu = n.cpuMs / framesPerCase;
        const double ratio = publicCpu > 0.0 ? nativeCpu / publicCpu : -1.0;
        const double spread = publicCpu > 0.0
            ? std::abs(a.cpuMs - z.cpuMs) / framesPerCase / publicCpu : -1.0;
        // qInfo/native publication count is emitted on capture stop, so
        // the 12 warmup native hits belong to the same total.
        CHECK(n.actualNativeFrames == framesPerCase + 12);
        CHECK(ratio > 0.0 && spread >= 0.0);
        for (const PilotBenchResult *sample : {&a, &n, &z}) {
            resolutionMatched = resolutionMatched
                && sample->physicalWidth == commonWidth
                && sample->physicalHeight == commonHeight;
            exactRequestedResolution = exactRequestedResolution
                && sample->physicalWidth == 1920U
                && sample->physicalHeight == 1080U;
        }
        cpuRatios.push_back(ratio);
        maxBaselineSpread = std::max(maxBaselineSpread, spread);
        sumPublicA1Cpu += a.cpuMs;
        sumNativeCpu += n.cpuMs;
        sumPublicA2Cpu += z.cpuMs;
        sumPublicA1Wall += a.wallMs;
        sumNativeWall += n.wallMs;
        sumPublicA2Wall += z.wallMs;
        nativeHitsIncludingWarmup += n.actualNativeFrames;
        output << "BENCH_CYCLE_" << (i + 1)
               << "_PUBLIC_A1_CPU_MS=" << a.cpuMs << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_NATIVE_CPU_MS=" << n.cpuMs << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_PUBLIC_A2_CPU_MS=" << z.cpuMs << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_CPU_RATIO=" << ratio << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_BASELINE_SPREAD=" << spread << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_PUBLIC_A1_P95_MS=" << a.requestP95Ms << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_NATIVE_P95_MS=" << n.requestP95Ms << '\n'
               << "BENCH_CYCLE_" << (i + 1)
               << "_PUBLIC_A2_P95_MS=" << z.requestP95Ms << '\n';
    }
    std::sort(cpuRatios.begin(), cpuRatios.end());
    const double medianRatio = cpuRatios[cpuRatios.size() / 2U];
    const double publicCpuPerFrame =
        (sumPublicA1Cpu + sumPublicA2Cpu) / (2.0 * repetitions * framesPerCase);
    const double nativeCpuPerFrame = sumNativeCpu / (repetitions * framesPerCase);
    const bool stableReference = maxBaselineSpread <= 0.15
                                 && resolutionMatched;
    const bool consistentImprovement = std::all_of(
        cpuRatios.begin(), cpuRatios.end(), [](double ratio) { return ratio < 0.95; });
    output << "PUBLIC_A1_TOTAL_CPU_MS=" << sumPublicA1Cpu << '\n'
           << "NATIVE_TOTAL_CPU_MS=" << sumNativeCpu << '\n'
           << "PUBLIC_A2_TOTAL_CPU_MS=" << sumPublicA2Cpu << '\n'
           << "PUBLIC_A1_TOTAL_WALL_MS=" << sumPublicA1Wall << '\n'
           << "NATIVE_TOTAL_WALL_MS=" << sumNativeWall << '\n'
           << "PUBLIC_A2_TOTAL_WALL_MS=" << sumPublicA2Wall << '\n'
           << "NATIVE_TOTAL_HITS_INCLUDING_WARMUP=" << nativeHitsIncludingWarmup << '\n'
           << "NATIVE_MEASURED_HITS="
           << (nativeHitsIncludingWarmup - repetitions * 12) << '\n'
           << "BENCH_RESOLUTION_MATCHED_ACROSS_CASES="
           << (resolutionMatched ? "YES" : "NO") << '\n'
           << "BENCH_ACTUAL_1080P="
           << (exactRequestedResolution ? "YES" : "NO") << '\n'
           << "PUBLIC_ABA_CPU_MS_PER_FRAME=" << publicCpuPerFrame << '\n'
           << "NATIVE_CPU_MS_PER_FRAME=" << nativeCpuPerFrame << '\n'
           << "CPU_RATIO_MEDIAN_OF_BRACKETED_CYCLES=" << medianRatio << '\n'
           << "CPU_RATIO_RANGE=" << cpuRatios.front() << ',' << cpuRatios.back() << '\n'
           << "PUBLIC_ABA_MAX_BASELINE_SPREAD=" << maxBaselineSpread << '\n'
           << "BENCH_STABILITY=" << (stableReference
                                          ? "BASELINE_WITHIN_15_PERCENT"
                                          : "UNSTABLE_BASELINE") << '\n'
           << "BENCH_PERFORMANCE_SIGNAL="
           << (stableReference && consistentImprovement
                   ? "REPEATED_HOSTED_GAIN_AT_OBSERVED_RESOLUTION_ONLY"
                   : "INSUFFICIENT_FOR_GAIN_CLAIM") << '\n'
           << "BENCH_QUALIFICATION=ADVISORY_NOT_FIELD_PROOF\n"
           << "BENCH_RELEASE_GATE=NOT_QUALIFIED\n";

    const std::string report = output.str();
    std::cout << report;
    const QString filename = qEnvironmentVariable("HYREMOTE_P3_BENCH_OUTPUT");
    if (!filename.isEmpty()) {
        QFile file(filename);
        CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        if (file.isOpen()) {
            const QByteArray bytes = QByteArray::fromStdString(report);
            CHECK(file.write(bytes) == bytes.size());
        }
    }
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
    if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_HIT") == "1"
        || qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_FALLBACK") == "1")
        previousQtMessageHandler = qInstallMessageHandler(pilotQtMessageHandler);

    testQuickFactoryAndOwnedFrame();
    testCompletedQuickGrabsCancelTimeouts();
    testStagedQuickSourceCaptureFollowsPaintedScene();
    testConcurrentQuickNativeRequestsKeepBothTerminals();
    testQuickCaptureProcessCostAba();
    testDestroyedQuickTargetReportsTargetLost();
    testUnavailableQuickCaptureHasBoundedRetryAndRecovers();
    testQuickInputIsQueuedToWindow();
    testQueuedQuickInputIsDroppedWhenSinkIsDestroyed();

    HyRemote::detail::resetFactories();
    if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_FALLBACK") == "1")
        CHECK(pilotNativeHits.load(std::memory_order_acquire) == 0);
    if (previousQtMessageHandler)
        qInstallMessageHandler(previousQtMessageHandler);
    if (failures != 0)
        std::cerr << failures << " Quick adapter checks failed\n";
    return failures == 0 ? 0 : 1;
}
