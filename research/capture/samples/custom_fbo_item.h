// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway custom QQuickFramebufferObject item (issue #3, case E).
//
// The renderer clears its FBO with a clear color that encodes a monotonically
// increasing counter. The spike can therefore decode a captured pixel and prove
// whether the capture contains freshly rendered custom OpenGL content.

#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>

namespace hyremote {
namespace spike {

class CustomFboItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit CustomFboItem(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    // Total number of frames the renderer has produced.
    static int renderedFrameCount();

    // Value encoded in the most recent rendered frame.
    static int lastEncodedValue();

    static void resetCounters();
};

}  // namespace spike
}  // namespace hyremote
