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

#include <algorithm>
#include <condition_variable>
#include <cstddef>
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
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
#  include <QDebug>
#  include <rhi/qrhi.h>
#  include "quick/quick_native_frame_source.hpp"
#endif
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

        m_state->onFrame = std::move(onFrame);
        m_state->onEvent = std::move(onEvent);
        m_state->active = true;
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
        if (qEnvironmentVariable("HYREMOTE_QUICK_NATIVE_FRAME") == "1") {
            m_state->native = std::make_shared<QuickNativeFrameSource>(m_state->target.data());
            m_state->native->start();
        }
#endif
        return true;
    }

    void stop() noexcept override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->onFrame = {};
        m_state->onEvent = {};
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
        m_state->pendingNative.reset();
        if (m_state->native) {
            m_state->native->stop();
            m_state->native.reset();
        }
        qInfo().nospace()
            << "HyRemote P3 native pilot: native_frames_published="
            << m_state->nativeFramesPublished.load(std::memory_order_relaxed);
        // Measured (request id 12000..12047) and warmup (11000..11011)
        // are accounted separately. The previous benchmark wrongly assumed
        // every warmup request was native and subtracted a fixed 12.
        qInfo().nospace()
            << "HyRemote P3 native pilot: native_measured_published="
            << m_state->nativeMeasuredFramesPublished.load(std::memory_order_relaxed);
        qInfo().nospace()
            << "HyRemote P3 native pilot: fallback_no_recent_render="
            << m_state->nativeAdmissionNoFreshRender.load(std::memory_order_relaxed)
            << " fallback_pending_busy="
            << m_state->nativeAdmissionBusy.load(std::memory_order_relaxed)
            << " fallback_native_deadline="
            << m_state->nativeDeadlineFallback.load(std::memory_order_relaxed)
            << " rejected_epoch="
            << m_state->nativeRejectEpoch.load(std::memory_order_relaxed)
            << " rejected_serial="
            << m_state->nativeRejectSerial.load(std::memory_order_relaxed)
            << " rejected_age="
            << m_state->nativeRejectAge.load(std::memory_order_relaxed)
            << " rejected_target="
            << m_state->nativeRejectTarget.load(std::memory_order_relaxed)
            << " rejected_geometry="
            << m_state->nativeRejectGeometry.load(std::memory_order_relaxed)
            << " rejected_pixel_format="
            << m_state->nativeRejectPixelFormat.load(std::memory_order_relaxed);
#endif

        // Ready/timer continuations may still exist after stop, but they all check active before
        // entering a Core callback. A callback that already entered is drained here.
        m_state->callbacksDrained.wait(lock, [this] { return m_state->callbacksInFlight == 0; });
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (!m_state->active)
                return false;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return false;

        const std::shared_ptr<State> state = m_state;
        return QMetaObject::invokeMethod(
            dispatcher,
            [state, request] { dispatchCaptureOnGuiThread(state, request); },
            Qt::QueuedConnection);
    }

private:
    static constexpr unsigned int kMaxGrabAttempts = 3U;

    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        QPointer<QQuickWindow> target;
        bool active = false;
        std::size_t callbacksInFlight = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
        std::shared_ptr<QuickNativeFrameSource> native;
        std::optional<hyremote::CaptureRequest> pendingNative; // mutex protected
        std::uint64_t pendingNativeSerial = 0;
        std::uint64_t pendingNativeEpoch = 0;
        long long pendingNativeAdmittedMs = 0;
        std::atomic<std::uint64_t> nativeFramesPublished{0};
        // Pilot diagnostics only. Public capture is a valid per-request
        // fallback; do not confuse native hit rate with callback correctness.
        std::atomic<std::uint64_t> nativeMeasuredFramesPublished{0};
        std::atomic<std::uint64_t> nativeAdmissionNoFreshRender{0};
        std::atomic<std::uint64_t> nativeAdmissionBusy{0};
        std::atomic<std::uint64_t> nativeDeadlineFallback{0};
        std::atomic<std::uint64_t> nativeRejectEpoch{0};
        std::atomic<std::uint64_t> nativeRejectSerial{0};
        std::atomic<std::uint64_t> nativeRejectAge{0};
        std::atomic<std::uint64_t> nativeRejectTarget{0};
        std::atomic<std::uint64_t> nativeRejectGeometry{0};
        std::atomic<std::uint64_t> nativeRejectPixelFormat{0};
