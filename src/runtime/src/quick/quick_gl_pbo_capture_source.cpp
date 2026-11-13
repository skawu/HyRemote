// EXPERIMENTAL, default OFF. Uses Qt6.8 public OpenGL RHI callbacks and
// PBO/fence facilities, not QtGuiPrivate, QRhi or platform-specific APIs.
// NOT RELEASE QUALIFIED. Reads only a non-multisampled default framebuffer
// before swap; other window targets retain the public Qt fallback.
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
#include <QTimer>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQuickRenderTarget>
#endif

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
constexpr GLenum kReadFramebuffer = 0x8CAA; // GL_READ_FRAMEBUFFER_BINDING
constexpr GLenum kPackRowLength = 0x0D02;
constexpr GLenum kPackSkipRows = 0x0D03;
constexpr GLenum kPackSkipPixels = 0x0D04;
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
std::atomic<unsigned> g_quickGlPboBuffersDeleted{0};
std::atomic<unsigned> g_quickGlPboInvalidations{0};

// #967: on this route a normal Qt frame is consumed by afterRendering as soon
// as it happens, so no fixed opportunity wait is needed. Fixed-SHA traces
// showed a 17ms timer + one display frame of pure waiting before glReadPixels,
// while the readback itself took ~1ms. Arm the bounded fallback update now;
// State::fallbackRenderTimerArmed stays set until the next render callback, so
// multiple Core requests in one render generation never schedule extra Qt
// updates. A normal frame still wins: it clears the arm before the timer fires.
// The 750ms terminal-outcome timer remains the hidden-window backstop.
constexpr int kNormalFrameOpportunityMs = QuickReadbackTiming::glFallbackRenderDelayMs;

// Qt 5.15 direct OpenGL draws into the current target before afterRendering.
// Qt 6 RHI flushes recorded work when entering external commands at that point.
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
    std::uint64_t contextEpoch = 0;
};

struct GlRetireResult {
    GlRetireJob job;
    std::shared_ptr<std::byte> pixels; // null on failure
    const char *failure = nullptr;
};

class GlRetireWorker final : public QThread {
public:
    GlRetireWorker(QOpenGLContext *context, QOffscreenSurface *surface,
                   std::function<void(GlRetireResult &&)> onResult,
                   std::function<void()> onReady = {})
        : m_context(context), m_surface(surface), m_onResult(std::move(onResult)),
          m_onReady(std::move(onReady))
    {
    }

    bool ready() const noexcept
    {
        return m_ready.load(std::memory_order_acquire)
            && !m_abort.load(std::memory_order_acquire);
    }

    void ownBuffer(GLuint buffer) noexcept
    {
        m_ownedBuffer.store(buffer, std::memory_order_release);
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
        m_ready.store(true, std::memory_order_release);
        if (m_onReady)
            m_onReady();
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
        m_ready.store(false, std::memory_order_release);
        const GLuint buffer = m_ownedBuffer.exchange(0, std::memory_order_acq_rel);
        if (buffer) {
            gl->glDeleteBuffers(1, &buffer);
            if (!gl->glIsBuffer(buffer))
                ++g_quickGlPboBuffersDeleted;
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
    std::function<void()> m_onReady;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<GlRetireJob> m_jobs;
    bool m_stop = false;
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_ready{false};
    std::atomic<GLuint> m_ownedBuffer{0};
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
        bool windowBound = false;
        bool connected = false;
        bool kickPosted = false;
        // Armed for one fallback update and released by the next render
        // callback (RenderCallbackGuard), not by the QTimer callback itself.
        bool fallbackRenderTimerArmed = false;
        unsigned int callbacksInFlight = 0;
        unsigned int renderCallbacksInFlight = 0;
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
        std::unique_ptr<GlRetireWorker> retireWorker;
        QPointer<QOpenGLContext> workerRenderContext;
        QObject *dispatcher = nullptr;
        QMetaObject::Connection workerFinished;
        std::uint64_t contextEpoch = 0;
        bool workerStopRequested = false;
        bool workerFailed = false;
        // All GL fields below may be ACCESSED ONLY in the render callback.
        GLuint pbo = 0;
        GLsync fence = nullptr;
        bool gpuPending = false;
        std::size_t pboCapacityBytes = 0; // render-thread-only under State mutex
        hyremote::TimePoint readbackRenderTime{};
        // One outstanding CPU conversion per source, independent of the GL fence.
        bool cpuConversionPending = false;
        std::vector<hyremote::CaptureRequest> converting;
        int pixelWidth = 0;
        int pixelHeight = 0;
        float dpr = 1.0f;
    };

    struct RenderCallbackGuard {
        State &state;
        bool entered = false;
        explicit RenderCallbackGuard(State &s) : state(s)
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            entered = state.active;
            if (entered) {
                ++state.renderCallbacksInFlight;
                // A natural or fallback Qt frame has arrived; the single
                // fallback update armed for this generation is now consumed.
                // Do not release it in the timer callback, or every request
                // arriving before the frame would force another update().
                state.fallbackRenderTimerArmed = false;
            }
        }
        ~RenderCallbackGuard()
        {
            if (!entered)
                return;
            std::lock_guard<std::mutex> lock(state.mutex);
            --state.renderCallbacksInFlight;
            state.drained.notify_all();
        }
    };

