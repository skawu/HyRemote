// SPDX-License-Identifier: Apache-2.0
#include "harness/frame_sink.h"

#include "harness/spike_types.h"

#include <QElapsedTimer>

namespace hyremote {
namespace spike {

namespace {

// Long enough that a producer which reuses its target buffer has certainly
// overwritten it before the consumer looks at the pixels.
constexpr int kConsumerDelayMs = 40;
constexpr int kMaxPending = 24;

}  // namespace

quint32 frameChecksum(const QImage &image)
{
    if (image.isNull())
        return 0u;

    const QImage normalized = image.convertToFormat(QImage::Format_ARGB32);
    const uchar *base = normalized.constBits();
    const qsizetype total = qsizetype(normalized.bytesPerLine()) * normalized.height();
    if (!base || total <= 0)
        return 0u;

    const qsizetype step = qMax<qsizetype>(1, total / 4096);
    quint32 hash = 2166136261u;
    for (qsizetype i = 0; i < total; i += step) {
        hash ^= base[i];
        hash *= 16777619u;
    }
    hash ^= quint32(normalized.width());
    hash *= 16777619u;
    hash ^= quint32(normalized.height());
    return hash;
}

FrameSink::FrameSink(QObject *parent)
    : QObject(parent)
{
}

FrameSink::~FrameSink()
{
    stop();
}

void FrameSink::start()
{
    if (m_worker)
        return;

    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("hyremote-spike-sink"));
    m_thread.start();
}

void FrameSink::stop()
{
    if (!m_worker)
        return;

    waitForPending(10000);
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
}

void FrameSink::submit(const QString &path,
                       const QImage &frame,
                       quint32 producerChecksum,
                       const std::function<QImage()> &borrowedView)
{
    if (!m_worker)
        return;

    {
        QMutexLocker locker(&m_mutex);
        if (m_pending >= kMaxPending) {
            ++m_droppedOverflow;
            return;
        }
        ++m_pending;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [this, path, frame, producerChecksum, borrowedView] {
            QThread::msleep(kConsumerDelayMs);

            // A borrowed view reads the producer's memory directly, so a rewrite
            // is visible here; a QImage copy is protected by copy-on-write.
            const QImage observed = borrowedView ? borrowedView() : frame;
            const quint32 consumerChecksum = frameChecksum(observed);

            QMutexLocker locker(&m_mutex);
            ++m_delivered;
            m_deliveredByPath[path] += 1;
            if (borrowedView)
                ++m_borrowed;
            if (observed.isNull()) {
                ++m_nullFrames;
            } else if (consumerChecksum == producerChecksum) {
                ++m_stable;
            } else {
                ++m_mutated;
                m_mutatedByPath[path] += 1;
            }
            --m_pending;
        },
        Qt::QueuedConnection);
}

bool FrameSink::waitForPending(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    for (;;) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_pending == 0)
                return true;
        }
        if (timer.elapsed() > timeoutMs)
            return false;
        pumpEvents(10);
    }
}

QVariantMap FrameSink::report() const
{
    QMutexLocker locker(&m_mutex);

    QVariantMap result;
    result.insert(QStringLiteral("enabled"), m_worker != nullptr);
    result.insert(QStringLiteral("consumerDelayMs"), kConsumerDelayMs);
    result.insert(QStringLiteral("delivered"), m_delivered);
    result.insert(QStringLiteral("stable"), m_stable);
    result.insert(QStringLiteral("mutated"), m_mutated);
    result.insert(QStringLiteral("nullFrames"), m_nullFrames);
    result.insert(QStringLiteral("droppedOverflow"), m_droppedOverflow);
    result.insert(QStringLiteral("borrowedViews"), m_borrowed);

    QVariantMap mutatedByPath;
    for (auto it = m_mutatedByPath.constBegin(); it != m_mutatedByPath.constEnd(); ++it)
        mutatedByPath.insert(it.key(), it.value());
    result.insert(QStringLiteral("mutatedByPath"), mutatedByPath);

    QVariantMap deliveredByPath;
    for (auto it = m_deliveredByPath.constBegin(); it != m_deliveredByPath.constEnd(); ++it)
        deliveredByPath.insert(it.key(), it.value());
    result.insert(QStringLiteral("deliveredByPath"), deliveredByPath);

    return result;
}

}  // namespace spike
}  // namespace hyremote
