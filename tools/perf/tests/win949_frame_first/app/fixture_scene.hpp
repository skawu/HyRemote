// WIN949 NON-PRODUCT shared frame scene (#949 Windows local verification).
//
// Shared by both fixture mains:
//   frame_fixture.cpp      - Qt6.8 Generic plugin route (-plugin hyremote:port=...)
//   frame_fixture_cpp.cpp  - C++ API route (required for Qt5.15, whose Generic
//                            plugin is Qt6-only)
//
// The scene is an ordinary QQuickItem: four deterministic quadrants, an 8-phase
// marker rectangle, a moving square and a pointer indicator. Neither main may
// include HyRemote headers here; only frame_fixture_cpp.cpp links the product.

#pragma once

#include <QColor>
#include <QDateTime>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <cstdio>

namespace win949 {

constexpr int kMarkerSize = 40;
constexpr int kMarkerX = 16;
constexpr int kMarkerY = 16;
constexpr int kMoverSize = 120;
constexpr int kPointerSize = 24;

struct Rgb {
    int r;
    int g;
    int b;
};

// Deterministic four-quadrant colours (device independent).
constexpr Rgb kQuad[4] = {{215, 36, 57}, {37, 212, 66}, {42, 69, 225}, {240, 200, 20}};
constexpr Rgb kMover{255, 0, 255};
constexpr Rgb kPointer{0, 255, 255};

// Eight unambiguous marker colours. A Viewer observation can therefore be
// attributed to an exact phase even when the pipeline lags by several phases;
// alternating black/white would make every lag a two-phase ambiguity.
// NOTE: 8 phases at 250 ms means a lag >= 2000 ms is colour-aliased; the probe
// flags such samples as ambiguous instead of reporting them as exact latency.
constexpr int kMarkerPhases = 8;
constexpr Rgb kMarkerPalette[kMarkerPhases] = {
    {0, 0, 0},       {255, 255, 255}, {255, 0, 0},   {0, 255, 0},
    {0, 0, 255},     {255, 255, 0},   {0, 255, 255}, {255, 0, 255},
};

inline QColor toColor(const Rgb &rgb)
{
    return QColor(rgb.r, rgb.g, rgb.b);
}

inline void emitLine(const char *tag, const QString &body)
{
    std::fprintf(stdout, "%s %s\n", tag, qPrintable(body));
    std::fflush(stdout);
}

inline QString stamp()
{
    return QStringLiteral("t=%1").arg(QDateTime::currentMSecsSinceEpoch());
}

class FixtureScene final : public QQuickItem
{
public:
    explicit FixtureScene(QQuickItem *parent = nullptr) : QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setAcceptHoverEvents(true);
    }

    void setSceneGeometry(int width, int height)
    {
        // Keep the mover centred on *this* geometry. Leaving it at the previous
        // centre made a resize silently move the magenta square over the
        // quadrant-4 sample point, which then looked like a pixel failure.
        m_w = width;
        m_h = height;
        setSize(QSizeF(width, height));
        if (m_moverX == 0 || !m_moverPlaced || m_moverCentred) {
            m_moverX = (width - kMoverSize) / 2;
            m_moverY = (height - kMoverSize) / 2;
            m_moverPlaced = true;
            m_moverCentred = true;
        }
        update();
    }

    void setMarkerPhase(int phase)
    {
        m_markerPhase = ((phase % kMarkerPhases) + kMarkerPhases) % kMarkerPhases;
        update();
    }

    void setMoverPosition(int x, int y)
    {
        m_moverX = x;
        m_moverY = y;
        m_moverCentred = false;
        update();
    }

