// R1 qualification probe: LEGACY Qt5.15 OpenGL Scene Graph only.
// Render-tap reads the framebuffer after NORMAL on-screen rendering.
// Public-grab comparison uses QQuickItem::grabToImage on identical content.
// This is a separate test app; does not ship or change the HyRemote Runtime.
//
// This code must NOT be adapted to Qt6 RHI's afterRendering as-is.
// The Qt6 signal fires before command-buffer submission; swapchain resources
// may not be accessible for readback. No QtPrivate, QPA or GPU vendor API.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QTimer>
#include <QSharedPointer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr int kStages = 12;
constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 360;

QColor stageColor(int phase)
{
    switch (phase % 3) {
    case 0: return QColor(224, 35, 45);
    case 1: return QColor(25, 205, 65);
    default: return QColor(35, 65, 230);
    }
}

bool colorMatches(QColor color, int phase)
{
    const QColor expected = stageColor(phase);
    return color.isValid() && std::abs(color.red() - expected.red()) <= 14
           && std::abs(color.green() - expected.green()) <= 14
           && std::abs(color.blue() - expected.blue()) <= 14;
}

class SceneItem final : public QQuickItem
{
public:
    explicit SceneItem(QQuickItem *parent = nullptr) : QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
    }

    void setPhase(int next)
    {
        m_phase = next;
        update();
    }

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override
    {
        QSGGeometryNode *node = static_cast<QSGGeometryNode *>(old);
        if (!node) {
            node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(
                QSGGeometry::defaultAttributes_Point2D(), 4);
            geometry->setDrawingMode(GL_TRIANGLE_STRIP);
            node->setGeometry(geometry);
            node->setFlag(QSGNode::OwnsGeometry);
            auto *material = new QSGFlatColorMaterial;
            node->setMaterial(material);
            node->setFlag(QSGNode::OwnsMaterial);
        }

        QSGGeometry::updateRectGeometry(node->geometry(), boundingRect());
        node->geometry()->markVertexDataDirty();
        node->markDirty(QSGNode::DirtyGeometry);
        auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
        material->setColor(stageColor(m_phase));
        node->markDirty(QSGNode::DirtyMaterial);
        return node;
    }

private:
    int m_phase = 0;
};

struct ProbeStats
{
    std::mutex mutex;
    std::vector<double> readbackMs;
    std::vector<double> publicGrabMs;
    int badPixels = 0;
    int glErrors = 0;
    int actualTapFrames = 0;
    int renderCallbacks = 0;
    int missingGlContexts = 0;
    int observedFramebufferBinding = -1;
    std::atomic<int> desiredEpoch{0};
    std::atomic<int> observedEpoch{-1};
    std::atomic<bool> active{true};
};

double percentile95(std::vector<double> xs)
{
    if (xs.empty())
        return -1;
    std::sort(xs.begin(), xs.end());
    const std::size_t pos = static_cast<std::size_t>(
        std::ceil(xs.size() * 0.95)) - 1;
    return xs[std::min(pos, xs.size() - 1)];
}

double mean(const std::vector<double> &xs)
{
    if (xs.empty())
        return -1;
    return std::accumulate(xs.begin(), xs.end(), 0.0)
           / static_cast<double>(xs.size());
}

