// SPDX-License-Identifier: Apache-2.0
#include "scenes/scene_host.h"

#include "scenes/custom_fbo_item.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QOpenGLWidget>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace hyremote {
namespace asyncspike {

namespace {

void pumpEvents(int ms)
{
    if (ms <= 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::AllEvents);
}

const char *kOverlayColor = "#ff00ff";

class GlPaintWidget : public QOpenGLWidget
{
public:
    explicit GlPaintWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
    {
        setMinimumSize(560, 400);
    }

    void setPhase(int phase)
    {
        m_phase = phase;
        update();
    }

protected:
    void paintGL() override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient gradient(0.0, 0.0, double(width()), double(height()));
        gradient.setColorAt(0.0, QColor(24, 90, 160));
        gradient.setColorAt(1.0, QColor(150, 55, 40));
        painter.fillRect(rect(), gradient);

        painter.setPen(QPen(QColor(240, 240, 240), 3.0));
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0),
                            width() / 4.0,
                            height() / 4.0);

        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("QOpenGLWidget\nphase %1").arg(m_phase));
    }

private:
    int m_phase = 0;
};

QUrl moduleSceneUrl(const QString &typeName)
{
    QUrl url;
    url.setScheme(QStringLiteral("qrc"));
    url.setPath(QStringLiteral("/qt/qml/HyRemoteAsyncSpike/%1.qml").arg(typeName));
    return url;
}

}  // namespace

SceneHost::SceneHost(SceneSpec spec)
    : m_spec(std::move(spec))
{
}

SceneHost::~SceneHost() = default;

void SceneHost::prepare()
{
    if (m_prepared)
        return;
    m_prepared = true;

    switch (m_spec.kind) {
    case SceneKind::QuickView:
        prepareQuickView();
        break;
    case SceneKind::QuickWidget:
        prepareQuickWidget();
        break;
    case SceneKind::OpenGlWidget:
        prepareOpenGlWidget();
        break;
    }
}

void SceneHost::prepareQuickView()
{
    CustomFboItem::resetCounters();

    auto *view = new QQuickView;
    view->setTitle(QStringLiteral("HyRemote async spike - %1").arg(m_spec.id));
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->resize(m_spec.windowSize);
    view->loadFromModule(QStringLiteral("HyRemoteAsyncSpike"), m_spec.qmlType);

    if (view->status() != QQuickView::Ready) {
        for (const QQmlError &error : view->errors())
            qWarning("ASYNC-SPIKE: QML load error: %s", qPrintable(error.toString()));
    }

    m_view = view;
}

void SceneHost::prepareQuickWidget()
{
    auto *root = new QWidget;
    root->setWindowTitle(QStringLiteral("HyRemote async spike - %1").arg(m_spec.id));

    auto *layout = new QVBoxLayout(root);
    layout->addWidget(new QLabel(QStringLiteral("parent widget around the QQuickWidget"), root));

    auto *row = new QHBoxLayout;
    auto *quickWidget = new QQuickWidget(root);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setClearColor(QColor(10, 12, 16));
    quickWidget->setSource(moduleSceneUrl(m_spec.qmlType));
    if (quickWidget->status() != QQuickWidget::Ready) {
        for (const QQmlError &error : quickWidget->errors())
            qWarning("ASYNC-SPIKE: QQuickWidget QML error: %s", qPrintable(error.toString()));
    }

    auto *sibling = new QLabel(QStringLiteral("raster sibling"), root);
    sibling->setAutoFillBackground(true);
    row->addWidget(quickWidget, 3);
    row->addWidget(sibling, 1);
    layout->addLayout(row, 1);

    root->resize(m_spec.windowSize);

    m_rootWidget = root;
    m_quickWidget = quickWidget;
}

void SceneHost::prepareOpenGlWidget()
{
    auto *root = new QWidget;
    root->setWindowTitle(QStringLiteral("HyRemote async spike - %1").arg(m_spec.id));

    auto *layout = new QVBoxLayout(root);
    layout->addWidget(new QLabel(QStringLiteral("parent widget around the QOpenGLWidget"), root));

    auto *glWidget = new GlPaintWidget(root);
    layout->addWidget(glWidget, 1);

    root->resize(m_spec.windowSize);

    m_rootWidget = root;
    m_glWidget = glWidget;
}

void SceneHost::show()
{
    if (m_view) {
        m_view->show();
        m_view->raise();
        m_view->requestActivate();
    } else if (m_rootWidget) {
        m_rootWidget->show();
        m_rootWidget->raise();
        m_rootWidget->activateWindow();
    }
    pumpEvents(300);
    pumpEvents(200);
}

void SceneHost::hide()
{
    if (m_view)
        m_view->hide();
    if (m_rootWidget)
        m_rootWidget->hide();
    pumpEvents(120);
}

void SceneHost::shutdown()
{
    if (m_view)
        m_view->hide();
    if (m_rootWidget)
        m_rootWidget->hide();
    pumpEvents(120);

    m_overlay = nullptr;
    delete m_view;
    m_view = nullptr;
    delete m_rootWidget;
    m_rootWidget = nullptr;
    m_quickWidget = nullptr;
    m_glWidget = nullptr;
}

