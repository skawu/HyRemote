#pragma once

// #609 P3 Qt6.8.3 experimental render-stage shadow observer.
// No readback, Core callback, RFB publication, frame caching or update requests.
#include <QMetaObject>

#include <memory>

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

    QuickNativeFrameShadow(const QuickNativeFrameShadow &) = delete;
    QuickNativeFrameShadow &operator=(const QuickNativeFrameShadow &) = delete;

private:
    struct State;
    QQuickWindow *m_window = nullptr; // only accessed while window emits its signal
    std::shared_ptr<State> m_state;
    QMetaObject::Connection m_afterRendering;
};

}  // namespace HyRemote::detail
