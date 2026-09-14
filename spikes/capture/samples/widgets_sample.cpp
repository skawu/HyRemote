#include "samples/widgets_sample.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <QtMath>

#include <cmath>

namespace hyremote {
namespace spike {

namespace {

// Exercises a custom QPainter path (compatibility matrix: "QWidget with custom
// QPainter").
class CustomPaintWidget : public QWidget
{
public:
    explicit CustomPaintWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(320, 200);
    }

    void setPhase(int phase)
    {
        m_phase = phase;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(20, 24, 30));

        const double width = this->width();
        const double height = this->height();

        QPainterPath path;
        path.moveTo(0.0, height / 2.0);
        for (int x = 0; x <= int(width); x += 3) {
            const double t = (double(x) + double(m_phase) * 4.0) / 28.0;
            path.lineTo(double(x), height / 2.0 + std::sin(t) * (height / 3.2));
        }
        painter.setPen(QPen(QColor(110, 200, 255), 2.0));
        painter.drawPath(path);

        painter.setPen(Qt::white);
        painter.drawText(QRect(8, 8, int(width) - 16, int(height) - 16),
                         Qt::AlignTop | Qt::AlignLeft,
                         QStringLiteral("custom QPainter widget\nphase %1").arg(m_phase));
    }

private:
    int m_phase = 0;
};

// Region that changes on every tick, so the target produces a bounded and
// predictable damage area.
class PulsingRegion : public QWidget
{
public:
    explicit PulsingRegion(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(140, 80);
        setAutoFillBackground(true);
    }

    void setStep(int step)
    {
        const int value = 60 + (step * 7) % 160;
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(value, 255 - value, 64));
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

WidgetsSample::WidgetsSample() = default;
WidgetsSample::~WidgetsSample() = default;

QString WidgetsSample::title() const
{
    return QStringLiteral("QWidget / raster baseline (child widgets, QPainter, popup, dialog)");
}

void WidgetsSample::prepare()
{
    auto *root = new QWidget;
    root->setWindowTitle(QStringLiteral("HyRemote SPIKE-01 - widgets target"));
    root->setAutoFillBackground(true);

    auto *layout = new QVBoxLayout(root);

    auto *header = new QLabel(QStringLiteral("HyRemote SPIKE-01 widgets target"), root);
    QFont headerFont = header->font();
    headerFont.setPointSize(headerFont.pointSize() + 4);
    header->setFont(headerFont);
    layout->addWidget(header);

    auto *row = new QHBoxLayout;
    auto *customPaint = new CustomPaintWidget(root);
    auto *pulsing = new PulsingRegion(root);
    row->addWidget(customPaint, 3);
    row->addWidget(pulsing, 1);
    layout->addLayout(row, 1);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Text:"), root));
    controls->addWidget(new QLineEdit(QStringLiteral("local input still works"), root), 2);
    controls->addWidget(new QPushButton(QStringLiteral("Button"), root));
    controls->addWidget(new QCheckBox(QStringLiteral("Check"), root));
    auto *progress = new QProgressBar(root);
    progress->setRange(0, 0);
    progress->setMaximumWidth(160);
    controls->addWidget(progress);
    layout->addLayout(controls, 0);

    m_counter = new QLabel(QStringLiteral("frame 0"), root);
    layout->addWidget(m_counter);

    root->resize(960, 600);

    auto *dialog = new QDialog(root);
    dialog->setWindowTitle(QStringLiteral("SPIKE-01 non-modal dialog"));
    dialog->setGeometry(1020, 120, 320, 180);
    {
        auto *dialogLayout = new QVBoxLayout(dialog);
        dialogLayout->addWidget(
            new QLabel(QStringLiteral("non-modal QDialog\nseparate top-level window"), dialog));
        dialogLayout->addWidget(new QPushButton(QStringLiteral("Dialog button"), dialog));
    }

    auto *menu = new QMenu(root);
    menu->addAction(QStringLiteral("Popup action 1"));
    menu->addAction(QStringLiteral("Popup action 2"));
    menu->addSeparator();
    menu->addAction(QStringLiteral("Popup action 3"));

    m_root = root;
    m_dialog = dialog;
    m_menu = menu;

    m_advanceScene = [customPaint, pulsing](int frameIndex) {
        customPaint->setPhase(frameIndex);
        pulsing->setStep(frameIndex);
    };
}

void WidgetsSample::show()
{
    if (m_root) {
        m_root->show();
        // An occluded window can be throttled by the compositor, which would
        // distort frame pacing measurements.
        m_root->raise();
        m_root->activateWindow();
    }
    if (m_dialog)
        m_dialog->show();
    if (m_menu && m_root)
        m_menu->popup(m_root->mapToGlobal(QPoint(220, 140)));

    pumpEvents(300);
}

void WidgetsSample::tick(int frameIndex)
{
    if (m_advanceScene)
        m_advanceScene(frameIndex);
    if (m_counter)
        m_counter->setText(QStringLiteral("frame %1").arg(frameIndex));
}

CaptureOutcome WidgetsSample::grabWidget()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::grab()");
    outcome.primary = true;
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_root) {
        outcome.notes = QStringLiteral("target widget destroyed");
        return outcome;
    }

