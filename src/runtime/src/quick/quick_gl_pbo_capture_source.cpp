// EXPERIMENTAL, default OFF. Uses Qt6.8 public OpenGL RHI callbacks and
// PBO/fence facilities, not QtGuiPrivate, QRhi or platform-specific APIs.
// NOT RELEASE QUALIFIED. A readable default framebuffer in afterFrameEnd is
// verified on Mesa/Xvfb only; EGLFS/front-buffer semantics remain unverified.
#include "quick/quick_gl_pbo_capture_source.hpp"
#include "quick/quick_readback_timing.hpp"
#include "quick/quick_readback_batch.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPointer>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#include "detail/perf_probe.hpp"
#include "detail/readback_frame_storage.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {
namespace {

constexpr GLenum kPackTarget = 0x88EB; // GL_PIXEL_PACK_BUFFER
constexpr GLenum kPackBinding = 0x88ED; // GL_PIXEL_PACK_BUFFER_BINDING
constexpr GLenum kStreamRead = 0x88E1; // GL_STREAM_READ
constexpr GLenum kMapRead = 0x0001; // GL_MAP_READ_BIT
constexpr GLenum kFenceGpu = 0x9117; // GL_SYNC_GPU_COMMANDS_COMPLETE
constexpr GLenum kAlready = 0x911A; // GL_ALREADY_SIGNALED
constexpr GLenum kSatisfied = 0x911C; // GL_CONDITION_SATISFIED

// Give the application's normal Qt render loop one frame opportunity before
// requesting a new render solely for a remote Viewer. The render callback
// consumes queued requests immediately if a normal frame arrives. Idle
// sources still get a bounded fallback update, followed by the existing
// 750ms terminal-outcome timer if the window cannot render.
constexpr int kNormalFrameOpportunityMs = QuickReadbackTiming::normalFrameOpportunityMs;

// Qt 5.15 direct OpenGL renders into its current framebuffer and exposes
// screen-scraping via afterRendering() before swap. Qt 6 RHI has a different
// submission lifecycle and uses afterFrameEnd(). This adapter is the ONLY
// version-specific portion of the GL PBO provider.
inline bool hasRequiredPboFenceApi(const QOpenGLContext *ctx) noexcept
{
    if (!ctx)
        return false;
    const auto version = ctx->format();
    // GLES 3.0 provides PBO, mapBufferRange and GLsync in core.
    // Desktop GLsync is only core as of OpenGL 3.2; advertising GL 3.0
    // alone is NOT sufficient for glFenceSync or a safe async provider.
    if (ctx->isOpenGLES())
        return version.majorVersion() >= 3;
    return version.majorVersion() > 3
        || (version.majorVersion() == 3 && version.minorVersion() >= 2);
}

inline qreal captureWindowDpr(QQuickWindow *window) noexcept
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return window->devicePixelRatio();
#else
    return window->effectiveDevicePixelRatio();
#endif
}

class GlPboCaptureSource final : public hyremote::CaptureSource
{
    struct State {
        std::mutex mutex;
        std::condition_variable drained;
        QPointer<QQuickWindow> window;
        std::uint64_t generation = 0;
        bool active = false;
        bool connected = false;
        bool kickPosted = false;
        bool fallbackRenderTimerArmed = false;
        bool cleanupRequested = false;
        unsigned int callbacksInFlight = 0;
        unsigned int cpuWorkersInFlight = 0;
        std::vector<hyremote::CaptureRequest> queued;
        std::vector<hyremote::CaptureRequest> inFlight;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
        QMetaObject::Connection frameConnection;
        QMetaObject::Connection invalidatedConnection;
        // All GL fields below may be ACCESSED ONLY in the render callback.
        GLuint pbo = 0;
        GLsync fence = nullptr;
        // GL object names are meaningful ONLY in their creating Context.
        // Qt may destroy that Context before an eventual delayed stop().
        QPointer<QOpenGLContext> resourceContext;
        bool gpuPending = false;
        std::size_t pboCapacityBytes = 0; // render-thread-only under State mutex
        hyremote::TimePoint readbackRenderTime{};
        // One outstanding CPU conversion per source, independent of the GL fence.
        bool cpuConversionPending = false;
        std::vector<hyremote::CaptureRequest> converting;
        int pixelWidth = 0;
        int pixelHeight = 0;
        float dpr = 1.0f;
        int forcedRenders = 0;
    };

