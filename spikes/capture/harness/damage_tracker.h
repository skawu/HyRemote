#pragma once

// SPIKE-01 throwaway damage probe (see spike_types.h).

#include <QObject>
#include <QRegion>
#include <QSet>

namespace hyremote {
namespace spike {

struct DamageSnapshot
{
    QRegion region;
    quint64 paintEvents = 0;
    quint64 updateRequests = 0;
    int observedObjects = 0;

    qint64 area() const;
};

// Collects damage information using only public Qt APIs.
//
// An application-level event filter sees the events Qt delivers to every object
// in the application, so QPaintEvent::region() can be observed without touching
// Qt private headers. QEvent::UpdateRequest is counted separately because Qt
// Quick requests full-window updates without exposing any region.
class DamageTracker : public QObject
{
    Q_OBJECT

public:
    explicit DamageTracker(QObject *parent = nullptr);

    void start();
    void stop();

    // Returns the damage accumulated since the previous call and resets it.
    DamageSnapshot takeSnapshot();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_active = false;
    QRegion m_pending;
    quint64 m_paintEvents = 0;
    quint64 m_updateRequests = 0;
    QSet<const QObject *> m_observed;
};

}  // namespace spike
}  // namespace hyremote
