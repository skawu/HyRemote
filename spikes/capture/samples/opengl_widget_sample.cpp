#include "samples/opengl_widget_sample.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

namespace hyremote {
namespace spike {

namespace {

class GlPaintWidget : public QOpenGLWidget
{
public:
    explicit GlPaintWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
    {
        setMinimumSize(420, 300);
    }

    void setPhase(int phase)
    {
        m_phase = phase;
        update();
    }

protected:
    void paintGL() override
    {
        // QPainter on a QOpenGLWidget is a supported public combination; it
        // exercises the same GL target that a hardware-accelerated widget uses.
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

}  // namespace

OpenGlWidgetSample::OpenGlWidgetSample() = default;
OpenGlWidgetSample::~OpenGlWidgetSample() = default;

QString OpenGlWidgetSample::title() const
{
    return QStringLiteral("QOpenGLWidget inside a QWidget hierarchy");
}

void OpenGlWidgetSample::prepare()
{
    auto *root = new QWidget;
    root->setWindowTitle(QStringLiteral("HyRemote SPIKE-01 - QOpenGLWidget target"));

    auto *layout = new QVBoxLayout(root);
    layout->addWidget(new QLabel(QStringLiteral("parent widget: plain widget siblings around a QOpenGLWidget"),
                                 root));

    auto *row = new QHBoxLayout;
    auto *glWidget = new GlPaintWidget(root);
    auto *pulse = new PulsingRegion(root);
    row->addWidget(glWidget, 3);
    row->addWidget(pulse, 1);
    layout->addLayout(row, 1);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("sibling widget:"), root));
    controls->addWidget(new QPushButton(QStringLiteral("sibling button"), root));
    layout->addLayout(controls, 0);

    root->resize(1000, 640);

    m_root = root;
    m_glWidget = glWidget;
    m_pulseWidget = pulse;
    m_advanceScene = [glWidget, pulse](int frameIndex) {
        glWidget->setPhase(frameIndex);
        pulse->setStep(frameIndex);
    };
    m_grabFramebuffer = [glWidget] { return glWidget->grabFramebuffer(); };
}

void OpenGlWidgetSample::show()
{
    if (m_root) {
        m_root->show();
        m_root->raise();
        m_root->activateWindow();
    }
    pumpEvents(400);
}

void OpenGlWidgetSample::tick(int frameIndex)
{
    if (m_advanceScene)
        m_advanceScene(frameIndex);
}

CaptureOutcome OpenGlWidgetSample::grabFramebuffer()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QOpenGLWidget::grabFramebuffer()");
    outcome.primary = true;
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_grabFramebuffer) {
        outcome.notes = QStringLiteral("no QOpenGLWidget available");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    outcome.frame = m_grabFramebuffer();
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "public GL-target capture: Qt makes the widget's context current, draws the widget and "
        "reads the framebuffer back. Includes a full glReadPixels of the widget only, not of the "
        "surrounding widget hierarchy");
    return outcome;
}

CaptureOutcome OpenGlWidgetSample::grabGlWidgetViaWidgetApi()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::grab() on the QOpenGLWidget");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_glWidget) {
        outcome.notes = QStringLiteral("no QOpenGLWidget available");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    const QPixmap pixmap = m_glWidget->grab();
    outcome.elapsedNs = timer.nsecsElapsed();
    outcome.frame = pixmap.toImage();
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "raster widget capture of a GL-backed widget; whether the GL content is included depends "
        "on QOpenGLWidget composing its FBO into the widget backing store");
    return outcome;
}

CaptureOutcome OpenGlWidgetSample::grabParentComposition()
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
        "the practical single-call path for a mixed raster+GL widget tree: captures the whole "
        "parent, so no per-widget composition is required");
    return outcome;
}

QVector<CaptureOutcome> OpenGlWidgetSample::captureAll()
{
    QVector<CaptureOutcome> outcomes;
    outcomes.append(grabFramebuffer());
    outcomes.append(grabGlWidgetViaWidgetApi());
    outcomes.append(grabParentComposition());
    return outcomes;
}

QStringList OpenGlWidgetSample::observations() const
{
    QStringList notes;
    notes.append(QStringLiteral(
        "grabFramebuffer() requires the widget's OpenGL context and must be called on the GUI "
        "thread; Qt performs makeCurrent()/paint/readback internally, so a blocking "
        "glReadPixels is unavoidable on this path."));
    notes.append(QStringLiteral(
        "grabFramebuffer() returns only the GL widget. In a hierarchy that also contains raster "
        "widgets, the caller must compose the rest itself, or use QWidget::grab() on the parent."));
    notes.append(QStringLiteral(
        "The non-blocking replacement for grabFramebuffer() is not a public API: asynchronous "
        "PBO/FBO readback requires custom GL code or Qt private scene graph integration, which "
        "belongs to the later GL/PBO follow-up instead of the v0.1 baseline."));
    return notes;
}

QVariantMap OpenGlWidgetSample::info() const
{
    QVariantMap info;
    if (m_glWidget) {
        info.insert(QStringLiteral("glWidgetSize"),
                    QStringLiteral("%1x%2").arg(m_glWidget->width()).arg(m_glWidget->height()));
        info.insert(QStringLiteral("glWidgetDevicePixelRatio"), m_glWidget->devicePixelRatioF());
    }
    if (m_root) {
        info.insert(QStringLiteral("parentSize"),
                    QStringLiteral("%1x%2").arg(m_root->width()).arg(m_root->height()));
    }
    return info;
}

QWidget *OpenGlWidgetSample::damageTargetWidget() const
{
    return m_root.data();
}

void OpenGlWidgetSample::shutdown()
{
    m_advanceScene = nullptr;
    m_grabFramebuffer = nullptr;

    if (m_root)
        m_root->hide();

    pumpEvents(100);

    delete m_root.data();
    m_root = nullptr;
    m_glWidget = nullptr;
    m_pulseWidget = nullptr;
}

}  // namespace spike
}  // namespace hyremote