#endif
    };

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

    static bool isActive(const std::shared_ptr<State> &state)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->active;
    }

    template <typename Handler, typename Payload>
    static void publishCallback(const std::shared_ptr<State> &state,
                                Handler State::*member,
                                Payload payload)
    {
        Handler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
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
                             hyremote::CaptureEventCode code,
                             const char *message,
                             bool recoverable)
    {
        hyremote::CaptureEvent event;
        event.code = code;
        event.message = message;
        event.recoverable = recoverable;
        publishCallback(state, &State::onEvent, std::move(event));
    }

    static void completeUnavailable(const std::shared_ptr<State> &state,
                                    const hyremote::CaptureRequest &request)
    {
        publishEvent(state,
                     hyremote::CaptureEventCode::TemporarilyUnavailable,
                     "Qt Quick capture remained unavailable after the bounded retry budget",
                     true);
        if (!isActive(state))
            return;

        hyremote::RemoteFrame completion;
        completion.timing.ptsSource = hyremote::PtsSource::Completion;
        completion.timing.requestTime = request.requestTime;
        completion.timing.completionTime = hyremote::Clock::now();
        completion.requestId = request.id;
        publishCallback(state, &State::onFrame, std::move(completion));
    }

    static void retryLater(const std::shared_ptr<State> &state,
                           const hyremote::CaptureRequest &request,
                           unsigned int attempt,
                           int delayMs)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher || !isActive(state))
            return;

        if (attempt >= kMaxGrabAttempts) {
            completeUnavailable(state, request);
            return;
        }

        QTimer::singleShot(delayMs, dispatcher, [state, request, attempt] {
            attemptGrabOnGuiThread(state, request, attempt + 1U);
        });
    }

    static void publishImage(const std::shared_ptr<State> &state,
                             const hyremote::CaptureRequest &request,
                             unsigned int attempt,
                             const QImage &grabbed)
    {
        if (!isActive(state))
            return;
        if (state->target.isNull()) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }
        if (grabbed.isNull()) {
            retryLater(state, request, attempt, 100);
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
            retryLater(state, request, attempt, 100);
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
                publishEvent(state,
                             hyremote::CaptureEventCode::BackendFailure,
                             "invalid Qt Quick grab image format or storage",
                             false);
                return;
            }
            const std::size_t bytesPerLine = width * 4U;
            const std::size_t sourceStride = static_cast<std::size_t>(image.bytesPerLine());
            const std::size_t sourceBytes = static_cast<std::size_t>(image.sizeInBytes());
            if (sourceStride < bytesPerLine || height > sourceBytes / sourceStride) {
                publishEvent(state,
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
                publishEvent(state,
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
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(image.width()),
                               static_cast<std::uint32_t>(image.height())};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.geometry.devicePixelRatio =
            image.devicePixelRatio() > 0.0 ? static_cast<float>(image.devicePixelRatio()) : 1.0F;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = completion;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        publishCallback(state, &State::onFrame, std::move(frame));
    }

#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
    static long long nativeNowMs()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // QImage retains the readback's QByteArray without a second full-size
    // allocation. BGRA8 is normalized through Qt's premultiplied ARGB32 path.
    // The exact byte size, readback format and surface epoch have already
    // been checked in the QRhi callback; this remains an experimental Kit.
    static QImage imageFromNative(QuickNativeSample sample)
    {
        if (sample.width <= 0 || sample.height <= 0
            || sample.width > std::numeric_limits<int>::max() / 4
            || sample.dpr <= 0.0)
            return {};
        // Use QRhi's *actual enum*, not guessed 1/2 constants. BGRA byte
        // ordering is directly representable as Qt ARGB32 only on LE; other
        // CPUs fail closed to public Qt capture until a measured swizzle
        // adapter is available. There is no Qt Quick/SoC platform assumption.
        QImage::Format format = QImage::Format_Invalid;
        if (sample.format == static_cast<int>(QRhiTexture::RGBA8))
            format = QImage::Format_RGBA8888_Premultiplied;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        else if (sample.format == static_cast<int>(QRhiTexture::BGRA8))
            format = QImage::Format_ARGB32_Premultiplied;
#endif
        if (format == QImage::Format_Invalid)
            return {};
        auto *owner = new QByteArray(std::move(sample.pixels));
        QImage view(reinterpret_cast<const uchar *>(owner->constData()),
                    sample.width, sample.height, sample.width * 4, format,
                    [](void *p) { delete static_cast<QByteArray *>(p); }, owner);
        if (view.isNull()) {
            delete owner;
            return {};
        }
        view.setDevicePixelRatio(sample.dpr);
        return view;
    }

    static void pollNative(const std::shared_ptr<State> &state,
                           const hyremote::CaptureRequest &request,
                           long long deadlineMs)
    {
        std::shared_ptr<QuickNativeFrameSource> native;
        std::uint64_t serial = 0;
        std::uint64_t epoch = 0;
        long long admittedMs = 0;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || !state->pendingNative
                || state->pendingNative->id != request.id)
                return;
            native = state->native;
            serial = state->pendingNativeSerial;
            epoch = state->pendingNativeEpoch;
            admittedMs = state->pendingNativeAdmittedMs;
        }
        QuickNativeSample sample;
        if (native && native->takeSample(sample)) {
            // Account for discarded native samples without weakening ANY
            // freshness, geometry, or epoch check. Async QRhi completion
            // may outlive a bounded native request; public grab is then the
            // expected safe fallback, not a fabricated native hit.
            if (sample.epoch != epoch || sample.epoch != native->epoch()) {
                state->nativeRejectEpoch.fetch_add(1, std::memory_order_relaxed);
            } else if (sample.renderSerial <= serial) {
                state->nativeRejectSerial.fetch_add(1, std::memory_order_relaxed);
            } else if (sample.sourceMs < admittedMs
                       || nativeNowMs() - sample.sourceMs > 80) {
                state->nativeRejectAge.fetch_add(1, std::memory_order_relaxed);
            } else if (state->target.isNull()) {
                state->nativeRejectTarget.fetch_add(1, std::memory_order_relaxed);
            } else {
                // Full Qt Quick contentItem target only. Geometry/DPR or
                // visibility changes fail closed to the public grab path.
                QQuickWindow *window = state->target.data();
                const int expectedW = qRound(window->width() * window->devicePixelRatio());
                const int expectedH = qRound(window->height() * window->devicePixelRatio());
                if (!window->isVisible() || sample.width != expectedW
                    || sample.height != expectedH
                    || qAbs(sample.dpr - window->devicePixelRatio()) > 0.01) {
                    state->nativeRejectGeometry.fetch_add(1, std::memory_order_relaxed);
                } else {
                    QImage nativeImage = imageFromNative(std::move(sample));
                    if (!nativeImage.isNull()) {
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            if (!state->active || !state->pendingNative
                                || state->pendingNative->id != request.id)
                                return;
                            state->pendingNative.reset();
                        }
                        // Native hit is counted only after the image passed
                        // source epoch, requested-frame and pixel checks.
                        state->nativeFramesPublished.fetch_add(1, std::memory_order_relaxed);
                        if (qEnvironmentVariable("HYREMOTE_P3_BENCH") == "1"
                            && request.id >= 12000U && request.id < 12048U)
                            state->nativeMeasuredFramesPublished.fetch_add(
                                1, std::memory_order_relaxed);
                        publishImage(state, request, 1U, nativeImage);
                        return;
                    }
                    state->nativeRejectPixelFormat.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        if (nativeNowMs() >= deadlineMs || !native) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active || !state->pendingNative
                    || state->pendingNative->id != request.id)
                    return;
                state->pendingNative.reset();
            }
            if (native)
                native->cancelRequest();
            state->nativeDeadlineFallback.fetch_add(1, std::memory_order_relaxed);
            attemptGrabOnGuiThread(state, request, 1U);
            return;
        }
        QObject *dispatcher = QCoreApplication::instance();
        if (dispatcher)
            QTimer::singleShot(10, dispatcher, [state, request, deadlineMs] {
                pollNative(state, request, deadlineMs);
            });
    }
