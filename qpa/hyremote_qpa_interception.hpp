#pragma once

#include <QHash>
#include <QObject>
#include <QRect>

#include <functional>

class QEvent;
class QWindow;

namespace HyRemote::Qpa::Internal {

enum class InterceptionObject {
    PlatformWindow,
    BackingStore,
};

enum class InterceptionEventType {
    Created,
    GeometryChanged,
    VisibilityChanged,
    SurfaceAboutToBeDestroyed,
    WindowDestroyed,
};

// QPA-neutral observation record. Deliberately carries no QPlatformWindow,
// QPlatformBackingStore or backend-native handle. Consumers may correlate one
// public QWindow lifetime through windowToken without acquiring native ownership.
struct InterceptionEvent
{
    quint64 windowToken = 0;
    InterceptionObject object = InterceptionObject::PlatformWindow;
    InterceptionEventType type = InterceptionEventType::Created;
    QRect geometry;
    bool visible = false;
    bool topLevel = false;
};

using InterceptionCallback = std::function<void(const InterceptionEvent &)>;

// Exact-Qt private adapter seam used only inside qpa/.
//
// The native delegate still creates and owns all QPA objects. This observer is
// attached only to the public QWindow QObject lifetime and to
// QPlatformSurfaceEvent notifications delivered to that QWindow. It never wraps,
// replaces, retains or deletes a native platform object.
//
// Backing-store interception is intentionally creation-only in QPA-02. Qt 6.8.3
// exposes no ownership-safe public destruction/flush observer for QBackingStore;
// inventing one here would require wrapping QPlatformBackingStore and could change
// its private RHI/backing-store semantics. A later presentation gate must qualify
// any stronger interception explicitly before using it.
class InterceptionSeam final : public QObject
{
public:
    explicit InterceptionSeam(InterceptionCallback callback = {}, QObject *parent = nullptr);

    void observePlatformWindowCreated(QWindow *window);
    void observeBackingStoreCreated(QWindow *window);

    qsizetype trackedWindowCount() const noexcept { return m_windows.size(); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct WindowRecord {
        quint64 token = 0;
        QRect geometry;
        bool visible = false;
        bool topLevel = false;
        bool platformWindowCreated = false;
    };

    WindowRecord &ensureTracked(QWindow *window);
    void refreshSnapshot(QWindow *window, WindowRecord &record);
    void emitEvent(const WindowRecord &record,
                   InterceptionObject object,
                   InterceptionEventType type) const;

    InterceptionCallback m_callback;
    QHash<QWindow *, WindowRecord> m_windows;
    quint64 m_nextToken = 1;
};

}  // namespace HyRemote::Qpa::Internal
