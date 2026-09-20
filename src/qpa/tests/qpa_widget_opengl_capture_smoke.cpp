#include "rfb_test_client.hpp"

#include <QApplication>
#include <QColor>
#include <QOpenGLFunctions>
#include <QPalette>
#include <QTimer>
#include <QWidget>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <iostream>

namespace {

constexpr quint16 kPort = 5994;

class SolidOpenGLWidget final : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    explicit SolidOpenGLWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
    {
    }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glClearColor(0.90F, 0.12F, 0.06F, 1.0F);
    }

    void paintGL() override
    {
        glClearColor(0.90F, 0.12F, 0.06F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }
};

bool isGlPatch(const QColor &color)
{
    return color.red() > 180 && color.green() < 90 && color.blue() < 70;
}

bool isRasterPatch(const QColor &color)
{
    return color.blue() > 170 && color.red() < 80 && color.green() < 130;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget parent;
    parent.setWindowTitle(QStringLiteral("HyRemote QWidget + QOpenGLWidget capture qualification"));
    parent.resize(360, 220);
    parent.move(120, 120);

    QPalette parentPalette = parent.palette();
    parentPalette.setColor(QPalette::Window, QColor(35, 35, 35));
    parent.setPalette(parentPalette);
    parent.setAutoFillBackground(true);

    SolidOpenGLWidget glChild(&parent);
    glChild.setGeometry(20, 20, 140, 120);

    QWidget rasterChild(&parent);
    rasterChild.setGeometry(210, 20, 120, 120);
    QPalette rasterPalette = rasterChild.palette();
    rasterPalette.setColor(QPalette::Window, QColor(25, 70, 210));
    rasterChild.setPalette(rasterPalette);
    rasterChild.setAutoFillBackground(true);

    parent.show();
    glChild.update();

    int result = 1;
    QTimer::singleShot(900, &app, [&] {
        if (!glChild.isValid() || !glChild.context()) {
            std::cerr << "FAIL: reference native platform did not create a valid QOpenGLWidget context\n";
            app.quit();
            return;
        }

        HyRemote::Qpa::Test::RfbTestClient viewer(kPort);
        if (!viewer.connectAndHandshake() || !viewer.requestFramebuffer()) {
            std::cerr << "FAIL: could not obtain production remote frame for QWidget/OpenGL hierarchy\n";
            app.quit();
            return;
        }
        if (viewer.geometry().size() != parent.size() || viewer.image().size() != parent.size()) {
            std::cerr << "FAIL: remote framebuffer geometry does not match parent QWidget client area\n";
            app.quit();
            return;
        }

        const QColor glPixel = viewer.image().pixelColor(90, 80);
        const QColor rasterPixel = viewer.image().pixelColor(270, 80);
        if (!isGlPatch(glPixel)) {
            std::cerr << "FAIL: production QWidget capture omitted or corrupted QOpenGLWidget content: "
                      << glPixel.red() << ',' << glPixel.green() << ',' << glPixel.blue() << '\n';
            app.quit();
            return;
        }
        if (!isRasterPatch(rasterPixel)) {
            std::cerr << "FAIL: production QWidget capture omitted raster sibling content: "
                      << rasterPixel.red() << ',' << rasterPixel.green() << ',' << rasterPixel.blue() << '\n';
            app.quit();
            return;
        }

        viewer.socket().disconnectFromHost();
        std::cout << "PASS: production QWidget capture composes QOpenGLWidget and raster siblings\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(15000, &app, [&] {
        std::cerr << "FAIL: QWidget/QOpenGLWidget production capture smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
