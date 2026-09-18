// SPDX-License-Identifier: Apache-2.0
#include "samples/quick_sample.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QQmlError>
#include <QSGRendererInterface>
#include <QThread>
#include <QTimer>

namespace hyremote {
namespace spike {

namespace {

QString threadLabel()
{
    QThread *thread = QThread::currentThread();
    const QString name = thread->objectName();
    const bool isGui = QCoreApplication::instance()
                       && thread == QCoreApplication::instance()->thread();
    return QStringLiteral("%1 (gui=%2)")
        .arg(name.isEmpty() ? QStringLiteral("<unnamed>") : name)
        .arg(isGui ? QStringLiteral("yes") : QStringLiteral("no"));
}

QString graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software:
        return QStringLiteral("Software");
    case QSGRendererInterface::OpenGL:
        return QStringLiteral("OpenGL");
    case QSGRendererInterface::Direct3D11:
        return QStringLiteral("Direct3D11");
    case QSGRendererInterface::Direct3D12:
        return QStringLiteral("Direct3D12");
    case QSGRendererInterface::Vulkan:
        return QStringLiteral("Vulkan");
    case QSGRendererInterface::Metal:
        return QStringLiteral("Metal");
    case QSGRendererInterface::Null:
        return QStringLiteral("Null");
    default:
        return QStringLiteral("Unknown");
    }
}

}  // namespace

QuickSample::QuickSample(QuickSceneSpec spec)
    : m_spec(std::move(spec))
{
}

QuickSample::~QuickSample() = default;

QString QuickSample::id() const
{
    return m_spec.id;
}

QString QuickSample::title() const
{
    return m_spec.title;
}

QString QuickSample::targetFamily() const
{
    return m_spec.targetFamily;
}

QStringList QuickSample::requiredEnv() const
{
    return m_spec.requiredEnv;
}

void QuickSample::prepare()
{
    auto *view = new QQuickView;
    view->setTitle(QStringLiteral("HyRemote SPIKE-01 - %1").arg(m_spec.id));
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->resize(m_requestedSize);

    // Public render-thread hooks. The connection is direct so that the lambda
    // really executes on the thread that emits the signal; this is how the
    // sample proves which thread performs scene graph rendering.
    QObject::connect(
        view, &QQuickWindow::beforeRendering, this,
        [this] {
            m_beforeRenderingCount.fetch_add(1, std::memory_order_relaxed);
            m_renderThreadId.store(reinterpret_cast<quintptr>(QThread::currentThreadId()),
                                   std::memory_order_relaxed);
        },
        Qt::DirectConnection);
    QObject::connect(
        view, &QQuickWindow::afterRendering, this,
        [this] { m_afterRenderingCount.fetch_add(1, std::memory_order_relaxed); },
        Qt::DirectConnection);

    view->loadFromModule(QStringLiteral("HyRemoteCaptureSpike"), m_spec.qmlType);

    if (view->status() != QQuickView::Ready) {
        for (const QQmlError &error : view->errors())
            qWarning("SPIKE-01: QML load error: %s", qPrintable(error.toString()));
    }

    m_view = view;
    m_rootItem = view->rootObject();
}

void QuickSample::show()
{
    if (!m_view)
        return;

    m_view->show();
    // Raise the window: an occluded window is throttled by the compositor, which
    // would distort the frame pacing measurements.
    m_view->raise();
    m_view->requestActivate();
    // Give the scene graph two real frames before the first capture.
    pumpEvents(200);
    pumpEvents(200);
}

void QuickSample::tick(int frameIndex)
{
    Q_UNUSED(frameIndex);
    // The scenes animate themselves; the harness must not modify application
    // state in order to keep the comparison across samples fair.
}

CaptureOutcome QuickSample::grabWindow()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QQuickWindow::grabWindow()");
    outcome.primary = true;
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_view) {
        outcome.notes = QStringLiteral("no QQuickWindow available");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    outcome.frame = m_view->grabWindow();
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "documented as callable from the GUI thread only; it renders the scene graph into an "
        "offscreen surface and reads it back, so the cost is roughly one full frame render plus "
        "a GPU->CPU readback");
    return outcome;
}

