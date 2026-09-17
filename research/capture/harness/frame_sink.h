#pragma once

// SPIKE-01 throwaway frame hand-off probe (see spike_types.h).
//
// The sink is deliberately trivial: it is not a transport. Its only purpose is
// to answer the ownership question from issue #3 - can a captured frame be
// handed to another thread while the producer keeps capturing?

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>

#include <QMap>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace hyremote {
namespace spike {

class FrameSink : public QObject
{
    Q_OBJECT

public:
    explicit FrameSink(QObject *parent = nullptr);
    ~FrameSink() override;

    void start();
    void stop();

    // Producer side (GUI thread). `producerChecksum` must be computed before the
    // producer is allowed to touch the frame again.
    //
    // When `borrowedView` is set the consumer reads that view instead of a
    // QImage copy. The view must reference producer-owned memory that stays
    // allocated for the lifetime of the run, so the probe detects a rewrite
    // without risking a use-after-free.
    void submit(const QString &path,
                const QImage &frame,
                quint32 producerChecksum,
                const std::function<QImage()> &borrowedView = {});

    // Blocks until the worker drained its queue, pumping the event loop so the
    // producer stays responsive.
    bool waitForPending(int timeoutMs);

    QVariantMap report() const;

private:
    QThread m_thread;
    QObject *m_worker = nullptr;

    mutable QMutex m_mutex;
    int m_pending = 0;
    int m_delivered = 0;
    int m_nullFrames = 0;
    int m_stable = 0;
    int m_mutated = 0;
    int m_droppedOverflow = 0;
    int m_borrowed = 0;
    QMap<QString, int> m_deliveredByPath;
    QMap<QString, int> m_mutatedByPath;
};

// Cheap, order-sensitive checksum over a bounded subsample of the pixel bytes.
quint32 frameChecksum(const QImage &image);

}  // namespace spike
}  // namespace hyremote
