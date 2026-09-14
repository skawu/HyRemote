#pragma once

// SPIKE-01 throwaway damage probe (see spike_types.h).

#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QSet>

class QWidget;

namespace hyremote {
namespace spike {

struct DamageSnapshot
{
    // Damage mapped into the shared target's own coordinate system and clipped
    // to the target rect. Empty when no target is bound.
    QRegion region;

    // Logical area of the shared target rect, in the target's own (logical)
    // coordinate system. Zero when no target is bound.
    qint64 targetArea = 0;

    quint64 paintEvents = 0;          // accepted, mapped into the target
    quint64 excludedPaintEvents = 0;  // outside the target subtree
    quint64 emptyRegions = 0;         // accepted, but nothing survived mapping/clipping
    quint64 updateRequests = 0;       // application-wide (Qt Quick posts these without geometry)
    int observedWidgets = 0;          // distinct accepted widgets

    // Area of the mapped damage region, in the target's logical coordinate
    // system.
    qint64 area() const;
};

// Collects damage information using only public Qt APIs, relative to one shared
// target widget.
//
// An application-level event filter receives the events Qt delivers to every
// object in the application, so QPaintEvent::region() can be observed without
// touching Qt private headers. A paint region is expressed in the coordinate
// system of the widget that painted it, so this tracker:
//
//   1. binds to one shared target QWidget (the root of the shared surface);
//   2. accepts paint events only from that target and its descendants within the
//      same window, and rejects anything whose parent chain crosses a window
//      boundary (popups, menus, dialogs, tooltips), which are a composition
//      policy question rather than part of the root target;
//   3. translates each accepted region into the target's coordinate system and
//      clips it to the visible intersection of the widget's ancestor chain and
//      the target rect;
//   4. only then unions it into the pending damage region.
//
// Without step 3 the union would mix child-local rectangles and would not be a
// region of the shared target.
class DamageTracker : public QObject
{
    Q_OBJECT

public:
    explicit DamageTracker(QObject *parent = nullptr);

    void start(QWidget *target);
    void stop();

    // Returns the damage accumulated since the previous call and resets it.
    DamageSnapshot takeSnapshot();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool accepts(const QWidget *widget) const;
    QRect visibleClipFor(const QWidget *widget) const;

    bool m_active = false;
    QPointer<QWidget> m_target;
    QRegion m_pending;
    quint64 m_paintEvents = 0;
    quint64 m_excludedPaintEvents = 0;
    quint64 m_emptyRegions = 0;
    quint64 m_updateRequests = 0;
    QSet<const QWidget *> m_observed;
};

}  // namespace spike
}  // namespace hyremote
