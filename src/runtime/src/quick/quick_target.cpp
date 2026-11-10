#include "quick/quick_target.hpp"

#include <QCoreApplication>
#include <QImage>
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_NORMAL_FRAME_TAP)
#  include <QOpenGLContext>
#  include <QOpenGLFunctions>
#  include <QGuiApplication>
#  include <atomic>
#endif
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPointer>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QtMath>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickItemGrabResult>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "detail/input_mailbox_admission.hpp"
#include "detail/perf_probe.hpp"
#include "detail/qt_window_system_input.hpp"
#include "quick/quick_image_frame_storage.hpp"
#include "quick/quick_capture_backend.hpp"
#include "quick/quick_capture_single_flight.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/types.hpp"

namespace HyRemote::detail {
namespace {

class QuickCaptureSource final : public hyremote::CaptureSource
{
public:
    explicit QuickCaptureSource(QQuickWindow *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.regionDamage = false;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        QCoreApplication *app = QCoreApplication::instance();
        if (!app)
            return false;

        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->active || m_state->target.isNull())
            return false;
        if (m_state->target->thread() != app->thread())
            return false;

        // Every start/stop is a new capture lifetime. GUI tasks and Qt grab
        // ready/timer callbacks from a prior lifetime must not publish into
        // new handlers even if the same CaptureSource is started again.
        ++m_state->generation;
        m_state->pendingRequests.clear();
        m_state->singleFlight.reset();
        m_state->onFrame = std::move(onFrame);
        m_state->onEvent = std::move(onEvent);
        m_state->active = true;
        return true;
    }

    void stop() noexcept override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        ++m_state->generation;
        m_state->pendingRequests.clear();
        m_state->singleFlight.reset();
        m_state->onFrame = {};
        m_state->onEvent = {};

        // Ready/timer continuations may still exist after stop, but they all check active before
        // entering a Core callback. A callback that already entered is drained here.
        m_state->callbacksDrained.wait(lock, [this] { return m_state->callbacksInFlight == 0; });
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return false;

        const std::shared_ptr<State> state = m_state;
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->active)
            return false;

        // Core can issue two requests before the GUI queue is serviced.
        // One GUI-dispatch batch is one fresh offscreen Qt grab, rather than
        // two independent GPU readbacks for the exact same capture instant.
        // Requests arriving AFTER this batch starts must create a NEW grab.
        // Keep at most one actual Qt grab in progress, rather than launching
        // another asynchronous offscreen render and risking out-of-order
        // completions. Keep distinct IDs, timestamps and terminal callbacks.
        if (state->singleFlight.needsDispatch()) {
            const std::uint64_t generation = state->generation;
            if (!QMetaObject::invokeMethod(
                    dispatcher, [state, generation] {
                        dispatchQueuedBatchOnGuiThread(state, generation);
                    }, Qt::QueuedConnection)) {
                return false;
            }
            state->singleFlight.dispatchPosted();
        }
        state->pendingRequests.push_back(request);
        HYREMOTE_P1_COUNT(QuickAcceptedRequests);
        return true;
    }

private:
    static constexpr unsigned int kMaxGrabAttempts = 3U;

    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        QPointer<QQuickWindow> target;
        bool active = false;
        std::uint64_t generation = 0;
        std::size_t callbacksInFlight = 0;
        std::vector<hyremote::CaptureRequest> pendingRequests;
        QuickCaptureSingleFlight singleFlight;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_NORMAL_FRAME_TAP)
    struct ExistingFrameAttempt
    {
        // 0 waiting for normal frame; 1 reading; 2 fall back; 3 completed.
        // Only the Qt render callback may transition 0→1; the GUI timer
        // may transition 0→2; either can end as 3 exactly once.
        std::atomic<int> status{0};
        QMetaObject::Connection submissionConnection;
        QPointer<QTimer> deadline;
    };
