// Standalone Qt 6.8 research probe. NOT linked to HyRemote Runtime.
// Deliberately never calls QRhiSwapChain::setFlags() or manages Qt's frame lifecycle.
#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <rhi/qrhi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch()).count();
}
std::uint64_t threadTag()
{
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}
struct State {
    std::atomic<int> frames{0};
    std::atomic<int> observed{0};
    std::atomic<int> reason{0}; // 1=no RHI, 2=no swapchain, 3=no transfer, 4=eligible
    std::atomic<int> api{-1};
    std::atomic<int> backend{-1};
    std::atomic<int> flags{0};
    std::atomic<int> sampleCount{-1};
    std::atomic<int> maxAsyncFrames{-1};
    std::atomic<int> submissions{0};
    std::atomic<int> completions{0};
    std::atomic<int> pixelWidth{0};
    std::atomic<int> pixelHeight{0};
    std::atomic<int> pixelFormat{-1};
    std::atomic<qint64> dataBytes{0};
    std::atomic<qint64> submitWallNs{0};
    std::atomic<qint64> submitEndNs{0};
    std::atomic<qint64> completionNs{0};
    std::atomic<std::uint64_t> renderThread{0};
    std::atomic<std::uint64_t> completionThread{0};
};
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const bool optIn = app.arguments().contains(QStringLiteral("--submit-readback"));
    auto state = std::make_shared<State>();

    QQuickWindow window;
    window.setTitle(QStringLiteral("HyRemote RHI resource capability probe - standalone"));
    window.resize(640, 360);
    window.setColor(QColor(20, 130, 215));
    QObject::connect(&window, &QQuickWindow::afterRendering, &window,
        [&window, state, optIn] {
            state->frames.fetch_add(1);
            if (state->observed.exchange(1) != 0)
                return;

            // The direct connection executes in Qt's scenegraph render thread.
            // Resource pointers must not survive this callback or a scenegraph reset.
            state->renderThread.store(threadTag());
            auto *renderer = window.rendererInterface();
            if (!renderer) {
                state->reason.store(1);
                return;
            }
            state->api.store(static_cast<int>(renderer->graphicsApi()));
            auto *rhi = static_cast<QRhi *>(
                renderer->getResource(&window, QSGRendererInterface::RhiResource));
            if (!rhi) {
                state->reason.store(1);
                return;
            }
            state->backend.store(static_cast<int>(rhi->backend()));
            state->maxAsyncFrames.store(rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames));
            auto *swapchain = static_cast<QRhiSwapChain *>(
                renderer->getResource(&window, QSGRendererInterface::RhiSwapchainResource));
            if (!swapchain) {
                state->reason.store(2);
                return;
            }
            state->flags.store(static_cast<int>(swapchain->flags()));
            state->sampleCount.store(swapchain->sampleCount());

            // A Qt-owned swapchain must not be retrofitted with transfer flags.
            // Multisampled sources are also prohibited by the QRhi readback contract.
            if (!swapchain->flags().testFlag(QRhiSwapChain::UsedAsTransferSource)
                || swapchain->sampleCount() != 1) {
                state->reason.store(3);
                return;
            }
            state->reason.store(4);
            if (!optIn)
                return;

            auto *cb = swapchain->currentFrameCommandBuffer();
            if (!cb)
                return;
            // Never block on QRhi::finish() or begin/end an extra offscreen frame.
            auto *result = new QRhiReadbackResult;
            result->completed = [state, result] {
                state->pixelWidth.store(result->pixelSize.width());
                state->pixelHeight.store(result->pixelSize.height());
                state->pixelFormat.store(static_cast<int>(result->format));
                state->dataBytes.store(result->data.size());
                state->completionThread.store(threadTag());
                state->completionNs.store(nowNs());
                state->completions.fetch_add(1);
                delete result;
            };
            const qint64 begin = nowNs();
            auto *batch = rhi->nextResourceUpdateBatch();
            QRhiReadbackDescription source; // null texture = eligible current swapchain
            batch->readBackTexture(source, result);
            cb->resourceUpdate(batch); // afterRendering: no render pass active
            state->submitWallNs.store(nowNs() - begin);
            state->submitEndNs.store(nowNs());
            state->submissions.fetch_add(1);
        }, Qt::DirectConnection);

    QTimer pulse;
    pulse.setInterval(33);
    bool flip = false;
    QObject::connect(&pulse, &QTimer::timeout, &app, [&] {
        flip = !flip;
        window.setColor(flip ? QColor(20, 130, 215) : QColor(215, 75, 25));
        if (state->completions.load() > 0)
            app.quit();
    });
    pulse.start();
    QTimer::singleShot(5000, &app, [&] { app.quit(); });
    window.show();
    const int eventLoopResult = app.exec();

    QJsonObject record;
    record.insert(QStringLiteral("schema"), 1);
    record.insert(QStringLiteral("scope"), QStringLiteral("standalone_qquickwindow_not_product"));
    record.insert(QStringLiteral("qt_runtime"), QString::fromLatin1(qVersion()));
    record.insert(QStringLiteral("qpa"), QGuiApplication::platformName());
    record.insert(QStringLiteral("mode"), optIn ? QStringLiteral("opt_in_readback")
                                                : QStringLiteral("inspection_only"));
    record.insert(QStringLiteral("frames"), state->frames.load());
    record.insert(QStringLiteral("graphics_api"), state->api.load());
    record.insert(QStringLiteral("rhi_backend"), state->backend.load());
    record.insert(QStringLiteral("swapchain_flags"), state->flags.load());
    record.insert(QStringLiteral("samples"), state->sampleCount.load());
    record.insert(QStringLiteral("max_async_readback_frames"), state->maxAsyncFrames.load());
    record.insert(QStringLiteral("submissions"), state->submissions.load());
    record.insert(QStringLiteral("completions"), state->completions.load());
    record.insert(QStringLiteral("pixel_width"), state->pixelWidth.load());
    record.insert(QStringLiteral("pixel_height"), state->pixelHeight.load());
    record.insert(QStringLiteral("pixel_format"), state->pixelFormat.load());
    record.insert(QStringLiteral("data_bytes"), static_cast<double>(state->dataBytes.load()));
    record.insert(QStringLiteral("submit_wall_ms"),
                  state->submitWallNs.load() / 1000000.0);
    record.insert(QStringLiteral("callback_after_submit_return"),
                  state->completions.load() > 0
                  && state->completionNs.load() > state->submitEndNs.load()
                  && state->submitEndNs.load() > 0);
    record.insert(QStringLiteral("completion_on_rhi_render_thread"),
                  state->completions.load() > 0
                  && state->completionThread.load() == state->renderThread.load());
    record.insert(QStringLiteral("callback_latency_ms"),
                  state->completions.load() > 0 && state->submitEndNs.load() > 0
                      ? (state->completionNs.load() - state->submitEndNs.load()) / 1000000.0
                      : -1.0);

    QString outcome;
    if (state->observed.load() == 0)
        outcome = QStringLiteral("NO_RENDER_FRAME");
    else if (state->reason.load() == 1)
        outcome = QStringLiteral("NO_RHI");
    else if (state->reason.load() == 2)
        outcome = QStringLiteral("NO_SWAPCHAIN");
    else if (state->reason.load() == 3)
        outcome = QStringLiteral("RESOURCE_NOT_READBACK_ELIGIBLE");
    else if (!optIn)
        outcome = QStringLiteral("ELIGIBLE_INSPECTION_ONLY");
    else if (state->submissions.load() == 0)
        outcome = QStringLiteral("NO_COMMAND_BUFFER");
    else if (state->completions.load() == 0)
        outcome = QStringLiteral("NO_CALLBACK_WITHIN_TIMEOUT");
    else if (state->dataBytes.load() <= 0 || state->pixelWidth.load() <= 0
             || state->pixelHeight.load() <= 0)
        outcome = QStringLiteral("EMPTY_OR_INVALID_RESULT");
    else
        outcome = QStringLiteral("CALLBACK_WITH_BYTES_NOT_NONBLOCKING_PROOF");
    record.insert(QStringLiteral("outcome"), outcome);
    const QByteArray json = QJsonDocument(record).toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, static_cast<std::size_t>(json.size()), stdout);
    std::fputc('\n', stdout);
    return eventLoopResult;
}
