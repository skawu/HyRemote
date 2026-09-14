#include "samples/quick_widget_sample.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QQmlError>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace hyremote {
namespace spike {

namespace {

class PulsingRegion : public QWidget
{
public:
    explicit PulsingRegion(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(160, 80);
        setAutoFillBackground(true);
    }

    void setStep(int step)
    {
        const int value = 60 + (step * 9) % 150;
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(value, 220 - value, 80));
        setPalette(pal);
        update();
    }
};

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

QUrl quickSceneUrl()
{
    QUrl url;
    url.setScheme(QStringLiteral("qrc"));
    url.setPath(QStringLiteral("/qt/qml/HyRemoteCaptureSpike/QuickScene.qml"));
    return url;
}

}  // namespace

QuickWidgetSample::QuickWidgetSample() = default;
QuickWidgetSample::~QuickWidgetSample() = default;

QString QuickWidgetSample::title() const
{
    return QStringLiteral("QQuickWidget embedded in a QWidget hierarchy");
}

void QuickWidgetSample::prepare()
{
    auto *root = new QWidget;
    root->setWindowTitle(QStringLiteral("HyRemote SPIKE-01 - QQuickWidget target"));

    auto *layout = new QVBoxLayout(root);
    layout->addWidget(new QLabel(QStringLiteral("parent widget: raster siblings around a QQuickWidget"),
                                 root));

    auto *row = new QHBoxLayout;
    auto *quickWidget = new QQuickWidget(root);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setClearColor(QColor(10, 12, 16));
    quickWidget->setSource(quickSceneUrl());
    if (quickWidget->status() != QQuickWidget::Ready) {
        for (const QQmlError &error : quickWidget->errors())
            qWarning("SPIKE-01: QQuickWidget QML error: %s", qPrintable(error.toString()));
    }

    auto *pulse = new PulsingRegion(root);
    row->addWidget(quickWidget, 3);
    row->addWidget(pulse, 1);
    layout->addLayout(row, 1);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("sibling widget:"), root));
    controls->addWidget(new QPushButton(QStringLiteral("sibling button"), root));
    layout->addLayout(controls, 0);

    root->resize(1000, 640);

    m_root = root;
    m_quickWidget = quickWidget;
    m_pulseWidget = pulse;
    m_advanceScene = [pulse](int frameIndex) { pulse->setStep(frameIndex); };
}

void QuickWidgetSample::show()
{
    if (m_root) {
        m_root->show();
        m_root->raise();
        m_root->activateWindow();
    }
    pumpEvents(400);
}

void QuickWidgetSample::tick(int frameIndex)
{
    // Only the raster sibling is advanced from the harness; the embedded Quick
    // scene animates itself.
    if (m_advanceScene)
        m_advanceScene(frameIndex);
}

CaptureOutcome QuickWidgetSample::grabQuickContent()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QQuickWidget::grabFramebuffer()");
    outcome.primary = true;
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_quickWidget) {
        outcome.notes = QStringLiteral("no QQuickWidget available");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    outcome.frame = m_quickWidget->grabFramebuffer();
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "Quick-specific capture: returns the Quick content only, at the Quick render target size. "
        "It does not include the raster siblings of the parent widget");
    return outcome;
}

CaptureOutcome QuickWidgetSample::grabParentComposition()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::grab() on the parent widget");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_root) {
        outcome.notes = QStringLiteral("no parent widget available");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    const QPixmap pixmap = m_root->grab();
    outcome.elapsedNs = timer.nsecsElapsed();
    outcome.frame = pixmap.toImage();
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "full-parent capture: includes the embedded Quick content and every raster sibling, which "
        "is what a whole-window sharing target needs");
    return outcome;
}

QVector<CaptureOutcome> QuickWidgetSample::captureAll()
{
    QVector<CaptureOutcome> outcomes;
    outcomes.append(grabQuickContent());
    outcomes.append(grabParentComposition());
    return outcomes;
}

QStringList QuickWidgetSample::observations() const
{
    QStringList notes;
    notes.append(QStringLiteral(
        "QQuickWidget::grabFramebuffer() returns the Quick render target, so the result excludes "
        "the surrounding raster widgets and has the Quick item size, not the window size."));
    notes.append(QStringLiteral(
        "Full-parent QWidget::grab() composes the embedded Quick content and the raster siblings "
        "into one image, which is the simpler contract for a whole-window target."));
    notes.append(QStringLiteral(
        "Input coordinates differ between the two paths: a viewer coordinate must be translated "
        "from the parent widget geometry into the QQuickWidget's own coordinate space before it "
        "is delivered to the embedded Quick item."));
    notes.append(QStringLiteral(
        "QQuickWidget owns an offscreen render target and its own render loop, so capture cost "
        "includes one extra composition/readback step compared with a native QQuickWindow."));
    return notes;
}

QVariantMap QuickWidgetSample::info() const
{
    QVariantMap info;
    if (m_quickWidget) {
        info.insert(QStringLiteral("quickWidgetSize"),
                    QStringLiteral("%1x%2")
                        .arg(m_quickWidget->width())
                        .arg(m_quickWidget->height()));
        info.insert(QStringLiteral("quickWidgetDpr"), m_quickWidget->devicePixelRatioF());
        info.insert(QStringLiteral("source"), m_quickWidget->source().toString());
        info.insert(QStringLiteral("quickWidgetStatus"), int(m_quickWidget->status()));
    }
    if (m_root) {
        info.insert(QStringLiteral("parentSize"),
                    QStringLiteral("%1x%2").arg(m_root->width()).arg(m_root->height()));
    }
    return info;
}

void QuickWidgetSample::shutdown()
{
    if (m_root)
        m_root->hide();

    pumpEvents(100);

    delete m_root.data();
    m_root = nullptr;
    m_quickWidget = nullptr;
    m_pulseWidget = nullptr;
}

}  // namespace spike
}  // namespace hyremote