void SceneHost::setTick(int value)
{
    if (QQuickItem *root = sceneRootItem())
        root->setProperty("tick", value);
}

int SceneHost::tick() const
{
    if (QQuickItem *root = sceneRootItem())
        return root->property("tick").toInt();
    return 0;
}

void SceneHost::setFrozen(bool frozen)
{
    if (QQuickItem *root = sceneRootItem())
        root->setProperty("frozen", frozen);
}

bool SceneHost::frozen() const
{
    if (QQuickItem *root = sceneRootItem())
        return root->property("frozen").toBool();
    return false;
}

QQuickWindow *SceneHost::window() const
{
    if (m_view)
        return m_view;
    if (m_quickWidget)
        return m_quickWidget->quickWindow();
    return nullptr;
}

QQuickItem *SceneHost::sceneRootItem() const
{
    if (m_view)
        return m_view->rootObject();
    if (m_quickWidget)
        return m_quickWidget->rootObject();
    return nullptr;
}

QQuickItem *SceneHost::contentItem() const
{
    if (QQuickWindow *w = window())
        return w->contentItem();
    return nullptr;
}

QString SceneHost::syncCaptureApi() const
{
    switch (m_spec.kind) {
    case SceneKind::QuickView:
        return QStringLiteral("QQuickWindow::grabWindow()");
    case SceneKind::QuickWidget:
        return QStringLiteral("QWidget::grab() on the parent widget");
    case SceneKind::OpenGlWidget:
        return QStringLiteral("QOpenGLWidget::grabFramebuffer()");
    }
    return QString();
}

QImage SceneHost::syncCapture()
{
    if (m_view)
        return m_view->grabWindow();
    if (m_glWidget)
        return static_cast<QOpenGLWidget *>(m_glWidget)->grabFramebuffer();
    if (m_rootWidget)
        return m_rootWidget->grab().toImage();
    return QImage();
}

void SceneHost::addOverlaySibling()
{
    QQuickWindow *w = window();
    if (!w || m_overlay)
        return;

    QQmlEngine *engine = m_view ? m_view->engine() : (m_quickWidget ? m_quickWidget->engine() : nullptr);
    if (!engine)
        return;

    auto *component = new QQmlComponent(engine, w);
    component->setData(QByteArray("import QtQuick\n"
                                  "Rectangle { color: \"")
                           + kOverlayColor
                           + QByteArray("\" }"),
                       QUrl(QStringLiteral("qrc:/qt/qml/HyRemoteAsyncSpike/OverlaySibling.qml")));
    if (component->isError()) {
        for (const QQmlError &error : component->errors())
            qWarning("ASYNC-SPIKE: overlay component error: %s", qPrintable(error.toString()));
        component->deleteLater();
        return;
    }

    QObject *object = component->create();
    component->deleteLater();
    auto *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        delete object;
        return;
    }

    // Same parent chain a QML Overlay/Popup uses: a child of the window's
    // contentItem, not a child of the application's root item.
    item->setParent(w->contentItem());
    item->setParentItem(w->contentItem());
    item->setZ(1000.0);
    item->setSize(QSizeF(160.0, 120.0));
    item->setPosition(QPointF(double(w->width()) - 200.0, double(w->height()) - 160.0));
    item->setVisible(true);
    m_overlay = item;
}

QColor SceneHost::overlayColor() const
{
    return QColor(QLatin1String(kOverlayColor));
}

QQuickItem *SceneHost::namedItem(const QString &objectName) const
{
    if (QQuickItem *root = sceneRootItem()) {
        if (QQuickItem *item = root->findChild<QQuickItem *>(objectName))
            return item;
    }
    if (QQuickItem *content = contentItem()) {
        if (QQuickItem *item = content->findChild<QQuickItem *>(objectName))
            return item;
    }
    return nullptr;
}

QRect SceneHost::rectInContentItem(QQuickItem *item) const
{
    QQuickItem *content = contentItem();
    if (!item || !content)
        return {};
    const QPointF origin = item->mapToItem(content, QPointF(0.0, 0.0));
    return QRect(origin.toPoint(), item->size().toSize());
}

QRect SceneHost::overlayRectInContentItem() const
{
    return rectInContentItem(m_overlay);
}

QRect SceneHost::tickPatchRectInContentItem() const
{
    return rectInContentItem(namedItem(QStringLiteral("tickPatch")));
}

QRect SceneHost::fboRectInContentItem() const
{
    return rectInContentItem(namedItem(QStringLiteral("fboItem")));
}

QPoint SceneHost::pointInTarget(const QPointF &contentItemPoint, QQuickItem *target) const
{
    QQuickItem *content = contentItem();
    if (!target || !content || target == content)
        return contentItemPoint.toPoint();

    const QPointF targetOrigin = target->mapToItem(content, QPointF(0.0, 0.0));
    return (contentItemPoint - targetOrigin).toPoint();
}