CaptureOutcome QuickSample::grabRootItemToImage()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QQuickItem::grabToImage() (async)");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_rootItem) {
        outcome.notes = QStringLiteral("no root item available");
        return outcome;
    }

    if (isPathDisabled(outcome.path)) {
        outcome.notes = QStringLiteral("disabled by --disable-path");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();

    const QSharedPointer<QQuickItemGrabResult> result = m_rootItem->grabToImage();
    if (!result) {
        outcome.elapsedNs = timer.nsecsElapsed();
        outcome.notes = QStringLiteral("grabToImage() returned a null result");
        return outcome;
    }

    if (result->image().isNull()) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(result.data(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
        timeout.start(3000);
        loop.exec(QEventLoop::AllEvents);
    }

    outcome.elapsedNs = timer.nsecsElapsed();
    outcome.frame = result->image();
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "asynchronous public API: the call returns immediately and the image becomes valid on a "
        "later event loop iteration, so the measured time is end-to-end latency, not CPU cost");
    return outcome;
}

QVector<CaptureOutcome> QuickSample::captureAll()
{
    QVector<CaptureOutcome> outcomes;
    if (!isPathDisabled(QStringLiteral("grabWindow")))
        outcomes.append(grabWindow());
    if (!isPathDisabled(QStringLiteral("grabToImage")))
        outcomes.append(grabRootItemToImage());
    return outcomes;
}

QStringList QuickSample::observations() const
{
    QStringList notes = m_spec.sceneNotes;

    const quintptr renderThreadId = m_renderThreadId.load(std::memory_order_relaxed);
    const int beforeCount = m_beforeRenderingCount.load(std::memory_order_relaxed);
    const int afterCount = m_afterRenderingCount.load(std::memory_order_relaxed);

    if (renderThreadId == 0) {
        notes.append(QStringLiteral(
            "QQuickWindow::beforeRendering never fired, so no render-thread observation was "
            "collected in this run."));
    } else {
        const bool foreignThread =
            renderThreadId != reinterpret_cast<quintptr>(QThread::currentThreadId());
        notes.append(
            QStringLiteral("QQuickWindow::beforeRendering fired %1 times (afterRendering %2); the "
                           "hook ran on %3. The scene graph is therefore rendered on %4.")
                .arg(beforeCount)
                .arg(afterCount)
                .arg(foreignThread ? QStringLiteral("a separate render thread")
                                   : QStringLiteral("the GUI thread"))
                .arg(foreignThread ? QStringLiteral("its own OS thread, so graphics work does "
                                                    "not have to run on the GUI thread")
                                   : QStringLiteral("the GUI thread, so this backend used a "
                                                    "single-threaded render loop")));
    }

    notes.append(QStringLiteral(
        "grabWindow() is documented as GUI-thread only. Calling it from beforeRendering/"
        "afterRendering (render thread) is not a supported public path and was deliberately not "
        "executed here; a render-thread capture needs QQuickWindow::setGraphicsDevice / "
        "QSGRendererInterface-level work."));
    notes.append(QStringLiteral(
        "Qt Quick exposes no public damage-region API: QQuickWindow::update() posts an "
        "UpdateRequest without geometry. Region-level damage for Quick requires the private "
        "renderer or application-declared dirty hints."));
    notes.append(QStringLiteral(
        "grabRootItemToImage() is asynchronous and therefore never blocks the GUI thread, but it "
        "delivers frames one event loop iteration later, so it cannot be driven at a fixed frame "
        "cadence without extra buffering."));
    return notes;
}

QVariantMap QuickSample::info() const
{
    QVariantMap info = extraInfo();
    info.insert(QStringLiteral("qmlType"), m_spec.qmlType);
    info.insert(QStringLiteral("requestedSize"),
                QStringLiteral("%1x%2").arg(m_requestedSize.width()).arg(m_requestedSize.height()));

    if (m_view) {
        info.insert(QStringLiteral("windowSize"),
                    QStringLiteral("%1x%2")
                        .arg(m_view->width())
                        .arg(m_view->height()));
        info.insert(QStringLiteral("devicePixelRatio"), m_view->devicePixelRatio());
        if (QSGRendererInterface *renderer = m_view->rendererInterface())
            info.insert(QStringLiteral("graphicsApi"), graphicsApiName(renderer->graphicsApi()));
    }
    if (m_rootItem)
        info.insert(QStringLiteral("rootItemIsItem"), true);

    info.insert(QStringLiteral("beforeRenderingCount"),
                m_beforeRenderingCount.load(std::memory_order_relaxed));
    info.insert(QStringLiteral("afterRenderingCount"),
                m_afterRenderingCount.load(std::memory_order_relaxed));
    return info;
}

void QuickSample::shutdown()
{
    if (m_view)
        m_view->hide();

    pumpEvents(100);

    delete m_view.data();
    m_view = nullptr;
    m_rootItem = nullptr;
}

}  // namespace spike
}  // namespace hyremote
