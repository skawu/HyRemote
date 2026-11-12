// EXPERIMENTAL, default OFF. Uses Qt6.8 public OpenGL RHI callbacks and
// PBO/fence facilities, not QtGuiPrivate, QRhi or platform-specific APIs.
// NOT RELEASE QUALIFIED. A readable default framebuffer in afterFrameEnd is
// verified on Mesa/Xvfb only; EGLFS/front-buffer semantics remain unverified.
#include "quick/quick_gl_pbo_capture_source.hpp"
#include "quick/quick_readback_timing.hpp"
#include "quick/quick_readback_batch.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QOffscreenSurface>
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
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
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
constexpr GLenum kTimedOut = 0x911B; // GL_TIMEOUT_EXPIRED
constexpr GLenum kSatisfied = 0x911C; // GL_CONDITION_SATISFIED
constexpr GLenum kWaitFailed = 0x911D; // GL_WAIT_FAILED

// #962 review B1/B2: observable session counters. They are test-only and
// never affect the production path; the self-test below drives the same
// allocation/reuse/worker-generation mechanism the capture source uses.
std::atomic<unsigned> g_quickGlPboBuffersCreated{0};
std::atomic<unsigned> g_quickGlPboBufferDataCalls{0};
std::atomic<unsigned> g_quickGlPboReuses{0};
std::atomic<unsigned> g_quickGlPboWorkersCreated{0};
std::atomic<unsigned> g_quickGlPboWorkersRetired{0};
std::atomic<unsigned> g_quickGlPboInvalidations{0};

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

// Session-scoped GPU retirement. One instance exists per (QQuickWindow, Qt
// render context) generation; it is never created per CaptureRequest. The Qt
// render callback submits glReadPixels + GLsync, then this worker performs the
// bounded glClientWaitSync, PBO map and the single CPU copy on its own
// current shared QOpenGLContext. That keeps the Qt render thread free of GPU
// waits and avoids scheduling extra window->update() frames to retire a fence.
struct GlRetireJob {
    GLuint pbo = 0;
    GLsync fence = nullptr;
    int width = 0;
    int height = 0;
    float dpr = 1.f;
    hyremote::TimePoint renderTime{};
    std::uint64_t generation = 0;
};

struct GlRetireResult {
    GlRetireJob job;
    std::shared_ptr<std::byte> pixels; // null on failure
    const char *failure = nullptr;
};

class GlRetireWorker final : public QThread {
public:
    GlRetireWorker(QOpenGLContext *context, QOffscreenSurface *surface,
                   std::function<void(GlRetireResult &&)> onResult)
        : m_context(context), m_surface(surface), m_onResult(std::move(onResult))
    {
    }

    void enqueue(GlRetireJob job)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push_back(std::move(job));
        }
        m_cv.notify_one();
    }

    void requestStop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_abort.store(true, std::memory_order_release);
        m_cv.notify_all();
    }

    // TEST-ONLY: simulate a slow retirement so the render/stop paths can be
    // proven not to wait for the worker. Never set by the product path.
    void setRetireDelayForTest(int milliseconds)
    {
        m_testDelayMs.store(milliseconds, std::memory_order_release);
    }

protected:
    void run() override
    {
        if (!m_context || !m_surface || !m_context->makeCurrent(m_surface)) {
            failQueued("GL shared context worker could not become current");
            delete m_context;
            m_context = nullptr;
            return;
        }
        auto *gl = m_context->extraFunctions();
        gl->initializeOpenGLFunctions();
        for (;;) {
            GlRetireJob job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [&] { return m_stop || !m_jobs.empty(); });
                if (m_jobs.empty())
                    break; // stop requested and no queued work
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }
            retireOne(gl, std::move(job));
        }
        m_context->doneCurrent();
        delete m_context;
        m_context = nullptr;
    }