    template<typename Callback>
    static bool postToDispatcher(const std::shared_ptr<State> &s, Callback callback)
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        return s->active && s->dispatcher
            && QMetaObject::invokeMethod(s->dispatcher, std::move(callback), Qt::QueuedConnection);
    }

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
        s->dispatcher = new QObject;
        s->workerStopRequested = false;
        s->workerFailed = false;
        s->onFrame = std::move(frame);
        s->onEvent = std::move(event);
        s->windowBound = true;
        s->active = true;
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
            // the GUI thread performs the synchronous stop; the GUI thread never
            // blocks on itself.
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [s] { stopOnGuiThread(s); }, Qt::BlockingQueuedConnection);
            return;
        }
        stopOnGuiThread(s);
    }

    static void stopOnGuiThread(const std::shared_ptr<State> &s) noexcept
    {
        std::unique_ptr<GlRetireWorker> worker;
        QOffscreenSurface *surface = nullptr;
        QObject *dispatcher = nullptr;
        {
            std::unique_lock<std::mutex> guard(s->mutex);
            s->active = false;
            ++s->generation;
            QObject::disconnect(s->frameConnection);
            QObject::disconnect(s->invalidatedConnection);
            QObject::disconnect(s->workerFinished);
            s->frameConnection = {};
            s->invalidatedConnection = {};
            s->workerFinished = {};
            s->connected = false;
            s->queued.clear();
            s->inFlight.clear();
            s->converting.clear();
            s->cpuConversionPending = false;
            s->kickPosted = false;
            s->fallbackRenderTimerArmed = false;
            s->onFrame = {};
            s->onEvent = {};
            s->drained.wait(guard, [&] { return s->renderCallbacksInFlight == 0; });
            worker = std::move(s->retireWorker);
            surface = std::exchange(s->offscreen, nullptr);
            dispatcher = std::exchange(s->dispatcher, nullptr);
            s->workerRenderContext.clear();
            resetReadbackState(*s);
        }

        // Cancellation shortens fence polling. Never report quiescence while
        // a driver call, CPU retirement, or Runtime worker can still execute.
        if (worker) {
            worker->requestStop();
            worker->wait();
            worker.reset();
            ++g_quickGlPboWorkersRetired;
        }
        delete dispatcher; // cancel pending GUI callbacks before unload
        if (surface) {
            surface->destroy();
            delete surface;
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
            QTimer::singleShot(750, s->dispatcher, [s, requestGeneration, id = req.id] {
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
        if (!postToDispatcher(s, [s, target, generation] {
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
                    s->frameConnection = QObject::connect(
                        target, &QQuickWindow::afterRendering, target,
                        [s, target] { onRender(s, target); },
                        Qt::DirectConnection);
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
            })) {
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
    static bool shouldArmFallbackRender(const State &s) noexcept
    {
        return s.active && s.dispatcher && !s.fallbackRenderTimerArmed
            && !s.queued.empty();
    }

    // #967 review: the QTimer callback runs on the GUI thread. QPointer and
    // QWindow access must be sampled BEFORE s->mutex is taken; the snapshot is
    // passed into the locked decision so an invalid or hidden window cannot
    // leave a live arm behind when no Qt frame is requested.
    enum class FallbackWindowState { Invalid, Hidden, Usable };

    static FallbackWindowState fallbackWindowState(
        const QPointer<QQuickWindow> &window) noexcept
    {
        if (window.isNull())
            return FallbackWindowState::Invalid;
        return window->isVisible() ? FallbackWindowState::Usable
                                   : FallbackWindowState::Hidden;
    }

    // Caller holds s.mutex. #967 P95 tail: share the fallback timer decision
    // with the GPU-free state-machine regression so the timer path and its
    // regression cannot drift apart.
    static bool evaluateFallbackRender(
        State &s, FallbackWindowState windowState) noexcept
    {
        // #967 review: an invalid or hidden window cannot produce a Qt frame,
        // so it must not keep the arm alive. windowState is a GUI-thread
        // snapshot taken before s->mutex; no QWindow call happens under lock.
        const bool needsFallbackRender =
            windowState == FallbackWindowState::Usable
            && !s.gpuPending
            && !s.cpuConversionPending
            && !s.queued.empty();
        // #967 P95 tail: an arm whose timer requested no Qt frame must not
        // outlive that timer. RenderCallbackGuard still owns the release when
        // a real update is issued, so one render generation keeps at most one
        // forced update. Releasing here lets the next Core request arm again
        // instead of waiting for a natural Qt frame on a static surface.
        if (!needsFallbackRender)
            s.fallbackRenderTimerArmed = false;
        return needsFallbackRender;
    }

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
        std::lock_guard<std::mutex> guard(s->mutex);
        if (s->generation != generation || !shouldArmFallbackRender(*s))
            return;
        s->fallbackRenderTimerArmed = true;
        QTimer::singleShot(kNormalFrameOpportunityMs, s->dispatcher,
            [s, window, generation] {
                const FallbackWindowState windowState =
                    fallbackWindowState(window);
                bool needsFallbackRender = false;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation)
                        return;
                    // #967 P95 tail: an arm whose timer produced no Qt frame
                    // must not survive its own callback. evaluateFallbackRender
                    // releases it only when no forced update is requested; a
                    // real update keeps it until RenderCallbackGuard.
                    needsFallbackRender =
                        evaluateFallbackRender(*s, windowState);
                }
                // needsFallbackRender already implies the Usable snapshot, so
                // update() is the only post-unlock GUI action. No second
                // lifetime check can strand a live arm here.
                if (needsFallbackRender) {
                    HYREMOTE_P1_COUNT(QuickGlExplicitRender);
                    window->update();
                }
            });
    }

    static void resetReadbackState(State &s)
    {
        // NEVER glDelete an old context's PBO name in the replacement
        // context: numeric names may alias unrelated Qt/driver resources.
        // If the original context disappeared, Qt/GL reclaimed those names.
        // The GLsync fence is owned by the session worker, never this state.
        // The worker owns deletion in the original share group.
        s.pbo = 0;
        s.fence = nullptr;
        s.gpuPending = false;
        s.pboCapacityBytes = 0;
        s.pixelWidth = 0;
        s.pixelHeight = 0;
    }

    static void onInvalidated(const std::shared_ptr<State> &s, bool sceneGraph = true)
    {
        RenderCallbackGuard callback(*s);
        if (!callback.entered)
            return;
        // If Qt has already destroyed the GL context, its objects are
        // reclaimed by context destruction. Do not touch an invalid context.
        std::vector<hyremote::CaptureRequest> revoked;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            generation = s->generation;
            ++s->contextEpoch;
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
            // The worker's finished connection is installed before it starts.
            // Retire its GL resources even if this window never renders again.
            if (s->retireWorker) {
                s->workerStopRequested = true;
                s->retireWorker->requestStop();
            }
            if (sceneGraph)
                ++g_quickGlPboInvalidations;
            s->gpuPending = false;
            s->pbo = 0;
            s->fence = nullptr;
            s->pboCapacityBytes = 0;
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
                              sceneGraph ? "Qt GL Scene Graph surface invalidated"
                                         : "Qt GL render target is not qualified for direct readback");
    }

    static void reportUnavailable(const std::shared_ptr<State> &s,
                                  std::uint64_t generation,
                                  std::vector<hyremote::CaptureRequest> requests,
                                  const char *reason)
    {
        postToDispatcher(s,
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
            });
    }

    static void reportFrame(const std::shared_ptr<State> &s,
                            std::uint64_t generation,
                            std::uint64_t contextEpoch,
                            std::vector<hyremote::CaptureRequest> requests,
                            std::shared_ptr<const hyremote::FrameStorage> storage,
                            int width, int height, float dpr,
                            hyremote::TimePoint renderTime)
    {
        postToDispatcher(s,
            [s, generation, contextEpoch, requests = std::move(requests),
             storage = std::move(storage), width, height, dpr, renderTime] {
                hyremote::FrameReadyHandler callback;
                bool current = false;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    current = s->contextEpoch == contextEpoch;
                    if (current && s->windowBound) {
                        auto *window = s->window.data();
                        const auto actualDpr = window ? captureWindowDpr(window) : 0;
                        current = window && window->isVisible()
                            && qRound(window->width() * actualDpr) == width
                            && qRound(window->height() * actualDpr) == height
                            && float(actualDpr) == dpr;
                    }
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
                    if (current)
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
            });
    }

    static void finishCpuConversion(
        const std::shared_ptr<State> &s,
        std::uint64_t generation,
        std::uint64_t contextEpoch,
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
            if (!s->active || s->generation != generation || s->contextEpoch != contextEpoch)
                return; // stop/restart revoked the work
            s->cpuConversionPending = false;
            requests.swap(s->converting); // timeout may already have retired IDs
        }
        if (!requests.empty()) {
            if (storage)
                reportFrame(s, generation, contextEpoch, std::move(requests),
                            std::move(storage), width, height, dpr, renderTime);
            else
                reportUnavailable(s, generation, std::move(requests),
                                  "GL PBO CPU frame conversion failed");
        }
    }

    enum class WorkerAdmission { Ready, Pending, Unavailable };

    static WorkerAdmission ensureSessionWorker(const std::shared_ptr<State> &s,
                                               QOpenGLContext *renderCtx)
    {
        std::lock_guard<std::mutex> guard(s->mutex);
        if (!s->active || !s->offscreen || !s->dispatcher || s->workerFailed)
            return WorkerAdmission::Unavailable;

        if (s->retireWorker) {
            if (s->workerStopRequested || s->workerRenderContext.data() != renderCtx) {
                if (!s->workerStopRequested)
                    ++s->contextEpoch;
                s->workerStopRequested = true;
                s->retireWorker->requestStop();
                return WorkerAdmission::Pending;
            }
            return s->retireWorker->ready() ? WorkerAdmission::Ready : WorkerAdmission::Pending;
        }

        auto shared = std::make_unique<QOpenGLContext>();
        shared->setFormat(renderCtx->format());
        shared->setShareContext(renderCtx);
        if (!shared->create() || !QOpenGLContext::areSharing(renderCtx, shared.get()))
            return WorkerAdmission::Unavailable;

        const auto generation = s->generation;
        auto worker = std::make_unique<GlRetireWorker>(
            shared.get(), s->offscreen,
            [s](GlRetireResult &&result) { onRetireResult(s, std::move(result)); },
            [s, generation] {
                postToDispatcher(s, [s, generation] {
                    requestFallbackRenderIfNoNormalFrame(s, s->window, generation);
                });
            });
        auto *identity = worker.get();
        shared->moveToThread(identity);
        if (shared->thread() != identity)
            return WorkerAdmission::Unavailable;
        s->workerFinished = QObject::connect(identity, &QThread::finished, s->dispatcher,
            [s, identity, generation] {
                std::unique_ptr<GlRetireWorker> finished;
                std::vector<hyremote::CaptureRequest> revoked;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != generation || s->retireWorker.get() != identity)
                        return;
                    if (!s->workerStopRequested) {
                        s->workerFailed = true;
                        revoked.swap(s->queued);
                    }
                    revoked.insert(revoked.end(), s->converting.begin(), s->converting.end());
                    s->converting.clear();
                    s->cpuConversionPending = false;
                    finished = std::move(s->retireWorker);
                    s->workerRenderContext.clear();
                    s->workerStopRequested = false;
                    resetReadbackState(*s);
                }
                finished->wait(); // already finished, on the GUI thread only
                finished.reset();
                ++g_quickGlPboWorkersRetired;
                if (!revoked.empty())
                    reportUnavailable(s, generation, std::move(revoked),
                                      "Qt GL worker generation retired");
                requestFallbackRenderIfNoNormalFrame(s, s->window, generation);
            }, Qt::QueuedConnection);
        identity->moveToThread(s->dispatcher->thread());
        shared.release(); // the worker destroys its context on its own thread
        s->workerRenderContext = renderCtx;
        s->retireWorker = std::move(worker);
        ++s->contextEpoch;
        ++g_quickGlPboWorkersCreated;
        identity->start(); // publish and start under the same ownership lock
        return WorkerAdmission::Pending;
    }

    static bool canSubmitQueuedReadback(const State &state)
    {
        return !state.gpuPending && !state.cpuConversionPending && !state.queued.empty();
    }

    static void onRetireResult(
        const std::shared_ptr<State> &s, GlRetireResult &&result,
        void (*beforeCpuAdoption)(const std::shared_ptr<State> &) = nullptr)
    {
        std::vector<hyremote::CaptureRequest> revoked;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != result.job.generation
                || s->contextEpoch != result.job.contextEpoch) {
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
            if (beforeCpuAdoption)
                beforeCpuAdoption(s);
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
            // #967: the worker fence has retired and the PBO is mapped; the
            // CPU row-normalization has not started yet. Emitted on the GL
            // worker thread with the same steady_clock domain as the GUI and
            // render-thread records.
            {
                std::lock_guard<std::mutex> guard(s->mutex);
                for (const auto &request : s->converting)
                    HYREMOTE_P1_TRACE_REQUEST(GpuReadbackReady, request);
            }
#endif
            finishCpuConversion(s, result.job.generation, result.job.contextEpoch,
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
        postToDispatcher(s, [s, gen = result.job.generation] {
            requestFallbackRenderIfNoNormalFrame(s, s->window, gen);
        });
    }


    static void onRender(const std::shared_ptr<State> &s, QQuickWindow *window)
    {
        RenderCallbackGuard callback(*s);
        if (!callback.entered)
            return;
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
                if (s->retireWorker) {
                    if (!s->workerStopRequested)
                        ++s->contextEpoch;
                    s->workerStopRequested = true;
                    s->retireWorker->requestStop();
                }
            }
            if (!unsupported.empty())
                reportUnavailable(s, generation, std::move(unsupported),
                                  "Qt OpenGL/ES 3+ render context unavailable");
            return;
        }
        auto *gl = ctx->extraFunctions();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (!window->renderTarget().isNull()) {
            onInvalidated(s, false);
            return;
        }
        window->beginExternalCommands();
        struct ExternalCommands {
            QQuickWindow *window;
            ~ExternalCommands() { window->endExternalCommands(); }
        } externalCommands{window};
