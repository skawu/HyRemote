// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway custom QQuickFramebufferObject capture case (issue #3, case E).

#include "samples/quick_sample.h"

#include <QRect>

namespace hyremote {
namespace spike {

class CustomFboSample : public QuickSample
{
public:
    CustomFboSample();

    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;

protected:
    QVariantMap extraInfo() const override;

private:
    // Logical coordinates of the custom FBO item inside the QML scene.
    QRect m_fboRegion{40, 40, 280, 280};
    int m_firstObserved = -1;
    int m_lastObserved = -1;
    int m_observedFrames = 0;
};

}  // namespace spike
}  // namespace hyremote