QPoint SceneHost::toDevicePoint(const QImage &image, const QPoint &logicalPoint)
{
    const qreal dpr = image.isNull() || image.devicePixelRatio() <= 0.0 ? 1.0 : image.devicePixelRatio();
    return QPoint(qRound(double(logicalPoint.x()) * dpr), qRound(double(logicalPoint.y()) * dpr));
}

QColor SceneHost::sampleColor(const QImage &image, const QPoint &devicePoint)
{
    if (image.isNull() || !image.rect().contains(devicePoint))
        return QColor();
    return image.pixelColor(devicePoint);
}

QColor SceneHost::sampleAtContentItemPoint(const QImage &image,
                                           const QPointF &contentItemPoint,
                                           QQuickItem *target) const
{
    const QPoint logical = pointInTarget(contentItemPoint, target);
    return sampleColor(image, toDevicePoint(image, logical));
}

int SceneHost::decodeTickAt(const QImage &image, QQuickItem *target) const
{
    const QRect patch = tickPatchRectInContentItem();
    if (patch.isEmpty())
        return -1;
    const QColor color = sampleAtContentItemPoint(image, patch.center(), target);
    if (!color.isValid())
        return -1;
    return (color.red() & 0xff) | ((color.green() & 0xff) << 8);
}

int SceneHost::decodeFboCounterAt(const QImage &image, QQuickItem *target) const
{
    const QRect rect = fboRectInContentItem();
    if (rect.isEmpty())
        return -1;
    const QColor color = sampleAtContentItemPoint(image, rect.center(), target);
    if (!color.isValid())
        return -1;
    return (color.red() & 0xff) | ((color.green() & 0xff) << 8);
}

int SceneHost::fboRendererFrameCount() const
{
    return CustomFboItem::renderedFrameCount();
}

int SceneHost::fboLastEncoded() const
{
    return CustomFboItem::lastEncodedValue();
}

QStringList SceneHost::observations() const
{
    QStringList notes = m_spec.notes;
    if (m_spec.kind == SceneKind::OpenGlWidget) {
        notes.append(QStringLiteral(
            "This scene has no QML item tree, so QQuickItem::grabToImage() is not applicable: "
            "Qt exposes no public asynchronous capture API for a QOpenGLWidget."));
    }
    return notes;
}

QVariantMap SceneHost::info() const
{
    QVariantMap info;
    info.insert(QStringLiteral("scene"), m_spec.id);
    info.insert(QStringLiteral("family"), m_spec.family);
    info.insert(QStringLiteral("requestedSize"),
                QStringLiteral("%1x%2").arg(m_spec.windowSize.width()).arg(m_spec.windowSize.height()));

    if (QQuickWindow *w = window()) {
        info.insert(QStringLiteral("windowSize"),
                    QStringLiteral("%1x%2").arg(w->width()).arg(w->height()));
        info.insert(QStringLiteral("devicePixelRatio"), w->effectiveDevicePixelRatio());
        info.insert(QStringLiteral("graphicsApi"),
                    QStringLiteral("%1").arg(int(w->rendererInterface()->graphicsApi())));
    }
    if (QQuickItem *root = sceneRootItem()) {
        info.insert(QStringLiteral("rootItemSize"),
                    QStringLiteral("%1x%2").arg(root->width()).arg(root->height()));
        if (QQuickItem *content = contentItem()) {
            const QPointF origin = root->mapToItem(content, QPointF(0.0, 0.0));
            info.insert(QStringLiteral("rootItemPositionInContentItem"),
                        QStringLiteral("%1,%2").arg(origin.x()).arg(origin.y()));
            info.insert(QStringLiteral("rootItemParentIsContentItem"),
                        root->parentItem() == content);
        }
    }
    if (QQuickItem *content = contentItem())
        info.insert(QStringLiteral("contentItemSize"),
                    QStringLiteral("%1x%2").arg(content->width()).arg(content->height()));

    if (m_overlay) {
        info.insert(QStringLiteral("overlayRectInContentItem"),
                    QStringLiteral("%1,%2 %3x%4")
                        .arg(overlayRectInContentItem().x())
                        .arg(overlayRectInContentItem().y())
                        .arg(overlayRectInContentItem().width())
                        .arg(overlayRectInContentItem().height()));
        info.insert(QStringLiteral("overlayColor"), overlayColor().name());
    }
    const QRect patch = tickPatchRectInContentItem();
    if (!patch.isEmpty()) {
        info.insert(QStringLiteral("tickPatchRectInContentItem"),
                    QStringLiteral("%1,%2 %3x%4")
                        .arg(patch.x())
                        .arg(patch.y())
                        .arg(patch.width())
                        .arg(patch.height()));
    }
    const QRect fbo = fboRectInContentItem();
    if (!fbo.isEmpty()) {
        info.insert(QStringLiteral("fboRectInContentItem"),
                    QStringLiteral("%1,%2 %3x%4")
                        .arg(fbo.x())
                        .arg(fbo.y())
                        .arg(fbo.width())
                        .arg(fbo.height()));
        info.insert(QStringLiteral("fboRendererFrameCount"), fboRendererFrameCount());
    }
    return info;
}

}  // namespace asyncspike
}  // namespace hyremote
