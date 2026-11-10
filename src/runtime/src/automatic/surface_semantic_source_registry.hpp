#pragma once

#include "detail/semantic_source_property_observer.hpp"

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QStringList>
#include <QtGlobal>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <utility>

// Experimental, QtCore-only composition layer. Never scans process windows or
// assumes that a QQuickWindow == a QML document. A surface-owner supplies its
// own stable membership ID, surface QObject and a verified semantic source.
// The caller also checks that the source *belongs* to that surface (e.g. in
// Quick: QQuickItem::window() == the selected QQuickWindow).
//
// This class does not notify ViewerCaptureDemand on its own. Its callbacks
// must be explicitly enabled by a future qualified integration producer.
// No property NOTIFY => no fast path; the existing 250ms bounded probe stays.
namespace HyRemote::Runtime::Automatic {

class SurfaceSemanticSourceRegistry final : public QObject
{
public:
    using InvalidationHandler = std::function<void(quint64 surfaceId)>;

    explicit SurfaceSemanticSourceRegistry(InvalidationHandler callback,
                                           QObject *parent = nullptr)
        : QObject(parent), m_callback(std::move(callback))
    {
    }

    ~SurfaceSemanticSourceRegistry() override { clear(); }

    SurfaceSemanticSourceRegistry(const SurfaceSemanticSourceRegistry &) = delete;
    SurfaceSemanticSourceRegistry &operator=(const SurfaceSemanticSourceRegistry &) = delete;

    bool upsertSurface(quint64 id, QObject *surface, bool visible)
    {
        if (QThread::currentThread() != thread() || !id || !surface
            || surface->thread() != thread())
            return false;

        const auto existing = m_entries.find(id);
        if (existing != m_entries.end() && existing->second->surface == surface) {
            Entry &entry = *existing->second;
            if (entry.visible != visible) {
                entry.visible = visible;
                if (!visible)
                    entry.observer.detach(); // hidden: no stale fast wake
            }
            return true;
        }

        removeSurface(id); // invalidates old surface generation and timer
        auto entry = std::make_unique<Entry>();
        entry->surface = surface;
        entry->visible = visible;
        entry->generation = ++m_generation;
        const std::uint64_t generation = entry->generation;
        entry->destroyedConnection = QObject::connect(
            surface, &QObject::destroyed, this, [this, id, generation] {
                const auto it = m_entries.find(id);
                if (it != m_entries.end() && it->second->generation == generation)
                    removeSurface(id);
            });
        m_entries.emplace(id, std::move(entry));
        return true;
    }

    // The caller must resolve 'source' in THIS surface, not from any global
    // window list. expectedSurface identity prevents accidental cross-surface
    // registration after a target has been replaced under the same ID.
    bool watchSource(quint64 id, QObject *expectedSurface, QObject *source,
                     const QStringList &semanticProperties)
    {
        if (QThread::currentThread() != thread())
            return false;
        const auto it = m_entries.find(id);
        if (it == m_entries.end() || !it->second->visible || !expectedSurface
            || it->second->surface != expectedSurface)
            return false;

        Entry &entry = *it->second;
        const std::uint64_t generation = entry.generation;
        const bool attached = entry.observer.attach(
            source, semanticProperties, [this, id, generation] {
                const auto it = m_entries.find(id);
                if (it == m_entries.end())
                    return;
                Entry &current = *it->second;
                if (current.generation != generation || !current.visible
                    || !current.surface || !current.observer.active())
                    return;
                if (m_callback)
                    m_callback(id);
            });
        return attached;
    }

    void removeSurface(quint64 id)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        auto it = m_entries.find(id);
        if (it == m_entries.end())
            return;
        // Erase before destruction so any callback inspecting the registry
        // cannot rediscover this old membership generation.
        auto removed = std::move(it->second);
        m_entries.erase(it);
        QObject::disconnect(removed->destroyedConnection);
        removed->observer.detach();
    }

    void clear()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        while (!m_entries.empty())
            removeSurface(m_entries.begin()->first);
    }

    bool isWatching(quint64 id) const
    {
        const auto it = m_entries.find(id);
        return it != m_entries.end() && it->second->visible
               && !it->second->surface.isNull() && it->second->observer.active();
    }

    std::size_t surfaceCount() const noexcept { return m_entries.size(); }

private:
    struct Entry
    {
        QPointer<QObject> surface;
        bool visible = false;
        std::uint64_t generation = 0;
        QMetaObject::Connection destroyedConnection;
        HyRemote::detail::SemanticSourcePropertyObserver observer;
    };

    InvalidationHandler m_callback;
    std::map<quint64, std::unique_ptr<Entry>> m_entries;
    std::uint64_t m_generation = 0;
};

} // namespace HyRemote::Runtime::Automatic
