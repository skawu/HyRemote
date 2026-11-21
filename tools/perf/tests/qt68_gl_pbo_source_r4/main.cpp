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
#include <thread>
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
    bool destroyWindowScenario = false;
    bool quitActiveScenario = false;
    bool quitConcurrentScenario = false;
    bool stopRetryLiveScenario = false;
#if defined(HYREMOTE_R4_D3D11)
    bool dispatchRejectionScenario = false;
#else
    bool rebuildScenario = false;
#endif
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--resize")
            resizeScenario = true;
        else if (std::string(argv[i]) == "--window-destroy")
            destroyWindowScenario = true;
        else if (std::string(argv[i]) == "--quit-active")
            quitActiveScenario = true;
        else if (std::string(argv[i]) == "--quit-concurrent")
            quitConcurrentScenario = true;
        else if (std::string(argv[i]) == "--stop-retry-live")
            stopRetryLiveScenario = true;
#if defined(HYREMOTE_R4_D3D11)
        else if (std::string(argv[i]) == "--dispatch-rejection")
            dispatchRejectionScenario = true;
#else
        else if (std::string(argv[i]) == "--rebuild-scene-graph")
            rebuildScenario = true;
#endif
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
    if (dispatchRejectionScenario) {
        const bool ok = HyRemote::detail::runQuickD3d11DispatchRejectionRegression();
        std::cout << "R4_D3D11_DISPATCH_REJECTION=" << (ok ? "PASS" : "FAIL") << '\n';
        return ok ? 0 : 34;
    }
