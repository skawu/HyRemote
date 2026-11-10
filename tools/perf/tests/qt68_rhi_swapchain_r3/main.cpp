// EXPERIMENT R3 ONLY. Never ship this QtGuiPrivate code in HyRemote.
// Independent Qt 6.8.3 QtQuick D3D11 readback of the ACTUAL rendered
// on-screen swapchain, without grabToImage, glReadPixels or an extra pass.
//
// Qt6 SceneGraph afterRendering: scenegraph passes RECORDED, but frame NOT
// submitted. QRhiSwapChain::currentFrameCommandBuffer() belongs to the active
// QRhi::beginFrame/endFrame. resourceUpdate() records an asynchronous copy.
// QRhiReadbackResult::completed must produce real 4-quadrant pixels before
// this experiment can report success. Zero-viewer and 1-request controls.
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include <rhi/qrhi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <string>

namespace {
constexpr int kStages = 12;
QColor colorFor(int stage, int quadrant)
{
    switch ((stage + quadrant) % 4) {
    case 0: return QColor(225, 42, 52);
    case 1: return QColor(34, 205, 68);
    case 2: return QColor(42, 70, 230);
    default: return QColor(214, 198, 38);
    }
}
bool near(const QColor &actual, const QColor &expected)
{
    return std::abs(actual.red() - expected.red()) < 16
        && std::abs(actual.green() - expected.green()) < 16
        && std::abs(actual.blue() - expected.blue()) < 16;
}
class Quadrants final : public QQuickItem
{
public:
    explicit Quadrants(QQuickItem *parent, std::atomic<int> *syncedStage,
                       std::atomic<int> *syncedWidth, std::atomic<int> *syncedHeight)
        : QQuickItem(parent), m_syncedStage(syncedStage),
          m_syncedWidth(syncedWidth), m_syncedHeight(syncedHeight)
    {
        setFlag(ItemHasContents, true);
    }
    void setStage(int stage)
    {
        m_stage = stage;
        update();
    }
protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override
    {
        if (!node) {
            node = new QSGNode;
            for (int i=0; i<4; ++i) {
                auto *q = new QSGGeometryNode;
                auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                q->setGeometry(g);
                q->setFlag(QSGNode::OwnsGeometry);
                auto *m = new QSGFlatColorMaterial;
                q->setMaterial(m);
                q->setFlag(QSGNode::OwnsMaterial);
                node->appendChildNode(q);
            }
        }
        auto *child = node->firstChild();
        for (int i=0; i<4 && child; ++i, child=child->nextSibling()) {
            auto *q = static_cast<QSGGeometryNode *>(child);
            const qreal halfWidth = width() / 2.;
            const qreal halfHeight = height() / 2.;
            const QRectF r((i % 2) * halfWidth, (i / 2) * halfHeight,
                           halfWidth, halfHeight);
            QSGGeometry::updateRectGeometry(q->geometry(), r);
            q->geometry()->markVertexDataDirty();
            q->markDirty(QSGNode::DirtyGeometry);
            static_cast<QSGFlatColorMaterial *>(q->material())
                ->setColor(colorFor(m_stage, i));
            q->markDirty(QSGNode::DirtyMaterial);
        }
        // Executed by Qt's QSG synchronization, not just GUI-side
        // QQuickItem::setStage(). A grab may only represent this generation
        // after all scenegraph geometry/materials have been updated.
        m_syncedWidth->store(static_cast<int>(width()), std::memory_order_release);
        m_syncedHeight->store(static_cast<int>(height()), std::memory_order_release);
        m_syncedStage->store(m_stage, std::memory_order_release);
        return node;
    }
private:
    int m_stage = 0;
    std::atomic<int> *m_syncedStage;
    std::atomic<int> *m_syncedWidth;
    std::atomic<int> *m_syncedHeight;
};

struct State
{
    std::atomic<int> outstandingStage{-1};
    // A callback may complete before GUI processes that stage. No second
    // readback for the SAME epoch, even if RHI renders another frame.
    std::atomic<int> lastSubmittedStage{-1};
    std::atomic<int> responseStage{-1};
    std::atomic<int> responseStatus{0}; // 0 waiting, 1 good, 2 wrong, 3 empty
    std::atomic<int> submitted{0};
    std::atomic<int> completed{0};
    std::atomic<int> callbackWrongThread{0};
    std::atomic<int> rejectedNoDemand{0};
    std::atomic<int> missingResource{0};
    std::atomic<int> invalidSwapchainFlags{0};
    std::atomic<int> asyncDataBytes{0};
    std::atomic<int> intendedWidth{0};
    std::atomic<int> intendedHeight{0};
    std::atomic<int> resizeRecreateCount{0};
    std::atomic<int> skippedUnsyncedSceneFrames{0};
    std::atomic<int> callbackPixelWidth{-1};
    std::atomic<int> callbackPixelHeight{-1};
    std::atomic<int> callbackFormat{-1};
    std::atomic<int> actualRenderCallbacks{0};
    std::atomic<bool> renderThreadDistinctFromGui{false};
    std::mutex latencyMutex;
    std::vector<int> asyncCallbackLatenciesMs;
    std::atomic<int> noViewerFrames{0};
    std::atomic<int> completionPumpRequests{0};
    std::atomic<int> maxAsyncReadbackFrames{-1};
    std::atomic<int> completionPumpBudgetExceeded{0};
    std::atomic<bool> viewerDemand{true};
    std::atomic<bool> alive{true};
    std::atomic<int> lastCallbackMs{0};
};
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    int width = 640, height = 360;
    int resizeWidth = 0, resizeHeight = 0;
    // -1 retains legacy behavior; >=0 caps artificial frames per source.
    int pumpBudget = -1;
#ifdef _WIN32
    QString backend = QStringLiteral("d3d11");
#else
    QString backend = QStringLiteral("opengl");
#endif
    for (int i=1; i<argc; ++i) {
        const QString arg=QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--backend="))
            backend = arg.mid(10);
        else if (arg.startsWith("--width="))
            width = arg.mid(8).toInt();
        else if (arg.startsWith("--height="))
            height = arg.mid(9).toInt();
        else if (arg.startsWith("--resize-width="))
            resizeWidth = arg.mid(15).toInt();
        else if (arg.startsWith("--resize-height="))
            resizeHeight = arg.mid(16).toInt();
        else if (arg.startsWith("--pump-budget="))
            pumpBudget = arg.mid(14).toInt();
        else {
            std::cerr << "R3_VERDICT=BLOCKED_UNKNOWN_ARG\n";
            return 11;
        }
    }
    if (backend != QLatin1String("d3d11") && backend != QLatin1String("opengl"))
        return 13;
    if (width<128 || height<128 || width>3840 || height>2160
        || ((resizeWidth == 0) != (resizeHeight == 0))
        || resizeWidth < 0 || resizeHeight < 0
        || resizeWidth > 3840 || resizeHeight > 2160
        || (resizeWidth != 0 && (resizeWidth < 128 || resizeHeight < 128))
        || pumpBudget < -1 || pumpBudget > 8)
        return 12;
    // Each backend separately qualifies with its exact Qt6 Kit.
    // Do not infer transfer-source support from OS, GPU vendor or RHI name.
    const auto graphicsApi = backend == QLatin1String("d3d11")
        ? QSGRendererInterface::Direct3D11 : QSGRendererInterface::OpenGL;
    QQuickWindow::setGraphicsApi(graphicsApi);
    QQuickWindow window;
    // Hosted Windows virtual desktop is only 1024x768. A decorated Win32
    // window requested at 1920x1080 was resized by the OS to 1028x749.
    // Use a borderless, positioned research surface to make the requested
    // render-target pixel size an actual testable condition, not a guess.
    window.setFlags(Qt::FramelessWindowHint);
    window.setPosition(0, 0);
    window.resize(width, height);
    window.setColor(Qt::black);
    std::atomic<int> synchronizedStage{-1};
    std::atomic<int> synchronizedWidth{-1};
    std::atomic<int> synchronizedHeight{-1};
    Quadrants scene(window.contentItem(), &synchronizedStage,
                    &synchronizedWidth, &synchronizedHeight);
    scene.setSize(QSizeF(width, height));
    auto state = std::make_shared<State>();
    state->intendedWidth.store(width);
    state->intendedHeight.store(height);
    int requested = -1;
    int verified = 0;
    bool finished = false;
    std::chrono::steady_clock::time_point requestStart{};
    int nextStage = 0;
    int framesWaited = 0;
    int completionPumpCallsThisStage = 0;
    qint64 stageStartedUtcMs = -1;
    qint64 startEpochMs = QDateTime::currentMSecsSinceEpoch();
    auto renderThread = &app; // for logging; actual callback is on QRhi thread

