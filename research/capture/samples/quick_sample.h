// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway Qt Quick capture case (issue #3, cases B/D/E, reused for
// Quick3D and custom QQuickFramebufferObject content).

#include "harness/spike_types.h"

#include <QPointer>
#include <QQuickItem>
#include <QQuickView>

#include <atomic>

namespace hyremote {
namespace spike {

struct QuickSceneSpec
{
    QString id;
    QString title;
    QString targetFamily;
    QString qmlType;         // type name inside the HyRemoteCaptureSpike module
    QStringList requiredEnv;  // "KEY=value" needed before application creation
    QStringList sceneNotes;
};

class QuickSample : public CaptureSample
{
public:
    explicit QuickSample(QuickSceneSpec spec);
    ~QuickSample() override;

    QString id() const override;
    QString title() const override;
    QString targetFamily() const override;
    QStringList requiredEnv() const override;

    void prepare() override;
    void show() override;
    void tick(int frameIndex) override;
    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;
    QVariantMap info() const override;

protected:
    virtual QVariantMap extraInfo() const { return {}; }

    QSize requestedSize() const { return m_requestedSize; }
    QQuickWindow *window() const { return m_view.data(); }

private:
    CaptureOutcome grabWindow();
    CaptureOutcome grabRootItemToImage();

    QuickSceneSpec m_spec;
    QSize m_requestedSize{960, 600};
    QPointer<QQuickView> m_view;
    QPointer<QQuickItem> m_rootItem;

    std::atomic<int> m_beforeRenderingCount{0};
    std::atomic<int> m_afterRenderingCount{0};
    std::atomic<quintptr> m_renderThreadId{0};
};

}  // namespace spike
}  // namespace hyremote