#endif

    static void dispatchCaptureOnGuiThread(const std::shared_ptr<State> &state,
                                           const hyremote::CaptureRequest &request)
    {
#if defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
        std::shared_ptr<QuickNativeFrameSource> native;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
            native = state->native;
        }
        const long long nowMs = nativeNowMs();
        QQuickWindow *window = state->target.data();
        // contentItem()->grabToImage() clears its isolated layer to
        // transparent, whereas an ordinary opaque swapchain includes the
        // QQuickWindow background (white by default). Only consider native
        // when those source-composition contracts can agree.
        const bool equivalentClearContract =
            window && window->isVisible()
            && window->color().alpha() == 0
            && window->format().alphaBufferSize() > 0;
        if (equivalentClearContract && native && native->renderActive(nowMs, 34)) {
            bool reserved = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
                // Core allows two in-flight requests. Never overwrite the
                // accepted request awaiting a native sample; additional
                // requests use the existing independent public grab path.
                if (!state->pendingNative) {
                    state->pendingNative = request;
                    state->pendingNativeSerial = native->renderSerial();
                    state->pendingNativeEpoch = native->epoch();
                    state->pendingNativeAdmittedMs = nowMs;
                    reserved = true;
                }
            }
            if (reserved) {
                native->requestNextRender();
                pollNative(state, request, nowMs + 60);
                return;
            }
            state->nativeAdmissionBusy.fetch_add(1, std::memory_order_relaxed);
        } else if (equivalentClearContract && native) {
            state->nativeAdmissionNoFreshRender.fetch_add(1, std::memory_order_relaxed);
        }