#endif

    struct GrabBatch
    {
        std::uint64_t generation = 0;
        std::vector<hyremote::CaptureRequest> requests;
    };
    using GrabBatchPtr = std::shared_ptr<const GrabBatch>;

    struct GrabAttempt
    {
        bool finished = false;
        QSharedPointer<QQuickItemGrabResult> result;
        QMetaObject::Connection readyConnection;
        QPointer<QTimer> timeoutTimer; // GUI-thread-only, cancelled when ready
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        std::chrono::steady_clock::time_point p1GrabStart{};
#endif
    };

    static bool isActive(const std::shared_ptr<State> &state,
                         std::uint64_t generation)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->active && state->generation == generation;
    }

    static void dispatchQueuedBatchOnGuiThread(const std::shared_ptr<State> &state,
                                               std::uint64_t generation)
    {
        std::vector<hyremote::CaptureRequest> requests;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            // The slot is acquired before dispatch leaves this lock. New
            // requests are buffered until this batch finishes (or exhausts
            // its bounded retry budget), preventing overlapping GPU readbacks
            // and out-of-order source frame publications.
            if (!state->singleFlight.beginDispatchedGrab())
                return;
            requests.swap(state->pendingRequests);
        }
        if (requests.empty()) {
            finishGrabOnGuiThread(state, generation);
            return;
        }

        HYREMOTE_P1_COUNT(QuickGuiBatches);
        auto batch = std::make_shared<GrabBatch>();
        batch->generation = generation;
        batch->requests = std::move(requests);
        attemptGrabOnGuiThread(state, std::move(batch), 1U);
    }

    template <typename Handler, typename Payload>
    static void publishCallback(const std::shared_ptr<State> &state,
                                Handler State::*member,
                                Payload payload,
                                std::uint64_t generation)
    {
        Handler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            callback = state.get()->*member;
            if (!callback)
                return;
            ++state->callbacksInFlight;
        }

        try {
            callback(std::move(payload));
        } catch (...) {
            // Core callbacks are not allowed to leak exceptions through a target adapter.
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->callbacksInFlight;
            if (state->callbacksInFlight == 0)
                state->callbacksDrained.notify_all();
        }
    }

    static void publishEvent(const std::shared_ptr<State> &state,
                             std::uint64_t generation,
                             hyremote::CaptureEventCode code,
                             const char *message,
                             bool recoverable)
    {
        hyremote::CaptureEvent event;
        event.code = code;
        event.message = message;
        event.recoverable = recoverable;
        publishCallback(state, &State::onEvent, std::move(event), generation);
    }

    static void finishGrabOnGuiThread(const std::shared_ptr<State> &state,
                                      std::uint64_t generation)
    {
        bool dispatchFailure = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            state->singleFlight.grabCompleted();
            if (!state->pendingRequests.empty()
                && state->singleFlight.needsDispatch()) {
                // Deferred requests are never satisfied from the previous
                // grab. Schedule one fresh GUI batch only AFTER the previous
                // callback has fully published every accepted identity.
                QObject *dispatcher = QCoreApplication::instance();
                if (dispatcher && QMetaObject::invokeMethod(
                        dispatcher, [state, generation] {
                            dispatchQueuedBatchOnGuiThread(state, generation);
                        }, Qt::QueuedConnection)) {
                    state->singleFlight.dispatchPosted();
                } else {
                    dispatchFailure = true;
                }
            }
        }
        if (dispatchFailure)
            publishEvent(state, generation,
                         hyremote::CaptureEventCode::BackendFailure,
                         "could not schedule deferred Qt Quick capture batch",
                         false);
    }

    static void completeUnavailable(const std::shared_ptr<State> &state,
                                    const GrabBatchPtr &batch)
    {
        publishEvent(state, batch->generation,
                     hyremote::CaptureEventCode::TemporarilyUnavailable,
                     "Qt Quick capture remained unavailable after the bounded retry budget",
                     true);
        if (!isActive(state, batch->generation))
            return;

        // Even when a shared Qt grab failed, EACH admitted Core request
        // needs its own terminal completion to release its in-flight slot.
        for (const auto &request : batch->requests) {
            hyremote::RemoteFrame completion;
            completion.timing.ptsSource = hyremote::PtsSource::Completion;
            completion.timing.requestTime = request.requestTime;
            completion.timing.completionTime = hyremote::Clock::now();
            completion.requestId = request.id;
            publishCallback(state, &State::onFrame, std::move(completion),
                            batch->generation);
        }
        finishGrabOnGuiThread(state, batch->generation);
    }

    static void retryLater(const std::shared_ptr<State> &state,
                           const GrabBatchPtr &batch,
                           unsigned int attempt,
                           int delayMs)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher || !isActive(state, batch->generation))
            return;

        if (attempt >= kMaxGrabAttempts) {
            completeUnavailable(state, batch);
            return;
        }

        QTimer::singleShot(delayMs, dispatcher, [state, batch, attempt] {
            attemptGrabOnGuiThread(state, batch, attempt + 1U);
        });
    }

    static void publishImage(const std::shared_ptr<State> &state,
                             const GrabBatchPtr &batch,
                             unsigned int attempt,
                             const QImage &grabbed)
    {
        if (!isActive(state, batch->generation))
            return;
        if (state->target.isNull()) {
            publishEvent(state, batch->generation,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }
        if (grabbed.isNull()) {
            retryLater(state, batch, attempt, 100);
            return;
        }

        QImage image;
        {
            HYREMOTE_P1_SCOPE(QuickConvert);
            // Qt 6 may already return the Core's exact premultiplied RGBA
            // layout. Qt 5's ARGB32_Premultiplied has a different byte order
            // and must still be converted before publication.
            image = grabbed.format() == QImage::Format_RGBA8888_Premultiplied
                        ? grabbed
                        : grabbed.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        }
        if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
            retryLater(state, batch, attempt, 100);
            return;
        }

        // The Qt grab already produced CPU-readable, implicitly shared pixels.
        // Retain that QImage through FrameStorage instead of allocating and
        // memcpy-ing a second full framebuffer for each Viewer request.
        std::shared_ptr<const hyremote::FrameStorage> storage;
        {
            HYREMOTE_P1_SCOPE(QuickBuffer);
            storage = QuickImageFrameStorage::create(image);
        }
        if (!storage) {
            // A malformed/unsupported image must not be exposed as RGBA. Keep
            // the previous owned-copy route as a bounded correctness fallback,
            // after checking all source row accesses.
            const std::size_t width = static_cast<std::size_t>(image.width());
            const std::size_t height = static_cast<std::size_t>(image.height());
            if (image.format() != QImage::Format_RGBA8888_Premultiplied
                || width > std::numeric_limits<std::size_t>::max() / 4U
                || image.bytesPerLine() <= 0 || image.sizeInBytes() <= 0
                || !image.constBits()) {
                publishEvent(state, batch->generation,
                             hyremote::CaptureEventCode::BackendFailure,
                             "invalid Qt Quick grab image format or storage",
                             false);
                return;
            }
            const std::size_t bytesPerLine = width * 4U;
            const std::size_t sourceStride = static_cast<std::size_t>(image.bytesPerLine());
            const std::size_t sourceBytes = static_cast<std::size_t>(image.sizeInBytes());
            if (sourceStride < bytesPerLine || height > sourceBytes / sourceStride) {
                publishEvent(state, batch->generation,
                             hyremote::CaptureEventCode::BackendFailure,
                             "invalid Qt Quick grab row stride",
                             false);
                return;
            }
            std::shared_ptr<hyremote::CpuFrameStorage> copied;
            {
                HYREMOTE_P1_SCOPE(QuickBuffer);
                copied = hyremote::CpuFrameStorage::createSinglePlane(bytesPerLine, height);
            }
            if (!copied || !copied->mutablePlane(0)) {
                publishEvent(state, batch->generation,
                             hyremote::CaptureEventCode::BackendFailure,
                             "failed to allocate owned Qt Quick frame storage",
                             false);
                return;
            }
            HYREMOTE_P1_BYTES(QuickBuffer, bytesPerLine * height);
            std::byte *destination = copied->mutablePlane(0);
            {
                HYREMOTE_P1_SCOPE(QuickCopy);
                for (int y = 0; y < image.height(); ++y)
                    std::memcpy(destination + static_cast<std::size_t>(y) * bytesPerLine,
                                image.constScanLine(y), bytesPerLine);
            }
            HYREMOTE_P1_BYTES(QuickCopy, bytesPerLine * height);
            storage = std::move(copied);
        }

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame prototype;
        prototype.geometry.size = {static_cast<std::uint32_t>(image.width()),
                                   static_cast<std::uint32_t>(image.height())};
        prototype.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        prototype.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        prototype.geometry.planeCount = 1;
        prototype.geometry.devicePixelRatio =
            image.devicePixelRatio() > 0.0 ? static_cast<float>(image.devicePixelRatio()) : 1.0F;
        prototype.storage = std::move(storage);
        prototype.timing.ptsSource = hyremote::PtsSource::Completion;
        prototype.timing.completionTime = completion;
        prototype.damage = hyremote::Damage::fullFrame();

        // One Qt offscreen render, ONE CPU conversion and ONE immutable
        // FrameStorage owner; distinct Core completions retain their original
        // request identities. Qt readback is never reused across GUI batches.
        for (const auto &request : batch->requests) {
            hyremote::RemoteFrame frame = prototype;
            frame.timing.requestTime = request.requestTime;
            frame.requestId = request.id;
            publishCallback(state, &State::onFrame, std::move(frame),
                            batch->generation);
        }
        finishGrabOnGuiThread(state, batch->generation);
    }