    QElapsedTimer timer;
    timer.start();
    const QPixmap pixmap = m_root->grab();
    outcome.elapsedNs = timer.nsecsElapsed();
    outcome.frame = pixmap.toImage();
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "grabs the widget through QWidget::render(); includes child widgets, excludes popups and "
        "dialogs that are separate top-level windows; triggers real paint events");
    return outcome;
}

CaptureOutcome WidgetsSample::renderIntoReusedImage()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::render() into reused QImage");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = true;

    if (!m_root) {
        outcome.notes = QStringLiteral("target widget destroyed");
        return outcome;
    }

    const qreal dpr = m_root->devicePixelRatioF();
    const QSize pixelSize(qCeil(m_root->width() * dpr), qCeil(m_root->height() * dpr));
    if (m_targetImage.size() != pixelSize) {
        m_targetImage = QImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
        m_targetImage.setDevicePixelRatio(dpr);
    }
    m_targetImage.fill(Qt::transparent);

    QElapsedTimer timer;
    timer.start();
    m_root->render(&m_targetImage);
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.frame = m_targetImage;  // shares storage with the tracked buffer
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "draws directly into a caller-owned QImage; the same buffer is reused every frame, so a "
        "consumer must not hold a reference across captures without a deep copy");

    // Detects whether handing the buffer to a consumer forced the producer to
    // reallocate it on the next write (QImage copy-on-write detach).
    ++m_reuseAttempts;
    const quint64 cacheKey = m_targetImage.cacheKey();
    if (m_lastReuseCacheKey != 0 && cacheKey != m_lastReuseCacheKey)
        ++m_reuseReallocations;
    m_lastReuseCacheKey = cacheKey;
    return outcome;
}

CaptureOutcome WidgetsSample::renderIntoBorrowedBuffer()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::render() into a borrowed raw pixel buffer");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = true;

    if (!m_root) {
        outcome.notes = QStringLiteral("target widget destroyed");
        return outcome;
    }

    const qreal dpr = m_root->devicePixelRatioF();
    const QSize pixelSize(qCeil(m_root->width() * dpr), qCeil(m_root->height() * dpr));
    const qsizetype bytesPerLine = qsizetype(pixelSize.width()) * 4;
    if (m_borrowedSize != pixelSize) {
        m_borrowedBuffer.resize(bytesPerLine * pixelSize.height());
        m_borrowedSize = pixelSize;
    }

    uchar *data = reinterpret_cast<uchar *>(m_borrowedBuffer.data());

    // Created locally and never shared, so QPainter writes straight into the
    // caller-owned memory exactly like a future external/DMA-BUF buffer would.
    QImage target(data, pixelSize.width(), pixelSize.height(), bytesPerLine,
                  QImage::Format_ARGB32_Premultiplied);
    target.setDevicePixelRatio(dpr);

    QElapsedTimer timer;
    timer.start();
    m_root->render(&target);
    outcome.elapsedNs = timer.nsecsElapsed();

    // The harness needs a frame for reporting, so it pays one deep copy. That
    // copy is NOT what the probe measures: the probe reads the raw memory.
    outcome.frame = target.copy();
    outcome.ok = !target.isNull();

    const int width = pixelSize.width();
    const int height = pixelSize.height();
    outcome.borrowedView = [data, width, height, bytesPerLine] {
        return QImage(data, width, height, bytesPerLine, QImage::Format_ARGB32_Premultiplied);
    };
    outcome.notes = QStringLiteral(
        "paints into a caller-owned raw pixel buffer and only publishes a borrowed view of that "
        "memory; this is the shape a zero-copy (GBM/DMA-BUF) producer would take. The measured "
        "time therefore excludes the one deep copy the harness performs for reporting");
    return outcome;
}

