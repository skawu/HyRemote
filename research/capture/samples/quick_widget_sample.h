#pragma once

// SPIKE-01 throwaway QQuickWidget capture case (issue #3, case D).

#include "harness/spike_types.h"

#include <QPointer>
#include <QQuickWidget>

#include <functional>

namespace hyremote {
namespace spike {

class QuickWidgetSample : public CaptureSample
{
public:
    QuickWidgetSample();
    ~QuickWidgetSample() override;

    QString id() const override { return QStringLiteral("quickwidget"); }
    QString title() const override;
    QString targetFamily() const override { return QStringLiteral("QQuickWidget"); }

    void prepare() override;
    void show() override;
    void tick(int frameIndex) override;
    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;
    QVariantMap info() const override;

    // The top-level parent widget is the shared target, not the embedded Quick
    // render target.
    QWidget *damageTargetWidget() const override;

private:
    CaptureOutcome grabQuickContent();
    CaptureOutcome grabParentComposition();

    std::function<void(int)> m_advanceScene;

    QPointer<QWidget> m_root;
    QPointer<QQuickWidget> m_quickWidget;
    QPointer<QWidget> m_pulseWidget;
};

}  // namespace spike
}  // namespace hyremote