#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_NORMAL_FRAME_TAP)
    // A STRICT experiment for Qt6.8+ OpenGL-RHI only. The public Qt render
    // signal afterFrameEnd means commands are submitted; unlike Qt5 classic
    // GL, afterRendering must NOT be used as a generic Qt6 completion point.
    // The GL context/readable backbuffer are independently checked in the
    // render thread; unavailable resources fall back to public item grab.
    //
    // ViewerCaptureDemand and Core's in-flight/bounded scheduling are upstream
    // of requestFrame(), so no unsolicited render frames are ever published.
    // No manual QQuickWindow::update(), no grabToImage() from render callbacks,
    // no QtPrivate, and no RHI/swapchain pointer dereferences.
    static bool tryExistingGlFrameOnGuiThread(
        const std::shared_ptr<State> &state,
        const GrabBatchPtr &batch, unsigned int grabAttempt,
        QQuickWindow *window)
    {
        if (!window || !window->isVisible())
            return false;
        const auto *ri = window->rendererInterface();
        if (!ri || ri->graphicsApi() != QSGRendererInterface::OpenGL)
            return false;
        // The renderer can scale the physical render target through DPR.
        // Only accept an exact integer physical framebuffer geometry; other
        // modes retain the correctness-proven public Qt grab.
        const qreal dpr = window->effectiveDevicePixelRatio();
        const qreal physicalWidth = window->width() * dpr;
        const qreal physicalHeight = window->height() * dpr;
        const int width = qRound(physicalWidth);
        const int height = qRound(physicalHeight);
        if (dpr <= 0.0 || width <= 0 || height <= 0
            || qAbs(physicalWidth - width) > 0.01
            || qAbs(physicalHeight - height) > 0.01
            || width > 8192 || height > 8192)
            return false;

        QObject *gui = QCoreApplication::instance();
        if (!gui || QThread::currentThread() != gui->thread())
            return false;

        auto tap = std::make_shared<ExistingFrameAttempt>();
        tap->submissionConnection = QObject::connect(
            window, &QQuickWindow::afterFrameEnd, gui,
            [state, batch, grabAttempt, tap, width, height, dpr] {
                int expected = 0;
                if (!tap->status.compare_exchange_strong(
                        expected, 1, std::memory_order_acq_rel))
                    return;
                QImage image;
                bool valid = false;
                if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
                    if (QOpenGLFunctions *gl = context->functions()) {
                        image = QImage(width, height,
                                       QImage::Format_RGBA8888_Premultiplied);
                        if (!image.isNull()) {
                            GLint previousPack = 4;
                            gl->glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
                            gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
                            // Qt6/GL afterFrameEnd may still have a current
                            // context. Its physical backing buffer must be
                            // validated on EACH frame, never cached across
                            // swaps/resizes/device recovery.
                            gl->glReadPixels(0, 0, width, height, GL_RGBA,
                                             GL_UNSIGNED_BYTE, image.bits());
                            const GLenum glError = gl->glGetError();
                            gl->glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
                            valid = glError == GL_NO_ERROR;
                        }
                    }
                }
                if (QObject *dispatcher = QCoreApplication::instance()) {
                    QMetaObject::invokeMethod(
                        dispatcher,
                        [state, batch, grabAttempt, tap, image = std::move(image),
                         valid, dpr] {
                            // Guard all callbacks against stop/restart and
                            // timeout before touching the old request batch.
                            int reading = 1;
                            if (!tap->status.compare_exchange_strong(
                                    reading, 3, std::memory_order_acq_rel))
                                return;
                            QObject::disconnect(tap->submissionConnection);
                            if (tap->deadline) {
                                tap->deadline->stop();
                                tap->deadline->deleteLater();
                                tap->deadline.clear();
                            }
                            if (!isActive(state, batch->generation))
                                return;
                            if (!valid || image.isNull()) {
                                attemptGrabOnGuiThread(state, batch,
                                                       grabAttempt, true);
                                return;
                            }
                            // glReadPixels returns lower-left origin. Flip on
                            // GUI, not on the live Scene Graph render thread.
                            QImage upright = image.mirrored(false, true);
                            upright.setDevicePixelRatio(dpr);
                            publishImage(state, batch, grabAttempt, upright);
                        }, Qt::QueuedConnection);
                }
            }, Qt::DirectConnection);

        if (!tap->submissionConnection)
            return false;

        auto *timer = new QTimer(gui);
        timer->setSingleShot(true);
        tap->deadline = timer;
        QObject::connect(timer, &QTimer::timeout, gui,
                         [state, batch, grabAttempt, tap] {
            if (tap->deadline) {
                tap->deadline->deleteLater();
                tap->deadline.clear();
            }
            int waiting = 0;
            if (tap->status.compare_exchange_strong(
                    waiting, 2, std::memory_order_acq_rel)) {
                // Static Qt scene: there may be no normal frame to observe.
                // Do not call QQuickWindow::update() to manufacture one;
                // request a bounded, independent public grab instead.
                QObject::disconnect(tap->submissionConnection);
                if (isActive(state, batch->generation))
                    attemptGrabOnGuiThread(state, batch, grabAttempt, true);
                return;
            }
            // A synchronous GPU readback started but has not returned.
            // Never launch a second overlapping Qt grab. Keep the Core
            // request bounded and report a terminal unavailable result
            // after the existing retry budget.
            if (waiting == 1) {
                QTimer::singleShot(2000, QCoreApplication::instance(),
                    [state, batch, tap] {
                        int reading = 1;
                        if (tap->status.compare_exchange_strong(
                                reading, 3, std::memory_order_acq_rel)) {
                            QObject::disconnect(tap->submissionConnection);
                            if (isActive(state, batch->generation))
                                completeUnavailable(state, batch);
                        }
                    });
            }
        });
        // 45ms gives normal Qt60Hz rendering up to ~2 frame opportunities
        // while preserving a short bounded fallback for static scenes.
        timer->start(45);
        return true;
    }
