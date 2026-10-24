#include "framebuffer_view.h"
#include "rfb_client.h"

#include <QColor>
#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>

#include <algorithm>
#include <cstdlib>

FramebufferView::FramebufferView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 240);
    m_placeholder = QStringLiteral("Not connected. Enter a remote IP and port, then Connect.");
}

void FramebufferView::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void FramebufferView::setCursorImage(const QImage &cursor, const QPoint &hotspot)
{
    if (cursor.isNull()) {
        unsetCursor();
        return;
    }
    setCursor(QCursor(QPixmap::fromImage(cursor), hotspot.x(), hotspot.y()));
}

void FramebufferView::clearFrame(const QString &placeholder)
{
    releaseHeldInput();
    m_frame = QImage();
    m_placeholder = placeholder;
    m_lastImagePoint = QPoint(-1, -1);
    update();
}

void FramebufferView::setPlaceholder(const QString &placeholder)
{
    if (m_frame.isNull()) {
        m_placeholder = placeholder;
        update();
    }
}

void FramebufferView::setInputEnabled(bool enabled)
{
    if (!enabled)
        releaseHeldInput();
    m_inputEnabled = enabled;
}

void FramebufferView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), QColor(24, 24, 28));

    if (m_frame.isNull()) {
        painter.setPen(QColor(190, 190, 200));
        painter.drawText(rect().adjusted(24, 24, -24, -24),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         m_placeholder);
        return;
    }

    const QRect target = imageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, m_frame);
    painter.setPen(QColor(70, 70, 80));
    painter.drawRect(target.adjusted(0, 0, -1, -1));
}

QRect FramebufferView::imageRect() const
{
    if (m_frame.isNull())
        return QRect();
    const double scale = std::min(static_cast<double>(width()) / m_frame.width(),
                                  static_cast<double>(height()) / m_frame.height());
    const int scaledWidth = std::max(1, static_cast<int>(m_frame.width() * scale));
    const int scaledHeight = std::max(1, static_cast<int>(m_frame.height() * scale));
    return QRect((width() - scaledWidth) / 2, (height() - scaledHeight) / 2,
                 scaledWidth, scaledHeight);
}

QPoint FramebufferView::mapToImage(const QPoint &widgetPoint) const
{
    const QRect target = imageRect();
    if (m_frame.isNull() || !target.contains(widgetPoint))
        return QPoint(-1, -1);
    const int x = (widgetPoint.x() - target.x()) * m_frame.width() / target.width();
    const int y = (widgetPoint.y() - target.y()) * m_frame.height() / target.height();
    return QPoint(std::clamp(x, 0, m_frame.width() - 1),
                  std::clamp(y, 0, m_frame.height() - 1));
}

void FramebufferView::sendPointer(const QPoint &widgetPoint, quint8 mask)
{
    const QPoint imagePoint = mapToImage(widgetPoint);
    if (imagePoint.x() < 0 || !onPointer)
        return;
    m_lastImagePoint = imagePoint;
    onPointer(mask, static_cast<quint16>(imagePoint.x()),
              static_cast<quint16>(imagePoint.y()));
}

void FramebufferView::releaseHeldInput()
{
    if (onKey) {
        for (quint32 keysym : m_heldKeys)
            onKey(keysym, false);
    }
    m_heldKeys.clear();

    if (m_buttons != 0 && onPointer && m_lastImagePoint.x() >= 0) {
        onPointer(0, static_cast<quint16>(m_lastImagePoint.x()),
                  static_cast<quint16>(m_lastImagePoint.y()));
    }
    m_buttons = 0;
}

quint8 FramebufferView::buttonMask(Qt::MouseButtons buttons)
{
    quint8 mask = 0;
    if (buttons.testFlag(Qt::LeftButton))
        mask |= 1;
    if (buttons.testFlag(Qt::MiddleButton))
        mask |= 2;
    if (buttons.testFlag(Qt::RightButton))
        mask |= 4;
    return mask;
}

void FramebufferView::mousePressEvent(QMouseEvent *event)
{
    if (!m_inputEnabled) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_buttons = buttonMask(event->buttons());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    sendPointer(event->position().toPoint(), m_buttons);
#else
    sendPointer(event->localPos().toPoint(), m_buttons);
#endif
    event->accept();
}

