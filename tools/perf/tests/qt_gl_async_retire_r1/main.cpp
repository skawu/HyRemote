// #953: Public-Qt API, REAL Qt Quick OpenGL shared-context PBO retirement proof.
// Non-product fixture. Exactly one naturally rendered frame submits glReadPixels
// and GLsync; a worker-owned *shared* QOpenGLContext waits/maps/deletes them.
// Critically: NO window->update() is issued to retire the fence. Qt5 and Qt6
// differ only in the legal Qt scenegraph callback chosen below.
// This is neither a drop-in CaptureSource nor a production safety qualification.
#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr int kWidth = 320, kHeight = 180;
constexpr unsigned int kPackBuffer = 0x88EB;       // GL_PIXEL_PACK_BUFFER
constexpr unsigned int kPackBinding = 0x88ED;      // GL_PIXEL_PACK_BUFFER_BINDING
constexpr unsigned int kStreamRead = 0x88E1;       // GL_STREAM_READ
constexpr unsigned int kMapRead = 0x0001;          // GL_MAP_READ_BIT
constexpr unsigned int kSyncGpu = 0x9117;          // GL_SYNC_GPU_COMMANDS_COMPLETE
constexpr unsigned int kAlready = 0x911A;          // GL_ALREADY_SIGNALED
constexpr unsigned int kTimedOut = 0x911B;         // GL_TIMEOUT_EXPIRED
constexpr unsigned int kSatisfied = 0x911C;        // GL_CONDITION_SATISFIED
constexpr unsigned int kWaitFailed = 0x911D;       // GL_WAIT_FAILED

QColor colorFor(int quadrant)
{
    switch (quadrant) {
    case 0: return QColor(215, 36, 57);
    case 1: return QColor(37, 212, 66);
    case 2: return QColor(42, 69, 225);
    default: return QColor(229, 179, 33);
    }
}

class RealScene final : public QQuickItem {
public:
    explicit RealScene(QQuickItem *parent): QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
        setSize(QSizeF(kWidth, kHeight));
    }
protected:
    QSGNode *updatePaintNode(QSGNode *previous, UpdatePaintNodeData *) override
    {
        QSGNode *root = previous;
        if (!root) {
            root = new QSGNode;
            for (int i = 0; i < 4; ++i) {
                auto *node = new QSGGeometryNode;
                auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                node->setGeometry(geometry);
                node->setFlag(QSGNode::OwnsGeometry);
                node->setMaterial(new QSGFlatColorMaterial);
                node->setFlag(QSGNode::OwnsMaterial);
                root->appendChildNode(node);
            }
        }
        auto *child = root->firstChild();
        for (int i = 0; i < 4; ++i) {
            auto *node = static_cast<QSGGeometryNode *>(child);
            const QRectF rect((i % 2) * kWidth / 2.,
                              (i / 2) * kHeight / 2.,
                              kWidth / 2., kHeight / 2.);
            QSGGeometry::updateRectGeometry(node->geometry(), rect);
            node->geometry()->markVertexDataDirty();
            node->markDirty(QSGNode::DirtyGeometry);
            static_cast<QSGFlatColorMaterial *>(node->material())->setColor(colorFor(i));
            node->markDirty(QSGNode::DirtyMaterial);
            child = child->nextSibling();
        }
        return root;
    }
};

bool qualifiedGl(QOpenGLContext *context)
{
    if (!context || !context->isValid())
        return false;
    const auto format = context->format();
    const int major = format.majorVersion();
    const int minor = format.minorVersion();
    // Desktop GLsync appeared in 3.2; OpenGL ES supports these operations
    // in ES3.0. Do not assume Qt's presence implies the needed GL commands.
    return context->isOpenGLES() ? major >= 3
        : (major > 3 || (major == 3 && minor >= 2));
}

struct Job {
    QOpenGLContext *shared = nullptr; // moved to worker thread before start()
    QOffscreenSurface *surface = nullptr; // created/destroyed on GUI thread
    GLuint pbo = 0;
    GLsync fence = nullptr;
    int width = 0;
    int height = 0;
};