private:
    void retireOne(QOpenGLExtraFunctions *gl, GlRetireJob job)
    {
        // requestStop() must abort a pending GPU wait promptly: stop() is
        // then bounded by map/copy time instead of the 750ms fence ceiling.
        const int delayMs = m_testDelayMs.load(std::memory_order_acquire);
        for (int i = 0; i < delayMs; ++i) {
            if (m_abort.load(std::memory_order_acquire))
                break;
            QThread::msleep(1);
        }
        bool signaled = false;
        for (int poll = 0; poll < 150; ++poll) {
            if (m_abort.load(std::memory_order_acquire))
                break;
            const GLenum status = gl->glClientWaitSync(job.fence, 0, 5'000'000ULL);
            if (status == kAlready || status == kSatisfied) {
                signaled = true;
                break;
            }
            if (status == kWaitFailed || status != kTimedOut)
                break;
        }
        if (!signaled) {
            if (job.fence)
                gl->glDeleteSync(job.fence);
            job.fence = nullptr;
            deliver(GlRetireResult{std::move(job), nullptr,
                                   "GL PBO fence remained unsignaled"});
            return;
        }
        const std::size_t bytes =
            std::size_t(job.width) * std::size_t(job.height) * 4u;
        std::shared_ptr<std::byte> owned;
        bool mappedOk = false;
        if (bytes > 0 && bytes <= 40u * 1024u * 1024u) {
            GLint oldPack = 0;
            gl->glGetIntegerv(kPackBinding, &oldPack);
            gl->glBindBuffer(kPackTarget, job.pbo);
            auto *mapped = static_cast<const std::byte *>(
                gl->glMapBufferRange(kPackTarget, 0, GLsizeiptr(bytes), kMapRead));
            if (mapped) {
                try {
                    owned = std::shared_ptr<std::byte>(
                        new (std::nothrow) std::byte[bytes],
                        std::default_delete<std::byte[]>());
                } catch (...) {
                    owned.reset();
                }
                if (owned)
                    std::memcpy(owned.get(), mapped, bytes);
                mappedOk = gl->glUnmapBuffer(kPackTarget) && owned != nullptr;
            }
            gl->glBindBuffer(kPackTarget, GLuint(oldPack));
        }
        if (job.fence)
            gl->glDeleteSync(job.fence);
        job.fence = nullptr;
        deliver(GlRetireResult{std::move(job), mappedOk ? std::move(owned) : nullptr,
                               mappedOk ? nullptr : "GL PBO could not map owned CPU frame"});
    }

    void failQueued(const char *reason)
    {
        std::deque<GlRetireJob> jobs;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            jobs.swap(m_jobs);
        }
        for (auto &job : jobs) {
            job.fence = nullptr; // owning Qt context is not current here
            deliver(GlRetireResult{std::move(job), nullptr, reason});
        }
    }

    void deliver(GlRetireResult &&result)
    {
        if (!m_onResult)
            return;
        try {
            m_onResult(std::move(result));
        } catch (...) {
            // The capture source's drain gate still guarantees shutdown.
        }
    }

    QOpenGLContext *m_context = nullptr;
    QOffscreenSurface *m_surface = nullptr;
    std::function<void(GlRetireResult &&)> m_onResult;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<GlRetireJob> m_jobs;
    bool m_stop = false;
    std::atomic<bool> m_abort{false};
    std::atomic<int> m_testDelayMs{0};
};

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
        // Session-scoped retire resources: created once per (window, Qt
        // render-context) generation, never per CaptureRequest. The
        // QOffscreenSurface is created/destroyed on the GUI thread; the
        // QOpenGLContext is created on the Qt render thread, moved onto
        // retireWorker and deleted by that worker.
        QOffscreenSurface *offscreen = nullptr;
        QOpenGLContext *sharedContext = nullptr;
        GlRetireWorker *retireWorker = nullptr;
        // At most one detached worker may be retiring. The render thread never
        // joins it; a GUI-thread completion handler clears this slot and asks
        // for one bounded resume frame. Keeping it to one also bounds the
        // number of shared contexts alive across an invalidation.
        GlRetireWorker *retiringWorker = nullptr;
        std::uint64_t contextEpoch = 0;
        bool workerStopRequested = false;
        bool stopTimedOut = false;
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
        // P1 (#962 review): creating the QOffscreenSurface is only legal on
        // the GUI thread, and so is every later destroy(). Enforce the
        // calling thread, not just the window's affinity.
        if (QThread::currentThread() != QCoreApplication::instance()->thread())
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
        // The QOffscreenSurface must be created on the GUI thread. The
        // session worker only calls makeCurrent() with it later.
        if (!s->offscreen) {
            auto *surface = new QOffscreenSurface;
            surface->setFormat(s->window->format());
            surface->create();
            if (!surface->isValid()) {
                delete surface;
                return false;
            }
            s->offscreen = surface;
        }
        s->workerStopRequested = false;
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
        if (!QCoreApplication::instance())
            return;
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            // P1 (#962 review): QOffscreenSurface and the session state are
            // GUI-thread owned. A worker/transport caller blocks here while
            // the GUI thread performs the bounded stop; the GUI thread never
            // blocks on itself.
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [s] { stopOnGuiThread(s); }, Qt::BlockingQueuedConnection);
            return;
        }
        stopOnGuiThread(s);
    }

    static void stopOnGuiThread(const std::shared_ptr<State> &s) noexcept
    {
        GlRetireWorker *worker = nullptr;
        GlRetireWorker *detached = nullptr;
        QOffscreenSurface *surface = nullptr;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
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
            s->workerStopRequested = true;
            worker = s->retireWorker;
            s->retireWorker = nullptr;
            detached = s->retiringWorker;
            s->retiringWorker = nullptr;
            s->sharedContext = nullptr; // the worker deletes its own context
            s->gpuPending = false;
            if (detached) {
                // Its queued finished handler owns deletion and s->offscreen.
                surface = nullptr;
            } else {
                surface = s->offscreen;
                s->offscreen = nullptr;
            }
        }
        std::vector<GlRetireWorker *> pending;
        if (worker) {
            worker->requestStop();
            // The worker aborts its fence poll on requestStop(); this 1s
            // bound is a safety net, not the expected path.
            if (worker->wait(1000)) {
                worker->deleteLater();
                ++g_quickGlPboWorkersRetired;
            } else {
                s->stopTimedOut = true;
                pending.push_back(worker);
            }
        }
        if (detached) {
            // Never delete this worker here: the completion task connected in
            // ensureSessionWorker owns it and the offscreen surface. Waiting
            // here still bounds the caller and proves quiescence.
            detached->requestStop();
            detached->wait(1000);
            if (!detached->isFinished())
                s->stopTimedOut = true;
            return;
        }
        if (pending.empty()) {
            if (surface) {
                surface->destroy();
                delete surface;
            }
            return;
        }
        // A genuinely stuck worker is never deleted while running and its
        // surface is destroyed only after the last worker is gone.
        auto remaining = std::make_shared<std::atomic<int>>(int(pending.size()));
        for (auto *w : pending) {
            QObject::connect(w, &QThread::finished, QCoreApplication::instance(),
                [w, surface, remaining] {
                    w->wait();
                    w->deleteLater();
                    ++g_quickGlPboWorkersRetired;
                    if (remaining->fetch_sub(1) == 1 && surface) {
                        surface->destroy();
                        delete surface;
                    }
                }, Qt::QueuedConnection);
        }
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
        // NEVER glDelete an old context's PBO name in the replacement
        // context: numeric names may alias unrelated Qt/driver resources.
        // If the original context disappeared, Qt/GL reclaimed those names.
        // The GLsync fence is owned by the session worker, never this state.
        const bool ownsCurrentContext = s.resourceContext
            && s.resourceContext.data() == QOpenGLContext::currentContext();
        if (ownsCurrentContext && !s.gpuPending && s.pbo)
            gl->glDeleteBuffers(1, &s.pbo);
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
            // The session worker may still hold the fence/PBO. Stop it and
            // drop the shared context; the old Qt render context reclaims its
            // GL names. Never glDelete from an invalid render context.
            if (s->retireWorker) {
                // Do NOT requestStop here: the GUI completion task connects
                // the finished signal first, then stops the worker. Calling
                // it here could let the worker exit before a listener exists.
                s->workerStopRequested = true;
            }
            ++g_quickGlPboInvalidations;
            s->sharedContext = nullptr;
            s->gpuPending = false;
            s->pbo = 0;
            s->fence = nullptr;
            s->pboCapacityBytes = 0;
            s->resourceContext = nullptr;
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

    static bool ensureSessionWorker(const std::shared_ptr<State> &s,
                                    QQuickWindow *window,
                                    QOpenGLContext *renderCtx)
    {
        Q_UNUSED(window);
        QOffscreenSurface *surface = nullptr;
        GlRetireWorker *existing = nullptr;
        QOpenGLContext *existingCtx = nullptr;
        bool retireRequested = false;
        GlRetireWorker *detached = nullptr;
        bool alreadyRetiring = false;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            surface = s->offscreen;
            existing = s->retireWorker;
            existingCtx = s->sharedContext;
            retireRequested = s->workerStopRequested;
            alreadyRetiring = s->retiringWorker != nullptr;
            if (!s->active)
                return false;
        }
        // P1 (#962 review): this runs inside the Qt render callback. Never
        // join the old worker here. Retire it asynchronously on the GUI
        // thread and refuse to build a new generation until that worker has
        // really finished, so two share groups never overlap.
        if (alreadyRetiring)
            return false;
        const bool matches = !retireRequested && existing && existingCtx
            && existingCtx->isValid()
            && existingCtx->shareGroup() == renderCtx->shareGroup();
        if (matches)
            return true;
        if (!surface)
            return false;
        if (existing) {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (s->retireWorker == existing)
                s->retireWorker = nullptr;
            s->retiringWorker = existing;
            s->sharedContext = nullptr;
            s->workerStopRequested = false;
            s->gpuPending = false;
            s->pbo = 0; // GL names belong to the old share group
            s->pboCapacityBytes = 0;
            s->resourceContext = nullptr;
            detached = existing;
        } else {
            std::lock_guard<std::mutex> guard(s->mutex);
            s->workerStopRequested = false;
        }
        if (detached) {
            // Connect before requestStop: a worker that exits immediately
            // must not lose its finish notification. The render thread only
            // detaches; join/deletion and the single resume frame happen on
            // the GUI thread.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [s, detached] {
                QObject::connect(detached, &QThread::finished,
                    QCoreApplication::instance(), [s, detached] {
                        detached->wait();
                        detached->deleteLater();
                        QOffscreenSurface *surface = nullptr;
                        QPointer<QQuickWindow> target;
                        std::uint64_t generation = 0;
                        bool resume = false;
                        {
                            std::lock_guard<std::mutex> guard(s->mutex);
                            if (s->retiringWorker == detached)
                                s->retiringWorker = nullptr;
                            s->drained.notify_all();
                            resume = s->active;
                            generation = s->generation;
                            target = s->window;
                            if (!resume) {
                                surface = s->offscreen;
                                s->offscreen = nullptr;
                            }
                        }
                        ++g_quickGlPboWorkersRetired;
                        if (!resume && surface) {
                            surface->destroy();
                            delete surface;
                        }
                        if (resume)
                            requestFallbackRenderIfNoNormalFrame(s, target, generation);
                    }, Qt::QueuedConnection);
                detached->requestStop();
            }, Qt::QueuedConnection);
            return false; // this frame must not use the retiring generation
        }
        auto *shared = new QOpenGLContext;
        shared->setFormat(renderCtx->format());
        shared->setShareContext(renderCtx);
        if (!shared->create() || !QOpenGLContext::areSharing(renderCtx, shared)) {
            delete shared;
            return false;
        }
        auto *worker = new GlRetireWorker(
            shared, surface,
            [s](GlRetireResult &&result) { onRetireResult(s, std::move(result)); });
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->workerStopRequested || s->retiringWorker) {
                delete worker; // never started
                delete shared;
                return false;
            }
            shared->moveToThread(worker);
            s->sharedContext = shared;
            s->retireWorker = worker;
            ++s->contextEpoch;
            ++g_quickGlPboWorkersCreated;
        }
        worker->start();
        return true;
    }

    static void onRetireResult(const std::shared_ptr<State> &s,
                               GlRetireResult &&result)
    {
        std::vector<hyremote::CaptureRequest> revoked;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != result.job.generation) {
                if (s->cpuWorkersInFlight > 0)
                    --s->cpuWorkersInFlight;
                s->drained.notify_all();
                return;
            }
            s->gpuPending = false;
            // Keep the conversion slot reserved until finishCpuConversion()
            // has adopted the CPU buffer AND claimed the requests belonging
            // to this readback. Clearing it here permits onRender() to install
            // a second batch in s->converting while the first frame is still
            // being normalized, mislabeling old pixels with new request IDs.
            if (!result.pixels) {
                s->cpuConversionPending = false;
                revoked.swap(s->converting);
            }
        }
        if (result.pixels) {
            finishCpuConversion(s, result.job.generation,
                                result.pixels, result.job.width, result.job.height,
                                result.job.dpr, result.job.renderTime);
        } else if (!revoked.empty()) {
            reportUnavailable(s, result.job.generation, std::move(revoked),
                              result.failure ? result.failure : "GL PBO retire failed");
        }
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (s->cpuWorkersInFlight > 0)
                --s->cpuWorkersInFlight;
            s->drained.notify_all();
        }
        if (auto *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [s, gen = result.job.generation] {
                QPointer<QQuickWindow> target;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != gen)
                        return;
                    target = s->window;
                }
                requestFallbackRenderIfNoNormalFrame(s, target, gen);
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
        // The session worker is created once per (window, Qt render context).
        // If its shared context cannot be qualified, fall back immediately
        // instead of entering the 750ms terminal path.
        if (!ensureSessionWorker(s, window, ctx)) {
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
                unsupported.insert(unsupported.end(),
                                   s->converting.begin(), s->converting.end());
                s->inFlight.clear();
                s->converting.clear();
                s->gpuPending = false;
                s->cpuConversionPending = false;
            }
            if (!unsupported.empty())
                reportUnavailable(s, generation, std::move(unsupported),
                                  "Qt shared OpenGL retire context unavailable");
            return;
        }
        std::vector<hyremote::CaptureRequest> failed;
        std::uint64_t gen = 0;
        bool kickAgain = false;
        const char *failureReason = "GL PBO frame readback unavailable";
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->gpuPending && s->pbo
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
            if (!s->gpuPending && s->cleanupRequested) {
                releaseGl(*s, gl);
                s->cleanupRequested = false;
            }
            // B1 (#962 review): an idle, valid PBO in the same Qt context is
            // the reusable session resource. An empty request queue is NOT a
            // reason to glDeleteBuffers and reallocate on the next capture.
            // Only stop/cleanup, invalidation, a render-context change or a
            // submission failure may release it. The worker owns the fence
            // until it completes, so no orphan fence can exist here.
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
                if (s->pbo) {
                    s->resourceContext = ctx;
                    ++g_quickGlPboBuffersCreated;
                }
            }
            if (!s->pbo) {
                failed.swap(s->queued);
                failureReason = "GL PBO allocation failed";
            } else {
                GLint oldPack = 4, oldPackPbo = 0;
                gl->glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
                gl->glGetIntegerv(kPackBinding, &oldPackPbo);
                if (s->gpuPending) {
                    // The session worker owns the fence and PBO until it
                    // completes. Never block the Qt render thread, map the
                    // PBO here, or schedule extra window->update() frames to
                    // retire a fence. The worker's completion callback clears
                    // gpuPending and requests one bounded fallback frame.
                    kickAgain = false;
                }
                if (!s->gpuPending && !s->cpuConversionPending
                    && !kickAgain && failed.empty()
                    && !s->queued.empty()) {
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
                                ++g_quickGlPboBufferDataCalls;
                            } else {
                                HYREMOTE_P1_COUNT(QuickGlPboReuse);
                                ++g_quickGlPboReuses;
                            }
                            gl->glFlush();
                            s->pixelWidth = w;
                            s->pixelHeight = h;
                            s->dpr = static_cast<float>(dpr);
                            s->gpuPending = true;
                            s->readbackRenderTime = hyremote::Clock::now();
                            s->forcedRenders = 0;
                            s->inFlight.swap(s->queued);
                            s->converting.swap(s->inFlight);
                            s->cpuConversionPending = true;
                            ++s->cpuWorkersInFlight;
                            GlRetireWorker *worker = s->retireWorker;
                            if (worker) {
                                GlRetireJob job;
                                job.pbo = s->pbo;
                                job.fence = s->fence;
                                job.width = w;
                                job.height = h;
                                job.dpr = static_cast<float>(dpr);
                                job.renderTime = s->readbackRenderTime;
                                job.generation = s->generation;
                                s->fence = nullptr; // the session worker owns it
                                worker->enqueue(std::move(job));
                            } else {
                                gl->glDeleteSync(s->fence);
                                s->fence = nullptr;
                                s->gpuPending = false;
                                s->cpuConversionPending = false;
                                failed.swap(s->converting);
                                --s->cpuWorkersInFlight;
                                failureReason = "GL PBO session worker unavailable";
                            }
                            #if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE

                            for (const auto &request : s->converting)
                                HYREMOTE_P1_TRACE_REQUEST(GpuReadbackSubmitted, request);

                            #endif
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
        if (!failed.empty())
            reportUnavailable(s, gen, std::move(failed), failureReason);
        if (kickAgain)
            QMetaObject::invokeMethod(window, [s, window, gen] {
                requestFallbackRenderIfNoNormalFrame(s, window, gen);
            }, Qt::QueuedConnection);
    }

    std::shared_ptr<State> m_state;
};
} // namespace

