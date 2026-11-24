#pragma once

#include <QPainter>
#include <QQuickPaintedItem>

#include <atomic>

class StagedQuickProbeItem final : public QQuickPaintedItem
{
public:
    static constexpr unsigned A = 0x143ca0U;
    static constexpr unsigned B = 0xe11e28U;

    explicit StagedQuickProbeItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void setStage(int stage)
    {
        m_requestedStage.store(stage, std::memory_order_release);
        update();
    }

    int paintedStage() const { return m_paintedStage.load(std::memory_order_acquire); }

    void paint(QPainter *painter) override
    {
        const int stage = m_requestedStage.load(std::memory_order_acquire);
        const QRectF left(0.0, 0.0, width() / 2.0, height());
        const QRectF right(width() / 2.0, 0.0, width() / 2.0, height());
        const QColor a = QColor::fromRgb(A);
        const QColor b = QColor::fromRgb(B);
        painter->fillRect(left, stage >= 1 ? b : a);
        painter->fillRect(right, stage >= 2 ? b : a);
        m_paintedStage.store(stage, std::memory_order_release);
    }

private:
    std::atomic<int> m_requestedStage{0};
    std::atomic<int> m_paintedStage{-1};
};