#endif

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const GrabBatchPtr &batch,
                                       unsigned int attempt,
                                       bool skipRenderTap = false)
    {
        if (!isActive(state, batch->generation))
            return;

        QQuickWindow *window = state->target.data();
        if (!window) {
            publishEvent(state, batch->generation,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }

        if (!window->isVisible() || window->visibility() == QWindow::Minimized
            || window->width() <= 0 || window->height() <= 0) {
            retryLater(state, batch, attempt, 250);
            return;
        }

        QQuickItem *content = window->contentItem();
        if (!content) {
            publishEvent(state, batch->generation,
                         hyremote::CaptureEventCode::BackendFailure,
                         "QQuickWindow has no content item",
                         false);
            return;
        }

        // grabToImage() takes logical item dimensions and applies DPR itself.
        const QSize logicalSize(qMax(1, qCeil(window->width())),
                                qMax(1, qCeil(window->height())));
        const auto grab = std::make_shared<GrabAttempt>();
        // RendererInterface is Qt's public backend identity, not evidence
        // of permission for a private GPU readback implementation. Public
        // Qt capture is the selected route on software/GL/D3D/Vulkan/Metal,
        // and also when the renderer has not yet reported an API.
        const auto *renderer = window->rendererInterface();
        const auto selection = selectQuickCaptureRoute(
            renderer ? renderer->graphicsApi() : QSGRendererInterface::Unknown);
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_NORMAL_FRAME_TAP) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (!skipRenderTap && tryExistingGlFrameOnGuiThread(
                                  state, batch, attempt, window))
            return;
#else
        Q_UNUSED(skipRenderTap);
#endif
        if (selection.route != QuickCaptureRoute::PublicQtGrabToImage)
            return; // future new route must implement its own safety contract
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        static_assert(static_cast<std::size_t>(QuickCaptureBackend::Other)
                          == static_cast<std::size_t>(
                              ::HyRemote::detail::p1perf::QuickGraphicsBackend::Other),
                      "Qt backend diagnostic categories must have identical order");
        HYREMOTE_P1_QUICK_BACKEND(static_cast<
            ::HyRemote::detail::p1perf::QuickGraphicsBackend>(selection.backend));
#endif
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        grab->p1GrabStart = std::chrono::steady_clock::now();
#endif
        {
            HYREMOTE_P1_SCOPE(QuickGrabSubmit);
            grab->result = content->grabToImage(logicalSize);
        }
        if (grab->result.isNull()) {
            retryLater(state, batch, attempt, 100);
            return;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return;

        grab->readyConnection = QObject::connect(
            grab->result.data(),
            &QQuickItemGrabResult::ready,
            dispatcher,
            [state, batch, attempt, grab] {
                if (grab->finished)
                    return;
                grab->finished = true;
                QObject::disconnect(grab->readyConnection);
                // The old QTimer::singleShot(2000, ...) always stayed queued
                // for two seconds even if the image completed in milliseconds.
                // Under sustained captures this retains request/state closures
                // and wakes the Qt event loop only to return immediately.
                // A cancelable, dispatcher-owned timer releases them on ready.
                if (grab->timeoutTimer) {
                    grab->timeoutTimer->stop();
                    grab->timeoutTimer->deleteLater();
                    grab->timeoutTimer.clear();
                }
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
                ::HyRemote::detail::p1perf::addAsyncWall(
                    ::HyRemote::detail::p1perf::Stage::QuickGrabReadyWall,
                    std::chrono::steady_clock::now() - grab->p1GrabStart);
#endif
                const QImage image = grab->result ? grab->result->image() : QImage();
                grab->result.clear();
                publishImage(state, batch, attempt, image);
            });

        // The deadline is owned by the GUI dispatcher and uses Qt's normal
        // QObject cancellation semantics on Qt 5.15 and Qt 6.x. The weak
        // QPointer in GrabAttempt avoids timer/closure ownership cycles.
        auto *deadline = new QTimer(dispatcher);
        deadline->setSingleShot(true);
        grab->timeoutTimer = deadline;
        QObject::connect(deadline, &QTimer::timeout, dispatcher,
                         [state, batch, attempt, grab] {
            if (grab->timeoutTimer) {
                grab->timeoutTimer->deleteLater();
                grab->timeoutTimer.clear();
            }
            if (grab->finished)
                return;
            grab->finished = true;
            QObject::disconnect(grab->readyConnection);
            grab->result.clear();
            if (!isActive(state, batch->generation))
                return;
            if (state->target.isNull()) {
                publishEvent(state, batch->generation,
                             hyremote::CaptureEventCode::TargetLost,
                             "the QQuickWindow target was destroyed during capture",
                             false);
                return;
            }
            retryLater(state, batch, attempt, 100);
        });
        deadline->start(2000);
    }

    std::shared_ptr<State> m_state;
};

