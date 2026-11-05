#pragma once

// #609 P3 Qt6.8.3 experimental render-stage shadow observer.
// Bounded asynchronous readback is counted and discarded; no Core callback,
// RFB publication, frame caching or update requests. Sampling is restricted to
// the D3D11 RHI backend, whose destruction contract completes pending readbacks.
#include <QMetaObject>

#include <cstdint>
#include <memory>

#include "quick/quick_native_frame_claim.hpp"

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

namespace HyRemote::detail {

class QuickNativeFrameShadow final
{
public:
    explicit QuickNativeFrameShadow(QQuickWindow *window);
    ~QuickNativeFrameShadow();

    void start();
    void stop() noexcept;

    // S2R-3 shadow claim simulator plumbing. Tokens carry metadata only and are
    // polled on the GUI thread; no pixel ever leaves this observer.
    bool takeCompletedToken(NativeFrameToken &token);
    void requestSample() noexcept;
    std::uint64_t renderSerial() const noexcept;
    std::uint64_t currentEpoch() const noexcept;
    long long lastRenderMs() const noexcept;
    bool renderActive(long long nowMs, long long windowMs) const noexcept;

    QuickNativeFrameShadow(const QuickNativeFrameShadow &) = delete;
    QuickNativeFrameShadow &operator=(const QuickNativeFrameShadow &) = delete;

private:
    struct State;
    QQuickWindow *m_window = nullptr; // only accessed while window emits its signal
    std::shared_ptr<State> m_state;
    QMetaObject::Connection m_afterRendering;
    QMetaObject::Connection m_sceneGraphInvalidated;
};

}  // namespace HyRemote::detail