    // Prevent a queued/running conversion from surviving CaptureSource::stop()
    // and subsequently executing code from an unloaded Runtime library.
    // The worker count must retire even if normalization/reporting throws.
    struct CpuWorkerDrainGuard {
        std::shared_ptr<State> state;
        ~CpuWorkerDrainGuard() noexcept
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->cpuWorkersInFlight;
            state->drained.notify_all();
        }
    };

public:
    explicit GlPboCaptureSource(QQuickWindow *window)
        : m_state(std::make_shared<State>())
    {
        m_state->window = window;
    }

    ~GlPboCaptureSource() override { stop(); }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities cap;
        cap.asynchronous = true;
        cap.regionDamage = false;
        cap.cpuReadable = true;
        cap.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return cap;
    }

    bool start(hyremote::FrameReadyHandler frame,
               hyremote::CaptureEventHandler event) override
    {
        auto s = m_state;
        std::lock_guard<std::mutex> guard(s->mutex);
        if (s->active || s->window.isNull() || !QCoreApplication::instance())
            return false;
        if (s->window->thread() != QCoreApplication::instance()->thread())
            return false;
        ++s->generation;
        s->queued.clear();
        s->inFlight.clear();
        s->converting.clear();
        s->cpuConversionPending = false;
        // A queued GUI dispatch from an earlier Session may never have run.
        // Never let its posted-token suppress the first request of this
        // generation. Old tasks still verify their captured generation.
        s->kickPosted = false;
        s->fallbackRenderTimerArmed = false;
        s->onFrame = std::move(frame);
        s->onEvent = std::move(event);
        s->active = true;
        // Do NOT clear cleanupRequested from the previous generation here.
        // Its fence/PBO objects belong to the Qt render thread and must
        // retire there, before the new generation can reuse GL objects.
        return true;
    }

    void stop() noexcept override
    {
        auto s = m_state;
        QPointer<QQuickWindow> target;
        {
            std::unique_lock<std::mutex> guard(s->mutex);
            s->active = false;
            ++s->generation;
            s->queued.clear();
            s->inFlight.clear();
            s->converting.clear();
            s->cpuConversionPending = false;
            s->kickPosted = false;
            s->fallbackRenderTimerArmed = false;
            s->onFrame = {};
            s->onEvent = {};
            s->cleanupRequested = true;
            // A worker may still execute code from the Runtime DLL even
            // after its generation has been revoked. Drain it before the
            // owner can unload the module or destroy the State.
            s->drained.wait(guard, [&] {
                return s->callbacksInFlight == 0
                    && s->cpuWorkersInFlight == 0;
            });
            target = s->window;
        }
        // The GL context belongs to Qt's render thread. Never glDeleteBuffers
        // from stop(): arrange a single final render-thread cleanup instead.
        if (target)
            QMetaObject::invokeMethod(target, [target] {
                if (target) target->update();
            }, Qt::QueuedConnection);
    }

    bool requestFrame(const hyremote::CaptureRequest &req) override
    {
        auto s = m_state;
        QPointer<QQuickWindow> target;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->window.isNull())
                return false;
            s->queued.push_back(req);
            // A valid Core request must have exactly one terminal outcome.
            // No render/frame-fence progress is possible for a hidden or
            // continuously idle QQuickWindow, so NEVER allow an admitted
            // request to stay queued/inFlight forever. This timer executes
            // on the GUI thread without doing GL work; any later GPU
            // completion observes that its request was already revoked.
            const std::uint64_t requestGeneration = s->generation;
            QTimer::singleShot(750, QCoreApplication::instance(), [s, requestGeneration, id = req.id] {
                std::vector<hyremote::CaptureRequest> expired;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != requestGeneration)
                        return;
                    // Each physical readback has one failure episode, even
                    // when it covers two Core request identities. Expiring
                    // the whole owning batch is safe: every ID receives one
                    // terminal completion, and its later timer finds no ID.
                    if (!retireTimedOutReadbackBatch(s->inFlight, id, expired)
                        && !retireTimedOutReadbackBatch(s->converting, id, expired))
                        retireTimedOutReadbackBatch(s->queued, id, expired);
                }
                if (!expired.empty())
                    reportUnavailable(s, requestGeneration, std::move(expired),
                                      "GL PBO capture request did not complete within 750ms");
            });
            if (s->kickPosted)
                return true;
            s->kickPosted = true;
            target = s->window;
            generation = s->generation;
        }
        if (!QMetaObject::invokeMethod(target, [s, target, generation] {
                if (!target) return;
                bool connectNow = false;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    s->kickPosted = false;
                    connectNow = !s->connected;
                    if (connectNow) s->connected = true;
                }
                if (connectNow) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    s->frameConnection = QObject::connect(
                        target, &QQuickWindow::afterRendering, target,
                        [s, target] { onRender(s, target); },
                        Qt::DirectConnection);
#else
                    s->frameConnection = QObject::connect(
                        target, &QQuickWindow::afterFrameEnd, target,
                        [s, target] { onRender(s, target); },
                        Qt::DirectConnection);
#endif
                    s->invalidatedConnection = QObject::connect(
                        target, &QQuickWindow::sceneGraphInvalidated, target,
                        [s] { onInvalidated(s); },
                        Qt::DirectConnection);
                }
                if (!target->isVisible()) {
                    // A hidden Surface must never start another GPU capture.
                    // Deliver a terminal recoverable unavailable result for
                    // every admitted request instead of waiting forever for
                    // a Qt scenegraph frame that may never occur.
                    std::vector<hyremote::CaptureRequest> hidden;
                    {
                        std::lock_guard<std::mutex> lock(s->mutex);
                        if (s->active && s->generation == generation)
                            hidden.swap(s->queued);
                    }
                    if (!hidden.empty())
                        reportUnavailable(s, generation, std::move(hidden),
                                          "GL PBO capture target is not visible");
                    return;
                }
                requestFallbackRenderIfNoNormalFrame(s, target, generation);
            }, Qt::QueuedConnection)) {
            // A failed GUI dispatch means this request was *rejected*:
            // Core will retry it. Remove its ID so a later dispatch cannot
            // also complete it. Other threads may already have received
            // 'true' while kickPosted was set; those requests MUST still
            // receive one terminal result instead of remaining stranded.
            std::vector<hyremote::CaptureRequest> acceptedButUnscheduled;
            {
                std::lock_guard<std::mutex> guard(s->mutex);
                if (s->generation == generation && s->active) {
                    s->kickPosted = false;
                    for (const auto &pending : s->queued) {
                        if (pending.id != req.id)
                            acceptedButUnscheduled.push_back(pending);
                    }
                    s->queued.clear();
                }
            }
            if (!acceptedButUnscheduled.empty())
                reportUnavailable(s, generation,
                                  std::move(acceptedButUnscheduled),
                                  "GL PBO GUI dispatch could not be scheduled");
            return false;
        }
        return true;
    }

