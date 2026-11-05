#include "quick/quick_native_frame_shadow.hpp"

#if !defined(HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW)
#  error "This QtPrivate translation unit is only compiled in the opt-in P3 branch"
#endif

#include <QDebug>
#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickwindow_p.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace HyRemote::detail {

struct QuickNativeFrameShadow::State
{
    std::atomic<bool> enabled{false};
    std::atomic<std::uint64_t> renderSignals{0};
    std::atomic<std::uint64_t> rhiSwapchainSeen{0};
    std::atomic<std::uint64_t> unqualifiedOutput{0};
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
    // boundary. The lambda takes no Qt lock, performs no readback and never
    // invokes Runtime/Core, GUI, network or file I/O on the render thread.
    // QObject sender/context lifetime guards the raw QQuickWindow pointer.
    m_afterRendering = QObject::connect(
        m_window, &QQuickWindow::afterRendering, m_window,
        [window = m_window, state] {
            if (!state->enabled.load(std::memory_order_acquire))
                return;
            state->renderSignals.fetch_add(1, std::memory_order_relaxed);
            auto *d = QQuickWindowPrivate::get(window);
            if (d && d->rhi && d->swapchain) {
                state->rhiSwapchainSeen.fetch_add(1, std::memory_order_relaxed);
            } else {
                state->unqualifiedOutput.fetch_add(1, std::memory_order_relaxed);
            }
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
    // that may itself be synchronizing with the GUI thread.
    qInfo().nospace()
        << "HyRemote P3 SHADOW ONLY: render_signals="
        << m_state->renderSignals.load(std::memory_order_relaxed)
        << " qt6_rhi_swapchain="
        << m_state->rhiSwapchainSeen.load(std::memory_order_relaxed)
        << " unqualified="
        << m_state->unqualifiedOutput.load(std::memory_order_relaxed)
        << " pixels_published=0";
}

}  // namespace HyRemote::detail
