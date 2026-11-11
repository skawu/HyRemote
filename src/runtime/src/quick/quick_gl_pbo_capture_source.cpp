// EXPERIMENTAL, default OFF. Uses Qt6.8 public OpenGL RHI callbacks and
// PBO/fence facilities, not QtGuiPrivate, QRhi or platform-specific APIs.
// NOT RELEASE QUALIFIED. A readable default framebuffer in afterFrameEnd is
// verified on Mesa/Xvfb only; EGLFS/front-buffer semantics remain unverified.
#include "quick/quick_gl_pbo_capture_source.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPointer>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>
#include <QTimer>

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
#include <utility>
#include <vector>

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
constexpr int kNormalFrameOpportunityMs = 17;

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
        bool cleanupRequested = false;
        unsigned int callbacksInFlight = 0;
        std::vector<hyremote::CaptureRequest> queued;
        std::vector<hyremote::CaptureRequest> inFlight;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
        QMetaObject::Connection frameConnection;
        QMetaObject::Connection invalidatedConnection;
        // All GL fields below may be ACCESSED ONLY in the render callback.
        GLuint pbo = 0;
        GLsync fence = nullptr;
        bool gpuPending = false;
        int pixelWidth = 0;
        int pixelHeight = 0;
        float dpr = 1.0f;
        int forcedRenders = 0;
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
        // A queued GUI dispatch from an earlier Session may never have run.
        // Never let its posted-token suppress the first request of this
        // generation. Old tasks still verify their captured generation.
        s->kickPosted = false;
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
            s->kickPosted = false;
            s->onFrame = {};
            s->onEvent = {};
            s->cleanupRequested = true;
            s->drained.wait(guard, [&] { return s->callbacksInFlight == 0; });
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
                    auto take = [&](std::vector<hyremote::CaptureRequest> &v) {
                        auto it = std::remove_if(v.begin(), v.end(),
                            [&](const hyremote::CaptureRequest &pending) {
                                if (pending.id != id) return false;
                                expired.push_back(pending);
                                return true;
                            });
                        v.erase(it, v.end());
                    };
                    take(s->queued);
                    take(s->inFlight);
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
                    s->frameConnection = QObject::connect(
                        target, &QQuickWindow::afterFrameEnd, target,
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
        QTimer::singleShot(kNormalFrameOpportunityMs, window.data(),
            [s, window, generation] {
                if (!window || !window->isVisible())
                    return;
                bool needsFallbackRender = false;
                {
                    std::lock_guard<std::mutex> guard(s->mutex);
                    needsFallbackRender = s->active
                        && s->generation == generation
                        && !s->gpuPending
                        && !s->queued.empty();
                }
                if (needsFallbackRender)
                    window->update();
            });
    }

    static void releaseGl(State &s, QOpenGLExtraFunctions *gl)
    {
        if (s.fence) {
            gl->glDeleteSync(s.fence);
            s.fence = nullptr;
        }
        if (s.pbo) {
            gl->glDeleteBuffers(1, &s.pbo);
            s.pbo = 0;
        }
        s.gpuPending = false;
        s.pixelWidth = 0;
        s.pixelHeight = 0;
        s.forcedRenders = 0;
    }

    static void onInvalidated(const std::shared_ptr<State> &s)
    {
        // If Qt has already destroyed the GL context, its objects are
        // reclaimed by context destruction. Do not touch an invalid context.
        std::lock_guard<std::mutex> guard(s->mutex);
        auto *ctx = QOpenGLContext::currentContext();
        if (ctx) releaseGl(*s, ctx->extraFunctions());
        else {
            s->pbo = 0;
            s->fence = nullptr;
            s->gpuPending = false;
        }
        s->cleanupRequested = false;
        // A hidden/destroyed Scene Graph may never produce the retirement
        // afterFrameEnd requested by stop(). Invalidating the graph already
        // releases its GL context; revoke subscription references here.
        if (!s->active && s->connected) {
            QObject::disconnect(s->frameConnection);
            QObject::disconnect(s->invalidatedConnection);
            s->frameConnection = {};
            s->invalidatedConnection = {};
            s->connected = false;
        }
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
                            int width, int height, float dpr)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher) return;
        QMetaObject::invokeMethod(dispatcher,
            [s, generation, requests = std::move(requests),
             storage = std::move(storage), width, height, dpr] {
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
                    prototype.timing.ptsSource = hyremote::PtsSource::Completion;
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

    static void requestOneRetirementRender(const std::shared_ptr<State> &s,
                                           QQuickWindow *window,
                                           std::uint64_t generation)
    {
        // At most two extra renders per submitted GPU readback. Never
        // continuously redraw a static application to wait for a fence.
        QTimer::singleShot(25, window, [s, window, generation] {
            bool shouldRender = false;
            {
                std::lock_guard<std::mutex> guard(s->mutex);
                if (s->active && s->generation == generation
                    && s->gpuPending && s->forcedRenders < 2) {
                    ++s->forcedRenders;
                    shouldRender = true;
                }
            }
            if (shouldRender) window->update();
        });
    }

    static void onRender(const std::shared_ptr<State> &s, QQuickWindow *window)
    {
        auto *ctx = QOpenGLContext::currentContext();
        if (!ctx || !window->rendererInterface()
            || window->rendererInterface()->graphicsApi()
                   != QSGRendererInterface::OpenGL
            || ctx->format().majorVersion() < 3)
            return;
        auto *gl = ctx->extraFunctions();
        std::vector<hyremote::CaptureRequest> completed;
        std::vector<hyremote::CaptureRequest> failed;
        std::shared_ptr<const hyremote::FrameStorage> frameStorage;
        std::uint64_t gen = 0;
        int frameWidth = 0, frameHeight = 0;
        float frameDpr = 1.f;
        bool pumpAgain = false;
        bool kickAgain = false;
        const char *failureReason = "GL PBO frame readback unavailable";
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (s->cleanupRequested) {
                releaseGl(*s, gl);
                s->cleanupRequested = false;
            }
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
            if (!s->gpuPending && s->queued.empty())
                return; // NO demand: no PBO allocation or readback GL work.
            if (!s->pbo)
                gl->glGenBuffers(1, &s->pbo);
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
                        const qreal currentDpr = window->effectiveDevicePixelRatio();
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
                        auto storage = hyremote::CpuFrameStorage::createSinglePlane(
                            stride, static_cast<std::size_t>(s->pixelHeight));
                        std::byte *dest = storage ? storage->mutablePlane(0) : nullptr;
                        const auto *mapped = static_cast<const std::byte *>(
                            gl->glMapBufferRange(kPackTarget, 0, GLsizeiptr(bytes), kMapRead));
                        if (mapped && dest) {
                            for (int y=0; y<s->pixelHeight; ++y) {
                                std::memcpy(dest + std::size_t(y)*stride,
                                            mapped + std::size_t(s->pixelHeight-1-y)*stride,
                                            stride);
                            }
                            frameStorage = std::move(storage);
                            frameWidth = s->pixelWidth;
                            frameHeight = s->pixelHeight;
                            frameDpr = s->dpr;
                            completed.swap(s->inFlight);
                        } else {
                            failed.swap(s->inFlight);
                            failureReason = "GL PBO could not map owned CPU frame";
                        }
                        if (mapped) gl->glUnmapBuffer(kPackTarget);
                        } // geometryMatches
                        gl->glDeleteSync(s->fence);
                        s->fence = nullptr;
                        s->gpuPending = false;
                        s->forcedRenders = 0;
                        kickAgain = !s->queued.empty();
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
                if (!s->gpuPending && !kickAgain && failed.empty()
                    && completed.empty() && !s->queued.empty()) {
                    const qreal dpr = window->effectiveDevicePixelRatio();
                    const int w = qRound(window->width()*dpr);
                    const int h = qRound(window->height()*dpr);
                    if (w>0 && h>0 && std::size_t(w) <=
                        std::numeric_limits<std::size_t>::max() / (4u*std::size_t(h))) {
                        GLint framebuffer = 0;
                        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
                        gl->glBindBuffer(kPackTarget, s->pbo);
                        gl->glBufferData(kPackTarget, GLsizeiptr(
                            std::size_t(w)*std::size_t(h)*4u), nullptr, kStreamRead);
                        gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        gl->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        s->fence = gl->glFenceSync(kFenceGpu, 0);
                        const bool valid = s->fence && gl->glGetError() == GL_NO_ERROR;
                        if (valid) {
                            gl->glFlush();
                            s->pixelWidth = w;
                            s->pixelHeight = h;
                            s->dpr = static_cast<float>(dpr);
                            s->gpuPending = true;
                            s->forcedRenders = 0;
                            s->inFlight.swap(s->queued);
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
        if (!completed.empty())
            reportFrame(s, gen, std::move(completed),
                        std::move(frameStorage), frameWidth, frameHeight, frameDpr);
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
