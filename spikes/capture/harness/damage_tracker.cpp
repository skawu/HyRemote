#include "harness/damage_tracker.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPaintEvent>
#include <QWidget>

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

void DamageTracker::start(QWidget *target)
{
    if (m_active || !QCoreApplication::instance())
        return;

    m_target = target;
    QCoreApplication::instance()->installEventFilter(this);
    m_active = true;
}

void DamageTracker::stop()
{
    if (!m_active || !QCoreApplication::instance())
        return;

    QCoreApplication::instance()->removeEventFilter(this);
    m_active = false;
    m_target = nullptr;
}

DamageSnapshot DamageTracker::takeSnapshot()
{
    DamageSnapshot snapshot;
    snapshot.region = m_pending;
    snapshot.targetArea = m_target ? qint64(m_target->width()) * qint64(m_target->height()) : 0;
    snapshot.paintEvents = m_paintEvents;
    snapshot.excludedPaintEvents = m_excludedPaintEvents;
    snapshot.emptyRegions = m_emptyRegions;
    snapshot.updateRequests = m_updateRequests;
    snapshot.observedWidgets = m_observed.size();

    m_pending = QRegion();
    m_paintEvents = 0;
    m_excludedPaintEvents = 0;
    m_emptyRegions = 0;
    m_updateRequests = 0;
    return snapshot;
}

bool DamageTracker::accepts(const QWidget *widget) const
{
    if (!m_target || !widget)
        return false;

    // Walk the real parent chain instead of using isAncestorOf(): a widget inside
    // a popup, menu, dialog or tooltip can still have the shared target somewhere
    // further up as its QObject parent (QDialog/QMenu take the target as parent),
    // but it lives in its own top-level window and is therefore not part of the
    // shared root surface. Crossing a window boundary must reject it.
    for (const QWidget *current = widget; current; current = current->parentWidget()) {
        if (current == m_target.data())
            return true;
        if (current->isWindow())
            return false;
    }
    return false;
}

QRect DamageTracker::visibleClipFor(const QWidget *widget) const
{
    QRect clip = m_target->rect();
    for (const QWidget *current = widget; current && current != m_target.data();
         current = current->parentWidget()) {
        // mapTo() is only valid while the target really is an ancestor, which
        // accepts() has already established.
        const QPoint origin = current->mapTo(m_target.data(), QPoint(0, 0));
        clip &= QRect(origin, current->size());
        if (clip.isEmpty())
            return clip;
    }
    return clip;
}

bool DamageTracker::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_active)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::UpdateRequest) {
        // Qt Quick requests a full-window update without exposing any geometry,
        // so this counter is application-wide and carries no region.
        ++m_updateRequests;
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::Paint)
        return QObject::eventFilter(watched, event);

    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (!accepts(widget)) {
        ++m_excludedPaintEvents;
        return QObject::eventFilter(watched, event);
    }

    ++m_paintEvents;
    if (m_observed.size() < 4096)
        m_observed.insert(widget);

    const auto *paintEvent = static_cast<const QPaintEvent *>(event);
    QRegion mapped = paintEvent->region();
    if (mapped.isEmpty())
        return QObject::eventFilter(watched, event);

    // The region arrives in the painting widget's own coordinate system; move it
    // into the shared target's coordinate system before it can be unioned.
    if (widget != m_target.data()) {
        const QPoint origin = widget->mapTo(m_target.data(), QPoint(0, 0));
        mapped.translate(origin);
    }
    mapped &= QRegion(visibleClipFor(widget));

    if (mapped.isEmpty()) {
        ++m_emptyRegions;
        return QObject::eventFilter(watched, event);
    }

    m_pending += mapped;
    return QObject::eventFilter(watched, event);
}

}  // namespace spike
}  // namespace hyremote