unsigned quickGlPboInvalidationCount()
{
    return g_quickGlPboInvalidations.load();
}

QuickGlPboStopProbe runQuickGlPboStopAbortProbe(int delayMs)
{
    QuickGlPboStopProbe out;
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);

    QOffscreenSurface surface;
    surface.setFormat(fmt);
    surface.create();
    if (!surface.isValid()) {
        out.detail = "offscreen-surface-unavailable";
        return out;
    }
    auto *render = new QOpenGLContext;
    render->setFormat(fmt);
    if (!render->create() || !render->makeCurrent(&surface)) {
        out.detail = "render-context-unavailable";
        delete render;
        return out;
    }
    out.ran = true;
    auto *gl = render->extraFunctions();
    gl->initializeOpenGLFunctions();

    constexpr int kW = 16;
    constexpr int kH = 16;
    const std::size_t bytes = std::size_t(kW) * kH * 4u;
    GLuint pbo = 0;
    gl->glGenBuffers(1, &pbo);
    gl->glBindBuffer(kPackTarget, pbo);
    gl->glBufferData(kPackTarget, GLsizeiptr(bytes), nullptr, kStreamRead);
    gl->glClearColor(0.f, 0.f, 0.f, 1.f);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    gl->glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLsync fence = gl->glFenceSync(kFenceGpu, 0);
    gl->glFlush();

    auto *shared = new QOpenGLContext;
    shared->setFormat(fmt);
    shared->setShareContext(render);
    if (!fence || !shared->create() || !QOpenGLContext::areSharing(render, shared)) {
        out.detail = "worker-setup-unavailable";
        if (fence)
            gl->glDeleteSync(fence);
        delete shared;
        gl->glBindBuffer(kPackTarget, 0);
        gl->glDeleteBuffers(1, &pbo);
        render->doneCurrent();
        delete render;
        return out;
    }

    auto *worker = new GlRetireWorker(shared, &surface,
        [](GlRetireResult &&) {});
    shared->moveToThread(worker);
    worker->setRetireDelayForTest(delayMs);
    worker->start();

    GlRetireJob job;
    job.pbo = pbo;
    job.fence = fence;
    job.width = kW;
    job.height = kH;
    job.dpr = 1.f;
    job.renderTime = hyremote::Clock::now();
    job.generation = 1;
    worker->enqueue(std::move(job));
    QThread::msleep(50); // let retireOne enter the injected delay

    const auto started = std::chrono::steady_clock::now();
    worker->requestStop();
    const bool joined = worker->wait(2000);
    out.joinMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    out.timedOut = !joined;
    out.ok = joined && out.joinMs < 500.0;
    if (joined)
        delete worker;
    else
        worker->deleteLater(); // never delete a running QThread

    gl->glBindBuffer(kPackTarget, 0);
    gl->glDeleteBuffers(1, &pbo);
    render->doneCurrent();
    delete render;
    out.detail = out.ok ? "PASS" : (joined ? "TOO_SLOW" : "TIMEOUT");
    return out;
}