#endif
        GLint readFramebuffer = -1;
        GLint samples = -1;
        gl->glGetIntegerv(kReadFramebuffer, &readFramebuffer);
        gl->glGetIntegerv(GL_SAMPLE_BUFFERS, &samples);
        if (readFramebuffer != 0 || samples != 0) {
            onInvalidated(s, false);
            return;
        }
        HYREMOTE_P1_SCOPE(QuickGlRenderCallback);
        // The session worker is created once per (window, Qt render context).
        // If its shared context cannot be qualified, fall back immediately
        // instead of entering the 750ms terminal path.
        const auto workerAdmission = ensureSessionWorker(s, ctx);
        if (workerAdmission == WorkerAdmission::Pending)
            return;
        if (workerAdmission == WorkerAdmission::Unavailable) {
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
        const char *failureReason = "GL PBO frame readback unavailable";
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            // B1 (#962 review): an idle, valid PBO in the same Qt context is
            // the reusable session resource. An empty request queue is NOT a
            // reason to glDeleteBuffers and reallocate on the next capture.
            // Only stop/cleanup, invalidation, a render-context change or a
            // submission failure may release it. The worker owns the fence
            // until it completes, so no orphan fence can exist here.
            if (!s->active || s->workerStopRequested || !s->retireWorker
                || !s->retireWorker->ready() || s->workerRenderContext.data() != ctx) {
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
                    s->retireWorker->ownBuffer(s->pbo);
                    ++g_quickGlPboBuffersCreated;
                }
            }
            if (!s->pbo) {
                failed.swap(s->queued);
                failureReason = "GL PBO allocation failed";
            } else {
                GLint oldPack = 4, oldPackPbo = 0;
                GLint oldRowLength = 0, oldSkipRows = 0, oldSkipPixels = 0;
                gl->glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
                gl->glGetIntegerv(kPackBinding, &oldPackPbo);
                gl->glGetIntegerv(kPackRowLength, &oldRowLength);
                gl->glGetIntegerv(kPackSkipRows, &oldSkipRows);
                gl->glGetIntegerv(kPackSkipPixels, &oldSkipPixels);
                if (canSubmitQueuedReadback(*s) && failed.empty()) {
                    const qreal dpr = captureWindowDpr(window);
                    const int w = qRound(window->width()*dpr);
                    const int h = qRound(window->height()*dpr);
                    if (w>0 && h>0 && std::size_t(w) <=
                        std::numeric_limits<std::size_t>::max() / (4u*std::size_t(h))) {
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
                        gl->glPixelStorei(kPackRowLength, 0);
                        gl->glPixelStorei(kPackSkipRows, 0);
                        gl->glPixelStorei(kPackSkipPixels, 0);
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
                            s->inFlight.swap(s->queued);
                            s->converting.swap(s->inFlight);
                            s->cpuConversionPending = true;
                            ++s->cpuWorkersInFlight;
                            GlRetireWorker *worker = s->retireWorker.get();
                            if (worker) {
                                GlRetireJob job;
                                job.pbo = s->pbo;
                                job.fence = s->fence;
                                job.width = w;
                                job.height = h;
                                job.dpr = static_cast<float>(dpr);
                                job.renderTime = s->readbackRenderTime;
                                job.generation = s->generation;
                                job.contextEpoch = s->contextEpoch;
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
                gl->glPixelStorei(kPackRowLength, oldRowLength);
                gl->glPixelStorei(kPackSkipRows, oldSkipRows);
                gl->glPixelStorei(kPackSkipPixels, oldSkipPixels);
            }
        }
        if (!failed.empty())
            reportUnavailable(s, gen, std::move(failed), failureReason);
    }

public:
    static bool batchOwnershipRegression()
    {
        auto s = std::make_shared<State>();
        QObject dispatcher;
        s->dispatcher = &dispatcher;
        s->active = true;
        s->generation = 1;
        const auto now = hyremote::Clock::now();
        s->converting.push_back({101, now});
        s->queued.push_back({102, now});
        s->gpuPending = true;
        s->cpuConversionPending = true;
        s->cpuWorkersInFlight = 1;
        std::vector<std::pair<hyremote::CaptureRequestId, unsigned>> delivered;
        s->onFrame = [&](hyremote::RemoteFrame frame) {
            if (!frame.requestId || !frame.storage)
                return;
            const auto plane = frame.storage->mapRead(0);
            if (plane)
                delivered.emplace_back(*frame.requestId, std::to_integer<unsigned>(plane->data[0]));
        };
        const auto submitNext = [](const std::shared_ptr<State> &state) {
            std::lock_guard<std::mutex> guard(state->mutex);
            if (!canSubmitQueuedReadback(*state))
                return;
            state->converting.swap(state->queued);
            state->gpuPending = true;
            state->cpuConversionPending = true;
            ++state->cpuWorkersInFlight;
        };
        const auto complete = [&](unsigned red, auto hook) {
            GlRetireJob job;
            job.generation = s->generation;
            job.width = 1;
            job.height = 1;
            job.dpr = 1;
            job.renderTime = now;
            auto pixels = std::shared_ptr<std::byte>(
                new std::byte[4]{std::byte(red), std::byte{0}, std::byte{0}, std::byte{255}},
                std::default_delete<std::byte[]>());
            onRetireResult(s, GlRetireResult{job, std::move(pixels), nullptr}, hook);
            QCoreApplication::processEvents();
        };
        // Deterministically enter the renderer admission window before the
        // first worker has adopted pixels and taken its matching batch.
        complete(17, +submitNext);
        const bool first = delivered.size() == 1 && delivered[0] == std::make_pair(
            hyremote::CaptureRequestId{101}, 17u);
        submitNext(s);
        complete(29, nullptr);
        return first && delivered.size() == 2 && delivered[1] == std::make_pair(
            hyremote::CaptureRequestId{102}, 29u);
    }

    static QuickGlPboFallbackPolicyRegression fallbackPolicyRegression()
    {
        QuickGlPboFallbackPolicyRegression out;
        out.ran = true;
        out.glFallbackDelayMs = kNormalFrameOpportunityMs;
        out.d3dFallbackDelayMs = QuickReadbackTiming::normalFrameOpportunityMs;
        auto s = std::make_shared<State>();
        QObject dispatcher;
        s->dispatcher = &dispatcher;
        s->active = true;
        const auto now = hyremote::Clock::now();
        s->queued.push_back({101, now});

        const bool armsOnce = shouldArmFallbackRender(*s);
        s->fallbackRenderTimerArmed = true;
        const bool suppressesSecondArm = !shouldArmFallbackRender(*s);
        // The real RenderCallbackGuard releases the arm when Qt renders;
        // releasing it in the QTimer callback would force one update per
        // request instead of one per render generation.
        {
            RenderCallbackGuard callback(*s);
            out.armReleasedByRender = callback.entered
                && !s->fallbackRenderTimerArmed;
        }
        s->queued.clear();
        const bool noArmWithoutDemand = !shouldArmFallbackRender(*s);
        out.oneArmPerRender = armsOnce && suppressesSecondArm
            && noArmWithoutDemand;

        // #967 P95 tail. Negative control: an arm left set after a timer that
        // requested no Qt frame blocks the next Core request from arming.
        s->fallbackRenderTimerArmed = true;
        s->queued.push_back({102, now});
        out.negativeControlBlocksNextArm = !shouldArmFallbackRender(*s);

        // A timer producing no forced update must release its own arm.
        s->queued.clear();
        const bool noUpdate = !evaluateFallbackRender(*s, FallbackWindowState::Usable);
        const bool released = !s->fallbackRenderTimerArmed;
        s->queued.push_back({103, now});
        out.noUpdateReleasesArm = noUpdate && released
            && shouldArmFallbackRender(*s);

        // A real forced update keeps the arm until RenderCallbackGuard, so one
        // render generation still schedules at most one update. This is the
        // negative example against one update per Core request.
        s->fallbackRenderTimerArmed = true;
        const bool needsUpdate =
            evaluateFallbackRender(*s, FallbackWindowState::Usable);
        out.updateKeepsArm = needsUpdate && s->fallbackRenderTimerArmed
            && !shouldArmFallbackRender(*s);

        // #967 review: the window can be destroyed or hidden between arming
        // and the timer. Both states must be fail-closed: no forced update is
        // requested, and the arm is released so the next request can arm.
        s->queued.clear();
        s->queued.push_back({104, now});
        s->fallbackRenderTimerArmed = true;
        const bool noUpdateOnInvalid =
            !evaluateFallbackRender(*s, FallbackWindowState::Invalid);
        out.invalidWindowReleasesArm = noUpdateOnInvalid
            && !s->fallbackRenderTimerArmed
            && shouldArmFallbackRender(*s);

        s->fallbackRenderTimerArmed = true;
        const bool noUpdateOnHidden =
            !evaluateFallbackRender(*s, FallbackWindowState::Hidden);
        out.hiddenWindowReleasesArm = noUpdateOnHidden
            && !s->fallbackRenderTimerArmed
            && shouldArmFallbackRender(*s);

        // A usable window restores one arm; the render callback releases it
        // and the following Core request can arm again.
        s->fallbackRenderTimerArmed = true;
        const bool restoredNeedsUpdate =
            evaluateFallbackRender(*s, FallbackWindowState::Usable);
        out.windowRestoredRearms = restoredNeedsUpdate
            && s->fallbackRenderTimerArmed
            && !shouldArmFallbackRender(*s);
        {
            RenderCallbackGuard callback(*s);
            out.windowRestoredRearms = out.windowRestoredRearms
                && callback.entered && !s->fallbackRenderTimerArmed
                && shouldArmFallbackRender(*s);
        }

        out.ok = out.oneArmPerRender && out.armReleasedByRender
            && out.negativeControlBlocksNextArm
            && out.noUpdateReleasesArm && out.updateKeepsArm
            && out.invalidWindowReleasesArm && out.hiddenWindowReleasesArm
            && out.windowRestoredRearms
            && out.glFallbackDelayMs == 0 && out.d3dFallbackDelayMs == 17;
        if (!out.ok)
            out.detail = "gl-fallback-window-arm-lifecycle-regressed";
        return out;
    }

private:
    std::shared_ptr<State> m_state;
};
} // namespace

