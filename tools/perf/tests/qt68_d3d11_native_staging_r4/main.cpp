// R4 independent Windows native D3D11 readback research: Qt 6.8.3 PUBLIC
// QSGRendererInterface + Windows SDK ONLY. No QtGuiPrivate/QRhi header.
// NOT a product backend and NOT a cross-platform implementation.
//
// afterRenderPassRecording: on the render thread, while Qt's D3D11 main
// render target is still bound, query its RTV via the public Qt-exported
// ID3D11DeviceContext and retain the source texture for this frame.
//
// afterRendering: after Qt has recorded its main render pass, copy the
// source texture to a CPU-readable D3D11 staging texture using CopyResource.
// On subsequent render frames, poll Map with D3D11_MAP_FLAG_DO_NOT_WAIT,
// rejecting a pipeline-blocking Map. Validate 15 actual CPU pixel results.
//
// Experimental frame pumps needed to allow Map/completion are explicitly
// recorded. This does not establish an efficient 60Hz production solution.
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;
constexpr int kStages = 15;

// Qt6 Quick RHI records D3D11 commands and flushes them at endFrame.
// The public beginExternalCommands/endExternalCommands pair is REQUIRED
// to put the RHI command buffer into a native D3D11 state. It flushes
// recorded Qt render commands before an external D3D11 API is accessed
// and restores QRhi's cached state after returning.
// Exact Qt6.8.3 qquickwindow.cpp and qrhid3d11.cpp confirm this.
class NativeCommandsScope
{
public:
    explicit NativeCommandsScope(QQuickWindow &window) : m_window(window)
    {
        m_window.beginExternalCommands();
    }
    ~NativeCommandsScope() { m_window.endExternalCommands(); }
    NativeCommandsScope(const NativeCommandsScope &) = delete;
    NativeCommandsScope &operator=(const NativeCommandsScope &) = delete;
private:
    QQuickWindow &m_window;
};

QColor color(int epoch)
{
    switch (epoch % 3) {
    case 0: return QColor(226, 37, 51);
    case 1: return QColor(34, 207, 64);
    default: return QColor(45, 67, 230);
    }
}

bool pixelMatches(const unsigned char *data, unsigned rowPitch,
                  unsigned width, unsigned height,
                  DXGI_FORMAT format, int epoch)
{
    if (!data || width < 32 || height < 32 || rowPitch < width*4u)
        return false;
    const bool bgra = format == DXGI_FORMAT_B8G8R8A8_UNORM
        || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const bool rgba = format == DXGI_FORMAT_R8G8B8A8_UNORM
        || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (!bgra && !rgba)
        return false;
    const unsigned char *p = data + (height/2u)*rowPitch + (width/2u)*4u;
    const int red = bgra ? p[2] : p[0];
    const int green = p[1];
    const int blue = bgra ? p[0] : p[2];
    const auto expected = color(epoch);
    return std::abs(red - expected.red()) <= 15
        && std::abs(green - expected.green()) <= 15
        && std::abs(blue - expected.blue()) <= 15;
}

double p95(std::vector<double> samples)
{
    if (samples.empty())
        return -1;
    std::sort(samples.begin(), samples.end());
    return samples[std::min(samples.size()-1,
        static_cast<std::size_t>(std::ceil(samples.size()*0.95) - 1))];
}

class SourceItem final : public QQuickItem
{
public:
    SourceItem(std::atomic<int> *synced, QQuickItem *parent)
        : QQuickItem(parent), m_synced(synced)
    {
        setFlag(ItemHasContents, true);
    }
    void setPhase(int value) { m_phase = value; update(); }
protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override
    {
        auto *node = static_cast<QSGGeometryNode *>(old);
        if (!node) {
            node = new QSGGeometryNode;
            auto *g = new QSGGeometry(
                QSGGeometry::defaultAttributes_Point2D(), 4);
            g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(g);
            node->setFlag(QSGNode::OwnsGeometry);
            auto *m = new QSGFlatColorMaterial;
            node->setMaterial(m);
            node->setFlag(QSGNode::OwnsMaterial);
        }
        QSGGeometry::updateRectGeometry(node->geometry(), boundingRect());
        node->geometry()->markVertexDataDirty();
        node->markDirty(QSGNode::DirtyGeometry);
        auto *m = static_cast<QSGFlatColorMaterial *>(node->material());
        m->setColor(color(m_phase));
        node->markDirty(QSGNode::DirtyMaterial);
        m_synced->store(m_phase, std::memory_order_release);
        return node;
    }
private:
    std::atomic<int> *m_synced;
    int m_phase = -1;
};