QuickGlPboSessionSelfTest runQuickGlPboSessionSelfTest()
{
    QuickGlPboSessionSelfTest out;
    g_quickGlPboBuffersCreated = 0;
    g_quickGlPboBufferDataCalls = 0;
    g_quickGlPboReuses = 0;
    g_quickGlPboWorkersCreated = 0;
    g_quickGlPboWorkersRetired = 0;
    g_quickGlPboInvalidations = 0;

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);

    QOffscreenSurface surface;
    surface.setFormat(fmt);
    surface.create();
    if (!surface.isValid()) {
        out.detail = "offscreen-surface-unavailable";
        return out;
    }

    struct Capture {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        GlRetireResult result;
    };

    constexpr int kW = 64;
    constexpr int kH = 32;
    const std::size_t bytes = std::size_t(kW) * kH * 4u;

    struct Gen {
        QOpenGLContext *render = nullptr;
        QOpenGLExtraFunctions *gl = nullptr;
        GLuint fbo = 0;
        GLuint tex = 0;
        GLuint pbo = 0;
        explicit operator bool() const { return render != nullptr; }
    };

    const auto makeGen = [&]() -> Gen {
        Gen g;
        g.render = new QOpenGLContext;
        g.render->setFormat(fmt);
        if (!g.render->create() || !g.render->makeCurrent(&surface)) {
            delete g.render;
            g.render = nullptr;
            return g;
        }
        g.gl = g.render->extraFunctions();
        g.gl->initializeOpenGLFunctions();
        g.gl->glGenFramebuffers(1, &g.fbo);
        g.gl->glBindFramebuffer(GL_FRAMEBUFFER, g.fbo);
        g.gl->glGenTextures(1, &g.tex);
        g.gl->glBindTexture(GL_TEXTURE_2D, g.tex);
        g.gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kW, kH, 0,
                           GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        g.gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, g.tex, 0);
        if (g.gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            g.gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
            g.gl->glDeleteFramebuffers(1, &g.fbo);
            g.gl->glDeleteTextures(1, &g.tex);
            g.render->doneCurrent();
            delete g.render;
            g.render = nullptr;
            return g;
        }
        g.gl->glGenBuffers(1, &g.pbo);
        ++g_quickGlPboBuffersCreated;
        g.gl->glBindBuffer(kPackTarget, g.pbo);
        g.gl->glBufferData(kPackTarget, GLsizeiptr(bytes), nullptr, kStreamRead);
        ++g_quickGlPboBufferDataCalls;
        return g;
    };

    const auto retireGen = [&](Gen &g) {
        if (!g.render)
            return;
        g.gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        g.gl->glBindBuffer(kPackTarget, 0);
        g.gl->glDeleteBuffers(1, &g.pbo);
        g.gl->glDeleteFramebuffers(1, &g.fbo);
        g.gl->glDeleteTextures(1, &g.tex);
        g.render->doneCurrent();
        delete g.render;
        g.render = nullptr;
    };

    const auto submit = [&](Gen &g, GlRetireWorker *worker, Capture *capture,
                            int r, int green, int b) -> bool {
        g.gl->glBindFramebuffer(GL_FRAMEBUFFER, g.fbo);
        g.gl->glBindBuffer(kPackTarget, g.pbo);
        g.gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        g.gl->glClearColor(r / 255.f, green / 255.f, b / 255.f, 1.f);
        g.gl->glClear(GL_COLOR_BUFFER_BIT);
        g.gl->glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        GLsync fence = g.gl->glFenceSync(kFenceGpu, 0);
        g.gl->glFlush();
        if (!fence)
            return false;
        {
            std::lock_guard<std::mutex> lock(capture->mutex);
            capture->done = false;
            capture->result = GlRetireResult{};
            GlRetireJob job;
            job.pbo = g.pbo;
            job.fence = fence;
            job.width = kW;
            job.height = kH;
            job.dpr = 1.f;
            job.renderTime = hyremote::Clock::now();
            job.generation = 1;
            worker->enqueue(std::move(job));
        }
        std::unique_lock<std::mutex> lock(capture->mutex);
        if (!capture->cv.wait_for(lock, std::chrono::seconds(5),
                                  [&] { return capture->done; }))
            return false;
        return capture->result.pixels != nullptr;
    };

    const auto pixelIs = [](const GlRetireResult &res, int r, int g, int b) {
        if (!res.pixels || res.job.width <= 0 || res.job.height <= 0)
            return false;
        const auto *p = reinterpret_cast<const unsigned char *>(res.pixels.get());
        return std::abs(int(p[0]) - r) <= 8
            && std::abs(int(p[1]) - g) <= 8
            && std::abs(int(p[2]) - b) <= 8
            && p[3] >= 240;
    };

    const auto makeWorker = [&](Gen &g, Capture *capture) -> GlRetireWorker * {
        auto *shared = new QOpenGLContext;
        shared->setFormat(fmt);
        shared->setShareContext(g.render);
        if (!shared->create() || !QOpenGLContext::areSharing(g.render, shared)) {
            delete shared;
            return nullptr;
        }
        auto *worker = new GlRetireWorker(shared, &surface,
            [capture](GlRetireResult &&res) {
                {
                    std::lock_guard<std::mutex> lock(capture->mutex);
                    capture->result = std::move(res);
                    capture->done = true;
                }
                capture->cv.notify_all();
            });
        shared->moveToThread(worker);
        ++g_quickGlPboWorkersCreated;
        worker->start();
        return worker;
    };

    // Generation 1: one context, one FBO/PBO and one worker. Two requests
    // must allocate once and reuse once.
    Gen gen1 = makeGen();
    if (!gen1) {
        out.detail = "render-context-unavailable";
        return out;
    }
    out.ran = true;
    auto *capture1 = new Capture;
    auto *worker1 = makeWorker(gen1, capture1);
    bool ok = worker1 != nullptr;
    if (ok)
        ok = submit(gen1, worker1, capture1, 255, 0, 0)
            && pixelIs(capture1->result, 255, 0, 0);
    out.pixelsOk = ok;
    if (ok) {
        ++g_quickGlPboReuses;
        ok = submit(gen1, worker1, capture1, 0, 255, 0)
            && pixelIs(capture1->result, 0, 255, 0);
        out.pixelsOk = out.pixelsOk && ok;
    }
    if (worker1) {
        std::unique_lock<std::mutex> lock(capture1->mutex);
        capture1->done = false;
        out.zeroViewerNoReadback = !capture1->cv.wait_for(
            lock, std::chrono::milliseconds(150), [&] { return capture1->done; });
        worker1->requestStop();
        worker1->wait();
        delete worker1;
        ++g_quickGlPboWorkersRetired;
    }
    delete capture1;

    // Invalidation: drop the whole first generation (context, FBO, PBO).
    ++g_quickGlPboInvalidations;
    retireGen(gen1);

    // Generation 2: fresh context/PBO/worker after the invalidation epoch.
    Gen gen2 = makeGen();
    bool ok2 = ok && static_cast<bool>(gen2);
    if (ok2) {
        auto *capture2 = new Capture;
        auto *worker2 = makeWorker(gen2, capture2);
        if (worker2) {
            ok2 = submit(gen2, worker2, capture2, 0, 0, 255)
                && pixelIs(capture2->result, 0, 0, 255);
            out.pixelsOk = out.pixelsOk && ok2;
            worker2->requestStop();
            worker2->wait();
            delete worker2;
            ++g_quickGlPboWorkersRetired;
        } else {
            ok2 = false;
        }
        delete capture2;
    }
    retireGen(gen2);

    out.buffersCreated = g_quickGlPboBuffersCreated.load();
    out.bufferDataCalls = g_quickGlPboBufferDataCalls.load();
    out.reuses = g_quickGlPboReuses.load();
    out.workersCreated = g_quickGlPboWorkersCreated.load();
    out.workersRetired = g_quickGlPboWorkersRetired.load();
    out.invalidations = g_quickGlPboInvalidations.load();
    out.ok = ok && ok2 && out.ran && out.pixelsOk && out.zeroViewerNoReadback
        && out.buffersCreated == 2 && out.bufferDataCalls == 2 && out.reuses == 1
        && out.workersCreated == 2 && out.workersRetired == 2
        && out.invalidations == 1;
    out.detail = out.ok ? "PASS" : "FAIL";
    return out;
}

std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickGlPboCaptureSource(QQuickWindow *window)
{
    return std::make_unique<GlPboCaptureSource>(window);
}

} // namespace HyRemote::detail
