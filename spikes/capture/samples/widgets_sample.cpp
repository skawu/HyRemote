#include "samples/widgets_sample.h"

#include "harness/damage_tracker.h"

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

// Real backing-store identity of a QImage, as opposed to QImage::cacheKey().
//
// QImage::constBits() is documented and implemented as "returns d->data without
// detaching", so this probe cannot itself force a copy. The control paths in this
// sample verify that: a producer target that nobody else references keeps the same
// address for every capture even though the probe runs on every capture.
quintptr storageAddress(const QImage &image)
{
    return reinterpret_cast<quintptr>(image.constBits());
}

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
    m_pulsing = pulsing;

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Text:"), root));
    auto *lineEdit = new QLineEdit(QStringLiteral("local input still works"), root);
    // NoFocus: a focused line edit blinks its cursor, which repaints
    // asynchronously and would make the deterministic damage controls flaky.
    lineEdit->setFocusPolicy(Qt::NoFocus);
    controls->addWidget(lineEdit, 2);
    controls->addWidget(new QPushButton(QStringLiteral("Button"), root));
    controls->addWidget(new QCheckBox(QStringLiteral("Check"), root));
    auto *progress = new QProgressBar(root);
    // A determinate value: an indeterminate QProgressBar animates forever, which
    // would pollute the damage controls in the same way.
    progress->setRange(0, 100);
    progress->setValue(40);
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
        auto *dialogLabel =
            new QLabel(QStringLiteral("non-modal QDialog\nseparate top-level window"), dialog);
        dialogLayout->addWidget(dialogLabel);
        dialogLayout->addWidget(new QPushButton(QStringLiteral("Dialog button"), dialog));
        m_dialogLabel = dialogLabel;
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
        m_reuseLastAddress = 0;  // our own allocation, not a probe event
    }

    // Storage-identity probe: the address is sampled before the first write of
    // this capture and after the last one, so the probe brackets every write that
    // could detach the image. A difference means this capture replaced the pixel
    // storage, which happens when QImage::detach() copies because a consumer still
    // held the previous frame.
    const quintptr addressBefore = storageAddress(m_targetImage);

    // Probe self-check: with no writer in between, the address must not move on
    // its own between captures.
    if (m_reuseLastAddress != 0 && addressBefore != m_reuseLastAddress)
        ++m_reuseStorageMovedBetweenCaptures;

    QElapsedTimer timer;
    timer.start();
    m_targetImage.fill(Qt::transparent);
    m_root->render(&m_targetImage);
    outcome.elapsedNs = timer.nsecsElapsed();

    const quintptr addressAfter = storageAddress(m_targetImage);

    outcome.storageProbeAvailable = addressBefore != 0 && addressAfter != 0;
    outcome.storageAddressBefore = addressBefore;
    outcome.storageAddressAfter = addressAfter;
    ++m_reuseProbes;
    if (outcome.storageProbeAvailable && addressBefore != addressAfter) {
        ++m_reuseStorageReplacements;
        outcome.storageReplacedDuringCapture = true;
    }
    m_reuseLastAddress = addressAfter;

    outcome.frame = m_targetImage;  // shares storage with the tracked buffer
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "draws directly into a caller-owned QImage that is reused for every capture; the measured "
        "time covers the buffer clear and the render, and the pixel storage address is probed with "
        "QImage::constBits() before the clear and after the render");
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
    const QSize pixelSize(qCeil(m_root->width() * dpr), qCeil(m_root->height() * dpr));

    QElapsedTimer timer;
    timer.start();
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    m_root->render(&image);
    outcome.elapsedNs = timer.nsecsElapsed();

    outcome.frame = image;
    outcome.ok = !outcome.frame.isNull();
    outcome.notes = QStringLiteral(
        "same public API as the reused variant, but the target image is allocated and cleared for "
        "every capture; the measured time covers the allocation, the clear and the render, so it is "
        "directly comparable with the reused variant");
    return outcome;
}

