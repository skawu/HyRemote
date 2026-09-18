// SPDX-License-Identifier: Apache-2.0
#pragma once

// Async capture spike throwaway custom QQuickFramebufferObject item (issue #16).
//
// The renderer clears its FBO with a clear color that encodes a monotonic counter,
// so a captured image can be attributed to the render that produced it. The
// `frozen` property stops the animation without discarding the last encoded
// value, which is what the fidelity comparison needs.

#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>

namespace hyremote {
namespace asyncspike {

class CustomFboItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool frozen READ frozen WRITE setFrozen NOTIFY frozenChanged)

public:
    explicit CustomFboItem(QQuickItem *parent = nullptr);

    bool frozen() const { return m_frozen; }
    void setFrozen(bool frozen);

    Renderer *createRenderer() const override;

    static int renderedFrameCount();
    static int lastEncodedValue();
    static void resetCounters();

signals:
    void frozenChanged();

private:
    bool m_frozen = false;
};

}  // namespace asyncspike
}  // namespace hyremote
