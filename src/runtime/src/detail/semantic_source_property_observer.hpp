#pragma once

#include <QByteArray>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <functional>
#include <utility>

// A deliberately UNWIRED QtCore-only study of a source-owned invalidation
// contract. This is NOT a general Qt Quick scene-dirty detector.
//
// An integration that OWNS the source/target provides both the exact QObject
// and the names of documented semantic Q_PROPERTY ... NOTIFY properties.
// HyRemote's grabToImage / render-loop events are never inputs. Qt 5.15 and
// Qt 6.8 public QObject/QMetaProperty/QMetaMethod APIs only.
//
// Fail-closed:
// - no global QGuiApplication::allWindows() or arbitrary QML tree crawling;
// - every requested property must exist AND have NOTIFY;
// - source and observer must belong to the calling GUI QObject thread;
// - no duplicate property names and no empty set;
// - no partial subscriptions or opportunistic fallback to render signals;
// - one coalesced callback per event-loop turn; detach/destroy cancel pending;
// - only the integration may forward callbacks to ViewerCaptureDemand,
//   which still enforces zero-viewer, RFB backpressure and bounded fallback.
//
// Qt's NOTIFY contract is only a hint that the *named property* changed.
// It does not prove arbitrary pixels, custom painting, video textures,
// animated scenegraph nodes, dynamic items or every QML binding changed.
namespace HyRemote::detail {

class SemanticSourcePropertyObserver final : public QObject
{
public:
    using Callback = std::function<void()>;

    explicit SemanticSourcePropertyObserver(QObject *parent = nullptr)
        : QObject(parent), m_coalescer(this)
    {
        m_coalescer.setInterval(0);
        m_coalescer.setSingleShot(true);
        QObject::connect(&m_coalescer, &QTimer::timeout, this, [this] {
            if (!m_active || m_source.isNull() || !m_callback)
                return;
            // Copy: the callback itself may call detach() or delete an owning
            // integration. Never access this after invoking the callback.
            const Callback callback = m_callback;
            callback();
        });
    }

    ~SemanticSourcePropertyObserver() override { detach(); }

    SemanticSourcePropertyObserver(const SemanticSourcePropertyObserver &) = delete;
    SemanticSourcePropertyObserver &operator=(const SemanticSourcePropertyObserver &) = delete;

    bool attach(QObject *source, const QStringList &propertyNames, Callback callback)
    {
        if (QThread::currentThread() != thread())
            return false;
        detach(); // a failed new attachment must leave no old producer enabled

        if (!source || !callback || propertyNames.isEmpty() || source->thread() != thread())
            return false;

        const QMetaObject *meta = source->metaObject();
        const int slotIndex = m_coalescer.metaObject()->indexOfSlot("start()");
        if (slotIndex < 0)
            return false;
        const QMetaMethod startSlot = m_coalescer.metaObject()->method(slotIndex);
        QSet<QByteArray> visited;
        QVector<QMetaMethod> methods;
        methods.reserve(propertyNames.size());

        for (const QString &name : propertyNames) {
            const QByteArray bytes = name.toUtf8();
            if (bytes.isEmpty() || visited.contains(bytes))
                return false;
            visited.insert(bytes);
            const int index = meta->indexOfProperty(bytes.constData());
            if (index < 0)
                return false;
            const QMetaProperty property = meta->property(index);
            if (!property.hasNotifySignal())
                return false;
            methods.append(property.notifySignal());
        }

        for (const QMetaMethod &method : methods) {
            const QMetaObject::Connection connection =
                QObject::connect(source, method, &m_coalescer, startSlot,
                                 Qt::AutoConnection);
            if (!connection) {
                detach();
                return false;
            }
            m_connections.append(connection);
        }

        const QMetaObject::Connection destroyed = QObject::connect(
            source, &QObject::destroyed, this, [this] { detach(); },
            Qt::AutoConnection);
        if (!destroyed) {
            detach();
            return false;
        }
        m_connections.append(destroyed);
        m_source = source;
        m_callback = std::move(callback);
        m_active = true;
        return true;
    }

    void detach()
    {
        // Integration contract: attach/detach/destructor on owning thread;
        // no BlockingQueuedConnection from a capture/render/transport worker.
        Q_ASSERT(QThread::currentThread() == thread());
        m_active = false;
        m_coalescer.stop();
        for (const QMetaObject::Connection &connection : m_connections)
            QObject::disconnect(connection);
        m_connections.clear();
        m_callback = {};
        m_source.clear();
    }

    bool active() const noexcept { return m_active && !m_source.isNull(); }

private:
    QTimer m_coalescer;
    QPointer<QObject> m_source;
    QVector<QMetaObject::Connection> m_connections;
    Callback m_callback;
    bool m_active = false;
};

} // namespace HyRemote::detail