    QObject::connect(&window, &QQuickWindow::afterRendering, &window,
       [&, state] {
        state->actualRenderCallbacks.fetch_add(1);
        if (QThread::currentThread() != app.thread())
            state->renderThreadDistinctFromGui.store(true);
        if (!state->alive.load())
            return;
        if (!state->viewerDemand.load()) {
            state->noViewerFrames.fetch_add(1);
            return;
        }
        const int stage = state->outstandingStage.load();
        if (stage<0 || state->lastSubmittedStage.load()==stage
            || state->submitted.load()>state->completed.load())
            return;
        // A new window swapchain extent may already exist before the item's
        // QSG geometry/materials are synchronized to its new generation.
        // Reject such intermediate frames instead of publishing old color.
        const int requiredW = state->intendedWidth.load();
        const int requiredH = state->intendedHeight.load();
        if (synchronizedStage.load(std::memory_order_acquire)!=stage
            || synchronizedWidth.load(std::memory_order_acquire)!=requiredW
            || synchronizedHeight.load(std::memory_order_acquire)!=requiredH) {
            state->skippedUnsyncedSceneFrames.fetch_add(1);
            return;
        }
        auto *ri = window.rendererInterface();
        if (!ri || ri->graphicsApi()!=graphicsApi) {
            state->missingResource.fetch_add(1);
            return;
        }
        auto *rhi = static_cast<QRhi *>(
            ri->getResource(&window, QSGRendererInterface::RhiResource));
        auto *sc = window.swapChain();
        if (rhi && state->maxAsyncReadbackFrames.load() < 0)
            state->maxAsyncReadbackFrames.store(
                rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames));
        if (!rhi || !sc) {
            state->missingResource.fetch_add(1);
            return;
        }
        if (!sc->flags().testFlag(QRhiSwapChain::UsedAsTransferSource)) {
            state->invalidSwapchainFlags.fetch_add(1);
            return;
        }
        auto *cb = sc->currentFrameCommandBuffer();
        if (!cb) {
            state->missingResource.fetch_add(1);
            return;
        }
        // A readback batch may be enqueued OUTSIDE renderpass here. In Qt6
        // afterRendering, scenegraph's renderpass has ended, but frame is
        // still open and its GPU commands have not yet been submitted.
        // The QRhi API owns/reclaims batch after resourceUpdate().
        const int expectedWidth = state->intendedWidth.load();
        const int expectedHeight = state->intendedHeight.load();
        auto *result = new QRhiReadbackResult;
        auto begin = std::chrono::steady_clock::now();
        result->completed = [state, result, stage, begin, expectedWidth, expectedHeight, renderThread]() {
            const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-begin).count();
            state->lastCallbackMs.store(static_cast<int>(took));
            {
                std::lock_guard<std::mutex> guard(state->latencyMutex);
                state->asyncCallbackLatenciesMs.push_back(static_cast<int>(took));
            }
            if (QThread::currentThread()!=renderThread->thread())
                state->callbackWrongThread.fetch_add(1); // expected: RHI thread != GUI
            const int bytes = result->data.size();
            state->asyncDataBytes.store(bytes);
            state->callbackPixelWidth.store(result->pixelSize.width());
            state->callbackPixelHeight.store(result->pixelSize.height());
            state->callbackFormat.store(static_cast<int>(result->format));
            bool good = result->pixelSize==QSize(expectedWidth,expectedHeight)
                && bytes == expectedWidth*expectedHeight*4;
            bool topDown = false, bottomUp = false;
            if (good) {
                QImage img(reinterpret_cast<const uchar *>(result->data.constData()),
                           expectedWidth,expectedHeight,expectedWidth*4,QImage::Format_RGBA8888);
                topDown = true;
                bottomUp = true;
                for (int q=0;q<4;++q) {
                    const int x=(q%2 ? expectedWidth*3/4 : expectedWidth/4);
                    const int y=(q/2 ? expectedHeight*3/4 : expectedHeight/4);
                    topDown &= near(img.pixelColor(x,y),colorFor(stage,q));
                    bottomUp &= near(img.pixelColor(x,expectedHeight-1-y),colorFor(stage,q));
                }
            }
            if (good && !topDown && !bottomUp) {
                QImage diagnostic(reinterpret_cast<const uchar *>(result->data.constData()),
                                  expectedWidth,expectedHeight,expectedWidth*4,QImage::Format_RGBA8888);
                std::cerr << "R3_BAD_SCENE_EPOCH stage=" << stage
                          << " size=" << expectedWidth << "x" << expectedHeight;
                for (int q=0; q<4; ++q) {
                    const int x=q%2?expectedWidth*3/4:expectedWidth/4;
                    const int y=q/2?expectedHeight*3/4:expectedHeight/4;
                    std::cerr << " q" << q << "_rgb="
                              << diagnostic.pixelColor(x,y).name().toStdString();
                }
                std::cerr << std::endl;
            }
            state->responseStage.store(stage);
            state->responseStatus.store(!good ? 3 : (topDown || bottomUp ? 1 : 2));
            state->completed.fetch_add(1);
            delete result;
        };
        state->lastSubmittedStage.store(stage);
        auto *batch = rhi->nextResourceUpdateBatch();
        QRhiReadbackDescription desc; // null texture means current transfer-source swapchain
        batch->readBackTexture(desc, result);
        cb->resourceUpdate(batch);
        state->submitted.fetch_add(1);
    }, Qt::DirectConnection);

    auto quit = [&](bool ok,const char *reason) {
        if(finished) return;
        finished=true;
        state->alive.store(false);
        std::vector<int> latencies;
        {
            std::lock_guard<std::mutex> guard(state->latencyMutex);
            latencies = state->asyncCallbackLatenciesMs;
        }
        std::sort(latencies.begin(), latencies.end());
        const int latencyP95 = latencies.empty() ? -1 : latencies[
            static_cast<std::size_t>(std::ceil(latencies.size()*0.95)-1)];
        std::cout<<"R3_RESULT_JSON={"
                 <<"\"qt\":\""<<QT_VERSION_STR<<"\","
                 <<"\"backend\":\""<<backend.toStdString()<<"\","
                 <<"\"size\":["<<width<<","<<height<<"],"
                 <<"\"final_expected_size\":["
                 <<state->intendedWidth.load()<<","
                 <<state->intendedHeight.load()<<"],"
                 <<"\"resize_recreate_count\":"<<state->resizeRecreateCount.load()<<","
                 <<"\"skipped_unsynced_scene_frames\":"
                 <<state->skippedUnsyncedSceneFrames.load()<<","
                 <<"\"last_qsg_synchronized_stage\":"<<synchronizedStage.load()<<","
                 <<"\"last_qsg_synchronized_geometry\":["
                 <<synchronizedWidth.load()<<","<<synchronizedHeight.load()<<"],"
                 <<"\"resized_during_test\":"
                 <<(resizeWidth>0?"true":"false")<<","
                 <<"\"stages_expected\":"<<kStages<<","
                 <<"\"stages_verified\":"<<verified<<","
                 <<"\"async_readbacks_submitted\":"<<state->submitted.load()<<","
                 <<"\"async_readbacks_completed\":"<<state->completed.load()<<","
                 <<"\"completion_pump_budget_per_stage\":"<<pumpBudget<<","
                 <<"\"completion_pump_requests\":"<<state->completionPumpRequests.load()<<","
                 <<"\"max_async_readback_frames\":"<<state->maxAsyncReadbackFrames.load()<<","
                 <<"\"completion_pump_budget_exceeded\":"<<state->completionPumpBudgetExceeded.load()<<","
                 <<"\"last_callback_latency_ms\":"<<state->lastCallbackMs.load()<<","
                 <<"\"callback_latency_p95_ms\":"<<latencyP95<<","
                 <<"\"render_loop\":\""
                 <<qEnvironmentVariable("QSG_RENDER_LOOP").toStdString()<<"\","
                 <<"\"render_thread_distinct_from_gui\":"
                 <<(state->renderThreadDistinctFromGui.load()?"true":"false")<<","
                 <<"\"callback_samples\":"<<latencies.size()<<","
                 <<"\"callback_bytes\":"<<state->asyncDataBytes.load()<<","
                 <<"\"callback_pixel_size\":["
                 <<state->callbackPixelWidth.load()<<","
                 <<state->callbackPixelHeight.load()<<"],"
                 <<"\"callback_texture_format\":"<<state->callbackFormat.load()<<","
                 <<"\"actual_qquickwindow_size\":["
                 <<window.width()<<","<<window.height()<<"],"
                 <<"\"actual_qquickwindow_dpr\":"<<window.effectiveDevicePixelRatio()<<","
                 <<"\"screen_geometry\":["
                 <<(window.screen()?window.screen()->geometry().width():0)<<","
                 <<(window.screen()?window.screen()->geometry().height():0)<<"],"
                 <<"\"missing_resource\":"<<state->missingResource.load()<<","
                 <<"\"missing_transfer_flag\":"<<state->invalidSwapchainFlags.load()<<","
                 <<"\"render_callbacks\":"<<state->actualRenderCallbacks.load()<<","
                 <<"\"no_viewer_render_callbacks\":"<<state->noViewerFrames.load()<<","
                 <<"\"last_response_status\":"<<state->responseStatus.load()<<","
                 <<"\"qt_private_research_only\":true,"
                 <<"\"qualified_for_product\":false,"
                 <<"\"elapsed_wall_ms\":"<<(QDateTime::currentMSecsSinceEpoch()-startEpochMs)
                 <<"}"<<std::endl;
        std::cout<<"R3_VERDICT="<<(ok?"PASS_SCOPED_D3D11_ASYNC_GPU_READBACK":reason)
                 <<std::endl;
        app.exit(ok?0:20);
    };

    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard,&QTimer::timeout,&app,[&] {
        quit(false,"FAIL_TIMEOUT_OR_NO_READBACK_CALLBACK");
    });

    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll,&QTimer::timeout,&app,[&] {
        if(finished) return;
        if (state->invalidSwapchainFlags.load()>0) {
            quit(false,"BLOCKED_DEFAULT_SWAPCHAIN_NOT_TRANSFER_SOURCE");
            return;
        }
        if (state->missingResource.load()>10) {
            quit(false,"BLOCKED_QT_RENDER_COMMAND_BUFFER_NOT_AVAILABLE");
            return;
        }
        if (nextStage == kStages) {
            if (state->viewerDemand.exchange(false)) {
                state->outstandingStage.store(-1);
                scene.setStage(kStages);
                window.update(); // source still renders, must NOT readback
                QTimer::singleShot(250,&app,[&] {
                    quit(verified==kStages && state->submitted.load()==kStages
                             && state->completed.load()==kStages
                             && state->noViewerFrames.load()>=1,
                         "FAIL_READBACK_COUNT_OR_ZERO_VIEWER");
                });
            }
            return;
        }
        if(requested >= 0) {
            if(state->responseStage.load()==requested) {
                const int status=state->responseStatus.exchange(0);
                if(status==1) {
                    ++verified;
                    requested=-1;
                    state->outstandingStage.store(-1);
                    state->responseStage.store(-1);
                    ++nextStage;
                    if (nextStage == kStages/2 && resizeWidth > 0) {
                        // No asynchronous copy outstanding: this boundary
                        // is safe to reconfigure the visible QtQuick surface.
                        // Every next copy must target the NEW pixel extent.
                        state->intendedWidth.store(resizeWidth);
                        state->intendedHeight.store(resizeHeight);
                        window.resize(resizeWidth, resizeHeight);
                        scene.setSize(QSizeF(resizeWidth, resizeHeight));
                        state->resizeRecreateCount.fetch_add(1);
                    }
                } else if(status>1) {
                    quit(false, status==2
                       ? "FAIL_ASYNC_GPU_READBACK_WRONG_QUADRANT_PIXELS"
                       : "FAIL_ASYNC_GPU_READBACK_EMPTY_OR_SIZE");
                }
            } else {
                // QRhi readbacks can require subsequent submitted frames;
                // such frames are test-only completion plumbing, NOT an
                // acceptable production static-CPU behavior.
                if (++framesWaited%4==0
                    && (pumpBudget<0 || completionPumpCallsThisStage<pumpBudget)) {
                    window.update();
                    ++completionPumpCallsThisStage;
                    state->completionPumpRequests.fetch_add(1);
                }
                if (pumpBudget>=0 && stageStartedUtcMs>=0
                    && QDateTime::currentMSecsSinceEpoch()-stageStartedUtcMs>1500) {
                    state->completionPumpBudgetExceeded.fetch_add(1);
                    quit(false, "BLOCKED_COMPLETION_REQUIRES_MORE_GPU_FRAMES");
                }
            }
            return;
        }
        requested=nextStage;
        framesWaited=0;
        completionPumpCallsThisStage=0;
        stageStartedUtcMs=QDateTime::currentMSecsSinceEpoch();
        state->outstandingStage.store(requested);
        scene.setStage(requested);
    });
    window.show();
    guard.start(30000);
    poll.start();
    QTimer::singleShot(200,&app,[&] {
        if(requested<0 && !finished) {
            requested=nextStage;
            completionPumpCallsThisStage=0;
            stageStartedUtcMs=QDateTime::currentMSecsSinceEpoch();
            state->outstandingStage.store(requested);
            scene.setStage(requested);
        }
    });
    return app.exec();
}
