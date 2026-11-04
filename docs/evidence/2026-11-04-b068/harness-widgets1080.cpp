// #609 baseline harness: frameless 1920x1080 Qt Widgets window with a deterministic static scene.
// It contains no HyRemote code; the Generic plugin is attached with -plugin for B1/B2 runs.
#include <QApplication>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QWidget>

class Scene final : public QWidget
{
public:
    Scene()
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setWindowTitle(QStringLiteral("B609|widgets"));
        resize(1920, 1080);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0.0, QColor(0x10, 0x24, 0x3a));
        gradient.setColorAt(1.0, QColor(0x08, 0x13, 0x1f));
        painter.fillRect(rect(), gradient);

        const int cellW = 80;
        const int cellH = 83;
        for (int row = 0; row * cellH < height(); ++row) {
            for (int col = 0; col * cellW < width(); ++col) {
                const int index = row * 24 + col;
                QColor color = QColor::fromHsl(((index * 29) % 360) * 255 / 359, 0.35 * 255, 0.30 * 255);
                painter.fillRect(col * cellW, row * cellH, cellW - 2, cellH - 2, color);
            }
        }

        QFont title = painter.font();
        title.setPixelSize(44);
        title.setBold(true);
        painter.setFont(title);
        painter.setPen(QColor(0xe9, 0xf1, 0xfa));
        painter.drawText(rect().adjusted(0, height() / 2 - 90, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("B609 static 1080p scene"));

        QFont subtitle = painter.font();
        subtitle.setPixelSize(28);
        subtitle.setBold(false);
        painter.setFont(subtitle);
        painter.setPen(QColor(0x8f, 0xc7, 0xee));
        painter.drawText(rect().adjusted(0, height() / 2 - 20, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("widgets/quick baseline, no animation"));
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Scene scene;
    scene.show();
    return app.exec();
}
