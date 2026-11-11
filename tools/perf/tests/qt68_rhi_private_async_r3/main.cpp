// Qt6.8.3 R3 isolated PRIVATE-API research probe. NOT product code.
//
// As verified in qtdeclarative v6.8.3, both qsgthreadedrenderloop.cpp and
// qsgrenderloop.cpp create their visible QRhiSwapChain with
// QRhiSwapChain::UsedAsTransferSource. Qt's own QSGRhiSupport uses
// QRhiReadbackDescription(null swapchain) -> QRhiResourceUpdateBatch ->
// QRhiCommandBuffer::resourceUpdate() -> QRhi::finish() for grabWindow.
// This study deliberately does NOT call finish() or wait on the render thread.
// It checks whether a readBackTexture scheduled after Qt's normal scene pass
// eventually provides the correct submitted pixel asynchronously.
//
// QRhi is QtGuiPrivate, with limited ABI compatibility. No transfer to
// HyRemote Core, SDK, Generic Runtime, or production approval is implied.
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
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
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr int kStages = 15;
QColor stageColor(int phase)
{
    switch (phase % 3) {
    case 0: return QColor(224, 36, 52);
    case 1: return QColor(36, 208, 67);
    default: return QColor(46, 68, 235);
    }
}

bool matches(const QRhiReadbackResult &result, int phase)
{
    if (result.pixelSize.width() < 32 || result.pixelSize.height() < 32)
        return false;
    if (result.data.size() < qsizetype(result.pixelSize.width())
                                  * result.pixelSize.height() * 4)
        return false;
    if (result.format != QRhiTexture::RGBA8
        && result.format != QRhiTexture::BGRA8)
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(
        result.data.constData());
    const qsizetype offset = (
        qsizetype(result.pixelSize.height() / 2) * result.pixelSize.width()
        + result.pixelSize.width() / 2) * 4;
    const int red = result.format == QRhiTexture::BGRA8 ? p[offset+2] : p[offset];
    const int green = p[offset+1];
    const int blue = result.format == QRhiTexture::BGRA8 ? p[offset] : p[offset+2];
    const QColor expected = stageColor(phase);
    return std::abs(red - expected.red()) <= 16
           && std::abs(green - expected.green()) <= 16
           && std::abs(blue - expected.blue()) <= 16;
}

double p95(std::vector<double> values)
{
    if (values.empty())
        return -1;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1,
        static_cast<std::size_t>(std::ceil(values.size() * 0.95) - 1))];
}

class SourceItem final : public QQuickItem
{
public:
    SourceItem(std::atomic<int> *synced, QQuickItem *parent)
        : QQuickItem(parent), m_synced(synced)
    {
        setFlag(ItemHasContents, true);
    }
    void phase(int n) { m_phase = n; update(); }
protected:
    QSGNode *updatePaintNode(QSGNode *existing, UpdatePaintNodeData *) override
    {
        auto *node = static_cast<QSGGeometryNode *>(existing);
        if (!node) {
            node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(
                QSGGeometry::defaultAttributes_Point2D(), 4);
            geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
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
        m_synced->store(m_phase, std::memory_order_release);
        return node;
    }
private:
    std::atomic<int> *m_synced;
    int m_phase = -1;
};

struct Results
{
    std::atomic<int> desired{-1};
    std::atomic<int> synced{-1};
    std::atomic<int> inflight{-1};
    std::atomic<int> lastCompleted{-1};
    std::atomic<int> enqueued{0};
    std::atomic<int> callbacks{0};
    std::atomic<int> afterFrameEnd{0};
    std::atomic<int> completed{0};
    std::atomic<int> wrongPixels{0};
    std::atomic<int> refused{0};
    std::atomic<int> renderPumpFrames{0};
    std::atomic<int> noDemandSkipped{0};
    std::atomic<bool> viewerDemand{true};
    std::atomic<bool> renderThreadDistinct{false};
    std::atomic<bool> invalidated{false};
    std::atomic<bool> transferFlag{false};
    std::atomic<bool> swapchainValid{false};
    std::vector<std::unique_ptr<QRhiReadbackResult>> results;
    // Only the render thread invokes completed(), so this vector is updated
    // from that thread and read only after window has ceased all rendering.
    std::vector<double> completionMs;
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    int width = 640, height = 360;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith(QLatin1String("--width=")))
            width = arg.mid(8).toInt();
        else if (arg.startsWith(QLatin1String("--height=")))
            height = arg.mid(9).toInt();
        else {
            std::cerr << "R3_VERDICT=BLOCKED_ARGUMENT\n";
            return 10;
        }
    }
    if (width < 32 || height < 32 || width > 3840 || height > 2160)
        return 11;