struct State
{
    std::atomic<int> desired{-1}, synced{-1};
    std::atomic<int> latestCompleted{-1};
    std::atomic<int> queued{0}, completed{0}, invalidPixels{0};
    std::atomic<int> renderCallbacks{0}, afterFrameEnd{0};
    std::atomic<int> sourceRtvSnapshots{0}, noDemandSkipped{0};
    std::atomic<int> framePumps{0}, mapStillDrawing{0}, rejected{0};
    std::atomic<int> lastHresult{0};
    std::atomic<int> rejectionReason{0};
    std::atomic<unsigned> sourceTextureWidth{0}, sourceTextureHeight{0};
    std::atomic<unsigned> sourceMipLevels{0}, sourceArraySize{0}, sourceSampleCount{0};
    std::atomic<bool> demand{true}, renderThreadDistinct{false}, sceneInvalidated{false};
    std::atomic<bool> deviceResourceObserved{false}, contextResourceObserved{false};

    // Only accessed on scene graph render thread; no GUI-thread touch.
    ComPtr<ID3D11Texture2D> capturedRenderTexture;
    ComPtr<ID3D11Texture2D> pendingStagingTexture;
    ComPtr<ID3D11DeviceContext> pendingContext;
    D3D11_TEXTURE2D_DESC pendingDesc{};
    int capturedEpoch = -1;
    int pendingEpoch = -1;
    std::chrono::steady_clock::time_point pendingStarted;
    std::vector<double> completionMs;
    std::atomic<int> lastFormat{-1};
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    int width = 640, height = 360;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a.startsWith(QLatin1String("--width=")))
            width = a.mid(8).toInt();
        else if (a.startsWith(QLatin1String("--height=")))
            height = a.mid(9).toInt();
        else {
            std::cerr << "R4_VERDICT=BLOCKED_UNKNOWN_ARGUMENT\n";
            return 10;
        }
    }
    if (width < 32 || height < 32 || width > 3840 || height > 2160)
        return 11;

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    State state;
    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(width, height);
    SourceItem item(&state.synced, window.contentItem());
    item.setSize(QSizeF(width,height));

    auto *iface = window.rendererInterface();
    QObject::connect(&window, &QQuickWindow::afterRenderPassRecording,
                     &window, [&] {
        if (QThread::currentThread() != app.thread())
            state.renderThreadDistinct.store(true, std::memory_order_release);
        if (!state.demand.load(std::memory_order_acquire))
            return;

        auto *device = static_cast<ID3D11Device *>(iface->getResource(
            &window, QSGRendererInterface::DeviceResource));
        auto *context = static_cast<ID3D11DeviceContext *>(iface->getResource(
            &window, QSGRendererInterface::DeviceContextResource));
        if (device)
            state.deviceResourceObserved.store(true, std::memory_order_release);
        if (context)
            state.contextResourceObserved.store(true, std::memory_order_release);
        if (!device || !context)
            return;

        const int phase = state.synced.load(std::memory_order_acquire);
        if (phase < 0)
            return;
        NativeCommandsScope nativeCommands(window);
        ComPtr<ID3D11RenderTargetView> view;
        context->OMGetRenderTargets(1, view.GetAddressOf(), nullptr);
        if (!view)
            return;
        ComPtr<ID3D11Resource> resource;
        view->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        if (!resource || FAILED(resource.As(&texture)) || !texture)
            return;
        // The borrowed render target is a real Qt-owned D3D11 texture;
        // ComPtr AddRef keeps it alive until a corresponding copy, but never
        // implies ownership over Qt swapchain retirement.
        D3D11_TEXTURE2D_DESC readDesc{};
        texture->GetDesc(&readDesc);
        state.sourceTextureWidth.store(readDesc.Width);
        state.sourceTextureHeight.store(readDesc.Height);
        state.sourceMipLevels.store(readDesc.MipLevels);
        state.sourceArraySize.store(readDesc.ArraySize);
        state.sourceSampleCount.store(readDesc.SampleDesc.Count);
        state.capturedRenderTexture = texture;
        state.capturedEpoch = phase;
        state.sourceRtvSnapshots.fetch_add(1, std::memory_order_acq_rel);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, [&] {
        state.renderCallbacks.fetch_add(1, std::memory_order_acq_rel);
        if (!state.demand.load(std::memory_order_acquire)) {
            state.noDemandSkipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        auto *device = static_cast<ID3D11Device *>(iface->getResource(
            &window, QSGRendererInterface::DeviceResource));
        auto *context = static_cast<ID3D11DeviceContext *>(iface->getResource(
            &window, QSGRendererInterface::DeviceContextResource));
        if (!device || !context) {
            state.rejected.fetch_add(1);
            return;
        }
        NativeCommandsScope nativeCommands(window);
        if (state.pendingStagingTexture) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT hr = context->Map(state.pendingStagingTexture.Get(),
                0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
                state.mapStillDrawing.fetch_add(1);
                return;
            }
            if (FAILED(hr)) {
                state.rejectionReason.store(3); // Map failure
                state.lastHresult.store(static_cast<int>(hr));
                state.rejected.fetch_add(1);
                return;
            }
            const bool correct = pixelMatches(
                static_cast<const unsigned char *>(mapped.pData),
                mapped.RowPitch, state.pendingDesc.Width, state.pendingDesc.Height,
                state.pendingDesc.Format, state.pendingEpoch);
            context->Unmap(state.pendingStagingTexture.Get(), 0);
            if (!correct)
                state.invalidPixels.fetch_add(1, std::memory_order_acq_rel);
            state.completionMs.push_back(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - state.pendingStarted).count());
            state.latestCompleted.store(state.pendingEpoch, std::memory_order_release);
            state.completed.fetch_add(1, std::memory_order_acq_rel);
            state.pendingStagingTexture.Reset();
            state.pendingContext.Reset();
            state.pendingEpoch = -1;
            return;
        }
        const int desired = state.desired.load(std::memory_order_acquire);
        if (desired < 0 || state.capturedEpoch != desired
            || !state.capturedRenderTexture)
            return;
        D3D11_TEXTURE2D_DESC desc{};
        state.capturedRenderTexture->GetDesc(&desc);
        if (desc.Width < unsigned(width) || desc.Height < unsigned(height)
            || desc.ArraySize != 1 || desc.MipLevels != 1
            || desc.SampleDesc.Count != 1) {
            state.rejectionReason.store(1); // source size/shape mismatch
            state.rejected.fetch_add(1);
            return;
        }
        const D3D11_TEXTURE2D_DESC sourceDesc = desc;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        const HRESULT created = device->CreateTexture2D(
            &desc, nullptr, staging.GetAddressOf());
        if (FAILED(created) || !staging) {
            state.rejectionReason.store(2); // D3D staging texture creation
            state.lastHresult.store(static_cast<int>(created));
            state.rejected.fetch_add(1);
            return;
        }
        state.pendingStagingTexture = staging;
        state.pendingContext = context;
        state.pendingDesc = sourceDesc;
        state.pendingEpoch = desired;
        state.lastFormat.store(static_cast<int>(sourceDesc.Format));
        state.pendingStarted = std::chrono::steady_clock::now();
        context->CopyResource(staging.Get(), state.capturedRenderTexture.Get());
        state.queued.fetch_add(1, std::memory_order_acq_rel);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::afterFrameEnd,
                     &window, [&] {
        state.afterFrameEnd.fetch_add(1, std::memory_order_relaxed);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated,
                     &window, [&] {
        state.sceneInvalidated.store(true, std::memory_order_release);
    }, Qt::DirectConnection);

    QElapsedTimer elapsed;
    bool finished = false, noDemand = false;
    int completedStages = 0, noDemandStartFrames = 0;
    auto report = [&](const char *verdict, bool okay) {
        if (finished)
            return;
        finished = true;
        state.demand.store(false, std::memory_order_release);
        std::cout << "R4_RESULT_JSON={"
                  << "\"schema\":\"hyremote-qt68-public-native-d3d11-staging/1\","
                  << "\"qt\":\"" << QT_VERSION_STR << "\","
                  << "\"graphics_backend\":\"d3d11\","
                  << "\"size\":[" << width << "," << height << "],"
                  << "\"stages_expected\":" << kStages << ","
                  << "\"stages_completed\":" << completedStages << ","
                  << "\"copy_commands_queued\":" << state.queued.load() << ","
                  << "\"nonblocking_map_completions\":" << state.completed.load() << ","
                  << "\"wrong_pixel_frames\":" << state.invalidPixels.load() << ","
                  << "\"map_still_drawing\":" << state.mapStillDrawing.load() << ","
                  << "\"rejected_readback_attempts\":" << state.rejected.load() << ","
                  << "\"last_hresult\":" << state.lastHresult.load() << ","
                  << "\"rejection_reason\":" << state.rejectionReason.load() << ","
                  << "\"window_logical_size\":[" << window.width() << "," << window.height() << "],"
                  << "\"window_dpr\":" << window.devicePixelRatio() << ","
                  << "\"screen_size\":[" << (window.screen() ? window.screen()->size().width() : 0)
                  << "," << (window.screen() ? window.screen()->size().height() : 0) << "],"
                  << "\"source_texture_size\":[" << state.sourceTextureWidth.load()
                  << "," << state.sourceTextureHeight.load() << "],"
                  << "\"source_texture_mips\":" << state.sourceMipLevels.load() << ","
                  << "\"source_texture_array_size\":" << state.sourceArraySize.load() << ","
                  << "\"source_texture_sample_count\":" << state.sourceSampleCount.load() << ","
                  << "\"source_rtv_snapshots\":" << state.sourceRtvSnapshots.load() << ","
                  << "\"native_format_enum\":" << state.lastFormat.load() << ","
                  << "\"render_callbacks\":" << state.renderCallbacks.load() << ","
                  << "\"after_frame_end\":" << state.afterFrameEnd.load() << ","
                  << "\"render_thread_distinct_from_gui\":"
                  << (state.renderThreadDistinct.load()?"true":"false") << ","
                  << "\"qt_device_resource_observed\":"
                  << (state.deviceResourceObserved.load()?"true":"false") << ","
                  << "\"qt_device_context_observed\":"
                  << (state.contextResourceObserved.load()?"true":"false") << ","
                  << "\"extra_frames_to_progress_gpu_copy\":" << state.framePumps.load() << ","
                  << "\"no_demand_render_frames_skipped\":" << state.noDemandSkipped.load() << ","
                  << "\"nonblocking_copy_complete_p95_ms\":" << p95(state.completionMs) << ","
                  << "\"uses_qtgui_private\":false,"
                  << "\"native_backend_specific\":true,"
                  << "\"readback_qualified_for_product\":false,"
                  << "\"user_site_eglfs_qualified\":false,"
                  << "\"elapsed_ms\":" << elapsed.elapsed()
                  << "}" << std::endl;
        std::cout << "R4_VERDICT=" << verdict << std::endl;
        app.exit(okay ? 0 : 20);
    };

    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] {
        report("FAIL_NO_NONBLOCKING_D3D11_COPY_COMPLETION", false);
    });

    QTimer tick;
    tick.setInterval(16);
    QObject::connect(&tick, &QTimer::timeout, &app, [&] {
        if (finished)
            return;
        if (state.sceneInvalidated.load() || state.invalidPixels.load()
            || state.rejected.load() > 0) {
            report("FAIL_DEVICE_LOSS_WRONG_PIXELS_OR_UNSUPPORTED_RTV",false);
            return;
        }
        if (noDemand) {
            if (state.renderCallbacks.load() > noDemandStartFrames
                && state.noDemandSkipped.load() >= 1) {
                const bool ready = completedStages == kStages
                    && state.queued.load() == kStages
                    && state.completed.load() == kStages;
                report(ready ? "PASS_SCOPED_PUBLIC_QT68_D3D11_STAGING"
                             : "FAIL_INCOMPLETE_NATIVE_STAGE",
                       ready);
            }
            return;
        }
        if (state.latestCompleted.load(std::memory_order_acquire)
            == completedStages) {
            ++completedStages;
            if (completedStages == kStages) {
                noDemand = true;
                state.demand.store(false, std::memory_order_release);
                noDemandStartFrames = state.renderCallbacks.load();
                item.setPhase(kStages); // scene still renders; no GPU->CPU copy
                return;
            }
            state.desired.store(completedStages, std::memory_order_release);
            item.setPhase(completedStages);
        } else if (state.queued.load() > state.completed.load()) {
            state.framePumps.fetch_add(1);
            window.update(); // measured, bounded by watchdog and one inflight
        }
    });

    window.show();
    elapsed.start();
    deadline.start(20000);
    tick.start();
    QTimer::singleShot(300, &app, [&] {
        state.desired.store(0, std::memory_order_release);
        item.setPhase(0);
    });
    return app.exec();
}