class FenceWorker final : public QThread {
public:
    Job job;
    std::vector<unsigned char> rgba;
    std::string reason = "NOT_STARTED";
    double gpuWaitMs = 0;
    bool contextWasCurrent = false;
protected:
    void run() override
    {
        // QOpenGLContext must be owned by this QThread; QOffscreenSurface is
        // only created/destroyed on GUI thread, then used here for makeCurrent.
        QOpenGLContext *context = job.shared;
        if (!context || !job.surface || context->thread() != QThread::currentThread()
            || !context->makeCurrent(job.surface)) {
            reason = "SHARED_CONTEXT_MAKE_CURRENT_FAILED";
            if (context && context->thread() == QThread::currentThread())
                delete context;
            return;
        }
        contextWasCurrent = QOpenGLContext::currentContext() == context;
        auto *gl = context->extraFunctions();
        gl->initializeOpenGLFunctions();
        const auto start = Clock::now();
        bool signaled = false;
        for (int poll = 0; poll < 150; ++poll) {
            // Bounded 5ms * 150, never block Qt's render/GUI thread.
            const auto status = gl->glClientWaitSync(job.fence, 0, 5'000'000ULL);
            if (status == kAlready || status == kSatisfied) {
                signaled = true;
                break;
            }
            if (status == kWaitFailed || status != kTimedOut) {
                reason = "GPU_FENCE_WAIT_FAILED";
                break;
            }
        }
        gpuWaitMs = std::chrono::duration<double, std::milli>(
            Clock::now() - start).count();

        const std::size_t stride = std::size_t(job.width) * 4;
        const std::size_t bytes = stride * std::size_t(job.height);
        if (signaled && job.width > 0 && job.height > 0
            && bytes <= 8 * 1024 * 1024) {
            GLint oldPack = 0;
            gl->glGetIntegerv(kPackBinding, &oldPack);
            gl->glBindBuffer(kPackBuffer, job.pbo);
            const void *mapped = gl->glMapBufferRange(kPackBuffer, 0,
                GLsizeiptr(bytes), kMapRead);
            if (mapped) {
                rgba.resize(bytes);
                const auto *src = static_cast<const unsigned char *>(mapped);
                // GL framebuffer is bottom-up. This is one owned CPU copy;
                // do NOT expose a mapped PBO after unmap().
                for (int y = 0; y < job.height; ++y)
                    std::memcpy(rgba.data() + std::size_t(y) * stride,
                        src + std::size_t(job.height - y - 1) * stride, stride);
                reason = gl->glUnmapBuffer(kPackBuffer)
                    ? "OK" : "GPU_BUFFER_UNMAP_FAILED";
                if (reason != "OK")
                    rgba.clear();
            } else {
                reason = "PBO_MAP_FAILED";
            }
            gl->glBindBuffer(kPackBuffer, GLuint(oldPack));
        } else if (reason == "NOT_STARTED") {
            reason = "GPU_FENCE_750MS_TIMEOUT_OR_BAD_SIZE";
        }
        // Both objects are shared GL objects. Delete them HERE while the
        // worker's shared context is current, never from a timer thread.
        if (job.fence)
            gl->glDeleteSync(job.fence);
        if (job.pbo)
            gl->glDeleteBuffers(1, &job.pbo);
        context->doneCurrent();
        delete context; // the object's affinity is now the worker thread
        job.shared = nullptr;
    }
};