Qt::KeyboardModifiers toQtModifiers(hyremote::InputModifiers modifiers)
{
    Qt::KeyboardModifiers result = Qt::NoModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Shift))
        result |= Qt::ShiftModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Control))
        result |= Qt::ControlModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Alt))
        result |= Qt::AltModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Meta))
        result |= Qt::MetaModifier;
    return result;
}

int toQtKey(hyremote::KeyCode key)
{
    using hyremote::KeyCode;
    switch (key) {
    case KeyCode::Enter: return Qt::Key_Return;
    case KeyCode::Escape: return Qt::Key_Escape;
    case KeyCode::Tab: return Qt::Key_Tab;
    case KeyCode::Backspace: return Qt::Key_Backspace;
    case KeyCode::DeleteForward: return Qt::Key_Delete;
    case KeyCode::Insert: return Qt::Key_Insert;
    case KeyCode::Home: return Qt::Key_Home;
    case KeyCode::End: return Qt::Key_End;
    case KeyCode::PageUp: return Qt::Key_PageUp;
    case KeyCode::PageDown: return Qt::Key_PageDown;
    case KeyCode::ArrowLeft: return Qt::Key_Left;
    case KeyCode::ArrowUp: return Qt::Key_Up;
    case KeyCode::ArrowRight: return Qt::Key_Right;
    case KeyCode::ArrowDown: return Qt::Key_Down;
    case KeyCode::Space: return Qt::Key_Space;
    case KeyCode::Shift: return Qt::Key_Shift;
    case KeyCode::Control: return Qt::Key_Control;
    case KeyCode::Alt: return Qt::Key_Alt;
    case KeyCode::Meta: return Qt::Key_Meta;
    case KeyCode::CapsLock: return Qt::Key_CapsLock;
    case KeyCode::NumLock: return Qt::Key_NumLock;
    case KeyCode::Unknown: return 0;
    default: break;
    }

    if (key >= KeyCode::Digit0 && key <= KeyCode::Digit9)
        return Qt::Key_0 + (static_cast<int>(key) - static_cast<int>(KeyCode::Digit0));
    if (key >= KeyCode::A && key <= KeyCode::Z)
        return Qt::Key_A + (static_cast<int>(key) - static_cast<int>(KeyCode::A));
    if (key >= KeyCode::F1 && key <= KeyCode::F12)
        return Qt::Key_F1 + (static_cast<int>(key) - static_cast<int>(KeyCode::F1));
    return 0;
}

