#pragma once

// Experimental, exact-Qt-Kit bridge: consume an already rendering Qt Quick
// swapchain frame only when the RHI explicitly permits transfer-source reads.
// It never starts a render pass and never calls Core on the render thread.
// Without both CMake and runtime opt-in, the product uses grabToImage only.
#include <QByteArray>
#include <QMetaObject>
#include <cstdint>
#include <memory>

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

namespace HyRemote::detail {

struct QuickNativeSample
{
    QByteArray pixels; // a single, owned, byte-ordered RGBA8/BGRA8 readback
    int width = 0;
    int height = 0;
    int format = 0; // QRhiTexture::Format, checked by the adapter
    double dpr = 1.0;
    std::uint64_t epoch = 0;
    std::uint64_t renderSerial = 0;
    long long sourceMs = 0;
};

class QuickNativeFrameSource final
{
public:
    explicit QuickNativeFrameSource(QQuickWindow *window);
    ~QuickNativeFrameSource();
    void start();
    void stop() noexcept;

    bool renderActive(long long nowMs, long long maxAgeMs) const noexcept;
    std::uint64_t renderSerial() const noexcept;
    std::uint64_t epoch() const noexcept;
    void requestNextRender() noexcept;
    void cancelRequest() noexcept;
    bool takeSample(QuickNativeSample &out);

private:
    struct State;
    QQuickWindow *m_window = nullptr; // accessed only by the Qt render signal
    std::shared_ptr<State> m_state;
    QMetaObject::Connection m_afterRendering;
    QMetaObject::Connection m_invalidated;
    QMetaObject::Connection m_initialized;
};

} // namespace HyRemote::detail
