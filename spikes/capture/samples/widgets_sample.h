#pragma once

// SPIKE-01 throwaway QWidget/raster capture case (issue #3, case A).

#include "harness/spike_types.h"

#include <QByteArray>
#include <QImage>
#include <QPointer>
#include <QSize>

#include <functional>

class QLabel;
class QMenu;
class QDialog;
class QWidget;

namespace hyremote {
namespace spike {

class WidgetsSample : public CaptureSample
{
public:
    WidgetsSample();
    ~WidgetsSample() override;

    QString id() const override { return QStringLiteral("widgets"); }
    QString title() const override;
    QString targetFamily() const override { return QStringLiteral("QWidget/raster"); }

    void prepare() override;
    void show() override;
    void tick(int frameIndex) override;
    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;
    QVariantMap info() const override;

private:
    CaptureOutcome grabWidget();
    CaptureOutcome renderIntoReusedImage();
    CaptureOutcome renderIntoFreshImage();
    CaptureOutcome renderIntoBorrowedBuffer();

    std::function<void(int)> m_advanceScene;

    QPointer<QWidget> m_root;
    QPointer<QLabel> m_counter;
    QPointer<QDialog> m_dialog;
    QPointer<QMenu> m_menu;

    QImage m_targetImage;
    quint64 m_reuseAttempts = 0;
    quint64 m_reuseReallocations = 0;
    quint64 m_lastReuseCacheKey = 0;

    QByteArray m_borrowedBuffer;
    QSize m_borrowedSize;
};

}  // namespace spike
}  // namespace hyremote
