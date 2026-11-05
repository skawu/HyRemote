#include "quick/quick_native_frame_shadow.hpp"

#if !defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
#  error "This QtPrivate translation unit is only compiled in the opt-in P3 branch"
#endif

#include <QDebug>
#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickwindow_p.h>

#include <rhi/qrhi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>

#include "quick/quick_native_frame_claim.hpp"
#include "quick/quick_native_frame_policy.hpp"

namespace HyRemote::detail {

namespace {

// A readback result must outlive the frame that records it. QRhi completes
// pending readbacks synchronously at the end of the submitting frame (and again
// from QRhi's destructor), so the completion callback is the only owner after
// the frame is submitted.
struct PendingReadback
{
    QRhiReadbackResult result;
    std::uint64_t generation = 0;
    std::uint64_t submitFrame = 0;
    int submitWidth = 0;
    int submitHeight = 0;
    long long submitMs = 0;
    int index = 0;
    long long submitGeneration = 0;
    std::uint64_t epoch = 0;
    double devicePixelRatio = 0.0;
};

long long steadyNowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

struct QuickNativeFrameShadow::State
{
    std::atomic<bool> enabled{false};
    std::atomic<bool> stopped{false};
    std::atomic<bool> invalidated{false};
    std::atomic<std::uint64_t> renderSignals{0};
    std::atomic<std::uint64_t> rhiSwapchainSeen{0};
    std::atomic<std::uint64_t> unqualifiedOutput{0};
    std::atomic<std::uint64_t> frameSeq{0};

    // P3-S1 shadow readback accounting. Pixels are never published.
    std::atomic<std::uint64_t> readbackSubmitted{0};
    std::atomic<std::uint64_t> readbackCompleted{0};
    std::atomic<std::uint64_t> readbackDropped{0};
    std::atomic<std::uint64_t> discardedBytes{0};
    std::atomic<bool> readbackInFlight{false};
    std::atomic<long long> lastReadbackSubmitMs{0};

    // P3-S1R lifecycle / integrity accounting.
    std::atomic<std::uint64_t> sameFrameCompletions{0};
    std::atomic<std::uint64_t> laterFrameCompletions{0};
    std::atomic<std::uint64_t> emptyResults{0};
    std::atomic<std::uint64_t> dataSizeMismatches{0};
    std::atomic<std::uint64_t> submitSizeDiffs{0};
    std::atomic<std::uint64_t> backendSkipped{0};
    std::atomic<std::uint64_t> swapchainSizeChanges{0};
    std::atomic<std::uint64_t> scenegraphInvalidations{0};
    std::atomic<std::uint64_t> completionsAfterInvalidation{0};
    std::atomic<std::uint64_t> inFlightAtStop{0};
    std::atomic<std::uint64_t> completionsAfterStop{0};
    std::atomic<std::uint64_t> minBytes{std::numeric_limits<std::uint64_t>::max()};
    std::atomic<std::uint64_t> maxBytes{0};
    std::atomic<int> lastFormat{-1};
    std::atomic<long long> maxFlightMs{0};
    std::atomic<int> lastWidth{0};
    std::atomic<int> lastHeight{0};
    std::atomic<long long> lastSurfaceWidth{0};
    std::atomic<long long> lastSurfaceHeight{0};
    std::atomic<long long> sizeGeneration{0};
    std::atomic<std::uint64_t> completionsAcrossRecreation{0};

