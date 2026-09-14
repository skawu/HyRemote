#include "harness/damage_tracker.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPaintEvent>

namespace hyremote {
namespace spike {

qint64 DamageSnapshot::area() const
{
    qint64 total = 0;
    for (const QRect &rect : region)
        total += qint64(rect.width()) * qint64(rect.height());
    return total;
}

DamageTracker::DamageTracker(QObject *parent)
    : QObject(parent)
{
}

void DamageTracker::start()
{
    if (m_active || !QCoreApplication::instance())
        return;

    QCoreApplication::instance()->installEventFilter(this);
    m_active = true;
}

void DamageTracker::stop()
{
    if (!m_active || !QCoreApplication::instance())
        return;

    QCoreApplication::instance()->removeEventFilter(this);
    m_active = false;
}

DamageSnapshot DamageTracker::takeSnapshot()
{
    DamageSnapshot snapshot;
    snapshot.region = m_pending;
    snapshot.paintEvents = m_paintEvents;
    snapshot.updateRequests = m_updateRequests;
    snapshot.observedObjects = m_observed.size();

    m_pending = QRegion();
    m_paintEvents = 0;
    m_updateRequests = 0;
    return snapshot;
}

bool DamageTracker::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        ++m_paintEvents;
        if (m_observed.size() < 4096)
            m_observed.insert(watched);

        const auto *paintEvent = static_cast<const QPaintEvent *>(event);
        if (!paintEvent->region().isEmpty())
            m_pending += paintEvent->region();
    } else if (event->type() == QEvent::UpdateRequest) {
        ++m_updateRequests;
        if (m_observed.size() < 4096)
            m_observed.insert(watched);
    }

    return QObject::eventFilter(watched, event);
}

}  // namespace spike
}  // namespace hyremote