private:
    static void requestFallbackRenderIfNoNormalFrame(
        const std::shared_ptr<State> &s,
        QPointer<QQuickWindow> window,
        std::uint64_t generation)
    {
        // Executed on GUI thread. A natural Qt frame may already have moved
        // the pending requests into inFlight; in that case do NOT schedule
        // another Qt rendering pass. This does not depend on Qt6 RHI APIs.
        // A quiet source still uses a bounded explicit update as fallback.
        if (!window)
            return;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != generation
                || s->fallbackRenderTimerArmed)
                return;
            s->fallbackRenderTimerArmed = true;
        }
        QTimer::singleShot(kNormalFrameOpportunityMs, window.data(),
            [s, window, generation] {
                bool needsFallbackRender = false;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation)
                        return;
                    s->fallbackRenderTimerArmed = false;
                    needsFallbackRender = !s->gpuPending
                        && !s->cpuConversionPending
                        && !s->queued.empty();
                }
                if (needsFallbackRender && window && window->isVisible()) {
                    HYREMOTE_P1_COUNT(QuickGlExplicitRender);
                    window->update();
                }
            });
    }

    static void releaseGl(State &s, QOpenGLExtraFunctions *gl)
    {
        // NEVER glDelete an old context's PBO/Fence name in the replacement
        // context: numeric names may alias unrelated Qt/driver resources.
        // If the original context disappeared, Qt/GL reclaimed those names.
        const bool ownsCurrentContext = s.resourceContext
            && s.resourceContext.data() == QOpenGLContext::currentContext();
        if (ownsCurrentContext) {
            if (s.fence)
                gl->glDeleteSync(s.fence);
            if (s.pbo)
                gl->glDeleteBuffers(1, &s.pbo);
        }
        s.pbo = 0;
        s.fence = nullptr;
        s.resourceContext = nullptr;
        s.gpuPending = false;
        s.pboCapacityBytes = 0;
        s.pixelWidth = 0;
        s.pixelHeight = 0;
        s.forcedRenders = 0;
    }

    static void onInvalidated(const std::shared_ptr<State> &s)
    {
        // If Qt has already destroyed the GL context, its objects are
        // reclaimed by context destruction. Do not touch an invalid context.
        std::vector<hyremote::CaptureRequest> revoked;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            generation = s->generation;
            if (s->active) {
                // A Scene Graph rebuild is a hard source-pixel epoch
                // boundary. Do not carry an already queued/mapped request
                // into a newly created GL context or wait 750ms to retire it.
                revoked.swap(s->queued);
                revoked.insert(revoked.end(),
                               s->inFlight.begin(), s->inFlight.end());
                s->inFlight.clear();
                revoked.insert(revoked.end(),
                               s->converting.begin(), s->converting.end());
                s->converting.clear();
            }
            auto *ctx = QOpenGLContext::currentContext();
            if (ctx) releaseGl(*s, ctx->extraFunctions());
            else {
                s->pbo = 0;
                s->fence = nullptr;
                s->gpuPending = false;
                s->pboCapacityBytes = 0;
                s->resourceContext = nullptr;
            }
            s->cleanupRequested = false;
            // A hidden/destroyed Scene Graph may never produce the stop
            // retirement frame. Qt has already reclaimed GL resources.
            if (!s->active && s->connected) {
                QObject::disconnect(s->frameConnection);
                QObject::disconnect(s->invalidatedConnection);
                s->frameConnection = {};
                s->invalidatedConnection = {};
                s->connected = false;
            }
        }
        if (!revoked.empty())
            reportUnavailable(s, generation, std::move(revoked),
                              "Qt GL Scene Graph surface invalidated");
    }

    static void reportUnavailable(const std::shared_ptr<State> &s,
                                  std::uint64_t generation,
                                  std::vector<hyremote::CaptureRequest> requests,
                                  const char *reason)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher) return;
        QMetaObject::invokeMethod(dispatcher,
            [s, generation, requests = std::move(requests), reason] {
                hyremote::FrameReadyHandler callback;
                hyremote::CaptureEventHandler eventCallback;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    callback = s->onFrame;
                    eventCallback = s->onEvent;
                    ++s->callbacksInFlight;
                }
                if (eventCallback) {
                    try {
                        hyremote::CaptureEvent e;
                        e.code = hyremote::CaptureEventCode::TemporarilyUnavailable;
                        e.recoverable = true;
                        e.message = reason;
                        eventCallback(e);
                    } catch (...) {}
                }
                if (callback) {
                    for (const auto &req : requests) {
                        try {
                            hyremote::RemoteFrame empty;
                            empty.requestId = req.id;
                            empty.timing.requestTime = req.requestTime;
                            empty.timing.completionTime = hyremote::Clock::now();
                            empty.timing.ptsSource = hyremote::PtsSource::Completion;
                            callback(std::move(empty));
                        } catch (...) {}
                    }
                }
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    --s->callbacksInFlight;
                    s->drained.notify_all();
                }
            }, Qt::QueuedConnection);
    }

    static void reportFrame(const std::shared_ptr<State> &s,
                            std::uint64_t generation,
                            std::vector<hyremote::CaptureRequest> requests,
                            std::shared_ptr<const hyremote::FrameStorage> storage,
                            int width, int height, float dpr,
                            hyremote::TimePoint renderTime)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher) return;
        QMetaObject::invokeMethod(dispatcher,
            [s, generation, requests = std::move(requests),
             storage = std::move(storage), width, height, dpr, renderTime] {
                hyremote::FrameReadyHandler callback;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    callback = s->onFrame;
                    ++s->callbacksInFlight;
                }
                if (callback) {
                    hyremote::RemoteFrame prototype;
                    prototype.geometry.size = {
                        static_cast<std::uint32_t>(width),
                        static_cast<std::uint32_t>(height)};
                    prototype.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
                    prototype.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
                    prototype.geometry.planeCount = 1;
                    prototype.geometry.devicePixelRatio = dpr;
                    prototype.damage = hyremote::Damage::fullFrame();
                    prototype.storage = storage;
                    // The content was produced during the actual Qt render,
                    // NOT when the GPU->CPU result arrived. Frame age must
                    // include fence retirement and worker conversion time.
                    prototype.timing.pts = renderTime;
                    prototype.timing.ptsSource = hyremote::PtsSource::Render;
                    prototype.timing.completionTime = hyremote::Clock::now();
                    for (const auto &req : requests) {
                        try {
                            auto frame = prototype;
                            frame.requestId = req.id;
                            frame.timing.requestTime = req.requestTime;
                            callback(std::move(frame));
                        } catch (...) {}
                    }
                }
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    --s->callbacksInFlight;
                    s->drained.notify_all();
                }
            }, Qt::QueuedConnection);
    }

    static void finishCpuConversion(
        const std::shared_ptr<State> &s,
        std::uint64_t generation,
        const std::shared_ptr<std::byte> &raw,
        int width, int height, float dpr,
        hyremote::TimePoint renderTime)
    {
        // Pool worker only: the PBO is unmapped and the bytes are CPU-owned.
        // Neither Qt's render thread nor the GUI thread flips the image rows.
        HYREMOTE_P1_SCOPE(QuickGlCpuRepack);
        const auto stride = static_cast<std::size_t>(width) * 4u;
        const auto storage = raw ? adoptReadbackFrameStorage(
            raw, stride * static_cast<std::size_t>(height), stride, stride,
            static_cast<std::size_t>(height), ReadbackRowOrder::BottomUp)
            : std::shared_ptr<const hyremote::FrameStorage>{};

        std::vector<hyremote::CaptureRequest> requests;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != generation)
                return; // stop/restart revoked the work
            s->cpuConversionPending = false;
            requests.swap(s->converting); // timeout may already have retired IDs
        }
        if (!requests.empty()) {
            if (storage)
                reportFrame(s, generation, std::move(requests),
                            std::move(storage), width, height, dpr, renderTime);
            else
                reportUnavailable(s, generation, std::move(requests),
                                  "GL PBO CPU frame conversion failed");
        }
        if (auto *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [s, generation] {
                QPointer<QQuickWindow> target;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation)
                        return;
                    target = s->window;
                }
                requestFallbackRenderIfNoNormalFrame(s, target, generation);
            }, Qt::QueuedConnection);
        }
    }

    static void requestOneRetirementRender(const std::shared_ptr<State> &s,
                                           QQuickWindow *window,
                                           std::uint64_t generation)
    {
        // At most two extra renders per submitted GPU readback. Never
        // continuously redraw a static application to wait for a fence.
        int delayMs = QuickReadbackTiming::firstRetirementRenderMs;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != generation
                || !s->gpuPending
                || s->forcedRenders >= QuickReadbackTiming::maxExtraRenders)
                return;
            if (s->forcedRenders > 0)
                delayMs = QuickReadbackTiming::secondRetirementRenderMs;
        }
        QTimer::singleShot(delayMs, window, [s, window, generation] {
            bool shouldRender = false;
            {
                std::lock_guard<std::mutex> guard(s->mutex);
                if (s->active && s->generation == generation
                    && s->gpuPending && s->forcedRenders < 2) {
                    ++s->forcedRenders;
                    shouldRender = true;
                }
            }
            if (shouldRender) {
                HYREMOTE_P1_COUNT(QuickGlRetirementRender);
                window->update();
            }
        });
    }

    static void onRender(const std::shared_ptr<State> &s, QQuickWindow *window)
    {
        auto *ctx = QOpenGLContext::currentContext();
        // The provider was qualified by Qt's declared graphics API, but
        // actual GL/ES context capabilities are only authoritative here on
        // Qt's render thread. An unsupported context is a terminal native
        // failure NOW, not a 750ms dead-end followed by another timeout.
        auto *renderer = window->rendererInterface();
        if (!ctx || !renderer
            || renderer->graphicsApi() != QSGRendererInterface::OpenGL
            || !hasRequiredPboFenceApi(ctx)) {
            std::vector<hyremote::CaptureRequest> unsupported;
            std::uint64_t generation = 0;
            {
                std::lock_guard<std::mutex> guard(s->mutex);
                if (!s->active)
                    return;
                generation = s->generation;
                unsupported.swap(s->queued);
                unsupported.insert(unsupported.end(),
                                   s->inFlight.begin(), s->inFlight.end());
                s->inFlight.clear();
                // No GL resource operations are safe without the qualified
                // context. Retire stale PBO/fence objects ONLY when the
                // next valid GL context enters this render callback.
                s->gpuPending = false;
                s->cleanupRequested = true;
            }
            if (!unsupported.empty())
                reportUnavailable(s, generation, std::move(unsupported),
                                  "Qt OpenGL/ES 3+ render context unavailable");
            return;
        }
        auto *gl = ctx->extraFunctions();
        HYREMOTE_P1_SCOPE(QuickGlRenderCallback);
        std::vector<hyremote::CaptureRequest> failed;
        std::shared_ptr<std::byte> conversionData;
        std::uint64_t gen = 0;
        int frameWidth = 0, frameHeight = 0;
        float frameDpr = 1.f;
        hyremote::TimePoint frameRenderTime{};
        bool pumpAgain = false;
        bool kickAgain = false;
        const char *failureReason = "GL PBO frame readback unavailable";
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if ((s->pbo || s->fence)
                && (!s->resourceContext || s->resourceContext.data() != ctx)) {
                // Missing sceneGraphInvalidated is not permission to reuse
                // stale GPU objects from an unrelated Qt GL context.
                gen = s->generation;
                failed.swap(s->inFlight);
                failureReason = "Qt OpenGL context changed during GPU readback";
                releaseGl(*s, gl); // safe: drops mismatched names without glDelete
                s->cleanupRequested = false;
                kickAgain = !s->queued.empty();
            }
            if (s->cleanupRequested) {
                releaseGl(*s, gl);
                s->cleanupRequested = false;
            }
            // The earliest 750ms deadline may have retired every request
            // attached to this GPU readback. Do not retain an orphaned
            // fence/PBO indefinitely or map pixels with no Core consumer.
            // All GL deletion remains on the owning render Context.
            if (s->gpuPending && s->inFlight.empty())
                releaseGl(*s, gl);
            if (!s->active) {
                // stop() intentionally kept the render connection alive
                // only until its GPU objects could be destroyed on the
                // owning GL thread. Release it now: otherwise each restart
                // or source destruction retains another shared State and
                // an unnecessary render callback until window destruction.
                if (s->connected) {
                    QObject::disconnect(s->frameConnection);
                    QObject::disconnect(s->invalidatedConnection);
                    s->frameConnection = {};
                    s->invalidatedConnection = {};
                    s->connected = false;
                }
                return;
            }
            gen = s->generation;
            if (!s->gpuPending
                && (s->queued.empty() || s->cpuConversionPending)
                && failed.empty())
                return; // NO demand or bounded conversion still pending.
            if (!s->pbo) {
                gl->glGenBuffers(1, &s->pbo);
                if (s->pbo)
                    s->resourceContext = ctx;
            }
            if (!s->pbo) {
                failed.swap(s->queued);
                failureReason = "GL PBO allocation failed";
            } else {
                GLint oldPack = 4, oldPackPbo = 0;
                gl->glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
                gl->glGetIntegerv(kPackBinding, &oldPackPbo);
                if (s->gpuPending && s->fence) {
                    const GLenum signaled = gl->glClientWaitSync(s->fence, 0, 0);
                    if (signaled == kAlready || signaled == kSatisfied) {
                        // A GPU copy submitted for the old Surface generation
                        // must never be relabelled with the resized window's
                        // current geometry. The public CaptureSource contract
                        // can recover with a new request once the target
                        // scene graph has synchronized its new dimensions.
                        const qreal currentDpr = captureWindowDpr(window);
                        const int currentW = qRound(window->width() * currentDpr);
                        const int currentH = qRound(window->height() * currentDpr);
                        const bool geometryMatches =
                            currentW == s->pixelWidth && currentH == s->pixelHeight
                            && std::abs(double(currentDpr) - double(s->dpr)) < 0.0001;
                        if (!geometryMatches) {
                            failed.swap(s->inFlight);
                            failureReason = "GL PBO source resized before readback retirement";
                        } else {
                        gl->glBindBuffer(kPackTarget, s->pbo);
                        const std::size_t stride =
                            static_cast<std::size_t>(s->pixelWidth) * 4u;
                        const std::size_t bytes =
                            stride * static_cast<std::size_t>(s->pixelHeight);
                        // GL context usage stays on Qt's render thread.
                        // Copy mapped pixels ONCE to CPU memory, unmap, then
                        // allocate/frame-flip on a separate bounded worker.
                        HYREMOTE_P1_SCOPE(QuickGlGpuMapCopy);
                        std::shared_ptr<std::byte> owned;
                        try {
                            // new byte[] is not value-initialized: avoid an
                            // unnecessary 8 MB zero-fill before the actual
                            // 1080p GPU->CPU memcpy on the render thread.
                            owned = std::shared_ptr<std::byte>(
                                new (std::nothrow) std::byte[bytes],
                                std::default_delete<std::byte[]>());
                        } catch (...) {
                            // Out of memory is a recoverable capture failure.
                        }
                        const auto *mapped = owned ? static_cast<const std::byte *>(
                            gl->glMapBufferRange(kPackTarget, 0, GLsizeiptr(bytes), kMapRead))
                            : nullptr;
                        if (mapped) {
                            std::memcpy(owned.get(), mapped, bytes);
                            HYREMOTE_P1_BYTES(QuickGlGpuMapCopy, bytes);
                        }
                        const bool mappingValid = mapped && gl->glUnmapBuffer(kPackTarget);
                        if (mappingValid) {
                            conversionData = std::move(owned);
                            frameWidth = s->pixelWidth;
                            frameHeight = s->pixelHeight;
                            frameDpr = s->dpr;
                            frameRenderTime = s->readbackRenderTime;
                            #if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE

                            for (const auto &request : s->inFlight)
                                HYREMOTE_P1_TRACE_REQUEST(GpuReadbackReady, request);

                            #endif
                            s->converting.swap(s->inFlight);
                            s->cpuConversionPending = true;
                            ++s->cpuWorkersInFlight;
                        } else {
                            failed.swap(s->inFlight);
                            failureReason = "GL PBO could not map owned CPU frame";
                        }
                        } // geometryMatches
                        gl->glDeleteSync(s->fence);
                        s->fence = nullptr;
                        s->gpuPending = false;
                        s->forcedRenders = 0;
                        kickAgain = !s->queued.empty() && !s->cpuConversionPending;
                    } else if (s->forcedRenders < 2) {
                        pumpAgain = true;
                    } else {
                        failed.swap(s->inFlight);
                        failureReason = "GL PBO fence remained unsignaled after bounded frames";
                        gl->glDeleteSync(s->fence);
                        s->fence = nullptr;
                        s->gpuPending = false;
                    }
                }
                if (!s->gpuPending && !s->cpuConversionPending
                    && !kickAgain && failed.empty()
                    && !conversionData && !s->queued.empty()) {
                    const qreal dpr = captureWindowDpr(window);
                    const int w = qRound(window->width()*dpr);
                    const int h = qRound(window->height()*dpr);
                    if (w>0 && h>0 && std::size_t(w) <=
                        std::numeric_limits<std::size_t>::max() / (4u*std::size_t(h))) {
                        GLint framebuffer = 0;
                        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
                        const auto wantedBytes =
                            std::size_t(w) * std::size_t(h) * 4u;
                        gl->glBindBuffer(kPackTarget, s->pbo);
                        // A completed GPU fence and unmapped buffer permit
                        // safe reuse. Only allocate on growth or major
                        // downsizing (>4x capacity waste).
                        const bool resizePbo =
                            s->pboCapacityBytes < wantedBytes
                            || s->pboCapacityBytes / 4u >= wantedBytes;
                        if (resizePbo) {
                            gl->glBufferData(kPackTarget,
                                             GLsizeiptr(wantedBytes),
                                             nullptr, kStreamRead);
                        }
                        gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        gl->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        HYREMOTE_P1_COUNT(QuickGlGpuSubmit);
                        s->fence = gl->glFenceSync(kFenceGpu, 0);
                        const bool valid = s->fence && gl->glGetError() == GL_NO_ERROR;
                        if (valid) {
                            if (resizePbo) {
                                s->pboCapacityBytes = wantedBytes;
                                HYREMOTE_P1_COUNT(QuickGlPboResize);
                            } else {
                                HYREMOTE_P1_COUNT(QuickGlPboReuse);
                            }
                            gl->glFlush();
                            s->pixelWidth = w;
                            s->pixelHeight = h;
                            s->dpr = static_cast<float>(dpr);
                            s->gpuPending = true;
                            s->readbackRenderTime = hyremote::Clock::now();
                            s->forcedRenders = 0;
                            s->inFlight.swap(s->queued);
                            #if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE

                            for (const auto &request : s->inFlight)
                                HYREMOTE_P1_TRACE_REQUEST(GpuReadbackSubmitted, request);

                            #endif
                            pumpAgain = true;
                        } else {
                            if (s->fence) gl->glDeleteSync(s->fence);
                            s->fence = nullptr;
                            failed.swap(s->queued);
                            failureReason = "GL frame readback or GPU fence submission failed";
                        }
                    } else {
                        failed.swap(s->queued);
                        failureReason = "GL capture dimensions invalid";
                    }
                }
                gl->glBindBuffer(kPackTarget, static_cast<GLuint>(oldPackPbo));
                gl->glPixelStorei(GL_PACK_ALIGNMENT, oldPack);
            }
        }
        if (conversionData) {
            // At most one conversion per source. The stop() quiescence gate
            // also drains running pool work before Runtime code can unload.
            try {
                QThreadPool::globalInstance()->start(
                    [s, gen, raw = std::move(conversionData),
                     frameWidth, frameHeight, frameDpr, frameRenderTime] {
                        CpuWorkerDrainGuard drain{s};
                        try {
                            finishCpuConversion(s, gen, raw,
                                                frameWidth, frameHeight, frameDpr,
                                                frameRenderTime);
                        } catch (...) {
                            std::vector<hyremote::CaptureRequest> abandoned;
                            {
                                std::lock_guard<std::mutex> lock(s->mutex);
                                if (s->active && s->generation == gen) {
                                    s->cpuConversionPending = false;
                                    abandoned.swap(s->converting);
                                }
                            }
                            if (!abandoned.empty()) {
                                try {
                                    reportUnavailable(s, gen, std::move(abandoned),
                                                      "GL PBO CPU worker failed");
                                } catch (...) {
                                    // The drain guard still guarantees shutdown.
                                }
                            }
                        }
                    });
            } catch (...) {
                std::vector<hyremote::CaptureRequest> notScheduled;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (s->active && s->generation == gen) {
                        s->cpuConversionPending = false;
                        notScheduled.swap(s->converting);
                    }
                    --s->cpuWorkersInFlight;
                    s->drained.notify_all();
                }
                if (!notScheduled.empty())
                    reportUnavailable(s, gen, std::move(notScheduled),
                                      "GL PBO CPU worker could not be scheduled");
            }
        }
        if (!failed.empty())
            reportUnavailable(s, gen, std::move(failed), failureReason);
        if (pumpAgain)
            requestOneRetirementRender(s, window, gen);
        if (kickAgain)
            QMetaObject::invokeMethod(window, [s, window, gen] {
                requestFallbackRenderIfNoNormalFrame(s, window, gen);
            }, Qt::QueuedConnection);
    }

    std::shared_ptr<State> m_state;
};
} // namespace

std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickGlPboCaptureSource(QQuickWindow *window)
{
    return std::make_unique<GlPboCaptureSource>(window);
}

} // namespace HyRemote::detail