std::optional<Qt::KeyboardModifier> modifierForQtKey(int key)
{
    switch (key) {
    case Qt::Key_Shift: return Qt::ShiftModifier;
    case Qt::Key_Control: return Qt::ControlModifier;
    case Qt::Key_Alt: return Qt::AltModifier;
    case Qt::Key_Meta: return Qt::MetaModifier;
    default: return std::nullopt;
    }
}

class QuickInputSink final : public hyremote::InputSink
{
public:
    explicit QuickInputSink(QQuickWindow *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
    }

    ~QuickInputSink() override { shutdown(); }

    void post(const hyremote::InputEvent &event) override
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            throw std::runtime_error("Qt application event dispatcher is unavailable");

        const std::shared_ptr<State> state = m_state;
        bool scheduleDrain = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;

            const InputMailboxAdmission::Class admissionClass = state->admission.classify(event);
            if (admissionClass == InputMailboxAdmission::Class::DropUnmatchedRelease)
                return;

            const unsigned long acceptedTimestamp = qtWindowSystemTimestamp();
            if (admissionClass == InputMailboxAdmission::Class::ProtectedRelease) {
                if (!state->admission.canAcceptProtectedRelease())
                    throw std::runtime_error("bounded Qt Quick protected-release mailbox is full");
                state->admission.acceptProtectedRelease(event);
            } else {
                if (!state->admission.canAcceptNormal())
                    throw std::runtime_error("bounded Qt Quick input mailbox is full");
                state->admission.acceptNormal(event);
            }
            state->pending.push_back(QueuedInput{event, acceptedTimestamp});