    // S2R-3: token production for the shadow claim simulator. Tokens carry only
    // metadata (serial/epoch/geometry); pixels are still discarded.
    std::atomic<std::uint64_t> epochValue{1};
    std::atomic<long long> lastRenderMs{0};
    std::atomic<bool> sampleRequested{false};
    std::atomic<std::uint64_t> tokensProduced{0};
    std::atomic<std::uint64_t> tokensRejected{0};
    std::mutex tokenMutex;
    std::optional<NativeFrameToken> tokenSlot;
};

QuickNativeFrameShadow::QuickNativeFrameShadow(QQuickWindow *window)
    : m_window(window)
    , m_state(std::make_shared<State>())
{
}

QuickNativeFrameShadow::~QuickNativeFrameShadow()
{
    stop();
}

void QuickNativeFrameShadow::start()
{
    if (!m_window || m_afterRendering)
        return;

    const std::shared_ptr<State> state = m_state;
    state->enabled.store(true, std::memory_order_release);

    // Render-stage invalidation (window destroy, backend change, device reset)
    // arrives before Qt destroys the QRhi. QRhi's own destructor drains pending
    // readbacks, so this hook only stops new submissions and records the fact.
    m_sceneGraphInvalidated = QObject::connect(
        m_window, &QQuickWindow::sceneGraphInvalidated, m_window,
        [state] {
            state->invalidated.store(true, std::memory_order_release);
            state->scenegraphInvalidations.fetch_add(1, std::memory_order_relaxed);
            state->epochValue.fetch_add(1, std::memory_order_relaxed);
        },
        Qt::DirectConnection);

    // DirectConnection keeps this observer at Qt's actual render-stage
    // boundary. The lambda takes no Qt lock, issues at most one bounded
    // non-blocking readback and never invokes Runtime/Core, GUI, network or
    // file I/O on the render thread. QObject sender/context lifetime guards the
    // raw QQuickWindow pointer.
    m_afterRendering = QObject::connect(
        m_window, &QQuickWindow::afterRendering, m_window,
        [window = m_window, state] {
            if (!state->enabled.load(std::memory_order_acquire))
                return;
            const std::uint64_t frame =
                state->frameSeq.fetch_add(1, std::memory_order_relaxed) + 1;
            state->renderSignals.fetch_add(1, std::memory_order_relaxed);
            auto *d = QQuickWindowPrivate::get(window);
            if (!d || !d->rhi || !d->swapchain) {
                state->unqualifiedOutput.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            state->rhiSwapchainSeen.fetch_add(1, std::memory_order_relaxed);

            if (state->stopped.load(std::memory_order_acquire)
                || state->invalidated.load(std::memory_order_acquire))
                return;

            // Only the exact backend whose destruction contract is proven is
            // sampled: D3D11 completes pending readbacks in destroy() before the
            // device is released. Other backends are counted, never sampled.
            if (d->rhi->backend() != QRhi::D3D11) {
                state->backendSkipped.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            const QSize surface = d->swapchain->surfacePixelSize();
            const long long previousWidth = state->lastSurfaceWidth.exchange(surface.width());
            const long long previousHeight = state->lastSurfaceHeight.exchange(surface.height());
            if (previousWidth != 0 && (previousWidth != surface.width()
                                       || previousHeight != surface.height())) {
                state->swapchainSizeChanges.fetch_add(1, std::memory_order_relaxed);
                state->sizeGeneration.fetch_add(1, std::memory_order_relaxed);
                state->epochValue.fetch_add(1, std::memory_order_relaxed);
            }

            state->lastRenderMs.store(steadyNowMs(), std::memory_order_relaxed);

            // Two sampling sources share the single in-flight slot:
            //   - scheduled diagnostic sampling (capped/interval) as before;
            //   - an on-demand sample requested by a pending Core request in the
            //     shadow claim simulator, which must not be hidden by the
            //     diagnostic cap or the 250 ms interval.
            const bool onDemand =
                state->sampleRequested.exchange(false, std::memory_order_acq_rel);
            if (!onDemand
                && state->readbackSubmitted.load(std::memory_order_relaxed)
                       >= kNativeFrameShadowMaxReadbacks)
                return;
            if (state->readbackInFlight.load(std::memory_order_acquire)) {
                // Keep the on-demand intent for the next natural frame instead
                // of silently losing it behind the single in-flight slot.
                if (onDemand)
                    state->sampleRequested.store(true, std::memory_order_release);
                state->readbackDropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const long long now = steadyNowMs();
            const long long previous = state->lastReadbackSubmitMs.load(std::memory_order_relaxed);
            if (!onDemand && previous != 0
                && now - previous < static_cast<long long>(kNativeFrameShadowMinIntervalMs))
                return;

            QRhiCommandBuffer *cb = d->swapchain->currentFrameCommandBuffer();
            if (!cb) {
                state->unqualifiedOutput.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            QRhiResourceUpdateBatch *batch = d->rhi->nextResourceUpdateBatch();
            if (!batch) {
                state->unqualifiedOutput.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            // The completion callback owns the result and deletes it. It never
            // touches the window, the render target or any Qt GUI object.
            auto *pending = new PendingReadback;
            pending->generation = state->readbackSubmitted.load(std::memory_order_relaxed) + 1;
            pending->submitFrame = frame;
            pending->submitWidth = surface.width();
            pending->submitHeight = surface.height();
            pending->submitMs = steadyNowMs();
            pending->index =
                static_cast<int>(state->readbackSubmitted.load(std::memory_order_relaxed)) + 1;
            pending->submitGeneration = state->sizeGeneration.load(std::memory_order_relaxed);
            pending->epoch = state->epochValue.load(std::memory_order_relaxed);
            pending->devicePixelRatio =
                window->width() > 0
                    ? static_cast<double>(surface.width()) / static_cast<double>(window->width())
                    : 0.0;
            pending->result.completed = [pending, state] {
                state->readbackCompleted.fetch_add(1, std::memory_order_relaxed);
                if (state->stopped.load(std::memory_order_relaxed))
                    state->completionsAfterStop.fetch_add(1, std::memory_order_relaxed);
                if (state->invalidated.load(std::memory_order_relaxed))
                    state->completionsAfterInvalidation.fetch_add(1, std::memory_order_relaxed);
                if (state->frameSeq.load(std::memory_order_relaxed) == pending->submitFrame)
                    state->sameFrameCompletions.fetch_add(1, std::memory_order_relaxed);
                else
                    state->laterFrameCompletions.fetch_add(1, std::memory_order_relaxed);

                const int width = pending->result.pixelSize.width();
                const int height = pending->result.pixelSize.height();
                const auto bytes = static_cast<std::uint64_t>(pending->result.data.size());
                if (width <= 0 || height <= 0 || bytes == 0) {
                    state->emptyResults.fetch_add(1, std::memory_order_relaxed);
                } else if (bytes != static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4U) {
                    state->dataSizeMismatches.fetch_add(1, std::memory_order_relaxed);
                }
                if (width != pending->submitWidth || height != pending->submitHeight)
                    state->submitSizeDiffs.fetch_add(1, std::memory_order_relaxed);
                if (state->sizeGeneration.load(std::memory_order_relaxed)
                    != pending->submitGeneration) {
                    state->completionsAcrossRecreation.fetch_add(1, std::memory_order_relaxed);
                }

                const long long flightMs = steadyNowMs() - pending->submitMs;
                long long previousFlight = state->maxFlightMs.load(std::memory_order_relaxed);
                while (flightMs > previousFlight
                       && !state->maxFlightMs.compare_exchange_weak(
                           previousFlight, flightMs, std::memory_order_relaxed)) {
                }
                if (state->stopped.load(std::memory_order_relaxed)
                    || state->invalidated.load(std::memory_order_relaxed)) {
                    qInfo().nospace()
                        << "HyRemote P3 SHADOW LATE COMPLETION: after_stop="
                        << (state->stopped.load(std::memory_order_relaxed) ? 1 : 0)
                        << " after_invalidation="
                        << (state->invalidated.load(std::memory_order_relaxed) ? 1 : 0)
                        << " n=" << pending->index
                        << " submit_ms=" << pending->submitMs
                        << " done_ms=" << steadyNowMs()
                        << " bytes=" << bytes
                        << " format=" << static_cast<int>(pending->result.format)
                        << " size=" << width << "x" << height
                        << " flight_ms=" << flightMs;
                }

                std::uint64_t previousMin = state->minBytes.load(std::memory_order_relaxed);
                while (bytes < previousMin
                       && !state->minBytes.compare_exchange_weak(
                           previousMin, bytes, std::memory_order_relaxed)) {
                }
                std::uint64_t previousMax = state->maxBytes.load(std::memory_order_relaxed);
                while (bytes > previousMax
                       && !state->maxBytes.compare_exchange_weak(
                           previousMax, bytes, std::memory_order_relaxed)) {
                }
                state->lastFormat.store(static_cast<int>(pending->result.format),
                                        std::memory_order_relaxed);
                state->lastWidth.store(width, std::memory_order_relaxed);
                state->lastHeight.store(height, std::memory_order_relaxed);

                // The token carries metadata only; pixels stay discarded. A
                // stopped/invalidated window never produces a claimable token.
                if (width > 0 && height > 0 && bytes > 0
                    && !state->stopped.load(std::memory_order_relaxed)
                    && !state->invalidated.load(std::memory_order_relaxed)) {
                    NativeFrameToken token;
                    token.serial = pending->submitFrame;
                    token.epoch = pending->epoch;
                    token.width = width;
                    token.height = height;
                    token.format = static_cast<int>(pending->result.format);
                    token.devicePixelRatio = pending->devicePixelRatio;
                    token.sourceMs = pending->submitMs;
                    {
                        std::lock_guard<std::mutex> lock(state->tokenMutex);
                        state->tokenSlot = token;
                    }
                    state->tokensProduced.fetch_add(1, std::memory_order_relaxed);
                } else {
                    state->tokensRejected.fetch_add(1, std::memory_order_relaxed);
                }

                state->discardedBytes.fetch_add(bytes, std::memory_order_relaxed);
                state->readbackInFlight.store(false, std::memory_order_release);
                delete pending;
            };

            // Null texture means "the current swapchain image". No finish(),
            // no blocking wait, no extra render pass is issued.
            state->readbackInFlight.store(true, std::memory_order_release);
            state->readbackSubmitted.fetch_add(1, std::memory_order_relaxed);
            state->lastReadbackSubmitMs.store(now, std::memory_order_relaxed);
            batch->readBackTexture(QRhiReadbackDescription{}, &pending->result);
            cb->resourceUpdate(batch);
        },
        Qt::DirectConnection);
}

void QuickNativeFrameShadow::stop() noexcept
{
    if (!m_state->enabled.exchange(false, std::memory_order_acq_rel))
        return;
    m_state->stopped.store(true, std::memory_order_release);
    if (m_state->readbackInFlight.load(std::memory_order_acquire))
        m_state->inFlightAtStop.fetch_add(1, std::memory_order_relaxed);
    QObject::disconnect(m_afterRendering);
    m_afterRendering = {};
    QObject::disconnect(m_sceneGraphInvalidated);
    m_sceneGraphInvalidated = {};
    {
        std::lock_guard<std::mutex> lock(m_state->tokenMutex);
        m_state->tokenSlot.reset();
    }
    // This state is also owned by any signal currently executing. That
    // callback touches only atomics, so stop need not wait for a render thread
    // that may itself be synchronizing with the GUI thread. A readback still in
    // flight keeps its own result alive through the completion callback.
    qInfo().nospace()
        << "HyRemote P3 SHADOW ONLY: render_signals="
        << m_state->renderSignals.load(std::memory_order_relaxed)
        << " qt6_rhi_swapchain="
        << m_state->rhiSwapchainSeen.load(std::memory_order_relaxed)
        << " unqualified="
        << m_state->unqualifiedOutput.load(std::memory_order_relaxed)
        << " readback_submitted="
        << m_state->readbackSubmitted.load(std::memory_order_relaxed)
        << " readback_completed="
        << m_state->readbackCompleted.load(std::memory_order_relaxed)
        << " readback_dropped="
        << m_state->readbackDropped.load(std::memory_order_relaxed)
        << " readback_discarded_bytes="
        << m_state->discardedBytes.load(std::memory_order_relaxed)
        << " readback_same_frame="
        << m_state->sameFrameCompletions.load(std::memory_order_relaxed)
        << " readback_later_frame="
        << m_state->laterFrameCompletions.load(std::memory_order_relaxed)
        << " readback_empty="
        << m_state->emptyResults.load(std::memory_order_relaxed)
        << " readback_data_size_mismatch="
        << m_state->dataSizeMismatches.load(std::memory_order_relaxed)
        << " readback_submit_size_diff="
        << m_state->submitSizeDiffs.load(std::memory_order_relaxed)
        << " readback_backend_skipped="
        << m_state->backendSkipped.load(std::memory_order_relaxed)
        << " swapchain_size_changes="
        << m_state->swapchainSizeChanges.load(std::memory_order_relaxed)
        << " scenegraph_invalidations="
        << m_state->scenegraphInvalidations.load(std::memory_order_relaxed)
        << " readback_after_invalidation="
        << m_state->completionsAfterInvalidation.load(std::memory_order_relaxed)
        << " readback_in_flight_at_stop="
        << m_state->inFlightAtStop.load(std::memory_order_relaxed)
        << " readback_after_stop="
        << m_state->completionsAfterStop.load(std::memory_order_relaxed)
        << " readback_across_recreation="
        << m_state->completionsAcrossRecreation.load(std::memory_order_relaxed)
        << " readback_max_flight_ms="
        << m_state->maxFlightMs.load(std::memory_order_relaxed)
        << " readback_min_bytes="
        << (m_state->minBytes.load(std::memory_order_relaxed)
                    == std::numeric_limits<std::uint64_t>::max()
                ? 0
                : m_state->minBytes.load(std::memory_order_relaxed))
        << " readback_max_bytes="
        << m_state->maxBytes.load(std::memory_order_relaxed)
        << " readback_format="
        << m_state->lastFormat.load(std::memory_order_relaxed)
        << " readback_size="
        << m_state->lastWidth.load(std::memory_order_relaxed)
        << "x"
        << m_state->lastHeight.load(std::memory_order_relaxed)
        << " tokens_produced="
        << m_state->tokensProduced.load(std::memory_order_relaxed)
        << " tokens_rejected="
        << m_state->tokensRejected.load(std::memory_order_relaxed)
        << " pixels_published=0";
}

bool QuickNativeFrameShadow::takeCompletedToken(NativeFrameToken &token)
{
    std::lock_guard<std::mutex> lock(m_state->tokenMutex);
    if (!m_state->tokenSlot)
        return false;
    token = *m_state->tokenSlot;
    m_state->tokenSlot.reset();
    return true;
}

void QuickNativeFrameShadow::requestSample() noexcept
{
    m_state->sampleRequested.store(true, std::memory_order_release);
}

std::uint64_t QuickNativeFrameShadow::renderSerial() const noexcept
{
    return m_state->frameSeq.load(std::memory_order_relaxed);
}

std::uint64_t QuickNativeFrameShadow::currentEpoch() const noexcept
{
    return m_state->epochValue.load(std::memory_order_relaxed);
}

long long QuickNativeFrameShadow::lastRenderMs() const noexcept
{
    return m_state->lastRenderMs.load(std::memory_order_relaxed);
}

bool QuickNativeFrameShadow::renderActive(long long nowMs, long long windowMs) const noexcept
{
    const long long last = m_state->lastRenderMs.load(std::memory_order_relaxed);
    return last > 0 && nowMs - last <= windowMs;
}

}  // namespace HyRemote::detail