bool pixelTruth(const FenceWorker &worker)
{
    const int w = worker.job.width, h = worker.job.height;
    if (w <= 0 || h <= 0 || worker.rgba.size() != std::size_t(w) * h * 4)
        return false;
    for (int q = 0; q < 4; ++q) {
        const int x = q % 2 ? w * 3 / 4 : w / 4;
        const int y = q / 2 ? h * 3 / 4 : h / 4;
        const unsigned char *p = worker.rgba.data()
            + (std::size_t(y) * w + x) * 4;
        const QColor expected = colorFor(q);
        if (std::abs(int(p[0]) - expected.red()) > 20
            || std::abs(int(p[1]) - expected.green()) > 20
            || std::abs(int(p[2]) - expected.blue()) > 20
            || p[3] < 230)
            return false;
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    bool cancelBeforeDelivery = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cancel")
            cancelBeforeDelivery = true;
        else {
            std::cerr << "R953=BLOCKED_BAD_ARGUMENT\n";
            return 9;
        }
    }

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);
    QGuiApplication app(argc, argv);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif

    // QOffscreenSurface must be CREATED and DESTROYED on the GUI thread.
    QOffscreenSurface surface;
    surface.setFormat(fmt);
    surface.create();
    if (!surface.isValid()) {
        std::cerr << "R953=UNSUPPORTED_OFFSCREEN_SURFACE\n";
        return 10;
    }

    QQuickWindow window;
    window.setFormat(fmt);
    window.setColor(Qt::black);
    window.resize(kWidth, kHeight);
    RealScene scene(window.contentItem());
    FenceWorker worker;
    std::atomic<unsigned> generation{1};
    std::atomic<int> submissions{0};
    int extraWindowUpdates = 0; // this prototype makes NO update() calls
    bool nativeQualified = false;
    std::string error;
    double submissionMs = 0;

    QObject::connect(&worker, &QThread::finished, &app, [&] {
        worker.wait(); // synchronize ownership before inspecting its data
        const bool imageValid = pixelTruth(worker);
        const bool delivered = generation.load() == 1 && !cancelBeforeDelivery;
        const bool correct = (cancelBeforeDelivery || imageValid)
            && nativeQualified && worker.contextWasCurrent
            && worker.reason == "OK" && submissions.load() == 1
            && extraWindowUpdates == 0;
        const bool suppressed = cancelBeforeDelivery && !delivered;
        std::cout << "R953_RESULT_JSON={\"qt\":\"" << QT_VERSION_STR
                  << "\",\"graphics_api\":\"OpenGL\",\"shared_context\":"
                  << (nativeQualified ? "true" : "false")
                  << ",\"worker_context_current\":"
                  << (worker.contextWasCurrent ? "true" : "false")
                  << ",\"gpu_wait_ms\":" << worker.gpuWaitMs
                  << ",\"submit_wall_ms\":" << submissionMs
                  << ",\"natural_frame_submissions\":" << submissions.load()
                  << ",\"extra_window_updates\":" << extraWindowUpdates
                  << ",\"pixel_truth\":" << (imageValid ? "true" : "false")
                  << ",\"cancel_suppressed_delivery\":"
                  << (suppressed ? "true" : "false")
                  << ",\"reason\":\"" << worker.reason
                  << "\",\"qualified_for_product\":false}" << std::endl;
        if (correct && (!cancelBeforeDelivery || suppressed)) {
            std::cout << "R953_VERDICT=PASS_SCOPED_SHARED_CONTEXT_GPU_RETIRE\n";
            app.exit(0);
        } else {
            std::cerr << "R953_VERDICT=FAIL_PROTOTYPE\n";
            app.exit(31);
        }
    }, Qt::QueuedConnection);

    const auto onFrame = [&] {
        if (submissions.load() != 0)
            return;
        QOpenGLContext *render = QOpenGLContext::currentContext();
        if (!qualifiedGl(render)) {
            error = "UNSUPPORTED_RENDER_GL_CONTEXT_OR_VERSION";
            app.exit(11); // QGuiApplication::exit is safe across threads
            return;
        }
        auto *shared = new QOpenGLContext;
        shared->setFormat(render->format());
        shared->setShareContext(render);
        if (!shared->create() || !QOpenGLContext::areSharing(render, shared)) {
            error = "UNSUPPORTED_GL_SHARE_GROUP";
            delete shared;
            app.exit(12);
            return;
        }

        const qreal dpr = window.devicePixelRatio();
        const int width = qRound(window.width() * dpr);
        const int height = qRound(window.height() * dpr);
        if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
            delete shared;
            error = "INVALID_FRAME_GEOMETRY";
            app.exit(13);
            return;
        }

        auto *gl = render->extraFunctions();
        gl->initializeOpenGLFunctions();
        GLint oldPack = 0;
        GLint oldAlign = 4;
        gl->glGetIntegerv(kPackBinding, &oldPack);
        gl->glGetIntegerv(GL_PACK_ALIGNMENT, &oldAlign);
        GLuint pbo = 0;
        gl->glGenBuffers(1, &pbo);
        gl->glBindBuffer(kPackBuffer, pbo);
        const auto count = GLsizeiptr(std::size_t(width) * height * 4);
        gl->glBufferData(kPackBuffer, count, nullptr, kStreamRead);
        gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const auto start = Clock::now();
        gl->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        GLsync fence = gl->glFenceSync(kSyncGpu, 0);
        gl->glFlush(); // make the shared synchronization object visible
        submissionMs = std::chrono::duration<double, std::milli>(
            Clock::now() - start).count();
        const GLenum status = gl->glGetError();
        gl->glBindBuffer(kPackBuffer, GLuint(oldPack));
        gl->glPixelStorei(GL_PACK_ALIGNMENT, oldAlign);
        if (!pbo || !fence || status != GL_NO_ERROR) {
            if (fence) gl->glDeleteSync(fence);
            if (pbo) gl->glDeleteBuffers(1, &pbo);
            delete shared;
            error = "FAILED_GL_PBO_SUBMISSION";
            app.exit(14);
            return;
        }
        // The shared context is NEVER made current in Qt's render thread.
        // QObject::moveToThread is invoked from its creation thread.
        shared->moveToThread(&worker);
        if (shared->thread() != &worker) {
            gl->glDeleteSync(fence);
            gl->glDeleteBuffers(1, &pbo);
            delete shared;
            error = "SHARED_CONTEXT_AFFINITY_FAILURE";
            app.exit(15);
            return;
        }
        worker.job = {shared, &surface, pbo, fence, width, height};
        nativeQualified = true;
        submissions.fetch_add(1);
        // Even in cancellation, the worker must retire the GL resources
        // before any Surface/context destruction or module unload.
        if (cancelBeforeDelivery)
            generation.fetch_add(1);
        worker.start();
        // No window->update(), timer-poll-on-render or extra Qt frame.
    };

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QObject::connect(&window, &QQuickWindow::afterFrameEnd,
                     &window, onFrame, Qt::DirectConnection);
#else
    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, onFrame, Qt::DirectConnection);
#endif
    QTimer::singleShot(6000, &app, [&] {
        if (submissions.load() == 0) {
            std::cerr << "R953_VERDICT=BLOCKED_NO_NATURAL_OPENGL_FRAME " << error << '\n';
            app.exit(16);
        }
    });
    window.show();
    const int exitCode = app.exec();
    if (worker.isRunning())
        worker.wait(); // bounded 750ms GPU wait, no runaway DLL worker
    // The offscreen surface remains live until worker stopped and context
    // was destroyed in the correct worker thread.
    return exitCode;
}
