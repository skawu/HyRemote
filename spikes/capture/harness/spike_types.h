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
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include <functional>

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
