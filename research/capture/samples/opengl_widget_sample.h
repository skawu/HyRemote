// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway QOpenGLWidget capture case (issue #3, case C).

#include "harness/spike_types.h"

#include <QImage>
#include <QPointer>

#include <functional>

class QWidget;

namespace hyremote {
namespace spike {

class OpenGlWidgetSample : public CaptureSample
{
public:
    OpenGlWidgetSample();
    ~OpenGlWidgetSample() override;

    QString id() const override { return QStringLiteral("openglwidget"); }
    QString title() const override;
    QString targetFamily() const override { return QStringLiteral("QOpenGLWidget"); }

    void prepare() override;
    void show() override;
    void tick(int frameIndex) override;
    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;
    QVariantMap info() const override;

    // The top-level parent widget is the shared target, not the GL child.
    QWidget *damageTargetWidget() const override;

private:
    CaptureOutcome grabFramebuffer();
    CaptureOutcome grabGlWidgetViaWidgetApi();
    CaptureOutcome grabParentComposition();

    std::function<void(int)> m_advanceScene;
    std::function<QImage()> m_grabFramebuffer;

    QPointer<QWidget> m_root;
    QPointer<QWidget> m_glWidget;
    QPointer<QWidget> m_pulseWidget;
};

}  // namespace spike
}  // namespace hyremote