#else
    if (qEnvironmentVariable("QSG_RHI_BACKEND") != QStringLiteral("opengl")) {
        std::cerr << "R4=BLOCKED_NOT_OPENGL_RHI\n";
        return 10;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#else
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::OpenGL);
#endif
#endif
    if (destroyWindowScenario || quitActiveScenario || quitConcurrentScenario
        || stopRetryLiveScenario) {
        // Real QQuickWindow/renderer, not a source-only state mock. Deleting
        // a live target or quitting the active Qt event loop must quiesce the
        // source BEFORE the Qt GUI/platform integration becomes unavailable.
        app.setQuitOnLastWindowClosed(false);
        auto dyingWindow = std::make_unique<QQuickWindow>();
        dyingWindow->resize(kW, kH);
        dyingWindow->setColor(Qt::black);
        auto *dyingScene = new RealScene(dyingWindow->contentItem());
        dyingScene->setPhase(0);
#if defined(HYREMOTE_R4_D3D11)
        auto dyingSource =
            HyRemote::detail::createExperimentalQuickD3d11StagingCaptureSource(
                dyingWindow.get());
#else
        auto dyingSource =
            HyRemote::detail::createExperimentalQuickGlPboCaptureSource(
                dyingWindow.get());
#endif
        if (!dyingSource) {
            std::cerr << "R4_LIFECYCLE=FAIL_NO_SOURCE\n";
            return 41;
        }

        int delivered = 0;
        int unexpected = 0;
        bool closing = false;
        bool lateRequestRejected = false;
        bool quitHookComplete = false;
        bool resourcesQuiescentOnDelete = true;
        bool stopPostRaceObserved = false;
        bool liveStopRecovered = false;
        HyRemote::detail::QuickNativeStopPostEvidence liveStopBefore;
        QTimer liveStopPoll;
        liveStopPoll.setInterval(10);
        std::thread concurrentStop;
        std::atomic<bool> stopEntered{false};
        std::atomic<bool> stopReturned{false};
        QObject::connect(&liveStopPoll, &QTimer::timeout, &app, [&] {
            if (!stopReturned.load(std::memory_order_acquire))
                return;
            liveStopPoll.stop();
#if defined(HYREMOTE_R4_D3D11)
            const auto after = HyRemote::detail::quickD3d11StopPostEvidence();
#else
            const auto after = HyRemote::detail::quickGlPboStopPostEvidence();
#endif
            liveStopRecovered = after.failures > liveStopBefore.failures
                && after.accepted > liveStopBefore.accepted;
            if (!liveStopRecovered)
                ++unexpected;
            // This is NOT a shutdown-rescued stop: Qt was allowed to service
            // the accepted queued stop before we request app termination.
            app.quit();
        });
        const bool started = dyingSource->start(
            [&](hyremote::RemoteFrame frame) {
                if (closing || !frame.storage || frame.requestId != 991
                    || !validateFrame(frame, 0, kW, kH)) {
                    ++unexpected;
                    return;
                }
                ++delivered;
                closing = true;
                QTimer::singleShot(0, &app, [&] {
                    if (destroyWindowScenario) {
                        // The window's destroyed() hook must drain while
                        // a still-started Source retains a real GPU allocation.
                        dyingWindow.reset();
#if !defined(HYREMOTE_R4_D3D11)
                        // Assert BEFORE an explicit stop() could conceal a
                        // broken destroyed() hook. Real GL PBO/fence/worker
                        // allocations must already have retired here.
                        const auto counters =
                            HyRemote::detail::quickGlPboResourceCounts();
                        resourcesQuiescentOnDelete =
                            counters.buffersCreated > 0
                            && counters.buffersCreated == counters.buffersDeleted
                            && counters.workersCreated == counters.workersRetired;
#endif
                        hyremote::CaptureRequest late;
                        late.id = 992;
                        late.requestTime = hyremote::Clock::now();
                        lateRequestRejected = !dyingSource->requestFrame(late);
                        dyingSource->stop(); // idempotence after target loss
                    } else {
                        if (stopRetryLiveScenario) {
#if defined(HYREMOTE_R4_D3D11)
                            liveStopBefore = HyRemote::detail::quickD3d11StopPostEvidence();
                            HyRemote::detail::quickD3d11RejectNextStopPostForTest(1);
#else
                            liveStopBefore = HyRemote::detail::quickGlPboStopPostEvidence();
                            HyRemote::detail::quickGlPboRejectNextStopPostForTest(1);
#endif
                            concurrentStop = std::thread([&] {
                                stopEntered.store(true, std::memory_order_release);
                                dyingSource->stop();
                                stopReturned.store(true, std::memory_order_release);
                            });
                            liveStopPoll.start();
                            return;
                        }
                        // Compete a Core worker stop against the final GUI
                        // aboutToQuit emission. A Qt BlockingQueued invoke
                        // could remain forever queued as the eventloop exits;
                        // the State CV must instead observe the direct hook.
                        if (quitConcurrentScenario) {
                            // Inject ONE transient rejected Qt stop post, then
                            // require a real accepted provider-originated
                            // queued invoke while the GUI loop is still busy
                            // in this callback. app.quit() may then cancel
                            // that queued event; the synchronous Qt lifecycle
                            // hook, not the queued event, must drain workers.
#if defined(HYREMOTE_R4_D3D11)
                            const auto before = HyRemote::detail::quickD3d11StopPostEvidence();
                            HyRemote::detail::quickD3d11RejectNextStopPostForTest(1);
#else
                            const auto before = HyRemote::detail::quickGlPboStopPostEvidence();
                            HyRemote::detail::quickGlPboRejectNextStopPostForTest(1);
#endif
                            concurrentStop = std::thread([&] {
                                stopEntered.store(true, std::memory_order_release);
                                dyingSource->stop();
                                stopReturned.store(true, std::memory_order_release);
                            });
                            const auto deadline = std::chrono::steady_clock::now()
                                + std::chrono::seconds(3);
                            while (std::chrono::steady_clock::now() < deadline) {
#if defined(HYREMOTE_R4_D3D11)
                                const auto after =
                                    HyRemote::detail::quickD3d11StopPostEvidence();
#else
                                const auto after =
                                    HyRemote::detail::quickGlPboStopPostEvidence();
#endif
                                if (after.failures > before.failures
                                    && after.accepted > before.accepted) {
                                    stopPostRaceObserved = true;
                                    break;
                                }
                                std::this_thread::yield();
                            }
                            if (!stopPostRaceObserved)
                                ++unexpected;
                        }
                        app.quit();
                    }
                    if (destroyWindowScenario) {
                        QTimer::singleShot(100, &app, [&] {
                            const bool pass = delivered == 1 && unexpected == 0
                                && lateRequestRejected && resourcesQuiescentOnDelete;
                            std::cout << "R4_LIFECYCLE="
                                      << (pass ? "PASS_WINDOW_DESTROY"
                                               : "FAIL_WINDOW_DESTROY") << '\n';
                            app.exit(pass ? 0 : 42);
                        });
                    }
                });
            },
            [&](const hyremote::CaptureEvent &) {
                ++unexpected;
            });
        if (!started) {
            std::cerr << "R4_LIFECYCLE=FAIL_START\n";
            return 43;
        }
        if (quitActiveScenario || quitConcurrentScenario || stopRetryLiveScenario) {
            QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
                hyremote::CaptureRequest late;
                late.id = 993;
                late.requestTime = hyremote::Clock::now();
                quitHookComplete = !dyingSource->requestFrame(late);
            }, Qt::DirectConnection);
        }
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &app, [&] {
            ++unexpected;
            app.exit(44);
        });
        guard.start(15000);
        dyingWindow->show();
        QTimer::singleShot(125, &app, [&] {
            hyremote::CaptureRequest request;
            request.id = 991;
            request.requestTime = hyremote::Clock::now();
            if (!dyingSource->requestFrame(request)) {
                ++unexpected;
                app.exit(45);
            }
        });
        const int result = app.exec();
        if (quitConcurrentScenario || stopRetryLiveScenario) {
            if (concurrentStop.joinable())
                concurrentStop.join();
        } else if (quitActiveScenario) {
            // Even with no GUI event loop left, a Core worker's repeated
            // stop() MUST return instead of posting a blocking Qt event.
            std::thread postLoopStop([&] { dyingSource->stop(); });
            postLoopStop.join();
        } else {
            dyingSource->stop();
        }
        if (quitActiveScenario || quitConcurrentScenario || stopRetryLiveScenario) {
            const bool pass = result == 0 && delivered == 1
                && unexpected == 0 && quitHookComplete
                && (!quitConcurrentScenario
                    || (stopPostRaceObserved
                        && stopEntered.load(std::memory_order_acquire)
                        && stopReturned.load(std::memory_order_acquire)))
                && (!stopRetryLiveScenario
                    || (liveStopRecovered
                        && stopEntered.load(std::memory_order_acquire)
                        && stopReturned.load(std::memory_order_acquire)));
            std::cout << "R4_LIFECYCLE="
                      << (stopRetryLiveScenario
                            ? (pass ? "PASS_STOP_RETRY_LIVE" : "FAIL_STOP_RETRY_LIVE")
                            : quitConcurrentScenario
                                ? (pass ? "PASS_QUIT_CONCURRENT" : "FAIL_QUIT_CONCURRENT")
                                : (pass ? "PASS_QUIT_ACTIVE" : "FAIL_QUIT_ACTIVE"))
                      << '\n';
            return pass ? 0 : 46;
        }
        return result;
    }

    QQuickWindow window;
    app.setQuitOnLastWindowClosed(false);
    window.resize(kW, kH);
    window.setColor(Qt::black);
    RealScene scene(window.contentItem());
    scene.setPhase(0);

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
#if !defined(HYREMOTE_R4_D3D11)
    bool pboCleanupWhileWindowAlive = false;
    bool rebuildRequested = false;
    bool rebuildVerified = false;
    int revokedEvents = 0;
    int revokedFrames = 0;
    std::atomic<int> graphInitializations{0};
    std::atomic<int> graphInvalidations{0};
    QMetaObject::Connection graphInitialized;
    QMetaObject::Connection graphInvalidated;
    if (rebuildScenario) {
        window.setPersistentSceneGraph(false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        window.setPersistentGraphics(false);
#else
        window.setPersistentOpenGLContext(false);
#endif
        graphInitialized = QObject::connect(
            &window, &QQuickWindow::sceneGraphInitialized, &window,
            [&] { ++graphInitializations; }, Qt::DirectConnection);
        graphInvalidated = QObject::connect(
            &window, &QQuickWindow::sceneGraphInvalidated, &window,
            [&] { ++graphInvalidations; }, Qt::DirectConnection);
    }
#endif

    std::function<void()> dispatchNext;
    std::function<void()> finishCase;
    const auto onFrame = [&](hyremote::RemoteFrame frame) {
#if !defined(HYREMOTE_R4_D3D11)
        if (rebuildRequested && frame.requestId && *frame.requestId == 888) {
            if (rebuildVerified || frame.storage || revokedEvents != 1 || ++revokedFrames != 1) {
                ++errors;
                app.exit(31);
            }
            return;
        }
#endif
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
        if (!validateFrame(frame, desired, currentW, currentH)
            || *frame.requestId != static_cast<hyremote::CaptureRequestId>(100 + desired)) {
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
#if !defined(HYREMOTE_R4_D3D11)
        if (rebuildRequested && !rebuildVerified && event.recoverable
            && event.message == "Qt GL Scene Graph surface invalidated") {
            ++revokedEvents;
            return;
        }
#endif
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
                    && hiddenUnavailableEvents == 1 && hiddenTerminalFrames == 1
#if !defined(HYREMOTE_R4_D3D11)
                    && pboCleanupWhileWindowAlive
                    && (!rebuildScenario || (rebuildVerified && revokedFrames == 1))
#endif
                    ;
                std::cout << "R4_RESULT_JSON={"
                    << "\"qt\":\"" << QT_VERSION_STR << "\","
#if defined(HYREMOTE_R4_D3D11)
                    << "\"backend\":\"Qt6_RHI_Direct3D11\","
#elif QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                    << "\"backend\":\"Qt6_RHI_OpenGL\","
#else
                    << "\"backend\":\"Qt5_OpenGL\","
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
#if !defined(HYREMOTE_R4_D3D11)
                    << "\"pbo_cleanup_while_window_alive\":"
                    << (pboCleanupWhileWindowAlive ? "true" : "false") << ","
                    << "\"scene_graph_rebuilt\":"
                    << (rebuildVerified ? "true" : "false") << ","
#endif
                    << "\"errors\":" << errors << ","
                    << "\"qualified_for_product\":false"
                    << "}\nR4_VERDICT="
                    << (valid ? "PASS_SCOPED_CAPTURE_SOURCE"
                              : "FAIL_SOURCE_LIFECYCLE_OR_PIXELS") << '\n';
                app.exit(valid ? 0 : 25);
            });
    };

    dispatchNext = [&] {
#if !defined(HYREMOTE_R4_D3D11)
        if (rebuildScenario && completed == 4 && !rebuildVerified) {
            if (!rebuildRequested) {
                rebuildRequested = true;
                hyremote::CaptureRequest abandoned;
                abandoned.id = 888;
                abandoned.requestTime = hyremote::Clock::now();
                if (!source->requestFrame(abandoned)) {
                    app.exit(32);
                    return;
                }
                window.hide();
                window.releaseResources();
                window.destroy();
            }
            if (graphInvalidations.load() == 0 || revokedFrames == 0) {
                QTimer::singleShot(20, &app, dispatchNext);
                return;
            }
            if (!window.isVisible()) {
                const auto retired = HyRemote::detail::quickGlPboResourceCounts();
                if (retired.buffersCreated == 0
                    || retired.buffersDeleted != retired.buffersCreated
                    || retired.workersRetired != retired.workersCreated) {
                    QTimer::singleShot(20, &app, dispatchNext);
                    return;
                }
                window.show();
            }
            if (graphInitializations.load() < 2) {
                QTimer::singleShot(20, &app, dispatchNext);
                return;
            }
            rebuildVerified = true;
        }
#endif
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
#if !defined(HYREMOTE_R4_D3D11)
            // Assert immediately: the application window remains visible,
            // and Qt has not rendered a retirement frame after stop().
            const auto resources = HyRemote::detail::quickGlPboResourceCounts();
            pboCleanupWhileWindowAlive = window.isVisible()
                && resources.buffersCreated > 0
                && resources.buffersCreated == resources.buffersDeleted
                && resources.workersCreated == resources.workersRetired;
            if (rebuildScenario)
                pboCleanupWhileWindowAlive = pboCleanupWhileWindowAlive
                    && resources.buffersCreated >= 2 && resources.workersCreated >= 2;
#endif
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
        std::cerr << "R4_VERDICT=FAIL_TIMEOUT completed=" << completed
#if !defined(HYREMOTE_R4_D3D11)
                  << " graph_init=" << graphInitializations.load()
                  << " graph_invalid=" << graphInvalidations.load()
#endif
                  << '\n';
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
    window.show();
    watchdog.start(25000);
    const int result = app.exec();
#if !defined(HYREMOTE_R4_D3D11)
    QObject::disconnect(graphInitialized);
    QObject::disconnect(graphInvalidated);
#endif
    return result;
}
