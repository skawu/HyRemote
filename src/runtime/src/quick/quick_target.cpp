#include "quick/quick_target.hpp"

#include <QCoreApplication>
#include <QImage>
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
#include <cmath>
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
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO)
#  include "quick/quick_gl_pbo_capture_source.hpp"
#endif
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING)
#  include "quick/quick_d3d11_staging_capture_source.hpp"
#endif
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO) || defined(HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING)
#  include "quick/quick_adaptive_capture_source.hpp"
#  include <QDebug>
#endif
#include "quick/quick_capture_single_flight.hpp"
#include "quick/quick_grab_geometry.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/types.hpp"

namespace HyRemote::detail {
namespace {

// Qt Quick output frame geometry is expressed in pixels, while grabToImage's
// targetSize is NOT the same pixel contract across Qt versions: Qt5 takes it
// literally, Qt6 multiplies it by the window's effective device pixel ratio.
// See tagged qtdeclarative qquickitemgrabresult.cpp and quick_grab_geometry.hpp.
// Direct Quick exports physical pixels; Automatic composite exports 1x logical.
inline qreal quickWindowDpr(const QQuickWindow *window) noexcept
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return window->devicePixelRatio();
#else
    return window->effectiveDevicePixelRatio();
#endif
}

inline QSize quickPhysicalSize(const QQuickWindow *window) noexcept
{
    const qreal dpr = quickWindowDpr(window);
    if (!std::isfinite(dpr) || dpr <= 0.0)
        return {};
    const qreal width = static_cast<qreal>(window->width()) * dpr;
    const qreal height = static_cast<qreal>(window->height()) * dpr;
    if (!std::isfinite(width) || !std::isfinite(height)
        || width < 1.0 || height < 1.0
        || width > 65535.0 || height > 65535.0)
        return {};
    return QSize(qRound(width), qRound(height));
}

inline QSize quickOutputPixelSize(
    const QQuickWindow *window, bool logicalPixelsForComposite) noexcept
{
    if (logicalPixelsForComposite) {
        if (window->width() <= 0 || window->height() <= 0
            || window->width() > 65535 || window->height() > 65535)
            return {};
        return QSize(window->width(), window->height());
    }
    return quickPhysicalSize(window);
}

class QuickCaptureSource final : public hyremote::CaptureSource
{
public:
    explicit QuickCaptureSource(QQuickWindow *target, bool logicalPixelsForComposite)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
        m_state->logicalPixelsForComposite = logicalPixelsForComposite;
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
        bool logicalPixelsForComposite = false; // immutable source identity
        bool active = false;
        std::uint64_t generation = 0;
        std::size_t callbacksInFlight = 0;
        std::vector<hyremote::CaptureRequest> pendingRequests;
        QuickCaptureSingleFlight singleFlight;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

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
        QSize requestedPixelSize;
        qreal requestedDpr = 1.0; // output pixel-grid metadata
        qreal windowDpr = 1.0; // actual Qt DPR to check async staleness
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
                             const QImage &grabbed,
                             const QSize &expectedPixelSize,
                             qreal expectedDpr,
                             qreal expectedWindowDpr)
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
        // An asynchronous grab may finish after a DPR change or resize.
        // Never publish old-size pixels under a new RFB Surface identity.
        const auto *window = state->target.data();
        // A direct Quick session promises native physical RFB dimensions;
        // that path MUST match the requested render target exactly.
        // Composite instead owns a logical output canvas and scales child
        // QImages into their destination rectangles. On high-DPI Qt Quick,
        // grabToImage() may return a native-sized image even when given a
        // logical targetSize. Rejecting that valid child solely because its
        // QImage pixel count differs would leave a blank composite and, after
        // repeated failures, could fault the parent capture Session.
        const bool acceptableImageGrid = state->logicalPixelsForComposite
            ? grabbed.width() > 0 && grabbed.height() > 0
              && grabbed.width() <= 65535 && grabbed.height() <= 65535
            : grabbed.size() == expectedPixelSize;
        if (!window || expectedPixelSize.isEmpty()
            || !acceptableImageGrid
            || quickOutputPixelSize(window, state->logicalPixelsForComposite)
                   != expectedPixelSize
            || std::abs(quickWindowDpr(window) - expectedWindowDpr) > 0.0001) {
            retryLater(state, batch, attempt, 100);
            return;
        }

