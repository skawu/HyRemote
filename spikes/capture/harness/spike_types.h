#pragma once

// SPIKE-01 experimental support types.
//
// Everything under spikes/capture/ is throwaway spike code for issue #3. These
// types are NOT a HyRemote public API, are not installed, and are expected to be
// deleted once the capture architecture decision is recorded.

#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include <functional>

class QWidget;

namespace hyremote {
namespace spike {
class DamageTracker;
}
}  // namespace hyremote

namespace hyremote {
namespace spike {

// Runs the event loop for `ms` so that the scene graph can render and Qt can
// deliver queued work. `ms <= 0` only drains events that are already queued.
inline void pumpEvents(int ms)
{
    if (ms <= 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::AllEvents);
}

// Result of a single capture operation.
struct CaptureOutcome
{
    QString path;          // stable label, e.g. "QWidget::grab()"
    bool primary = false;  // candidate for the v0.1 baseline
    bool ok = false;
    QImage frame;
    QString callerThread;  // thread observed while the call was made
    QString notes;
    qint64 elapsedNs = 0;

    // True when the returned frame may reference storage that the producer
    // reuses on the next capture (relevant for buffer ownership checks).
    bool producerReusesBuffer = false;

    // Set when the frame is only a view onto a producer-owned raw buffer. The
    // callable builds a QImage over that memory without copying it, so the
    // hand-off probe observes exactly what a zero-copy consumer would see.
    std::function<QImage()> borrowedView;

    // Storage-identity probe. Only paths that keep a producer target buffer can
    // report it; a zero address means "not observed".
    //
    // The addresses come from QImage::constBits(), which is documented and
    // implemented as returning d->data without detaching, so the probe itself
    // does not perturb the buffer. This is a real backing-store identity, unlike
    // QImage::cacheKey(), which mixes a serial number with a detach counter and
    // therefore changes for content edits and for detaches that do not reallocate.
    bool storageProbeAvailable = false;
    bool storageReplacedDuringCapture = false;
    quintptr storageAddressBefore = 0;
    quintptr storageAddressAfter = 0;
};

// Deterministic result of one damage-mapping control executed by a sample.
//
// The control exists because a damage ratio alone cannot show whether child
// regions were mapped into the shared target coordinate system: an unmapped
// (buggy) tracker still produces a plausible-looking ratio. A control therefore
// repaints exactly one known widget and compares the observed damage region with
// the rectangle that widget occupies in the target's coordinate system.
struct DamageControlResult
{
    QString description;
    QRect expectedTargetRect;  // in the shared target's coordinate system
    bool expectEmpty = false;  // true when the repaint must NOT reach the target
    qint64 observedArea = 0;
    QRect observedBoundingRect;
    quint64 observedPaintEvents = 0;
    quint64 observedExcludedPaintEvents = 0;
    bool passed = false;
};

// One independently testable capture target.
//
// Derived from QObject without Q_OBJECT on purpose: the spike only needs a
// valid QObject context for signal connections (for example the render-thread
// hooks of QQuickWindow), not introspection.
class CaptureSample : public QObject
{
public:
    using QObject::QObject;
    ~CaptureSample() override = default;

    virtual QString id() const = 0;
    virtual QString title() const = 0;
    virtual QString targetFamily() const = 0;

    // Process-wide settings this sample needs before QGuiApplication exists,
    // encoded as "KEY=value". Applied by main() before application creation.
    virtual QStringList requiredEnv() const { return {}; }

    // Substring patterns of capture paths the harness must not execute. Used to
    // measure expensive asynchronous paths separately from the baseline.
    void setDisabledPaths(const QStringList &paths) { m_disabledPaths = paths; }

    virtual void prepare() = 0;  // build the object tree (before show)
    virtual void show() = 0;     // make the target visible
    virtual void tick(int frameIndex) = 0;

    // Must be called on the GUI thread.
    virtual QVector<CaptureOutcome> captureAll() = 0;

    virtual QStringList observations() const = 0;
    virtual void shutdown() = 0;

    // QWidget whose coordinate system defines the shared target for damage
    // mapping. Null for target families without a QWidget root (native Quick
    // windows), where no region-carrying paint event can be attributed.
    virtual QWidget *damageTargetWidget() const { return nullptr; }

    // The harness hands its damage tracker to the sample so that the sample can
    // run deterministic mapping controls against it.
    virtual void setDamageTracker(DamageTracker *tracker) { Q_UNUSED(tracker); }

    // Deterministic controls for the damage mapping rules. Executed after the
    // measurement loop; an empty result means the sample has no control to run.
    virtual QVector<DamageControlResult> runDamageControls() { return {}; }

    // Recorded once per run (render backend, native handle, ...).
    virtual QVariantMap info() const { return {}; }

protected:
    bool isPathDisabled(const QString &path) const
    {
        for (const QString &pattern : m_disabledPaths) {
            if (!pattern.isEmpty() && path.contains(pattern, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QStringList m_disabledPaths;
};

}  // namespace spike
}  // namespace hyremote