            if (!state->drainScheduled) {
                state->drainScheduled = true;
                scheduleDrain = true;
            }
        }

        if (!scheduleDrain)
            return;

        if (!QMetaObject::invokeMethod(dispatcher, [state] { drainOnGuiThread(state); }, Qt::QueuedConnection)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->drainScheduled = false;
            throw std::runtime_error("failed to queue remote input drain to the Qt Quick GUI thread");
        }
    }

    void shutdown() noexcept override
    {
        QObject *dispatcher = QCoreApplication::instance();
        const std::shared_ptr<State> state = m_state;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->shutdownRequested)
                return;
            state->shutdownRequested = true;
            state->active = false;
            state->pending.clear();
            state->admission.resetAll();
            state->drainScheduled = false;
        }

        if (!dispatcher)
            return;

        const auto release = [state] { releaseHeldStateOnGuiThread(state); };
        if (QThread::currentThread() == dispatcher->thread()) {
            release();
            return;
        }
        (void)QMetaObject::invokeMethod(dispatcher, release, Qt::QueuedConnection);
    }

private:
    struct QueuedInput
    {
        hyremote::InputEvent event;
        unsigned long acceptedTimestamp = 0;
    };

    struct State
    {
        std::mutex mutex;
        QPointer<QQuickWindow> target;
        bool active = true;
        bool drainScheduled = false;
        bool shutdownRequested = false;
        std::deque<QueuedInput> pending;
        InputMailboxAdmission admission;

        QtWindowSystemPointerState pointer;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::vector<int> heldKeys;
    };

    static void deliverPointer(const std::shared_ptr<State> &state,
                               QQuickWindow *window,
                               const hyremote::InputEvent &event,
                               unsigned long timestamp)
    {
        const std::optional<hyremote::MappedInputPoint> mapped =
            hyremote::mapPointerToTarget(event,
                                        static_cast<float>(window->width()),
                                        static_cast<float>(window->height()));
        if (!mapped)
            return;

        const QPointF local(mapped->x, mapped->y);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPointF global = window->mapToGlobal(local);
#else
        const QPointF global = QPointF(window->mapToGlobal(local.toPoint()));
#endif
        deliverQtWindowSystemPointer(window, event, local, global, timestamp, state->pointer);
    }

    static void deliverKey(const std::shared_ptr<State> &state,
                           QQuickWindow *window,
                           const hyremote::InputEvent &event)
    {
        const int key = toQtKey(event.key);
        if (key == 0)
            return;

        state->modifiers = toQtModifiers(event.modifiers);
        auto held = std::find(state->heldKeys.begin(), state->heldKeys.end(), key);
        if (event.pressed) {
            if (held == state->heldKeys.end())
                state->heldKeys.push_back(key);
        } else if (held != state->heldKeys.end()) {
            const auto modifier = modifierForQtKey(key);
            if (!modifier || !(state->modifiers & *modifier))
                state->heldKeys.erase(held);
        }

        if (event.pressed && !window->isActive()) {
            window->requestActivate();
            window->raise();
        }

        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease, key, state->modifiers);
        QCoreApplication::sendEvent(window, &keyEvent);
    }

    static void deliverText(QQuickWindow *window, const hyremote::InputEvent &event)
    {
        if (event.textUtf8.empty())
            return;
        QInputMethodEvent inputMethod;
        inputMethod.setCommitString(QString::fromUtf8(event.textUtf8.data(),
                                                      static_cast<qsizetype>(event.textUtf8.size())));
        QCoreApplication::sendEvent(window, &inputMethod);
    }

    static void releaseHeldStateOnGuiThread(const std::shared_ptr<State> &state)
    {
        QQuickWindow *window = state->target.data();
        if (window)
            releaseQtWindowSystemPointer(window, qtWindowSystemTimestamp(), state->pointer);
        else
            state->pointer = {};

        if (!window) {
            state->heldKeys.clear();
            state->modifiers = Qt::NoModifier;
            return;
        }

        const std::vector<int> held = state->heldKeys;
        for (int key : held) {
            if (modifierForQtKey(key))
                continue;
            QKeyEvent release(QEvent::KeyRelease, key, state->modifiers);
            QCoreApplication::sendEvent(window, &release);
        }
        for (int key : held) {
            const auto modifier = modifierForQtKey(key);
            if (!modifier)
                continue;
            state->modifiers &= ~Qt::KeyboardModifiers(*modifier);
            QKeyEvent release(QEvent::KeyRelease, key, state->modifiers);
            QCoreApplication::sendEvent(window, &release);
        }
        state->heldKeys.clear();
        state->modifiers = Qt::NoModifier;
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state, const QueuedInput &queued)
    {
        QQuickWindow *window = state->target.data();
        if (!window)
            return;

        switch (queued.event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointer(state, window, queued.event, queued.acceptedTimestamp);
            break;
        case hyremote::InputEventKind::Key:
            deliverKey(state, window, queued.event);
            break;
        case hyremote::InputEventKind::Text:
            deliverText(window, queued.event);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        std::deque<QueuedInput> batch;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->admission.resetAll();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->admission.pendingBatchTaken();
            state->drainScheduled = false;
        }

        for (const QueuedInput &queued : batch) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
            }
            deliverOnGuiThread(state, queued);
        }
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

TargetComponents createQuickTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetComponents result;
    auto *window = qobject_cast<QQuickWindow *>(target);
    if (!window)
        return result;

    result.supported = true;
    result.capture = std::make_unique<QuickCaptureSource>(window);
    if (remoteInputEnabled)
        result.input = std::make_shared<QuickInputSink>(window);
    return result;
}

}  // namespace HyRemote::detail