        QImage image;
        {
            HYREMOTE_P1_SCOPE(QuickConvert);
            // Qt 6 may already return the Core's exact premultiplied RGBA
            // layout. Qt 5's ARGB32_Premultiplied has a different byte order
            // and must still be converted before publication.
            // Qt5 OpenGL/ES readbacks commonly produce RGBX8888 for
            // opaque scenes. Qt guarantees its X byte is 255, so this format
            // is already byte-for-byte equivalent to premultiplied RGBA and
            // can share the QImage CPU buffer with no full-frame conversion.
            const auto format = grabbed.format();
            image = (format == QImage::Format_RGBA8888_Premultiplied
                     || format == QImage::Format_RGBX8888)
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
            if ((image.format() != QImage::Format_RGBA8888_Premultiplied
                 && image.format() != QImage::Format_RGBX8888)
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
        // Metadata describes the owned OUTPUT canvas contract. The Qt6
        // offscreen texture may round fractional DPR into +/-1 buffer pixel;
        // that rounding does not change the logical Composite canvas DPR.
        prototype.geometry.devicePixelRatio = static_cast<float>(expectedDpr);
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

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const GrabBatchPtr &batch,
                                       unsigned int attempt)
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

        // Direct Quick captures its physical grid, but the frozen Automatic
        // canvas is logical 1x. Preserve both contracts without duplicating
        // the shared CaptureSource or performing a redundant 2.25x readback.
        const QSize pixelSize = quickOutputPixelSize(
            window, state->logicalPixelsForComposite);
        if (pixelSize.isEmpty()) {
            retryLater(state, batch, attempt, 100);
            return;
        }
        const auto grab = std::make_shared<GrabAttempt>();
        grab->requestedPixelSize = pixelSize;
        grab->windowDpr = quickWindowDpr(window);
        const QSize qtTargetSize = quickGrabTargetForOutput(
            pixelSize, window->size(), grab->windowDpr,
            state->logicalPixelsForComposite);
        if (qtTargetSize.isEmpty()) {
            retryLater(state, batch, attempt, 100);
            return;
        }
        grab->requestedDpr = state->logicalPixelsForComposite
            ? 1.0 : grab->windowDpr;
        // RendererInterface is Qt's public backend identity, not evidence
        // of permission for a private GPU readback implementation. Public
        // Qt capture is the selected route on software/GL/D3D/Vulkan/Metal,
        // and also when the renderer has not yet reported an API.
        const auto *renderer = window->rendererInterface();
        const auto selection = selectQuickCaptureRoute(
            renderer ? renderer->graphicsApi() : QSGRendererInterface::Unknown);
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
            grab->result = content->grabToImage(qtTargetSize);
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
                publishImage(state, batch, attempt, image,
                             grab->requestedPixelSize, grab->requestedDpr,
                             grab->windowDpr);
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

TargetComponents createQuickTargetComponents(
    QObject *target, bool remoteInputEnabled, bool logicalPixelsForComposite)
{
    TargetComponents result;
    auto *window = qobject_cast<QQuickWindow *>(target);
    if (!window)
        return result;

    result.supported = true;
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO) || defined(HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING)
    const auto *ri = window->rendererInterface();
    const auto selection = selectQuickCaptureRoute(
        ri ? ri->graphicsApi() : QSGRendererInterface::Unknown,
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO)
        true,
#else
        false,
#endif
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING)
        true,
#else
        false,
#endif
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO)
        true
#else
        false
#endif
    );
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO)
    if (selection.route == QuickCaptureRoute::ExperimentalOpenGlPbo) {
        qInfo() << "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=GL_RHI_PBO_SOURCE";
        result.capture = std::make_unique<AdaptiveQuickCaptureSource>(
            window, createExperimentalQuickGlPboCaptureSource(window),
            std::make_unique<QuickCaptureSource>(
                window, logicalPixelsForComposite));
    }
#endif
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING)
    if (selection.route == QuickCaptureRoute::ExperimentalD3d11Staging) {
        qInfo() << "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=D3D11_STAGING_SOURCE";
        result.capture = std::make_unique<AdaptiveQuickCaptureSource>(
            window, createExperimentalQuickD3d11StagingCaptureSource(window),
            std::make_unique<QuickCaptureSource>(
                window, logicalPixelsForComposite));
    }
#endif
    if (!result.capture) {
        qInfo() << "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=PUBLIC_QT_FALLBACK";
        result.capture = std::make_unique<QuickCaptureSource>(
            window, logicalPixelsForComposite);
    }
#else
    result.capture = std::make_unique<QuickCaptureSource>(
        window, logicalPixelsForComposite);
#endif
    if (remoteInputEnabled)
        result.input = std::make_shared<QuickInputSink>(window);
    return result;
}

}  // namespace HyRemote::detail
