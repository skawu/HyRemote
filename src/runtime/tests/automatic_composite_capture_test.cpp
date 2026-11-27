#include "automatic/composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>

#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/storage.hpp"

namespace {

class SolidCapture final : public hyremote::CaptureSource
{
public:
    explicit SolidCapture(QRgb color, int pixelWidth = 2, int pixelHeight = 2, float dpr = 1.0F,
                          std::shared_ptr<const hyremote::FrameStorage> *storageObserver = nullptr)
        : m_color(color), m_pixelWidth(pixelWidth), m_pixelHeight(pixelHeight), m_dpr(dpr)
        , m_storageObserver(storageObserver)
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities caps;
        caps.asynchronous = true;
        caps.cpuReadable = true;
        caps.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return caps;
    }

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler) override
    {
        m_onFrame = std::move(onFrame);
        return true;
    }

    void stop() noexcept override { m_onFrame = {}; }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        if (!m_onFrame)
            return false;
        const int width = m_pixelWidth;
        const int height = m_pixelHeight;
        const std::size_t stride = static_cast<std::size_t>(width) * 4U;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(stride, height);
        if (!storage || !storage->mutablePlane(0))
            return false;

        auto *bytes = reinterpret_cast<unsigned char *>(storage->mutablePlane(0));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset = static_cast<std::size_t>(y) * stride
                                           + static_cast<std::size_t>(x) * 4U;
                bytes[offset + 0] = static_cast<unsigned char>(qRed(m_color));
                bytes[offset + 1] = static_cast<unsigned char>(qGreen(m_color));
                bytes[offset + 2] = static_cast<unsigned char>(qBlue(m_color));
                bytes[offset + 3] = static_cast<unsigned char>(qAlpha(m_color));
            }
        }

        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(width),
                               static_cast<std::uint32_t>(height)};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.geometry.devicePixelRatio = m_dpr;
        frame.storage = std::move(storage);
        if (m_storageObserver)
            *m_storageObserver = frame.storage;
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        m_onFrame(std::move(frame));
        return true;
    }

private:
    QRgb m_color;
    int m_pixelWidth;
    int m_pixelHeight;
    float m_dpr;
    std::shared_ptr<const hyremote::FrameStorage> *m_storageObserver;
    hyremote::FrameReadyHandler m_onFrame;
};

// Legal std::function target with a potentially allocating/throwing copy.
// The composite must not copy it on a zero-viewer cancellation path: that
// would drop Core's terminal request notification under memory pressure.
struct ThrowOnCopyFrameHandler
{
    explicit ThrowOnCopyFrameHandler(std::vector<hyremote::RemoteFrame> *frames)
        : frames(frames) {}

    ThrowOnCopyFrameHandler(const ThrowOnCopyFrameHandler &other)
        : frames(other.frames)
    {
        throw std::bad_alloc();
    }

    ThrowOnCopyFrameHandler(ThrowOnCopyFrameHandler &&) noexcept = default;

    void operator()(hyremote::RemoteFrame frame) const
    {
        frames->push_back(std::move(frame));
    }

    std::vector<hyremote::RemoteFrame> *frames;
};

// Hold one child's completion while another child is removed. This exercises
// a real accepted composite request, not a synthetic call to the model.
class DeferredSolidCapture final : public hyremote::CaptureSource
{
public:
    explicit DeferredSolidCapture(std::function<void()> *release)
        : m_solid(qRgba(0, 0, 255, 128), 2, 2), m_release(release)
    {
    }

    ~DeferredSolidCapture() override
    {
        if (m_release)
            *m_release = {};
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        return m_solid.capabilities();
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        return m_solid.start(std::move(onFrame), std::move(onEvent));
    }

    void stop() noexcept override
    {
        m_request.reset();
        m_solid.stop();
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        m_request = request;
        if (m_release)
            *m_release = [this] { deliver(); };
        return true;
    }

private:
    void deliver()
    {
        if (!m_request)
            return;
        auto request = *m_request;
        m_request.reset();
        m_solid.requestFrame(request);
    }

