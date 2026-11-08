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
#include <chrono>
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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
#  include <QDebug>
#  include "quick/quick_native_frame_claim.hpp"
#  include "quick/quick_native_frame_policy.hpp"
#  include "quick/quick_native_frame_shadow.hpp"
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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        // Human A authorized a shadow-only experiment. Both the exact-Kit
        // build option and this explicit process opt-in must be present.
        const NativeFrameExperimentFacts nativeFacts{
            true,
            qEnvironmentVariable("HYREMOTE_P3_SHADOW_OBSERVER") == "1",
            QT_VERSION == QT_VERSION_CHECK(6, 8, 3),
            !m_state->target.isNull(),
        };
        if (nativeFrameExperimentMode(nativeFacts)
            == NativeFrameExperimentMode::ShadowObservationOnly) {
            m_nativeShadow = std::make_unique<QuickNativeFrameShadow>(
                m_state->target.data());
            m_nativeShadow->start();
            m_state->nativeShadow = m_nativeShadow.get();

            // S2R-3: optional claim simulator. It requires the shadow observer
            // to be active as well, so SIMULATE_CLAIM alone cannot arm a ledger
            // without a token source or change any product behaviour.
            if (m_state->nativeShadow
                && qEnvironmentVariable("HYREMOTE_P3_SHADOW_SIMULATE_CLAIM") == "1") {
                m_state->claimLedger = std::make_unique<RequestLedger>(4);
                m_state->claimCache = std::make_unique<NativeFrameCache>();
                auto *timer = new QTimer(app);
                // 10 ms keeps poll latency below the 10 ms term used by the
                // 60 ms wait budget; 25 ms would already exceed it once the
                // 23 ms QRhi completion is added.
                timer->setInterval(10);
                QObject::connect(timer, &QTimer::timeout, timer,
                                 [state = m_state] { drainClaimTokens(state); });
                timer->start();
                m_state->claimPoll = timer;
            }
        }