void FramebufferView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_inputEnabled) {
        QWidget::mouseMoveEvent(event);
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    sendPointer(event->position().toPoint(), m_buttons);
#else
    sendPointer(event->localPos().toPoint(), m_buttons);
#endif
    event->accept();
}

void FramebufferView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_inputEnabled) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_buttons = buttonMask(event->buttons());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    sendPointer(event->position().toPoint(), m_buttons);
#else
    sendPointer(event->localPos().toPoint(), m_buttons);
#endif
    event->accept();
}

void FramebufferView::wheelEvent(QWheelEvent *event)
{
    if (!m_inputEnabled || !onPointer) {
        QWidget::wheelEvent(event);
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint imagePoint = mapToImage(event->position().toPoint());
#else
    const QPoint imagePoint = mapToImage(event->posF().toPoint());
#endif
    if (imagePoint.x() < 0)
        return;
    const int delta = event->angleDelta().y();
    if (delta == 0)
        return;
    // RFB pointer masks are bit values: wheel-up=0x08, wheel-down=0x10.
    // Merge the pulse with any held real button, then restore that held mask
    // instead of sending an unconditional zero (which would release a drag).
    const quint8 wheelMask = rfbWheelMaskForDelta(delta);
    const int steps = std::max(1, std::abs(delta) / 120);
    for (int index = 0; index < steps; ++index) {
        onPointer(static_cast<quint8>(m_buttons | wheelMask),
                  static_cast<quint16>(imagePoint.x()),
                  static_cast<quint16>(imagePoint.y()));
        onPointer(m_buttons, static_cast<quint16>(imagePoint.x()),
                  static_cast<quint16>(imagePoint.y()));
    }
    event->accept();
}

quint32 FramebufferView::keysymFor(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 0xFF0D;
    case Qt::Key_Backspace:
        return 0xFF08;
    case Qt::Key_Tab:
        return 0xFF09;
    case Qt::Key_Escape:
        return 0xFF1B;
    case Qt::Key_Delete:
        return 0xFFFF;
    case Qt::Key_Insert:
        return 0xFF63;
    case Qt::Key_Home:
        return 0xFF50;
    case Qt::Key_End:
        return 0xFF57;
    case Qt::Key_PageUp:
        return 0xFF55;
    case Qt::Key_PageDown:
        return 0xFF56;
    case Qt::Key_Left:
        return 0xFF51;
    case Qt::Key_Up:
        return 0xFF52;
    case Qt::Key_Right:
        return 0xFF53;
    case Qt::Key_Down:
        return 0xFF54;
    case Qt::Key_Shift:
        return 0xFFE1;
    case Qt::Key_Control:
        return 0xFFE3;
    case Qt::Key_Alt:
        return 0xFFE9;
    case Qt::Key_Meta:
        // Match the Host key map: X11 Meta_L/Meta_R, not the XF86Meta keysym.
        return 0xFFE7;
    case Qt::Key_Space:
        return 0x20;
    default:
        break;
    }

    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12)
        return 0xFFBE + static_cast<quint32>(event->key() - Qt::Key_F1);

    const QString text = event->text();
    if (text.isEmpty())
        return 0;
    const uint codepoint = text.at(0).unicode();
    if (codepoint >= 0x20 && codepoint <= 0x7e)
        return codepoint;
    if (codepoint > 0x7e)
        return 0x01000000u | codepoint;
    return 0;
}

void FramebufferView::keyPressEvent(QKeyEvent *event)
{
    if (!m_inputEnabled || !onKey) {
        QWidget::keyPressEvent(event);
        return;
    }
    const quint32 keysym = keysymFor(event);
    if (keysym == 0) {
        QWidget::keyPressEvent(event);
        return;
    }
    m_heldKeys.insert(keysym);
    onKey(keysym, true);
    event->accept();
}

void FramebufferView::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_inputEnabled || !onKey) {
        QWidget::keyReleaseEvent(event);
        return;
    }
    const quint32 keysym = keysymFor(event);
    if (keysym == 0) {
        QWidget::keyReleaseEvent(event);
        return;
    }
    m_heldKeys.remove(keysym);
    onKey(keysym, false);
    event->accept();
}

void FramebufferView::focusOutEvent(QFocusEvent *event)
{
    releaseHeldInput();
    QWidget::focusOutEvent(event);
}
