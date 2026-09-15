#include "hyremote_qpa_interception.hpp"

#include <QEvent>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#include <utility>

namespace HyRemote::Qpa::Internal {

InterceptionSeam::InterceptionSeam(InterceptionCallback callback, QObject *parent)
    : QObject(parent)
    , m_callback(std::move(callback))
{
}

InterceptionSeam::WindowRecord &InterceptionSeam::ensureTracked(QWindow *window)
{
    auto it = m_windows.find(window);
    if (it != m_windows.end())
        return it.value();

    WindowRecord record;
    record.token = m_nextToken++;
    refreshSnapshot(window, record);

    it = m_windows.insert(window, record);
    window->installEventFilter(this);

    QObject::connect(window, &QObject::destroyed, this, [this, window] {
        auto recordIt = m_windows.find(window);
        if (recordIt == m_windows.end())
            return;

        const WindowRecord record = recordIt.value();
        if (record.platformWindowCreated)
            emitEvent(record, InterceptionObject::PlatformWindow, InterceptionEventType::WindowDestroyed);
        if (record.backingStoreCreated)
            emitEvent(record, InterceptionObject::BackingStore, InterceptionEventType::WindowDestroyed);
        m_windows.erase(recordIt);
    });

    return it.value();
}

void InterceptionSeam::refreshSnapshot(QWindow *window, WindowRecord &record)
{
    record.geometry = window->geometry();
    record.visible = window->isVisible();
    record.topLevel = window->isTopLevel();
}

void InterceptionSeam::emitEvent(const WindowRecord &record,
                                 InterceptionObject object,
                                 InterceptionEventType type) const
{
    if (!m_callback)
        return;

    InterceptionEvent event;
    event.windowToken = record.token;
    event.object = object;
    event.type = type;
    event.geometry = record.geometry;
    event.visible = record.visible;
    event.topLevel = record.topLevel;
    m_callback(event);
}

void InterceptionSeam::observePlatformWindowCreated(QWindow *window)
{
    if (!window)
        return;

    WindowRecord &record = ensureTracked(window);
    refreshSnapshot(window, record);
    if (!record.platformWindowCreated) {
        record.platformWindowCreated = true;
        emitEvent(record, InterceptionObject::PlatformWindow, InterceptionEventType::Created);
    }
}

void InterceptionSeam::observeBackingStoreCreated(QWindow *window)
{
    if (!window)
        return;

    WindowRecord &record = ensureTracked(window);
    refreshSnapshot(window, record);
    if (!record.backingStoreCreated) {
        record.backingStoreCreated = true;
        emitEvent(record, InterceptionObject::BackingStore, InterceptionEventType::Created);
    }
}

bool InterceptionSeam::eventFilter(QObject *watched, QEvent *event)
{
    auto *window = qobject_cast<QWindow *>(watched);
    auto it = window ? m_windows.find(window) : m_windows.end();
    if (!window || it == m_windows.end())
        return QObject::eventFilter(watched, event);

    WindowRecord &record = it.value();

    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
        refreshSnapshot(window, record);
        if (record.platformWindowCreated)
            emitEvent(record, InterceptionObject::PlatformWindow, InterceptionEventType::GeometryChanged);
        break;
    case QEvent::Show:
    case QEvent::Hide:
        refreshSnapshot(window, record);
        if (record.platformWindowCreated)
            emitEvent(record, InterceptionObject::PlatformWindow, InterceptionEventType::VisibilityChanged);
        break;
    case QEvent::PlatformSurface: {
        const auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            refreshSnapshot(window, record);
            if (record.backingStoreCreated) {
                emitEvent(record,
                          InterceptionObject::BackingStore,
                          InterceptionEventType::SurfaceAboutToBeDestroyed);
                record.backingStoreCreated = false;
            }
            if (record.platformWindowCreated) {
                emitEvent(record,
                          InterceptionObject::PlatformWindow,
                          InterceptionEventType::SurfaceAboutToBeDestroyed);
                record.platformWindowCreated = false;
            }
        }
        break;
    }
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

}  // namespace HyRemote::Qpa::Internal