#endif
        return true;
    }

    void stop() noexcept override
    {
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        if (m_state->claimPoll) {
            m_state->claimPoll->stop();
            m_state->claimPoll->deleteLater();
            m_state->claimPoll = nullptr;
        }
        if (m_state->claimLedger) {
            const std::size_t pendingAtStop = m_state->claimLedger->size();
            m_state->claimLedger->cancelAll();
            if (m_state->claimCache)
                m_state->claimCache->clear();
            ClaimStats &stats = m_state->claimStats;
            stats.cancelled += static_cast<std::uint64_t>(pendingAtStop);
            const std::uint64_t terminals = stats.nativeClaims + stats.grabClaims
                                            + stats.unavailableClaims + stats.cancelled;
            stats.unterminated =
                stats.admitted > terminals ? stats.admitted - terminals : 0;
            qInfo().nospace() << "HyRemote P3 SHADOW CLAIM SIM: admitted=" << stats.admitted
                              << " cached_eligible=" << stats.cachedEligible
                              << " wait_eligible=" << stats.waitEligible
                              << " grab_only=" << stats.grabOnly
                              << " native_claims=" << stats.nativeClaims
                              << " grab_claims=" << stats.grabClaims
                              << " unavailable_claims=" << stats.unavailableClaims
                              << " unterminated=" << stats.unterminated
                              << " grab_induced_tokens=" << stats.grabInducedTokens
                              << " simulated_native_then_real_grab="
                              << stats.simulatedNativeThenRealGrab
                              << " tokens=" << stats.tokensReceived
                              << " stale_tokens=" << stats.staleTokens
                              << " duplicate_terminals=" << stats.duplicateTerminals
                              << " cancelled=" << stats.cancelled
                              << " pixels_published=0";
            m_state->claimLedger.reset();
            m_state->claimCache.reset();
        }
        m_state->nativeShadow = nullptr;
        if (m_nativeShadow) {
            m_nativeShadow->stop();
            m_nativeShadow.reset();
        }
#endif
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->onFrame = {};
        m_state->onEvent = {};

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
            [state, request] { attemptGrabOnGuiThread(state, request, 1U); },
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

#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        // S2R-3 shadow claim simulator. Diagnostics only: the real grab path is
        // untouched and no native pixel is ever published.
        QuickNativeFrameShadow *nativeShadow = nullptr;
        std::unique_ptr<RequestLedger> claimLedger;
        std::unique_ptr<NativeFrameCache> claimCache;
        NativeFrameClaimPolicy claimPolicy;
        ClaimStats claimStats;
        std::uint64_t lastConsumedSerial = 0;
        long long lastGrabSubmitMs = 0; // GUI thread only
        QTimer *claimPoll = nullptr;
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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        if (state->claimLedger
            && state->claimLedger->claim(request.id, ClaimKind::Unavailable)) {
            ++state->claimStats.unavailableClaims;
        }
#endif
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

#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
    static long long claimNowMs()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // GUI thread: consume metadata tokens completed by the shadow observer.
    // A token only satisfies a waiting request when it was rendered after that
    // request was admitted; otherwise it becomes a bounded unserved cache entry
    // for a later request (explicitly reusedUnserved, never "newer than request").
    static void drainClaimTokens(const std::shared_ptr<State> &state)
    {
        if (!state->nativeShadow || !state->claimCache || !state->claimLedger)
            return;
        NativeFrameToken token;
        while (state->nativeShadow->takeCompletedToken(token)) {
            ++state->claimStats.tokensReceived;
            if (token.epoch != state->nativeShadow->currentEpoch()) {
                ++state->claimStats.staleTokens;
                continue;
            }
            // A frame that only appeared because our own grab requested a
            // render must not be counted as a natural window frame. Without this
            // filter the simulator would "save" grabs that it caused itself.
            const long long grabMs = state->lastGrabSubmitMs;
            if (grabMs > 0 && token.sourceMs >= grabMs
                && token.sourceMs - grabMs <= 120) {
                ++state->claimStats.grabInducedTokens;
                continue;
            }
            bool claimed = false;
            if (auto id = state->claimLedger->oldestWaiting()) {
                const auto entry = state->claimLedger->entry(*id);
                if (entry && token.serial > entry->admissionSerial
                    && state->claimLedger->claim(*id, ClaimKind::Native)) {
                    ++state->claimStats.nativeClaims;
                    state->lastConsumedSerial = token.serial;
                    claimed = true;
                }
            }
            if (!claimed)
                state->claimCache->offer(token);
        }
    }

    // Admission decision for diagnostics. The real grab is still issued; this
    // only records whether a native frame could have served the request.
    static void simulateAdmission(const std::shared_ptr<State> &state,
                                  const hyremote::CaptureRequest &request)
    {
        if (!state->claimLedger || !state->nativeShadow)
            return;
        const long long now = claimNowMs();
        if (!state->claimLedger->admit(
                request.id, state->nativeShadow->renderSerial(), now)) {
            ++state->claimStats.duplicateTerminals;
            return;
        }
        ++state->claimStats.admitted;

        // Two frame periods of recent rendering qualifies as "animation active";
        // a static window takes the grab path without waiting.
        const bool active = state->nativeShadow->renderActive(now, 34);
        AdmissionDecision decision = decideAdmission(*state->claimCache,
                                                     state->lastConsumedSerial,
                                                     state->nativeShadow->currentEpoch(),
                                                     now,
                                                     active,
                                                     state->claimPolicy);
        switch (decision.action) {
        case AdmissionAction::UseCached:
            if (state->claimLedger->claim(request.id, ClaimKind::Native, true)) {
                ++state->claimStats.cachedEligible;
                ++state->claimStats.nativeClaims;
                state->lastConsumedSerial = decision.token.serial;
            } else {
                ++state->claimStats.duplicateTerminals;
            }
            break;
        case AdmissionAction::WaitForNative:
            state->claimLedger->beginWait(request.id);
            ++state->claimStats.waitEligible;
            state->nativeShadow->requestSample();
            break;
        case AdmissionAction::GrabNow:
            state->claimLedger->beginGrab(request.id);
            ++state->claimStats.grabOnly;
            break;
        }
    }

    static void simulateGrabTerminal(const std::shared_ptr<State> &state,
                                     const hyremote::CaptureRequest &request)
    {
        if (!state->claimLedger)
            return;
        const auto entry = state->claimLedger->entry(request.id);
        if (!entry)
            return;
        if (state->claimLedger->claim(request.id, ClaimKind::Grab)) {
            ++state->claimStats.grabClaims;
            // A grab renders current content; any older native token must not
            // be replayed afterwards as if it were newer.
            if (state->claimCache)
                state->claimCache->clear();
            if (state->nativeShadow) {
                const auto barrier = state->nativeShadow->renderSerial();
                if (barrier > state->lastConsumedSerial)
                    state->lastConsumedSerial = barrier;
            }
        } else if (entry->kind == ClaimKind::Native) {
            // Expected in shadow mode: the simulated native claim never
            // preempts the real grab, which still produced the Core frame.
            ++state->claimStats.simulatedNativeThenRealGrab;
        }
    }
#endif

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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        simulateGrabTerminal(state, request);
#endif
        publishCallback(state, &State::onFrame, std::move(frame));
    }

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const hyremote::CaptureRequest &request,
                                       unsigned int attempt)
    {
        if (!isActive(state))
            return;

#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        if (attempt == 0)
            simulateAdmission(state, request);
#endif

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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
        state->lastGrabSubmitMs = claimNowMs();
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
#if defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
    std::unique_ptr<QuickNativeFrameShadow> m_nativeShadow;
#endif
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
