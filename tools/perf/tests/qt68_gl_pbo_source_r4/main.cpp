// R4: ACTUAL Qt6.8.3 public OpenGL-RHI CaptureSource runtime test.
// Deliberately does not use QtGuiPrivate, a Viewer, network or QML business hints.
// It checks the real Core request→frame callback contract, immutable CPU storage,
// four-quadrant pixel truth, request time/ID, and stop/restart on a visible window.
#if defined(HYREMOTE_R4_D3D11)
#include "quick/quick_d3d11_staging_capture_source.hpp"
#else
#include "quick/quick_gl_pbo_capture_source.hpp"
#endif
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QTimer>
#include <QThread>
#include <QtGlobal>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr int kW = 640, kH = 360, kPhases = 12;
constexpr int kResizedW = 800, kResizedH = 450;

QColor colorFor(int phase)
{
    switch (phase % 3) {
    case 0: return QColor(215, 36, 57);
    case 1: return QColor(37, 212, 66);
    default: return QColor(42, 69, 225);
    }
}

class RealScene final : public QQuickItem {
public:
    explicit RealScene(QQuickItem *parent): QQuickItem(parent) {
        setFlag(ItemHasContents, true);
        setWidth(kW);
        setHeight(kH);
    }
    void setPhase(int phase) {
        m_phase = phase;
        update();
    }
    void setSceneSize(int width, int height) {
        m_sceneW = width;
        m_sceneH = height;
        setWidth(width);
        setHeight(height);
        update();
    }
protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override {
        QSGNode *root = old;
        if (!root) {
            root = new QSGNode;
            for (int q = 0; q < 4; ++q) {
                auto *rect = new QSGGeometryNode;
                auto *geo = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                geo->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                rect->setGeometry(geo);
                rect->setFlag(QSGNode::OwnsGeometry);
                auto *mat = new QSGFlatColorMaterial;
                rect->setMaterial(mat);
                rect->setFlag(QSGNode::OwnsMaterial);
                root->appendChildNode(rect);
            }
        }
        auto *rect = root->firstChild();
        for (int q = 0; q < 4; ++q) {
            const double x = (q % 2) * m_sceneW / 2.0;
            const double y = (q / 2) * m_sceneH / 2.0;
            auto *r = static_cast<QSGGeometryNode *>(rect);
            QSGGeometry::updateRectGeometry(r->geometry(), QRectF(x, y, m_sceneW / 2.0, m_sceneH / 2.0));
            r->geometry()->markVertexDataDirty();
            r->markDirty(QSGNode::DirtyGeometry);
            auto *material = static_cast<QSGFlatColorMaterial *>(r->material());
            material->setColor(colorFor(m_phase + q));
            r->markDirty(QSGNode::DirtyMaterial);
            rect = rect->nextSibling();
        }
        return root;
    }
private:
    int m_phase = 0;
    int m_sceneW = kW;
    int m_sceneH = kH;
};