#ifdef _WIN32
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif

    // The result storage outlives the QQuickWindow and all render callbacks.
    // A receiver must never use an async readback result after device loss.
    auto state = std::make_shared<Results>();
    QQuickWindow window;
    window.resize(width, height);
    window.setColor(Qt::black);
    SourceItem source(&state->synced, window.contentItem());
    source.setSize(QSizeF(width, height));

    bool quit = false;
    int completedStages = 0;
    bool enteredNoDemand = false;
    int noDemandStartFrames = -1;
    QElapsedTimer elapsed;
    auto *ri = window.rendererInterface();

    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, [&, shared = state] {
        const int currentFrame = shared->callbacks.fetch_add(
            1, std::memory_order_acq_rel) + 1;
        Q_UNUSED(currentFrame);
        if (QThread::currentThread() != app.thread())
            shared->renderThreadDistinct.store(true, std::memory_order_release);
        if (!shared->viewerDemand.load(std::memory_order_acquire)) {
            shared->noDemandSkipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const int target = shared->desired.load(std::memory_order_acquire);
        if (target < 0 || shared->synced.load(std::memory_order_acquire) != target
            || shared->inflight.load(std::memory_order_acquire) >= 0)
            return;

        auto *rhi = static_cast<QRhi *>(
            ri->getResource(&window, QSGRendererInterface::RhiResource));
        auto *sc = static_cast<QRhiSwapChain *>(
            ri->getResource(&window, QSGRendererInterface::RhiSwapchainResource));
        if (!rhi || !sc || sc->rhi() != rhi
            || !rhi->isRecordingFrame()) {
            shared->refused.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        shared->swapchainValid.store(true, std::memory_order_release);
        if (!(sc->flags() & QRhiSwapChain::UsedAsTransferSource)) {
            shared->refused.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        shared->transferFlag.store(true, std::memory_order_release);
        QRhiCommandBuffer *cb = sc->currentFrameCommandBuffer();
        if (!cb) {
            shared->refused.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        auto holder = std::make_unique<QRhiReadbackResult>();
        QRhiReadbackResult *raw = holder.get();
        shared->results.push_back(std::move(holder));
        const auto started = std::chrono::steady_clock::now();
        const std::weak_ptr<Results> weak = shared;
        raw->completed = [weak, raw, started, target] {
            if (const auto locked = weak.lock()) {
                locked->completionMs.push_back(
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count());
                if (!matches(*raw, target))
                    locked->wrongPixels.fetch_add(1, std::memory_order_acq_rel);
                locked->lastCompleted.store(target, std::memory_order_release);
                locked->completed.fetch_add(1, std::memory_order_acq_rel);
                locked->inflight.store(-1, std::memory_order_release);
            }
        };

        shared->inflight.store(target, std::memory_order_release);
        QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
        updates->readBackTexture(QRhiReadbackDescription(), raw);
        // Qt's QSGRhiSupport::grabAndBlockInCurrentFrame (v6.8.3) performs
        // resourceUpdate after QQuickWindow::renderSceneGraph. Unlike that
        // function, we never call rhi->finish() here.
        cb->resourceUpdate(updates);
        shared->enqueued.fetch_add(1, std::memory_order_acq_rel);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::afterFrameEnd,
                     &window, [s = state] {
        s->afterFrameEnd.fetch_add(1, std::memory_order_acq_rel);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated,
                     &window, [s = state] {
        s->invalidated.store(true, std::memory_order_release);
    }, Qt::DirectConnection);

    auto finish = [&](const char *verdict, bool okay) {
        if (quit)
            return;
        quit = true;
        state->viewerDemand.store(false, std::memory_order_release);
        const bool zeroV = state->noDemandSkipped.load() >= 1
            && enteredNoDemand;
        std::cout << "R3_RESULT_JSON={"
                  << "\"schema\":\"hyremote-qt68-private-async-r3/1\","
                  << "\"qt\":\"" << QT_VERSION_STR << "\","
                  << "\"graphics_backend\":\"d3d11\","
                  << "\"size\":[" << width << "," << height << "],"
                  << "\"stages_expected\":" << kStages << ","
                  << "\"stages_completed\":" << completedStages << ","
                  << "\"rhi_recorded_readbacks\":" << state->enqueued.load() << ","
                  << "\"rhi_async_callbacks\":" << state->completed.load() << ","
                  << "\"wrong_pixel_frames\":" << state->wrongPixels.load() << ","
                  << "\"refused_frames\":" << state->refused.load() << ","
                  << "\"swapchain_resource_valid\":"
                  << (state->swapchainValid.load() ? "true" : "false") << ","
                  << "\"swapchain_transfer_flag_enabled\":"
                  << (state->transferFlag.load() ? "true" : "false") << ","
                  << "\"scenegraph_thread_distinct_from_gui\":"
                  << (state->renderThreadDistinct.load() ? "true" : "false") << ","
                  << "\"normal_render_callbacks\":" << state->callbacks.load() << ","
                  << "\"after_frame_end\":" << state->afterFrameEnd.load() << ","
                  << "\"frame_pumps_while_waiting\":" << state->renderPumpFrames.load() << ","
                  << "\"no_demand_render_frames_skipped\":" << state->noDemandSkipped.load() << ","
                  << "\"zero_viewer_readback_gate\":"
                  << (zeroV ? "true" : "false") << ","
                  << "\"readback_completion_p95_ms\":" << p95(state->completionMs) << ","
                  << "\"readback_results_retained_until_window_shutdown\":true,"
                  << "\"sync_rhi_finish_calls\":0,"
                  << "\"qtgui_private_used\":true,"
                  << "\"production_api_authorized\":false,"
                  << "\"product_qualified\":false,"
                  << "\"elapsed_ms\":" << elapsed.elapsed()
                  << "}" << std::endl;
        std::cout << "R3_VERDICT=" << verdict << std::endl;
        app.exit(okay ? 0 : 20);
    };

    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] {
        finish("FAIL_TIMEOUT_OR_UNAVAILABLE_GPU_READBACK", false);
    });

    QTimer tick;
    tick.setInterval(16);
    QObject::connect(&tick, &QTimer::timeout, &app, [&] {
        if (quit)
            return;
        if (state->invalidated.load(std::memory_order_acquire)) {
            finish("FAIL_SCENEGRAPH_DEVICE_INVALIDATED", false);
            return;
        }
        if (state->wrongPixels.load() != 0) {
            finish("FAIL_ASYNC_READBACK_WRONG_PIXEL", false);
            return;
        }
        if (enteredNoDemand) {
            if (state->callbacks.load() > noDemandStartFrames
                && state->noDemandSkipped.load() >= 1) {
                finish("PASS_R3_QT68_D3D11_ASYNC_SWAPCHAIN_PIXEL_PROVEN",
                       completedStages == kStages
                       && state->completed.load() == kStages
                       && state->enqueued.load() == kStages
                       && state->wrongPixels.load() == 0);
            }
            return;
        }
        if (state->lastCompleted.load(std::memory_order_acquire) == completedStages) {
            ++completedStages;
            if (completedStages == kStages) {
                enteredNoDemand = true;
                noDemandStartFrames = state->callbacks.load();
                state->viewerDemand.store(false, std::memory_order_release);
                source.phase(kStages);
                return;
            }
            state->desired.store(completedStages, std::memory_order_release);
            source.phase(completedStages);
            return;
        }
        if (state->inflight.load(std::memory_order_acquire) >= 0) {
            // RHI readback callbacks need GPU progress through future frame
            // submissions. This creates bounded *extra render frames* and
            // must be measured before any product admission.
            state->renderPumpFrames.fetch_add(1, std::memory_order_relaxed);
            window.update();
        }
    });

    window.show();
    elapsed.start();
    deadline.start(15000);
    tick.start();
    QTimer::singleShot(300, &app, [&] {
        state->desired.store(0, std::memory_order_release);
        source.phase(0);
    });
    return app.exec();
}