    void setPointerPosition(int x, int y)
    {
        m_pointerX = x;
        m_pointerY = y;
        m_pointerVisible = true;
        update();
    }

protected:
    void hoverMoveEvent(QHoverEvent *event) override
    {
        const QPointF p = eventPosition(event);
        handlePointer(p.x(), p.y());
        QQuickItem::hoverMoveEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPointF p = eventPosition(event);
        handlePointer(p.x(), p.y());
        QQuickItem::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const QPointF p = eventPosition(event);
        handlePointer(p.x(), p.y());
        QQuickItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const QPointF p = eventPosition(event);
        handlePointer(p.x(), p.y());
        QQuickItem::mouseReleaseEvent(event);
    }

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override
    {
        auto *root = static_cast<QSGNode *>(oldNode);
        if (!root) {
            root = new QSGNode;
            // 0..3 quadrants, 4 marker, 5 mover, 6 pointer indicator.
            for (int i = 0; i < 7; ++i)
                root->appendChildNode(makeRect());
        }

        const qreal halfW = m_w / 2.0;
        const qreal halfH = m_h / 2.0;
        setRect(root->childAtIndex(0), 0, 0, halfW, halfH, kQuad[0]);
        setRect(root->childAtIndex(1), halfW, 0, halfW, halfH, kQuad[1]);
        setRect(root->childAtIndex(2), 0, halfH, halfW, halfH, kQuad[2]);
        setRect(root->childAtIndex(3), halfW, halfH, halfW, halfH, kQuad[3]);
        setRect(root->childAtIndex(4), kMarkerX, kMarkerY, kMarkerSize, kMarkerSize,
                kMarkerPalette[m_markerPhase]);
        setRect(root->childAtIndex(5), m_moverX, m_moverY, kMoverSize, kMoverSize, kMover);
        if (m_pointerVisible)
            setRect(root->childAtIndex(6), m_pointerX, m_pointerY, kPointerSize, kPointerSize, kPointer);
        else
            setRect(root->childAtIndex(6), -1000, -1000, 1, 1, kPointer);
        return root;
    }

private:
    static QPointF eventPosition(QHoverEvent *event)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return event->position();
#else
        return event->posF();
#endif
    }

    static QPointF eventPosition(QMouseEvent *event)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return event->position();
#else
        return event->localPos();
#endif
    }

    static QSGGeometryNode *makeRect()
    {
        auto *node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        auto *material = new QSGFlatColorMaterial;
        material->setColor(Qt::white);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        return node;
    }

    static void setRect(QSGNode *node, qreal x, qreal y, qreal w, qreal h, const Rgb &rgb)
    {
        auto *rect = static_cast<QSGGeometryNode *>(node);
        if (!rect)
            return;
        auto *geometry = rect->geometry();
        if (!geometry)
            return;
        auto *vertices = geometry->vertexDataAsPoint2D();
        vertices[0].set(static_cast<float>(x), static_cast<float>(y));
        vertices[1].set(static_cast<float>(x + w), static_cast<float>(y));
        vertices[2].set(static_cast<float>(x), static_cast<float>(y + h));
        vertices[3].set(static_cast<float>(x + w), static_cast<float>(y + h));
        auto *material = static_cast<QSGFlatColorMaterial *>(rect->material());
        if (material)
            material->setColor(toColor(rgb));
        rect->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    void handlePointer(qreal x, qreal y)
    {
        const int px = static_cast<int>(x);
        const int py = static_cast<int>(y);
        if (px == m_lastPointerX && py == m_lastPointerY)
            return;
        m_lastPointerX = px;
        m_lastPointerY = py;
        setPointerPosition(px, py);
        emitLine("WIN949_POINTER", QStringLiteral("x=%1 y=%2 %3").arg(px).arg(py).arg(stamp()));
    }

    int m_w = 1920;
    int m_h = 1080;
    int m_markerPhase = 0;
    int m_moverX = 0;
    int m_moverY = 0;
    bool m_moverPlaced = false;
    bool m_moverCentred = true;
    int m_pointerX = 0;
    int m_pointerY = 0;
    bool m_pointerVisible = false;
    int m_lastPointerX = -1;
    int m_lastPointerY = -1;
};

}  // namespace win949