#endif
        attemptGrabOnGuiThread(state, request, 1U);
    }

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const hyremote::CaptureRequest &request,
                                       unsigned int attempt)
    {
        if (!isActive(state))
            return;

        QQuickWindow *window = state->target.data();
        if (!window) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }

        if (!window->isVisible() || window->visibility() == QWindow::Minimized
            || window->width() <= 0 || window->height() <= 0) {
            retryLater(state, request, attempt, 250);
            return;
        }

        QQuickItem *content = window->contentItem();
        if (!content) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "QQuickWindow has no content item",
                         false);
            return;
        }

        // grabToImage() takes logical item dimensions and applies DPR itself.
        const QSize logicalSize(qMax(1, qCeil(window->width())),
                                qMax(1, qCeil(window->height())));
        const auto grab = std::make_shared<GrabAttempt>();
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        grab->p1GrabStart = std::chrono::steady_clock::now();
#endif
        {
            HYREMOTE_P1_SCOPE(QuickGrabSubmit);
            grab->result = content->grabToImage(logicalSize);
        }
        if (grab->result.isNull()) {
            retryLater(state, request, attempt, 100);
            return;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return;

        grab->readyConnection = QObject::connect(
            grab->result.data(),
            &QQuickItemGrabResult::ready,
            dispatcher,
            [state, request, attempt, grab] {
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
                publishImage(state, request, attempt, image);
            });

        // The deadline is owned by the GUI dispatcher and uses Qt's normal
        // QObject cancellation semantics on Qt 5.15 and Qt 6.x. The weak
        // QPointer in GrabAttempt avoids timer/closure ownership cycles.
        auto *deadline = new QTimer(dispatcher);
        deadline->setSingleShot(true);
        grab->timeoutTimer = deadline;
        QObject::connect(deadline, &QTimer::timeout, dispatcher,
                         [state, request, attempt, grab] {
            if (grab->timeoutTimer) {
                grab->timeoutTimer->deleteLater();
                grab->timeoutTimer.clear();
            }
            if (grab->finished)
                return;
            grab->finished = true;
            QObject::disconnect(grab->readyConnection);
            grab->result.clear();
            if (!isActive(state))
                return;
            if (state->target.isNull()) {
                publishEvent(state,
                             hyremote::CaptureEventCode::TargetLost,
                             "the QQuickWindow target was destroyed during capture",
                             false);
                return;
            }
            retryLater(state, request, attempt, 100);
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