    SolidCapture m_solid;
    std::function<void()> *m_release;
    std::optional<hyremote::CaptureRequest> m_request;
};

// Emits a late teardown diagnostic to exercise retired child callbacks.
// A replaced adapter must not report failures for its new SurfaceId owner.
class StopEventCapture final : public hyremote::CaptureSource
{
public:
    StopEventCapture(QRgb color, std::function<void()> &emitLateError)
        : m_solid(color, 4, 4), m_emitLateError(emitLateError) {}

    hyremote::CaptureCapabilities capabilities() const override
    {
        return m_solid.capabilities();
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        m_onEvent = std::move(onEvent);
        m_emitLateError = [this] { emitError(); };
        return m_solid.start(std::move(onFrame), {});
    }

    void stop() noexcept override
    {
        m_solid.stop();
        emitError();
        m_onEvent = {};
        m_emitLateError = {};
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        return m_solid.requestFrame(request);
    }

private:
    void emitError()
    {
        if (m_onEvent) {
            hyremote::CaptureEvent event;
            event.code = hyremote::CaptureEventCode::BackendFailure;
            event.message = "source diagnostic";
            event.recoverable = false;
            m_onEvent(event);
        }
    }

    SolidCapture m_solid;
    hyremote::CaptureEventHandler m_onEvent;
    std::function<void()> &m_emitLateError;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForFrame(std::vector<hyremote::RemoteFrame> &frames, std::size_t count)
{
    QElapsedTimer timer;
    timer.start();
    while (frames.size() < count && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return frames.size() >= count;
}

QImage imageView(const hyremote::RemoteFrame &frame)
{
    if (!frame.storage)
        return {};
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return {};
    return QImage(reinterpret_cast<const uchar *>(plane->data),
                  static_cast<int>(frame.geometry.size.width),
                  static_cast<int>(frame.geometry.size.height),
                  static_cast<qsizetype>(plane->stride),
                  QImage::Format_RGBA8888_Premultiplied);
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using HyRemote::Runtime::Automatic::CompositeTarget;
    QObject leftTarget;
    QObject rightTarget;
    CompositeTarget composite;
    composite.upsertSurface(1, &leftTarget, QRect(-2, 0, 2, 2), true);
    composite.upsertSurface(2, &rightTarget, QRect(0, 0, 2, 2), true);

    // The RFB bootstrap and idle-release permission must use the composite
    // OUTPUT grid, not child native DPR or absolute window coordinates.
    CompositeTarget noVisibleCanvas;
    if (!check(composite.initialFramebufferPixelSize() == QSize(4, 2),
               "composite reports visible output pixel dimensions")
        || !check(noVisibleCanvas.initialFramebufferPixelSize().isEmpty(),
                  "empty composite does not claim a framebuffer")) {
        return 1;
    }

    const auto childReleases = std::make_shared<std::atomic<int>>(0);
    const HyRemote::detail::BuiltinTargetResolver resolver =
        [&](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            if (remoteInputEnabled)
                return result;
            if (target == &leftTarget) {
                result.supported = true;
                // High-DPI child: four physical pixels per logical two-unit surface.
                result.capture = std::make_unique<SolidCapture>(qRgba(255, 0, 0, 255),
                                                                 4, 4, 2.0F);
            } else if (target == &rightTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(0, 255, 0, 255));
            }
            return result;
        };

    auto components = composite.createTargetComponents(false, resolver);
    if (!check(components.supported && components.capture, "automatic composite creates capture source"))
        return 1;

    std::vector<hyremote::RemoteFrame> frames;
    if (!check(components.capture->start(
                   [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "automatic composite capture starts")) {
        return 2;
    }

    if (!check(components.capture->requestFrame({1, hyremote::Clock::now()}), "frame request accepted")
        || !check(waitForFrame(frames, 1), "composite frame arrives")) {
        return 3;
    }

    const QImage image = imageView(frames.front());
    // #806 regression: left capture is 4x4 physical pixels with DPR=2 and
    // right capture is 2x2 with DPR=1; both are rendered into a 4x2 logical
    // output. A max(child DPR)=2 result wrongly enlarges watermark font pixels.
    if (!check(frames.front().geometry.devicePixelRatio == 1.0F,
               "composite output DPR follows its logical canvas, not its 2x child")
        || !check(image.size() == QSize(4, 2), "surface union becomes one logical canvas")
        || !check(image.pixelColor(0, 0) == QColor(255, 0, 0, 255), "left surface is composed")
        || !check(image.pixelColor(3, 0) == QColor(0, 255, 0, 255), "right surface is composed")) {
        return 4;
    }

    // #1086: hiding every surface must clear pixels without collapsing the
    // framebuffer to 1x1; restoring visibility must publish genuinely new pixels.
    composite.setSurfaceVisible(1, false);
    composite.setSurfaceVisible(2, false);
    frames.clear();
    if (!check(components.capture->requestFrame({2, hyremote::Clock::now()}),
               "hidden composite accepts refresh")
        || !check(waitForFrame(frames, 1), "hidden composite publishes clear frame")) {
        components.capture->stop();
        return 5;
    }
    const QImage cleared = imageView(frames.front());
    bool fullyCleared = cleared.size() == QSize(4, 2);
    for (int y = 0; fullyCleared && y < cleared.height(); ++y) {
        for (int x = 0; x < cleared.width(); ++x)
            fullyCleared = cleared.pixelColor(x, y) == QColor(0, 0, 0, 0);
    }
    if (!check(fullyCleared, "hidden composite clears every pixel at the last valid size")
        || !check(frames.front().requestId == hyremote::CaptureRequestId{2}
                      && frames.front().damage.kind == hyremote::DamageKind::FullFrame,
                  "hidden clear frame carries fresh request identity and full damage")) {
        components.capture->stop();
        return 6;
    }

    composite.setSurfaceVisible(1, true);
    composite.setSurfaceVisible(2, true);
    frames.clear();
    if (!check(components.capture->requestFrame({3, hyremote::Clock::now()}),
               "restored composite accepts refresh")
        || !check(waitForFrame(frames, 1), "restored composite publishes new frame")) {
        components.capture->stop();
        return 7;
    }
    const QImage restored = imageView(frames.front());
    if (!check(restored.size() == QSize(4, 2)
                   && restored.pixelColor(0, 0) == QColor(255, 0, 0, 255)
                   && restored.pixelColor(3, 0) == QColor(0, 255, 0, 255)
                   && frames.front().requestId == hyremote::CaptureRequestId{3},
               "restored composite publishes fresh visible pixels")) {
        components.capture->stop();
        return 8;
    }

    components.capture->stop();
    std::cout << "PASS: automatic runtime composes multiple application surfaces into one capture target\n";

    // #609: one full-canvas, DPR1 premultiplied child is already the exact
    // composite output: reuse its immutable FrameStorage without allocating,
    // clearing or drawing a second full-size picture. Verify semi-transparent
    // premultiplied samples, negative-origin canvas and per-request metadata.
    QObject singleTarget;
    CompositeTarget single;
    single.upsertSurface(3, &singleTarget, QRect(-7, 3, 3, 2), true);
    std::shared_ptr<const hyremote::FrameStorage> childStorage;
    const HyRemote::detail::BuiltinTargetResolver singleResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (!input && target == &singleTarget) {
                result.supported = true;
                // RGB is already premultiplied for alpha=128 (64/32/16 at full opacity).
                result.capture = std::make_unique<SolidCapture>(
                    qRgba(32, 16, 8, 128), 3, 2, 1.0F, &childStorage);
            }
            return result;
        };
    auto singleComponents = single.createTargetComponents(false, singleResolver);
    std::vector<hyremote::RemoteFrame> singleFrames;
    if (!check(singleComponents.capture->start(
                   [&](hyremote::RemoteFrame frame) { singleFrames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "single full-canvas capture starts")
        || !check(singleComponents.capture->requestFrame({42, hyremote::Clock::now()}),
                  "single full-canvas frame requested")
        || !check(waitForFrame(singleFrames, 1), "single full-canvas frame delivered")) {
        return 5;
    }
    const auto &singleResult = singleFrames.front();
    const QImage singleImage = imageView(singleResult);
    const uchar *singleBytes = singleImage.constBits();
    if (!check(childStorage && singleResult.storage == childStorage,
               "full-canvas fast path shares immutable child storage")
        || !check(singleResult.geometry.devicePixelRatio == 1.0F
                      && singleImage.size() == QSize(3, 2),
                  "full-canvas output has correct logical geometry")
        || !check(singleBytes && singleBytes[0] == 32 && singleBytes[1] == 16
                      && singleBytes[2] == 8 && singleBytes[3] == 128,
                  "full-canvas fast path preserves premultiplied transparent pixels")
        || !check(singleResult.requestId == hyremote::CaptureRequestId{42}
                      && singleResult.damage.kind == hyremote::DamageKind::FullFrame,
                  "full-canvas fast path retains composite request/damage metadata")) {
        return 6;
    }
    singleComponents.capture->stop();

    // #790: draining child owners one at a time must complete the previous
    // lifetime before restart. Reuse the existing target/resolver, not a new
    // fixture or timing-sensitive concurrency harness.
    singleFrames.clear();
    if (!check(singleComponents.capture->start(
                   [&](hyremote::RemoteFrame frame) { singleFrames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "composite restarts after allocation-free child teardown")
        || !check(singleComponents.capture->requestFrame({43, hyremote::Clock::now()}),
                  "restarted composite accepts a new capture request")
        || !check(waitForFrame(singleFrames, 1), "restarted composite delivers a fresh frame")
        || !check(singleFrames.front().requestId == hyremote::CaptureRequestId{43},
                  "restarted composite preserves the new request identity")) {
        singleComponents.capture->stop();
        return 6;
    }
    singleComponents.capture->stop();

    // A single 2x-DPR child needs scaling into the logical canvas. It must
    // retain the old compositor (no direct storage alias) and #806 DPR=1.
    QObject scaledTarget;
    CompositeTarget scaled;
    scaled.upsertSurface(4, &scaledTarget, QRect(5, 5, 2, 2), true);
    std::shared_ptr<const hyremote::FrameStorage> highDprStorage;
    const HyRemote::detail::BuiltinTargetResolver scaledResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (!input && target == &scaledTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(
                    qRgba(0, 255, 0, 255), 4, 4, 2.0F, &highDprStorage);
            }
            return result;
        };
    auto scaledComponents = scaled.createTargetComponents(false, scaledResolver);
    std::vector<hyremote::RemoteFrame> scaledFrames;
    if (!check(scaledComponents.capture->start(
                   [&](hyremote::RemoteFrame frame) { scaledFrames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "scaled child capture starts")
        || !check(scaledComponents.capture->requestFrame({43, hyremote::Clock::now()}),
                  "scaled child frame requested")
        || !check(waitForFrame(scaledFrames, 1), "scaled child frame delivered")) {
        return 7;
    }
    const auto &scaledResult = scaledFrames.front();
    const QImage scaledImage = imageView(scaledResult);
    if (!check(highDprStorage && scaledResult.storage != highDprStorage,
               "DPR2 child must retain pixel-scaling compositor")
        || !check(scaledResult.geometry.devicePixelRatio == 1.0F
                      && scaledImage.size() == QSize(2, 2),
                  "DPR2 child output stays logical DPR1")
        || !check(scaledImage.pixelColor(0, 0) == QColor(0, 255, 0, 255),
                  "DPR2 child's pixels are still composited correctly")) {
        return 8;
    }
    scaledComponents.capture->stop();


    // #790: on the final Viewer disconnect an already received Quick child
    // image must not remain owned by a pending multi-surface composite while
    // a second child has not completed. A canceled request gets one empty
    // terminal completion (not stale pixels); late child frames are ignored.
    // A subsequent Viewer request still assembles a genuinely new full frame.
    QObject idleBase;
    QObject idlePopup;
    CompositeTarget idleComposite;
    idleComposite.upsertSurface(70, &idleBase, QRect(0, 0, 4, 4), true);
    idleComposite.upsertSurface(71, &idlePopup, QRect(1, 1, 2, 2), true);
    std::shared_ptr<const hyremote::FrameStorage> baseStorage;
    std::function<void()> releaseIdlePopup;
    const HyRemote::detail::BuiltinTargetResolver idleResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (input)
                return result;
            if (target == &idleBase) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(
                    qRgba(255, 0, 0, 255), 4, 4, 1.0F, &baseStorage);
            } else if (target == &idlePopup) {
                result.supported = true;
                result.capture = std::make_unique<DeferredSolidCapture>(&releaseIdlePopup);
            }
            return result;
        };
    auto idleComponents = idleComposite.createTargetComponents(false, idleResolver);
    std::vector<hyremote::RemoteFrame> idleFrames;
    hyremote::FrameReadyHandler noCopyCallback =
        ThrowOnCopyFrameHandler(&idleFrames);
    if (!check(idleComponents.capture && idleComponents.capture->start(
                   std::move(noCopyCallback),
                   [](const hyremote::CaptureEvent &) {}),
               "idle composite capture starts without copying a fallible callback")
        || !check(idleComponents.capture->requestFrame({56, hyremote::Clock::now()}),
                  "idle composite accepts request"))
        return 29;
    QElapsedTimer idleTimer;
    idleTimer.start();
    while ((!releaseIdlePopup || !baseStorage) && idleTimer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    if (!check(static_cast<bool>(releaseIdlePopup) && baseStorage && idleFrames.empty(),
               "base pixels remain owned by unfinished composite"))
        return 30;
    const std::weak_ptr<const hyremote::FrameStorage> pendingPixels = baseStorage;
    baseStorage.reset();
    if (!check(!pendingPixels.expired(), "pending composite really owns the completed pixels"))
        return 31;
    idleComponents.capture->releaseIdleResources();
    if (!check(pendingPixels.expired(), "idle release retires unfinished composite pixels")
        || !check(idleFrames.size() == 1 && idleFrames.front().requestId == hyremote::CaptureRequestId{56}
                  && !idleFrames.front().storage,
                  "canceled request gives exactly one empty terminal completion"))
        return 32;
    releaseIdlePopup();
    if (!check(idleFrames.size() == 1, "late child completion cannot revive canceled frame"))
        return 33;

    releaseIdlePopup = {};
    if (!check(idleComponents.capture->requestFrame({57, hyremote::Clock::now()}),
               "fresh viewer request admitted after idle cancellation"))
        return 34;
    QElapsedTimer restartTimer;
    restartTimer.start();
    while (!releaseIdlePopup && restartTimer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    if (!check(static_cast<bool>(releaseIdlePopup) && idleFrames.size() == 1,
               "fresh request waits for second child"))
        return 35;
    releaseIdlePopup();
    if (!check(waitForFrame(idleFrames, 2), "fresh request completes after viewer return")
        || !check(idleFrames[1].requestId == hyremote::CaptureRequestId{57}
                  && idleFrames[1].storage,
                  "new request has independent identity and pixels"))
        return 36;
    idleComponents.capture->stop();

    std::cout << "PASS: composite multi-surface, no stale-frame fallback, single-canvas immutable fast path, DPR fallback, and #958 overlapped alpha/Z/hide/output-identity truth\n";
    return 0;
}
