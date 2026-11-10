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
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
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
    explicit Quadrants(QQuickItem *parent) : QQuickItem(parent)
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
        return node;
    }
private:
    int m_stage = 0;
};

struct State
{
    std::atomic<int> outstandingStage{-1};
    std::atomic<int> responseStage{-1};
    std::atomic<int> responseStatus{0}; // 0 waiting, 1 good, 2 wrong, 3 empty
    std::atomic<int> submitted{0};
    std::atomic<int> completed{0};
    std::atomic<int> callbackWrongThread{0};
    std::atomic<int> rejectedNoDemand{0};
    std::atomic<int> missingResource{0};
    std::atomic<int> invalidSwapchainFlags{0};
    std::atomic<int> asyncDataBytes{0};
    std::atomic<int> actualRenderCallbacks{0};
    std::atomic<int> noViewerFrames{0};
    std::atomic<bool> viewerDemand{true};
    std::atomic<bool> alive{true};
    std::atomic<int> lastCallbackMs{0};
};
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    int width = 640, height = 360;
    for (int i=1; i<argc; ++i) {
        const QString arg=QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--width="))
            width = arg.mid(8).toInt();
        else if (arg.startsWith("--height="))
            height = arg.mid(9).toInt();
        else {
            std::cerr << "R3_VERDICT=BLOCKED_UNKNOWN_ARG\n";
            return 11;
        }
    }
    if (width<128 || height<128 || width>3840 || height>2160)
        return 12;
    // The first stage is D3D11 ONLY. Other QRhi backends must qualify on
    // their own Kit and render loop, without assuming transfer-source flags.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickWindow window;
    window.resize(width, height);
    window.setColor(Qt::black);
    Quadrants scene(window.contentItem());
    scene.setSize(QSizeF(width, height));
    auto state = std::make_shared<State>();
    int requested = -1;
    int verified = 0;
    bool finished = false;
    std::chrono::steady_clock::time_point requestStart{};
    int nextStage = 0;
    int framesWaited = 0;
    qint64 startEpochMs = QDateTime::currentMSecsSinceEpoch();
    auto renderThread = &app;

    QObject::connect(&window, &QQuickWindow::afterRendering, &window,
       [&, state] {
        state->actualRenderCallbacks.fetch_add(1);
        if (!state->alive.load())
            return;
        if (!state->viewerDemand.load()) {
            state->noViewerFrames.fetch_add(1);
            return;
        }
        const int stage = state->outstandingStage.load();
        if (stage<0 || state->submitted.load()>state->completed.load())
            return;
        auto *ri = window.rendererInterface();
        if (!ri || ri->graphicsApi()!=QSGRendererInterface::Direct3D11) {
            state->missingResource.fetch_add(1);
            return;
        }
        auto *rhi = static_cast<QRhi *>(
            ri->getResource(&window, QSGRendererInterface::RhiResource));
        auto *sc = window.swapChain();
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
        auto *result = new QRhiReadbackResult;
        auto begin = std::chrono::steady_clock::now();
        result->completed = [state, result, stage, begin, width, height, renderThread]() {
            const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-begin).count();
            state->lastCallbackMs.store(static_cast<int>(took));
            if (QThread::currentThread()!=renderThread->thread())
                state->callbackWrongThread.fetch_add(1); // expected: RHI thread != GUI
            const int bytes = result->data.size();
            state->asyncDataBytes.store(bytes);
            bool good = result->pixelSize==QSize(width,height)
                && bytes == width*height*4;
            bool topDown = false, bottomUp = false;
            if (good) {
                QImage img(reinterpret_cast<const uchar *>(result->data.constData()),
                           width,height,width*4,QImage::Format_RGBA8888);
                topDown = true;
                bottomUp = true;
                for (int q=0;q<4;++q) {
                    const int x=(q%2 ? width*3/4 : width/4);
                    const int y=(q/2 ? height*3/4 : height/4);
                    topDown &= near(img.pixelColor(x,y),colorFor(stage,q));
                    bottomUp &= near(img.pixelColor(x,height-1-y),colorFor(stage,q));
                }
            }
            state->responseStage.store(stage);
            state->responseStatus.store(!good ? 3 : (topDown || bottomUp ? 1 : 2));
            state->completed.fetch_add(1);
            delete result;
        };
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
        std::cout<<"R3_RESULT_JSON={"
                 <<"\"qt\":\""<<QT_VERSION_STR<<"\","
                 <<"\"backend\":\"d3d11\","
                 <<"\"size\":["<<width<<","<<height<<"],"
                 <<"\"stages_expected\":"<<kStages<<","
                 <<"\"stages_verified\":"<<verified<<","
                 <<"\"async_readbacks_submitted\":"<<state->submitted.load()<<","
                 <<"\"async_readbacks_completed\":"<<state->completed.load()<<","
                 <<"\"last_callback_latency_ms\":"<<state->lastCallbackMs.load()<<","
                 <<"\"callback_bytes\":"<<state->asyncDataBytes.load()<<","
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
                } else if(status>1) {
                    quit(false, status==2
                       ? "FAIL_ASYNC_GPU_READBACK_WRONG_QUADRANT_PIXELS"
                       : "FAIL_ASYNC_GPU_READBACK_EMPTY_OR_SIZE");
                }
            } else {
                // QRhi readbacks can require subsequent submitted frames;
                // such frames are test-only completion plumbing, NOT an
                // acceptable production static-CPU behavior.
                if (++framesWaited%4==0)
                    window.update();
            }
            return;
        }
        requested=nextStage;
        framesWaited=0;
        state->outstandingStage.store(requested);
        scene.setStage(requested);
    });
    window.show();
    guard.start(30000);
    poll.start();
    QTimer::singleShot(200,&app,[&] {
        if(requested<0 && !finished) {
            requested=nextStage;
            state->outstandingStage.store(requested);
            scene.setStage(requested);
        }
    });
    return app.exec();
}