CaptureOutcome WidgetsSample::renderIntoFreshImage()
{
    CaptureOutcome outcome;
    outcome.path = QStringLiteral("QWidget::render() into fresh QImage");
    outcome.callerThread = threadLabel();
    outcome.producerReusesBuffer = false;

    if (!m_root) {
        outcome.notes = QStringLiteral("target widget destroyed");
        return outcome;
    }

    const qreal dpr = m_root->devicePixelRatioF();
    QImage image(QSize(qCeil(m_root->width() * dpr), qCeil(m_root->height() * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);

    QElapsedTimer timer;
    timer.start();
    m_root->render(&image);
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.frame = image;
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "same public API as the reused variant; measured time includes the per-frame buffer "
        "allocation and clear");
    return outcome;
}

QVector<CaptureOutcome> WidgetsSample::captureAll()
{
    QVector<CaptureOutcome> outcomes;
    outcomes.append(grabWidget());
    outcomes.append(renderIntoReusedImage());
    outcomes.append(renderIntoFreshImage());
    outcomes.append(renderIntoBorrowedBuffer());
    return outcomes;
}

QStringList WidgetsSample::observations() const
{
    QStringList notes;
    notes.append(QStringLiteral(
        "QWidget::grab() and QWidget::render() must be called on the GUI thread. Both are "
        "implemented by repainting the widget through QWidget::render(), so the call is "
        "synchronous and blocks the GUI thread for its duration."));
    notes.append(QStringLiteral(
        "Neither call needs a current OpenGL context for raster widgets. A QWidget hierarchy that "
        "contains a QOpenGLWidget/D3D child is the case that needs a graphics context (see the "
        "openglwidget sample)."));
    notes.append(QStringLiteral(
        "Popups, QMenu and non-modal QDialog are separate top-level windows and are NOT part of a "
        "top-level widget capture. The target adapter must decide whether to share only the main "
        "window or to compose extra windows."));
    notes.append(QStringLiteral(
        "Damage can be observed with public APIs by installing an application-level event filter "
        "and reading QPaintEvent::region(). Capture itself emits paint events, so capture-induced "
        "damage has to be separated from application damage."));

    if (m_reuseAttempts > 0) {
        notes.append(
            QStringLiteral("Reused target buffer: %1 reallocations were observed in %2 capture "
                           "attempts. QImage is copy-on-write, so publishing a shallow copy to a "
                           "consumer forces the producer's next write to detach and allocate a new "
                           "buffer. Handing out a QImage therefore does not give a free buffer "
                           "recycling scheme, and it is also why a consumer that keeps a QImage "
                           "copy never observes torn pixels.")
                .arg(m_reuseReallocations)
                .arg(m_reuseAttempts));
    }
    return notes;
}

QVariantMap WidgetsSample::info() const
{
    QVariantMap info;
    if (m_root) {
        info.insert(QStringLiteral("widgetSize"),
                    QStringLiteral("%1x%2").arg(m_root->width()).arg(m_root->height()));
        info.insert(QStringLiteral("devicePixelRatio"), m_root->devicePixelRatioF());
        info.insert(QStringLiteral("isWindow"), m_root->isWindow());
    }

    int visibleTopLevels = 0;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->isVisible())
            ++visibleTopLevels;
    }
    info.insert(QStringLiteral("visibleTopLevelWidgets"), visibleTopLevels);
    info.insert(QStringLiteral("reusedBufferAttempts"), m_reuseAttempts);
    info.insert(QStringLiteral("reusedBufferReallocations"), m_reuseReallocations);
    return info;
}

void WidgetsSample::shutdown()
{
    m_advanceScene = nullptr;

    if (m_menu)
        m_menu->hide();
    if (m_dialog)
        m_dialog->hide();
    if (m_root)
        m_root->hide();

    pumpEvents(100);

    delete m_menu.data();
    delete m_dialog.data();
    delete m_root.data();

    m_menu = nullptr;
    m_dialog = nullptr;
    m_root = nullptr;
    m_counter = nullptr;
    m_targetImage = QImage();
}

}  // namespace spike
}  // namespace hyremote