bool validateFrame(const hyremote::RemoteFrame &frame, int phase, int expectedW, int expectedH)
{
    if (!hyremote::validateFrame(frame).ok || !frame.storage
        || frame.geometry.size.width != static_cast<std::uint32_t>(expectedW)
        || frame.geometry.size.height != static_cast<std::uint32_t>(expectedH)
        || frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || frame.geometry.planeCount != 1 || !frame.requestId
        || !frame.timing.requestTime || !frame.timing.completionTime
        || frame.timing.ptsSource != hyremote::PtsSource::Render
        || frame.timing.pts == hyremote::TimePoint{}
        || frame.timing.pts > *frame.timing.completionTime
        || *frame.timing.completionTime < *frame.timing.requestTime)
        return false;
    auto pixels = frame.storage->mapRead(0);
    if (!pixels || !pixels->data || pixels->stride < static_cast<std::size_t>(expectedW) * 4
        || pixels->bytes < pixels->stride * static_cast<std::size_t>(expectedH))
        return false;
    for (int q = 0; q < 4; ++q) {
        const int x = (q % 2) ? expectedW * 3 / 4 : expectedW / 4;
        const int y = (q / 2) ? expectedH * 3 / 4 : expectedH / 4;
        const auto *p = reinterpret_cast<const unsigned char *>(
            pixels->data + std::size_t(y) * pixels->stride + std::size_t(x * 4));
        const QColor expected = colorFor(phase + q);
        if (std::abs(int(p[0]) - expected.red()) > 15
            || std::abs(int(p[1]) - expected.green()) > 15
            || std::abs(int(p[2]) - expected.blue()) > 15)
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    bool resizeScenario = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--resize")
            resizeScenario = true;
        else {
            std::cerr << "R4=BLOCKED_UNKNOWN_ARGUMENT\n";
            return 9;
        }
    }
#if defined(HYREMOTE_R4_D3D11)
    if (qEnvironmentVariable("QSG_RHI_BACKEND") != QStringLiteral("d3d11")) {
        std::cerr << "R4=BLOCKED_NOT_D3D11_RHI\n";
        return 10;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#else
    if (qEnvironmentVariable("QSG_RHI_BACKEND") != QStringLiteral("opengl")) {
        std::cerr << "R4=BLOCKED_NOT_OPENGL_RHI\n";
        return 10;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    QQuickWindow window;
    window.resize(kW, kH);
    window.setColor(Qt::black);
    RealScene scene(window.contentItem());
    scene.setPhase(0);
    window.show();

#if defined(HYREMOTE_R4_D3D11)
    auto source = HyRemote::detail::createExperimentalQuickD3d11StagingCaptureSource(&window);
#else
    auto source = HyRemote::detail::createExperimentalQuickGlPboCaptureSource(&window);
#endif
    if (!source || !source->capabilities().cpuReadable) {
        std::cerr << "R4=FAIL_INVALID_CAPABILITY\n";
        return 11;
    }

    int desired = 0;
    int completed = 0;
    int errors = 0;
    int staleCallbacks = 0;
    int restarts = 0;
    bool intentionallyStopped = false;
    bool fullyStopped = false;
    bool hiddenRequestPending = false;
    int hiddenUnavailableEvents = 0;
    int hiddenTerminalFrames = 0;
    std::set<std::uint64_t> seenRequestIds;
    int currentW = kW;
    int currentH = kH;
    bool resized = false;

    std::function<void()> dispatchNext;
    std::function<void()> finishCase;
    const auto onFrame = [&](hyremote::RemoteFrame frame) {
        if (hiddenRequestPending && frame.requestId && *frame.requestId == 777) {
            if (hiddenUnavailableEvents != 1 || frame.storage
                || !frame.timing.requestTime || !frame.timing.completionTime
                || *frame.timing.completionTime < *frame.timing.requestTime) {
                ++errors;
                std::cerr << "R4=FAIL_HIDDEN_TERMINAL_FRAME_CONTRACT\n";
                app.exit(29);
                return;
            }
            ++hiddenTerminalFrames;
            hiddenRequestPending = false;
            // FrameReadyHandler executes inside the source's callback
            // accounting. stop() waits for it to drain, so invoke stop
            // AFTER returning to the Qt event loop, never recursively.
            QTimer::singleShot(0, &app, finishCase);
            return;
        }
        if (fullyStopped || intentionallyStopped) {
            ++staleCallbacks;
            return;
        }
        if (!validateFrame(frame, desired, currentW, currentH)) {
            ++errors;
            std::cerr << "R4=FAIL_WRONG_OWNED_PIXELS_OR_FRAME_METADATA phase="
                      << desired << '\n';
            app.exit(21);
            return;
        }
        if (!seenRequestIds.insert(*frame.requestId).second) {
            ++errors;
            app.exit(22);
            return;
        }
        ++completed;
        QTimer::singleShot(35, &app, dispatchNext);
    };

    const auto onEvent = [&](const hyremote::CaptureEvent &event) {
        if (hiddenRequestPending
            && event.code == hyremote::CaptureEventCode::TemporarilyUnavailable
            && event.recoverable) {
            ++hiddenUnavailableEvents;
            return;
        }
        ++errors;
        std::cerr << "R4=FAIL_CAPTURE_EVENT code="
                  << static_cast<int>(event.code) << " reason="
                  << event.message << '\n';
        app.exit(23);
    };

    auto startSource = [&] {
        if (!source->start(onFrame, onEvent)) {
            ++errors;
            std::cerr << "R4=FAIL_START\n";
            app.exit(24);
        }
    };

    finishCase = [&] {
        fullyStopped = true;
        source->stop();
        QTimer::singleShot(250, &app, [&] {
                const bool valid = errors == 0 && staleCallbacks == 0
                    && completed == kPhases && restarts == 2
                    && seenRequestIds.size() == kPhases
                    && hiddenUnavailableEvents == 1 && hiddenTerminalFrames == 1;
                std::cout << "R4_RESULT_JSON={"
                    << "\"qt\":\"" << QT_VERSION_STR << "\","
#if defined(HYREMOTE_R4_D3D11)
                    << "\"backend\":\"Qt6_RHI_Direct3D11\","
#else
                    << "\"backend\":\"Qt6_RHI_OpenGL\","
#endif
#if defined(HYREMOTE_R4_D3D11)
                    << "\"source\":\"real_D3d11StagingCaptureSource\","
#else
                    << "\"source\":\"real_GlPboCaptureSource\","
#endif
                    << "\"phases_expected\":" << kPhases << ","
                    << "\"owned_frames_validated\":" << completed << ","
                    << "\"unique_request_ids\":" << seenRequestIds.size() << ","
                    << "\"stop_restart_cycles\":" << restarts << ","
                    << "\"abandoned_predispatch_request_isolated\":"
                    << (seenRequestIds.count(999) == 0 ? "true" : "false") << ","
                    << "\"hidden_unavailable_events\":" << hiddenUnavailableEvents << ","
                    << "\"resize_scenario\":" << (resizeScenario ? "true" : "false") << ","
                    << "\"resize_verified\":" << (resized ? "true" : "false") << ","
                    << "\"final_pixel_size\":[" << currentW << "," << currentH << "],"

                    << "\"hidden_terminal_remote_frames\":" << hiddenTerminalFrames << ","
                    << "\"late_callbacks_after_stop\":" << staleCallbacks << ","
                    << "\"errors\":" << errors << ","
                    << "\"qualified_for_product\":false"
                    << "}\nR4_VERDICT="
                    << (valid ? "PASS_SCOPED_CAPTURE_SOURCE"
                              : "FAIL_SOURCE_LIFECYCLE_OR_PIXELS") << '\n';
                app.exit(valid ? 0 : 25);
            });
    };

    dispatchNext = [&] {
        if (completed >= kPhases) {
            if (hiddenRequestPending || hiddenTerminalFrames)
                return;
            hiddenRequestPending = true;
            window.hide();
            QTimer::singleShot(80, &app, [&] {
                hyremote::CaptureRequest hidden;
                hidden.id = 777;
                hidden.requestTime = hyremote::Clock::now();
                if (!source->requestFrame(hidden)) {
                    ++errors;
                    app.exit(30);
                }
            });
            return;
        }
        if (completed == 6 && restarts == 1) {
            intentionallyStopped = true;
            source->stop();
            ++restarts;
            QTimer::singleShot(70, &app, [&] {
                intentionallyStopped = false;
                startSource();
                QTimer::singleShot(50, &app, dispatchNext);
            });
            return;
        }
        if (resizeScenario && !resized && completed == 9) {
            resized = true;
            currentW = kResizedW;
            currentH = kResizedH;
            // Both the QQuickWindow swapchain and QSG content must be
            // updated. Capture of a previous framebuffer size must fail,
            // rather than being published as the new Surface generation.
            window.resize(currentW, currentH);
            scene.setSceneSize(currentW, currentH);
            QTimer::singleShot(110, &app, dispatchNext);
            return;
        }
        desired = completed;
        scene.setPhase(desired);
        hyremote::CaptureRequest request;
        request.id = static_cast<hyremote::CaptureRequestId>(100 + desired);
        request.requestTime = hyremote::Clock::now();
        if (!source->requestFrame(request)) {
            ++errors;
            std::cerr << "R4=FAIL_REQUEST_REJECTED\n";
            app.exit(26);
        }
    };

    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &app, [&] {
        ++errors;
        std::cerr << "R4_VERDICT=FAIL_TIMEOUT completed=" << completed << '\n';
        app.exit(27);
    });

    QTimer::singleShot(250, &app, [&] {
        startSource();
        // Explicitly stop before the queued GUI dispatcher has executed.
        // A stale posted-token must NOT suppress the next generation's
        // first real CaptureRequest. Any old ID=999 frame is forbidden.
        hyremote::CaptureRequest abandoned;
        abandoned.id = 999;
        abandoned.requestTime = hyremote::Clock::now();
        if (!source->requestFrame(abandoned)) {
            ++errors;
            app.exit(28);
            return;
        }
        intentionallyStopped = true;
        source->stop();
        ++restarts;
        intentionallyStopped = false;
        startSource();
        dispatchNext();
    });
    watchdog.start(25000);
    return app.exec();
}
