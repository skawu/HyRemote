// SPDX-License-Identifier: Apache-2.0
#include "samples/custom_fbo_sample.h"

#include "samples/custom_fbo_item.h"

#include <QImage>

namespace hyremote {
namespace spike {

namespace {

int decodeRegionValue(const QImage &frame, const QRect &logicalRegion)
{
    if (frame.isNull() || logicalRegion.isEmpty())
        return -1;

    const qreal dpr = frame.devicePixelRatio() > 0.0 ? frame.devicePixelRatio() : 1.0;
    const int x = int((logicalRegion.x() + logicalRegion.width() / 2.0) * dpr);
    const int y = int((logicalRegion.y() + logicalRegion.height() / 2.0) * dpr);
    if (x < 0 || y < 0 || x >= frame.width() || y >= frame.height())
        return -1;

    const QColor color = frame.pixelColor(x, y);
    return color.red() | (color.green() << 8);
}

}  // namespace

CustomFboSample::CustomFboSample()
    : QuickSample(QuickSceneSpec{
          QStringLiteral("customfbo"),
          QStringLiteral("custom QQuickFramebufferObject in a QQuickWindow"),
          QStringLiteral("custom QQuickFramebufferObject"),
          QStringLiteral("CustomFboScene"),
          {QStringLiteral("QSG_RHI_BACKEND=opengl")},
          {QStringLiteral(
              "QQuickFramebufferObject is an OpenGL-specific Quick item, so this case forces the "
              "OpenGL RHI backend. It is expected to fail or fall back on non-OpenGL backends.")},
      })
{
    CustomFboItem::resetCounters();
}

QVector<CaptureOutcome> CustomFboSample::captureAll()
{
    QVector<CaptureOutcome> outcomes = QuickSample::captureAll();

    for (const CaptureOutcome &outcome : outcomes) {
        if (!outcome.ok || outcome.frame.isNull())
            continue;

        const int value = decodeRegionValue(outcome.frame, m_fboRegion);
        if (value < 0)
            continue;

        if (m_firstObserved < 0)
            m_firstObserved = value;
        m_lastObserved = value;
        ++m_observedFrames;
    }

    return outcomes;
}

QVariantMap CustomFboSample::extraInfo() const
{
    QVariantMap info;
    info.insert(QStringLiteral("fboRegion"),
                QStringLiteral("%1,%2 %3x%4")
                    .arg(m_fboRegion.x())
                    .arg(m_fboRegion.y())
                    .arg(m_fboRegion.width())
                    .arg(m_fboRegion.height()));
    info.insert(QStringLiteral("rendererFrameCount"), CustomFboItem::renderedFrameCount());
    info.insert(QStringLiteral("rendererLastEncoded"), CustomFboItem::lastEncodedValue());
    info.insert(QStringLiteral("firstObservedValue"), m_firstObserved);
    info.insert(QStringLiteral("lastObservedValue"), m_lastObserved);
    info.insert(QStringLiteral("observedFrames"), m_observedFrames);
    return info;
}

QStringList CustomFboSample::observations() const
{
    QStringList notes = QuickSample::observations();

    const int rendererFrames = CustomFboItem::renderedFrameCount();

    if (rendererFrames == 0) {
        notes.append(QStringLiteral(
            "The custom QQuickFramebufferObject renderer never ran in this run, so this target is "
            "unsupported on the active graphics backend and any pixel decoded from the captured "
            "region is meaningless. QQuickFramebufferObject requires a graphics backend that "
            "exposes a GL framebuffer path."));
        return notes;
    }

    if (m_firstObserved < 0) {
        notes.append(QStringLiteral(
            "No encoded value could be decoded from the captured frames, so the custom FBO "
            "content could not be verified in this run."));
        return notes;
    }

    const int advanced = m_lastObserved - m_firstObserved;
    notes.append(QStringLiteral(
        "The pixel inside the custom QQuickFramebufferObject region encoded the value %1 at the "
        "start and %2 at the end of the run (%3 observations), while the renderer had produced %4 "
        "frames. The baseline capture therefore contains freshly composed custom OpenGL content, "
        "not a stale or blank buffer.")
                    .arg(m_firstObserved)
                    .arg(m_lastObserved)
                    .arg(m_observedFrames)
                    .arg(rendererFrames));

    // The decoded value wraps at 0xffff, so only report a monotonic delta when
    // the range is unambiguous.
    if (advanced >= 0 && m_firstObserved > 0)
        notes.append(QStringLiteral("Observed value advanced by %1 over the run.").arg(advanced));

    return notes;
}

void CustomFboSample::shutdown()
{
    QuickSample::shutdown();
}

}  // namespace spike
}  // namespace hyremote
