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
#include <memory>

#include "quick/quick_native_frame_policy.hpp"

namespace HyRemote::detail {

namespace {

// A readback result must outlive the frame that records it; QRhi invokes the
// completion callback on the render thread after the frame is submitted.
struct PendingReadback
{
    QRhiReadbackResult result;
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
    std::atomic<std::uint64_t> renderSignals{0};
    std::atomic<std::uint64_t> rhiSwapchainSeen{0};
    std::atomic<std::uint64_t> unqualifiedOutput{0};

    // P3-S1 shadow readback accounting. Pixels are never published.
    std::atomic<std::uint64_t> readbackSubmitted{0};
    std::atomic<std::uint64_t> readbackCompleted{0};
    std::atomic<std::uint64_t> readbackDropped{0};
    std::atomic<std::uint64_t> discardedBytes{0};
    std::atomic<bool> readbackInFlight{false};
    std::atomic<long long> lastReadbackSubmitMs{0};
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
            state->renderSignals.fetch_add(1, std::memory_order_relaxed);
            auto *d = QQuickWindowPrivate::get(window);
            if (!d || !d->rhi || !d->swapchain) {
                state->unqualifiedOutput.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            state->rhiSwapchainSeen.fetch_add(1, std::memory_order_relaxed);

            // Bounded shadow sampling: hard cap + minimum interval + at most
            // one request in flight. Nothing here may block or wait.
            if (state->readbackSubmitted.load(std::memory_order_relaxed)
                >= kNativeFrameShadowMaxReadbacks)
                return;
            if (state->readbackInFlight.load(std::memory_order_acquire)) {
                state->readbackDropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const long long now = steadyNowMs();
            const long long previous = state->lastReadbackSubmitMs.load(std::memory_order_relaxed);
            if (previous != 0
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

            // The completion callback owns the result and deletes it. It must
            // never touch the window, the render target or any Qt GUI object.
            auto *pending = new PendingReadback;
            pending->result.completed = [pending, state] {
                state->discardedBytes.fetch_add(
                    static_cast<std::uint64_t>(pending->result.data.size()),
                    std::memory_order_relaxed);
                state->readbackCompleted.fetch_add(1, std::memory_order_relaxed);
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
    QObject::disconnect(m_afterRendering);
    m_afterRendering = {};
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
        << " pixels_published=0";
}

}  // namespace HyRemote::detail