void printResult(const std::string &mode, int width, int height,
                 const ProbeStats &stat, int completedStages,
                 double wallMs, double cpuMs, int staticTailFrames)
{
    std::cout << "R1_RESULT_JSON={"
              << "\"mode\":\"" << mode << "\","
              << "\"qt\":\"" << QT_VERSION_STR << "\","
              << "\"backend\":\"Qt5_Legacy_OpenGL\","
              << "\"size_px\":[" << width << "," << height << "],"
              << "\"stages_expected\":" << kStages << ","
              << "\"stages_correct_pixels\":" << completedStages << ","
              << "\"render_callbacks\":" << stat.renderCallbacks << ","
              << "\"render_tap_readbacks\":" << stat.actualTapFrames << ","
              << "\"readback_p95_ms\":" << percentile95(stat.readbackMs) << ","
              << "\"readback_mean_ms\":" << mean(stat.readbackMs) << ","
              << "\"public_grab_p95_ms\":" << percentile95(stat.publicGrabMs) << ","
              << "\"public_grab_mean_ms\":" << mean(stat.publicGrabMs) << ","
              << "\"static_tail_render_callbacks\":" << staticTailFrames << ","
              << "\"gl_errors\":" << stat.glErrors << ","
              << "\"missing_gl_context\":" << stat.missingGlContexts << ","
              << "\"framebuffer_binding\":" << stat.observedFramebufferBinding << ","
              << "\"bad_pixel_frames\":" << stat.badPixels << ","
              << "\"wall_ms\":" << wallMs << ","
              << "\"process_cpu_ms\":" << cpuMs << ","
              << "\"qualified_for_product\":false"
              << "}" << std::endl;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    std::string mode = "render-tap";
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--mode=public-grab")
            mode = "public-grab";
        else if (arg == "--mode=render-tap")
            mode = "render-tap";
        else if (arg.rfind("--width=", 0) == 0)
            width = std::stoi(arg.substr(8));
        else if (arg.rfind("--height=", 0) == 0)
            height = std::stoi(arg.substr(9));
        else {
            std::cerr << "R1=BLOCKED_UNRECOGNIZED_ARG " << arg << '\n';
            return 10;
        }
    }
    if (width < 32 || height < 32 || width > 3840 || height > 2160) {
        std::cerr << "R1=BLOCKED_INVALID_SIZE\n";
        return 11;
    }

    if (qEnvironmentVariable("QT_QUICK_BACKEND") == QStringLiteral("software")
        || qEnvironmentVariable("QSG_RHI_BACKEND").size()) {
        std::cerr << "R1=BLOCKED_NO_LEGACY_OPENGL_SCENEGRAPH\n";
        return 12;
    }
    QQuickWindow window;
    window.resize(width, height);
    window.setColor(Qt::black);
    SceneItem item(window.contentItem());
    item.setSize(QSizeF(width, height));

    ProbeStats stats;
    QElapsedTimer timer;
    std::clock_t cpuStart = std::clock();
    QSharedPointer<QQuickItemGrabResult> pendingGrab;
    int completed = 0;
    int lastStaticFrames = -1;
    bool done = false;

    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, [&stats, width, height, mode] {
        stats.renderCallbacks++;
        if (mode != "render-tap" || !stats.active.load(std::memory_order_acquire))
            return;
        auto *context = QOpenGLContext::currentContext();
        if (!context) {
            stats.missingGlContexts++;
            return;
        }
        if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
            return;
        QOpenGLFunctions *gl = context->functions();
        GLint previousPack = 4;
        GLint framebuffer = 0;
        gl->glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width) * height * 4u);
        const auto started = std::chrono::steady_clock::now();
        gl->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
        const GLenum error = gl->glGetError();
        const auto stopped = std::chrono::steady_clock::now();
        gl->glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
        const double ms = std::chrono::duration<double, std::milli>(
            stopped - started).count();
        stats.actualTapFrames++;
        stats.observedFramebufferBinding = framebuffer;
        stats.readbackMs.push_back(ms);
        if (error != GL_NO_ERROR) {
            stats.glErrors++;
            return;
        }
        const std::size_t offset = (
            (static_cast<std::size_t>(height) / 2u) * width
            + (static_cast<std::size_t>(width) / 2u)) * 4u;
        const QColor color(pixels[offset], pixels[offset + 1],
                           pixels[offset + 2]);
        const int epoch = stats.desiredEpoch.load(std::memory_order_acquire);
        if (colorMatches(color, epoch)) {
            stats.observedEpoch.store(epoch, std::memory_order_release);
        } else {
            stats.badPixels++;
        }
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated,
                     &window, [&stats] {
        stats.active.store(false, std::memory_order_release);
    }, Qt::DirectConnection);

    auto reportAndExit = [&](bool success, const char *reason) {
        if (done)
            return;
        done = true;
        stats.active.store(false, std::memory_order_release);
        const double cpuMs = 1000.0 *
            static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
        const double wallMs = static_cast<double>(timer.elapsed());
        printResult(mode, width, height, stats, completed, wallMs, cpuMs,
                    stats.renderCallbacks - lastStaticFrames);
        std::cout << "R1_VERDICT=" << (success ? "PASS_SCOPED_QT5_OPENGL_PIXEL_CORRECT"
                                           : reason) << std::endl;
        app.exit(success ? 0 : 20);
    };

    // Fixed maximum duration: a failed/unsupported frame read must terminate
    // rather than give a misleading zero-cost result.
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &app, [&] {
        reportAndExit(false, "FAIL_TIMEOUT_OR_STALE_FRAME");
    });

    std::function<void()> nextStage;
    nextStage = [&] {
        if (completed >= kStages) {
            lastStaticFrames = stats.renderCallbacks;
            QTimer::singleShot(400, &app, [&] {
                reportAndExit(stats.glErrors == 0
                             && stats.missingGlContexts == 0,
                             "FAIL_GL_CONTEXT_OR_READBACK");
            });
            return;
        }
        stats.desiredEpoch.store(completed, std::memory_order_release);
        item.setPhase(completed);
        if (mode == "public-grab") {
            const int wanted = completed;
            QTimer::singleShot(35, &app, [&, wanted] {
                const auto start = std::chrono::steady_clock::now();
                pendingGrab = item.grabToImage();
                if (!pendingGrab) {
                    reportAndExit(false, "FAIL_PUBLIC_GRAB_REJECTED");
                    return;
                }
                QObject::connect(pendingGrab.data(), &QQuickItemGrabResult::ready,
                                 &app, [&, wanted, start] {
                    const QImage image = pendingGrab->image();
                    const auto stop = std::chrono::steady_clock::now();
                    stats.publicGrabMs.push_back(
                        std::chrono::duration<double, std::milli>(stop - start).count());
                    if (image.isNull() || image.width() <= 0 || image.height() <= 0
                        || !colorMatches(image.pixelColor(
                            image.width() / 2, image.height() / 2), wanted)) {
                        reportAndExit(false, "FAIL_PUBLIC_GRAB_WRONG_PIXEL");
                        return;
                    }
                    pendingGrab.clear();
                    ++completed;
                    QTimer::singleShot(35, &app, nextStage);
                });
            });
        }
    };

    QTimer poll;
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (done || mode != "render-tap" || completed >= kStages)
            return;
        if (stats.observedEpoch.load(std::memory_order_acquire) == completed) {
            ++completed;
            QTimer::singleShot(35, &app, nextStage);
        }
    });

    window.show();
    timer.start();
    guard.start(25000);
    poll.start();
    QTimer::singleShot(300, &app, nextStage);
    return app.exec();
}
