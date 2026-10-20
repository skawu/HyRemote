#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QWidget>

#include <functional>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class FramebufferView final : public QWidget
{
public:
    explicit FramebufferView(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void setCursorImage(const QImage &cursor, const QPoint &hotspot);
    void clearFrame(const QString &placeholder);
    void setPlaceholder(const QString &placeholder);
    bool hasFrame() const { return !m_frame.isNull(); }
    void setInputEnabled(bool enabled);
    bool inputEnabled() const { return m_inputEnabled; }

    std::function<void(quint8 buttonMask, quint16 x, quint16 y)> onPointer;
    std::function<void(quint32 keysym, bool pressed)> onKey;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QRect imageRect() const;
    QPoint mapToImage(const QPoint &widgetPoint) const;
    void sendPointer(const QPoint &widgetPoint, quint8 buttonMask);
    void releaseHeldInput();
    static quint8 buttonMask(Qt::MouseButtons buttons);
    static quint32 keysymFor(QKeyEvent *event);

    QImage m_frame;
    QString m_placeholder;
    bool m_inputEnabled = false;
    quint8 m_buttons = 0;
    QPoint m_lastImagePoint{-1, -1};
    QSet<quint32> m_heldKeys;
};