unsigned quickGlPboInvalidationCount()
{
    return g_quickGlPboInvalidations.load();
}

QuickGlPboResourceCounts quickGlPboResourceCounts()
{
    return {g_quickGlPboBuffersCreated.load(), g_quickGlPboBuffersDeleted.load(),
            g_quickGlPboWorkersCreated.load(), g_quickGlPboWorkersRetired.load()};
}

bool runQuickGlPboBatchOwnershipRegression()
{
    return GlPboCaptureSource::batchOwnershipRegression();
}

QuickGlPboFallbackPolicyRegression runQuickGlPboFallbackPolicyRegression()
{
    return GlPboCaptureSource::fallbackPolicyRegression();
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
    gl->glBindBuffer(kPackTarget, 0);
    worker->ownBuffer(pbo);
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
    if (!joined)
        worker->wait(); // keep the still-owned surface alive even on test failure
    delete worker;
    out.bufferDeleted = !gl->glIsBuffer(pbo);
    out.ok = joined && out.joinMs < 500.0 && out.bufferDeleted;
    if (!out.bufferDeleted)
        gl->glDeleteBuffers(1, &pbo);
    render->doneCurrent();
    delete render;
    out.detail = out.ok ? "PASS" : (!joined ? "TIMEOUT" :
        (out.bufferDeleted ? "TOO_SLOW" : "PBO_NOT_DELETED"));
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