// Deterministic controls for the storage-identity probe. They answer the two
// questions the probe has to be able to answer before its result can be used:
// can it see a storage replacement at all, and does it stay silent when the
// buffer is unique?
void WidgetsSample::runStorageIdentityControls()
{
    {
        // Positive control: while a shallow copy is alive, the next write must
        // detach (QImage::detach() copies when the reference count is not 1) and
        // therefore relocate the pixel storage.
        QImage image(QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
        const quintptr before = storageAddress(image);
        const QImage shallowConsumer = image;  // keeps the shared data alive
        Q_UNUSED(shallowConsumer);
        image.fill(Qt::transparent);
        const quintptr after = storageAddress(image);

        ++m_detachControlProbes;
        if (before != 0 && after != 0 && before != after)
            ++m_detachControlDetections;
    }

    {
        // Negative control: with no other reference, a write must not relocate
        // the storage.
        QImage image(QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
        const quintptr before = storageAddress(image);
        image.fill(Qt::transparent);
        const quintptr after = storageAddress(image);

        ++m_uniqueWriteControlProbes;
        if (before != 0 && after != 0 && before == after)
            ++m_uniqueWriteControlStable;
    }
}

QVector<CaptureOutcome> WidgetsSample::captureAll()
{
    runStorageIdentityControls();

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
        "Damage regions must be mapped into the shared target coordinate system before they are "
        "unioned. Child widgets report QPaintEvent::region() in their own coordinate system, so the "
        "tracker binds the top-level window widget as the shared target, translates each accepted "
        "child region with QWidget::mapTo() and clips it to the visible part of the ancestor chain."));

    if (m_reuseProbes > 0) {
        notes.append(
            QStringLiteral("Storage identity (real backing-store address from QImage::constBits(), "
                           "not QImage::cacheKey()): the reused producer target changed its pixel "
                           "storage in %1 of %2 captures, and the address never moved on its own "
                           "between captures (%3 self-check hits). QImage uses implicit data "
                           "sharing: QImage::detach() copies when the reference count is not 1, so "
                           "a producer that published a shared QImage pays for a new allocation "
                           "exactly on the captures where a consumer still held the previous frame. "
                           "This is a copy-on-write semantic, not a fixed per-frame allocation "
                           "rate: with no consumer holding a frame the same buffer is reused with "
                           "no replacement at all.")
                .arg(m_reuseStorageReplacements)
                .arg(m_reuseProbes)
                .arg(m_reuseStorageMovedBetweenCaptures));
    }

    if (m_detachControlProbes > 0) {
        notes.append(
            QStringLiteral("Storage-identity probe controls: the synthetic shared-write control "
                           "relocated the storage in %1 of %2 runs (probe can detect a "
                           "replacement), and the unique-write control kept the same address in "
                           "%3 of %4 runs (the probe does not report phantom replacements).")
                .arg(m_detachControlDetections)
                .arg(m_detachControlProbes)
                .arg(m_uniqueWriteControlStable)
                .arg(m_uniqueWriteControlProbes));
    }

    if (m_borrowedProbes > 0) {
        notes.append(
            QStringLiteral("Storage identity is not safety: the caller-owned raw pixel buffer "
                           "kept the same address in all %1 captures (%2 replacements) while the "
                           "hand-off probe still observed its content rewritten, so an address "
                           "check cannot replace an explicit ownership contract.")
                .arg(m_borrowedProbes)
                .arg(m_borrowedStorageReplacements));
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
    info.insert(QStringLiteral("reusedBufferStorageProbes"), m_reuseProbes);
    info.insert(QStringLiteral("reusedBufferStorageReplacements"), m_reuseStorageReplacements);
    info.insert(QStringLiteral("reusedBufferStorageMovedBetweenCaptures"),
                m_reuseStorageMovedBetweenCaptures);
    info.insert(QStringLiteral("borrowedBufferStorageProbes"), m_borrowedProbes);
    info.insert(QStringLiteral("borrowedBufferStorageReplacements"), m_borrowedStorageReplacements);
    info.insert(QStringLiteral("detachControlProbes"), m_detachControlProbes);
    info.insert(QStringLiteral("detachControlDetections"), m_detachControlDetections);
    info.insert(QStringLiteral("uniqueWriteControlProbes"), m_uniqueWriteControlProbes);
    info.insert(QStringLiteral("uniqueWriteControlStable"), m_uniqueWriteControlStable);
    return info;
}

QWidget *WidgetsSample::damageTargetWidget() const
{
    // The top-level window widget is the shared target: this case shares exactly
    // one top-level window, and the popup menu and the dialog are separate
    // windows that the tracker excludes.
    return m_root.data();
}

void WidgetsSample::setDamageTracker(DamageTracker *tracker)
{
    m_damageTracker = tracker;
}

// Deterministic controls for the damage mapping rules. A damage ratio on its own
// cannot distinguish "child regions were mapped into the shared target" from
// "child-local regions were unioned as if they already were target coordinates",
// so the mapping is checked against a known rectangle instead.
QVector<DamageControlResult> WidgetsSample::runDamageControls()
{
    QVector<DamageControlResult> results;
    if (!m_damageTracker || !m_root || !m_pulsing)
        return results;

    // Let anything still pending from the measurement loop settle, so that it
    // cannot be attributed to a control step.
    pumpEvents(80);
    m_damageTracker->takeSnapshot();

    {
        DamageControlResult result;
        result.description = QStringLiteral(
            "repaint only the pulsing child widget; the damage region must be exactly that child's "
            "rectangle expressed in the shared target coordinate system");

        QWidget *pulsing = m_pulsing.data();
        result.expectedTargetRect =
            QRect(pulsing->mapTo(m_root.data(), QPoint(0, 0)), pulsing->size());
        pulsing->update();
        pumpEvents(80);

        const DamageSnapshot snapshot = m_damageTracker->takeSnapshot();
        result.observedArea = snapshot.area();
        result.observedBoundingRect = snapshot.region.boundingRect();
        result.observedPaintEvents = snapshot.paintEvents;
        result.observedExcludedPaintEvents = snapshot.excludedPaintEvents;
        result.passed =
            !result.expectedTargetRect.isEmpty()
            && result.observedBoundingRect == result.expectedTargetRect
            && result.observedArea == qint64(result.expectedTargetRect.width())
                                          * qint64(result.expectedTargetRect.height());
        results.append(result);
    }

    if (m_dialogLabel) {
        DamageControlResult result;
        result.description = QStringLiteral(
            "repaint a widget inside the separate top-level dialog; it must be excluded from the "
            "shared target damage even though the dialog's parent widget is the target");
        result.expectEmpty = true;

        m_dialogLabel->update();
        pumpEvents(80);

        const DamageSnapshot snapshot = m_damageTracker->takeSnapshot();
        result.observedArea = snapshot.area();
        result.observedBoundingRect = snapshot.region.boundingRect();
        result.observedPaintEvents = snapshot.paintEvents;
        result.observedExcludedPaintEvents = snapshot.excludedPaintEvents;
        result.passed = snapshot.area() == 0 && snapshot.excludedPaintEvents > 0;
        results.append(result);
    }

    return results;
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
