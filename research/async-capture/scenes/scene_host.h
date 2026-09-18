// SPDX-License-Identifier: Apache-2.0
#pragma once

// Async capture spike throwaway scene host (issue #16).
//
// One SceneHost wraps one capture target: a QQuickView, a QQuickWidget or a
// QOpenGLWidget hierarchy. It owns the deterministic tick patch (an 8-bit exact
// colour encoding of a counter the harness drives), an optional overlay sibling
// parented to the window's contentItem(), and the decode helpers the probes use.
//
// All coordinates used by the probes are expressed in the window's contentItem
// coordinate system and converted per capture target, which avoids calling
// QQuickItem::mapTo() between items that are not on the same ancestor chain.

#include <QColor>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QQuickItem;
class QQuickView;
class QQuickWidget;
class QQuickWindow;
class QWidget;

namespace hyremote {
namespace asyncspike {

enum class SceneKind {
    QuickView,     // QQuickView with a QML root item
    QuickWidget,   // QQuickWidget embedded in a QWidget hierarchy
    OpenGlWidget,  // QOpenGLWidget hierarchy, no QML
};

struct SceneSpec
{
    QString id;
    QString title;
    QString family;
    SceneKind kind = SceneKind::QuickView;
    QString qmlType;          // QML type name inside the HyRemoteAsyncSpike module
    QSize windowSize{960, 600};
    QStringList requiredEnv;  // "KEY=value" applied before application creation
    QStringList notes;
};

class SceneHost : public QObject
{
    Q_OBJECT

public:
    explicit SceneHost(SceneSpec spec);
    ~SceneHost() override;

    const SceneSpec &spec() const { return m_spec; }
    QString id() const { return m_spec.id; }
    QString family() const { return m_spec.family; }

    void prepare();
    void show();
    void hide();
    void shutdown();

    // Scene state driven from the harness on the GUI thread.
    void setTick(int value);
    int tick() const;
    void setFrozen(bool frozen);
    bool frozen() const;

    // Capture targets. All of these are public Qt API.
    QQuickWindow *window() const;
    QQuickView *quickView() const { return m_view; }
    QQuickWidget *quickWidget() const { return m_quickWidget; }
    QWidget *widget() const { return m_rootWidget; }
    QWidget *glWidget() const { return m_glWidget; }
    QQuickItem *sceneRootItem() const;   // the QML root item / root object
    QQuickItem *contentItem() const;     // the window's contentItem()

    // Synchronous public whole-window capture for this target family, used as the
    // baseline the asynchronous path is compared against.
    QString syncCaptureApi() const;
    QImage syncCapture();

    // Overlay sibling: a QML-declared item parented to contentItem(), which is the
    // same parent chain QML overlays, popups and tooltips use. It is deliberately
    // NOT a descendant of the QML root item.
    void addOverlaySibling();
    QQuickItem *overlayItem() const { return m_overlay; }
    QColor overlayColor() const;

    // Deterministic landmarks, in contentItem coordinates.
    QQuickItem *namedItem(const QString &objectName) const;
    QRect rectInContentItem(QQuickItem *item) const;
    QRect overlayRectInContentItem() const;
    QRect tickPatchRectInContentItem() const;
    QRect fboRectInContentItem() const;

    // Converts a contentItem-space point into the coordinate system of one capture
    // target (sceneRootItem(), contentItem() or, for window grabs, contentItem()).
    QPoint pointInTarget(const QPointF &contentItemPoint, QQuickItem *target) const;

    // QImage helpers.
    static QPoint toDevicePoint(const QImage &image, const QPoint &logicalPoint);
    static QColor sampleColor(const QImage &image, const QPoint &devicePoint);
    QColor sampleAtContentItemPoint(const QImage &image,
                                    const QPointF &contentItemPoint,
                                    QQuickItem *target) const;
    int decodeTickAt(const QImage &image, QQuickItem *target) const;
    int decodeFboCounterAt(const QImage &image, QQuickItem *target) const;

    int fboRendererFrameCount() const;
    int fboLastEncoded() const;

    QStringList observations() const;
    QVariantMap info() const;

private:
    void prepareQuickView();
    void prepareQuickWidget();
    void prepareOpenGlWidget();

    SceneSpec m_spec;
    QQuickView *m_view = nullptr;
    QQuickWidget *m_quickWidget = nullptr;
    QWidget *m_rootWidget = nullptr;
    QWidget *m_glWidget = nullptr;
    QQuickItem *m_overlay = nullptr;
    bool m_prepared = false;
};

}  // namespace asyncspike
}  // namespace hyremote
