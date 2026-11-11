// EXPERIMENTAL / default OFF. Qt6 Quick D3D11 native staging provider.
// Uses ONLY public QSGRendererInterface resources + Windows SDK; no QRhi
// private headers, platform names in Core, or second runtime implementation.
// Ref: #937 actual visible Qt6.8.3 D3D11 RTV/staging pixel experiment.
// Qualification for product requires fixed-SHA Windows Viewer A/B.
#include "quick/quick_d3d11_staging_capture_source.hpp"
#include "quick/quick_readback_timing.hpp"
#include "quick/quick_readback_batch.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThreadPool>
#include <QTimer>
#include <QtMath>

#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#include "detail/perf_probe.hpp"
#include "detail/readback_frame_storage.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {
namespace {
using Microsoft::WRL::ComPtr;
constexpr int kNormalOpportunityMs = QuickReadbackTiming::normalFrameOpportunityMs;
constexpr int kMaxExtraRenders = QuickReadbackTiming::maxExtraRenders;
constexpr int kRequestDeadlineMs = 750;

bool isBgra(DXGI_FORMAT format) noexcept
{
    return format == DXGI_FORMAT_B8G8R8A8_UNORM
        || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}
bool isRgba(DXGI_FORMAT format) noexcept
{
    return format == DXGI_FORMAT_R8G8B8A8_UNORM
        || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}
class NativeCommandsScope {
public:
    explicit NativeCommandsScope(QQuickWindow *window) : m_window(window)
    {
        m_window->beginExternalCommands();
    }
    ~NativeCommandsScope() { m_window->endExternalCommands(); }
    NativeCommandsScope(const NativeCommandsScope &) = delete;
    NativeCommandsScope &operator=(const NativeCommandsScope &) = delete;
private:
    QQuickWindow *m_window;
};

class D3d11StagingCaptureSource final : public hyremote::CaptureSource
{
    struct State {
        std::mutex mutex;
        std::condition_variable drained;
        QPointer<QQuickWindow> target;
        std::uint64_t generation = 0;
        bool active = false;
        bool connected = false;
        bool guiKickPosted = false;
        bool fallbackTimerArmed = false;
        bool cpuConversionPending = false;
        bool cleanupRequested = false;
        unsigned int callbacksInFlight = 0;
        unsigned int cpuWorkersInFlight = 0;
        std::vector<hyremote::CaptureRequest> queued;
        std::vector<hyremote::CaptureRequest> submitted;
        std::vector<hyremote::CaptureRequest> converting;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
        QMetaObject::Connection passConnection;
        QMetaObject::Connection renderConnection;
        QMetaObject::Connection invalidationConnection;

        // Only accessed under mutex inside Qt's render-thread callbacks.
        // Never share these Qt-owned D3D11 resources with a CPU pool worker.
        ComPtr<ID3D11Texture2D> capturedTarget;
        ComPtr<ID3D11Texture2D> staging;
        ComPtr<ID3D11Device> stagingDevice;
        D3D11_TEXTURE2D_DESC stagingDesc{};
        // Retain at most ONE completed staging texture, not one per frame.
        // Once Map/Unmap succeeds the CPU result owns its bytes separately.
        ComPtr<ID3D11Texture2D> reusableStaging;
        ComPtr<ID3D11Device> reusableDevice;
        D3D11_TEXTURE2D_DESC reusableDesc{};
        hyremote::TimePoint capturedPts{};
        hyremote::TimePoint submittedPts{};
        int pixelWidth = 0, pixelHeight = 0;
        float dpr = 1.0f;
        int forcedRenders = 0;
    };

    struct WorkerDrain {
        std::shared_ptr<State> state;
        ~WorkerDrain() noexcept
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->cpuWorkersInFlight;
            state->drained.notify_all();
        }
    };

public:
    explicit D3d11StagingCaptureSource(QQuickWindow *window)
        : m_state(std::make_shared<State>())
    {
        m_state->target = window;
    }
    ~D3d11StagingCaptureSource() override { stop(); }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities cap;
        cap.asynchronous = true;
        cap.cpuReadable = true;
        cap.regionDamage = false;
        cap.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return cap;
    }

    bool start(hyremote::FrameReadyHandler frame,
               hyremote::CaptureEventHandler event) override
    {
        auto s = m_state;
        auto *app = QCoreApplication::instance();
        if (!app)
            return false;
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->active || !s->target || s->target->thread() != app->thread())
            return false;
        ++s->generation;
        s->queued.clear();
        s->submitted.clear();
        s->converting.clear();
        s->cpuConversionPending = false;
        s->guiKickPosted = false;
        s->fallbackTimerArmed = false;
        s->onFrame = std::move(frame);
        s->onEvent = std::move(event);
        s->active = true;
        // An old GPU generation is released in the next render callback.
        return true;
    }

    void stop() noexcept override
    {
        auto s = m_state;
        QPointer<QQuickWindow> window;
        {
            std::unique_lock<std::mutex> lock(s->mutex);
            s->active = false;
            ++s->generation;
            s->queued.clear();
            s->submitted.clear();
            s->converting.clear();
            s->cpuConversionPending = false;
            s->guiKickPosted = false;
            s->fallbackTimerArmed = false;
            s->cleanupRequested = true;
            s->onFrame = {};
            s->onEvent = {};
            // Prevent any CPU worker from executing after Runtime DLL unload.
            s->drained.wait(lock, [&] {
                return s->callbacksInFlight == 0
                    && s->cpuWorkersInFlight == 0;
            });
            window = s->target;
        }
        // Release borrowed GPU resources at Qt's next render-thread callback.
        if (window)
            QMetaObject::invokeMethod(window, [window] {
                if (window && window->isVisible())
                    window->update();
            }, Qt::QueuedConnection);
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        auto s = m_state;
        auto *app = QCoreApplication::instance();
        if (!app)
            return false;
        QPointer<QQuickWindow> window;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if (!s->active || !s->target)
                return false;
            s->queued.push_back(request);
            generation = s->generation;
            QTimer::singleShot(kRequestDeadlineMs, app,
                [s, generation, id = request.id] {
                    std::vector<hyremote::CaptureRequest> expired;
                    {
                        std::lock_guard<std::mutex> lock(s->mutex);
                        if (!s->active || s->generation != generation)
                            return;
                        // A single D3D11 CopyResource may own many Core IDs.
                        // One stalled GPU batch is ONE failure episode, not
                        // one failure per request/timer (plus no duplicates).
                        if (!retireTimedOutReadbackBatch(s->submitted, id, expired)
                            && !retireTimedOutReadbackBatch(s->converting, id, expired))
                            retireTimedOutReadbackBatch(s->queued, id, expired);
                    }
                    if (!expired.empty())
                        deliver(s, generation, std::move(expired), {},
                                0, 0, 1.f, {}, "D3D11 capture exceeded bounded deadline");
                });
            if (s->guiKickPosted)
                return true;
            s->guiKickPosted = true;
            window = s->target;
        }

        if (QMetaObject::invokeMethod(window, [s, window, generation] {
                if (!window) return;
                bool shouldConnect = false;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    s->guiKickPosted = false;
                    shouldConnect = !s->connected;
                    if (shouldConnect)
                        s->connected = true;
                }
                if (shouldConnect) {
                    s->passConnection = QObject::connect(
                        window, &QQuickWindow::afterRenderPassRecording,
                        window, [s, window] { onRenderPass(s, window); },
                        Qt::DirectConnection);
                    s->renderConnection = QObject::connect(
                        window, &QQuickWindow::afterRendering,
                        window, [s, window] { onAfterRender(s, window); },
                        Qt::DirectConnection);
                    s->invalidationConnection = QObject::connect(
                        window, &QQuickWindow::sceneGraphInvalidated,
                        window, [s] { onInvalidated(s); },
                        Qt::DirectConnection);
                }
                if (!window->isVisible()) {
                    std::vector<hyremote::CaptureRequest> hidden;
                    {
                        std::lock_guard<std::mutex> lock(s->mutex);
                        if (s->active && s->generation == generation)
                            hidden.swap(s->queued);
                    }
                    if (!hidden.empty())
                        deliver(s, generation, std::move(hidden), {},
                                0, 0, 1.f, {}, "D3D11 target is hidden");
                    return;
                }
                requestFallbackRender(s, window, generation);
            }, Qt::QueuedConnection))
            return true;

        std::vector<hyremote::CaptureRequest> accepted;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if (s->active && s->generation == generation) {
                s->guiKickPosted = false;
                for (const auto &r : s->queued) {
                    if (r.id != request.id)
                        accepted.push_back(r);
                }
                s->queued.clear();
            }
        }
        if (!accepted.empty())
            deliver(s, generation, std::move(accepted), {}, 0, 0,
                    1.f, {}, "D3D11 GUI dispatch failed");
        return false; // This request was rejected, not accepted by Core.
    }

private:
    static void deliver(const std::shared_ptr<State> &s,
                        std::uint64_t generation,
                        std::vector<hyremote::CaptureRequest> requests,
                        std::shared_ptr<const hyremote::FrameStorage> storage,
                        int width, int height, float dpr,
                        hyremote::TimePoint renderPts,
                        const char *error = nullptr)
    {
        auto *app = QCoreApplication::instance();
        if (!app || requests.empty())
            return;
        QMetaObject::invokeMethod(app,
            [s, generation, requests = std::move(requests),
             storage = std::move(storage), width, height, dpr, renderPts, error] {
                hyremote::FrameReadyHandler frameCallback;
                hyremote::CaptureEventHandler eventCallback;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != generation)
                        return;
                    frameCallback = s->onFrame;
                    eventCallback = s->onEvent;
                    ++s->callbacksInFlight;
                }
                if (!storage && eventCallback) {
                    try {
                        hyremote::CaptureEvent event;
                        event.code = hyremote::CaptureEventCode::TemporarilyUnavailable;
                        event.recoverable = true;
                        event.message = error ? error : "D3D11 readback unavailable";
                        eventCallback(event);
                    } catch (...) {}
                }
                if (frameCallback) {
                    for (const auto &request : requests) {
                        try {
                            hyremote::RemoteFrame frame;
                            frame.requestId = request.id;
                            frame.timing.requestTime = request.requestTime;
                            frame.timing.completionTime = hyremote::Clock::now();
                            frame.timing.ptsSource = hyremote::PtsSource::Completion;
                            if (storage) {
                                frame.geometry.size = {
                                    static_cast<std::uint32_t>(width),
                                    static_cast<std::uint32_t>(height)};
                                frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
                                frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
                                frame.geometry.devicePixelRatio = dpr;
                                frame.geometry.planeCount = 1;
                                frame.damage = hyremote::Damage::fullFrame();
                                frame.storage = storage;
                                frame.timing.pts = renderPts;
                                frame.timing.ptsSource = hyremote::PtsSource::Render;
                            }
                            frameCallback(std::move(frame));
                        } catch (...) {}
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    --s->callbacksInFlight;
                    s->drained.notify_all();
                }
            }, Qt::QueuedConnection);
    }

    static void requestFallbackRender(const std::shared_ptr<State> &s,
                                      QPointer<QQuickWindow> window,
                                      std::uint64_t generation)
    {
        if (!window)
            return;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if (!s->active || s->generation != generation
                || s->fallbackTimerArmed)
                return;
            s->fallbackTimerArmed = true;
        }
        QTimer::singleShot(kNormalOpportunityMs, window.data(),
            [s, window, generation] {
                bool shouldUpdate = false;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != generation)
                        return;
                    s->fallbackTimerArmed = false;
                    shouldUpdate = !s->staging && !s->cpuConversionPending
                        && !s->queued.empty();
                }
                if (shouldUpdate && window && window->isVisible()) {
                    HYREMOTE_P1_COUNT(QuickGlExplicitRender);
                    window->update();
                }
            });
    }

    static void requestRetirementRender(const std::shared_ptr<State> &s,
                                        QPointer<QQuickWindow> window,
                                        std::uint64_t generation)
    {
        if (!window)
            return;
        int delayMs = QuickReadbackTiming::firstRetirementRenderMs;
        {
            std::lock_guard<std::mutex> guard(s->mutex);
            if (!s->active || s->generation != generation
                || !s->staging || s->forcedRenders >= kMaxExtraRenders)
                return;
            if (s->forcedRenders > 0)
                delayMs = QuickReadbackTiming::secondRetirementRenderMs;
        }
        QTimer::singleShot(delayMs, window.data(),
            [s, window, generation] {
                bool shouldRender = false;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (s->active && s->generation == generation && s->staging
                        && s->forcedRenders < kMaxExtraRenders) {
                        ++s->forcedRenders;
                        shouldRender = true;
                    }
                }
                if (shouldRender && window && window->isVisible()) {
                    HYREMOTE_P1_COUNT(QuickGlRetirementRender);
                    window->update();
                }
            });
    }

    static void releaseActiveReadback(State &state)
    {
        state.capturedTarget.Reset();
        state.staging.Reset();
        state.stagingDevice.Reset();
        state.stagingDesc = {};
        state.forcedRenders = 0;
        state.pixelWidth = state.pixelHeight = 0;
    }

    static void releaseNative(State &state)
    {
        releaseActiveReadback(state);
        // Full source/device invalidation discards even an idle GPU resource.
        state.reusableStaging.Reset();
        state.reusableDevice.Reset();
        state.reusableDesc = {};
    }

    static bool canReuseStaging(const State &state, ID3D11Device *device,
                                const D3D11_TEXTURE2D_DESC &desc) noexcept
    {
        const auto &old = state.reusableDesc;
        return state.reusableStaging
            && state.reusableDevice.Get() == device
            && old.Width == desc.Width && old.Height == desc.Height
            && old.MipLevels == desc.MipLevels
            && old.ArraySize == desc.ArraySize
            && old.Format == desc.Format
            && old.SampleDesc.Count == desc.SampleDesc.Count
            && old.SampleDesc.Quality == desc.SampleDesc.Quality
            && old.Usage == desc.Usage
            && old.BindFlags == desc.BindFlags
            && old.CPUAccessFlags == desc.CPUAccessFlags
            && old.MiscFlags == desc.MiscFlags;
    }

    static void onInvalidated(const std::shared_ptr<State> &s)
    {
        std::vector<hyremote::CaptureRequest> lost;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            generation = s->generation;
            if (s->active) {
                lost.swap(s->queued);
                lost.insert(lost.end(), s->submitted.begin(), s->submitted.end());
                s->submitted.clear();
                // Conversion uses CPU-owned bytes but the pixels belong to
                // the invalidated Surface. Cancel their delivery now; the
                // worker still drains its memory before CaptureSource stop.
                lost.insert(lost.end(), s->converting.begin(), s->converting.end());
                s->converting.clear();
            }
            releaseNative(*s);
            s->cleanupRequested = false;
            if (!s->active && s->connected) {
                QObject::disconnect(s->passConnection);
                QObject::disconnect(s->renderConnection);
                QObject::disconnect(s->invalidationConnection);
                s->connected = false;
            }
        }
        if (!lost.empty())
            deliver(s, generation, std::move(lost), {}, 0, 0, 1.f, {},
                    "D3D11 scenegraph was invalidated");
    }

    static void onRenderPass(const std::shared_ptr<State> &s,
                             QQuickWindow *window)
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        if (!s->active || s->queued.empty() || s->staging
            || s->cpuConversionPending || !window->rendererInterface()
            || window->rendererInterface()->graphicsApi()
                != QSGRendererInterface::Direct3D11)
            return;

        auto *iface = window->rendererInterface();
        NativeCommandsScope native(window);
        auto *context = static_cast<ID3D11DeviceContext *>(
            iface->getResource(window, QSGRendererInterface::DeviceContextResource));
        if (!context)
            return;
        ComPtr<ID3D11RenderTargetView> view;
        context->OMGetRenderTargets(1, view.GetAddressOf(), nullptr);
        if (!view)
            return;
        ComPtr<ID3D11Resource> resource;
        view->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        if (!resource || FAILED(resource.As(&texture)) || !texture)
            return;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        const qreal dpr = window->effectiveDevicePixelRatio();
        const int w = qRound(window->width() * dpr);
        const int h = qRound(window->height() * dpr);
        if (w <= 0 || h <= 0
            || desc.Width != static_cast<UINT>(w)
            || desc.Height != static_cast<UINT>(h)
            || desc.ArraySize != 1 || desc.MipLevels != 1
            || desc.SampleDesc.Count != 1
            || (!isBgra(desc.Format) && !isRgba(desc.Format)))
            return;
        s->capturedTarget = std::move(texture);
        s->capturedPts = hyremote::Clock::now();
    }

    static void finishCpu(const std::shared_ptr<State> &s,
                          std::uint64_t generation,
                          std::shared_ptr<std::byte> raw, std::size_t bytes,
                          std::size_t pitch, std::size_t rowBytes,
                          int width, int height, float dpr, DXGI_FORMAT format,
                          hyremote::TimePoint pts)
    {
        const auto storage = adoptReadbackFrameStorage(
            std::move(raw), bytes, pitch, rowBytes, static_cast<std::size_t>(height),
            ReadbackRowOrder::TopDown,
            isBgra(format) ? ReadbackPixelOrder::Bgra8888
                           : ReadbackPixelOrder::Rgba8888);
        std::vector<hyremote::CaptureRequest> requests;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if (!s->active || s->generation != generation)
                return;
            s->cpuConversionPending = false;
            requests.swap(s->converting);
        }
        if (!requests.empty())
            deliver(s, generation, std::move(requests), storage,
                    width, height, dpr, pts,
                    storage ? nullptr : "D3D11 CPU frame conversion failed");

        if (auto *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [s, generation] {
                QPointer<QQuickWindow> window;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (!s->active || s->generation != generation) return;
                    window = s->target;
                }
                requestFallbackRender(s, window, generation);
            }, Qt::QueuedConnection);
        }
    }

    static void onAfterRender(const std::shared_ptr<State> &s,
                              QQuickWindow *window)
    {
        std::shared_ptr<std::byte> raw;
        std::vector<hyremote::CaptureRequest> failed;
        const char *failure = "D3D11 staging readback failed";
        std::uint64_t generation = 0;
        std::size_t bytes = 0, pitch = 0, rowBytes = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        int width = 0, height = 0;
        float dpr = 1.f;
        hyremote::TimePoint pts{};
        bool pump = false;
        bool tryNext = false;
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if (s->cleanupRequested) {
                releaseNative(*s);
                s->cleanupRequested = false;
            }
            if (!s->active) {
                if (s->connected) {
                    QObject::disconnect(s->passConnection);
                    QObject::disconnect(s->renderConnection);
                    QObject::disconnect(s->invalidationConnection);
                    s->connected = false;
                }
                return;
            }
            generation = s->generation;
            if (!s->staging && (s->queued.empty() || s->cpuConversionPending))
                return;
            auto *iface = window->rendererInterface();
            if (!iface || iface->graphicsApi() != QSGRendererInterface::Direct3D11)
                return;
            NativeCommandsScope native(window);
            auto *device = static_cast<ID3D11Device *>(
                iface->getResource(window, QSGRendererInterface::DeviceResource));
            auto *context = static_cast<ID3D11DeviceContext *>(
                iface->getResource(window, QSGRendererInterface::DeviceContextResource));
            if (!device || !context)
                return; // the 750ms deadline terminates admitted requests

            if (s->staging) {
                if (s->stagingDevice.Get() != device) {
                    failed.swap(s->submitted);
                    failure = "D3D11 device changed during readback";
                    releaseNative(*s);
                } else {
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    const HRESULT hr = context->Map(s->staging.Get(), 0,
                        D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
                    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
                        if (s->forcedRenders < kMaxExtraRenders) pump = true;
                        else {
                            failed.swap(s->submitted);
                            failure = "D3D11 staging Map not ready after bounded renders";
                            releaseNative(*s);
                        }
                    } else if (FAILED(hr)) {
                        failed.swap(s->submitted);
                        failure = "D3D11 nonblocking staging Map failed";
                        releaseNative(*s);
                    } else {
                        pitch = static_cast<std::size_t>(mapped.RowPitch);
                        width = s->pixelWidth;
                        height = s->pixelHeight;
                        rowBytes = static_cast<std::size_t>(width) * 4u;
                        dpr = s->dpr;
                        pts = s->submittedPts;
                        format = s->stagingDesc.Format;
                        const bool geometryValid = qRound(window->width()
                            * window->effectiveDevicePixelRatio()) == width
                            && qRound(window->height()
                            * window->effectiveDevicePixelRatio()) == height
                            && std::abs(double(window->effectiveDevicePixelRatio())
                                        - double(dpr)) < 0.0001;
                        if (geometryValid && pitch >= rowBytes && height > 0
                            && pitch <= std::numeric_limits<std::size_t>::max()
                                    / static_cast<std::size_t>(height)) {
                            bytes = pitch * static_cast<std::size_t>(height);
                            raw = std::shared_ptr<std::byte>(
                                new (std::nothrow) std::byte[bytes],
                                std::default_delete<std::byte[]>());
                            if (raw)
                                std::memcpy(raw.get(), mapped.pData, bytes);
                        }
                        context->Unmap(s->staging.Get(), 0);
                        if (raw) {
                            // The mapped result has been copied into owned
                            // CPU bytes and unmapped: safely reuse this same
                            // GPU staging allocation for the next request.
                            s->reusableStaging = std::move(s->staging);
                            s->reusableDevice = s->stagingDevice;
                            s->reusableDesc = s->stagingDesc;
                            s->converting.swap(s->submitted);
                            s->cpuConversionPending = true;
                            ++s->cpuWorkersInFlight;
                            releaseActiveReadback(*s);
                        } else {
                            failed.swap(s->submitted);
                            failure = "D3D11 resize or CPU readback allocation failed";
                            releaseNative(*s);
                        }
                        tryNext = !s->queued.empty() && !s->cpuConversionPending;
                    }
                }
            } else if (s->capturedTarget && !s->queued.empty()) {
                D3D11_TEXTURE2D_DESC desc{};
                s->capturedTarget->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.MiscFlags = 0;
                ComPtr<ID3D11Texture2D> staging;
                if (canReuseStaging(*s, device, desc)) {
                    staging = std::move(s->reusableStaging);
                    s->reusableDevice.Reset();
                    s->reusableDesc = {};
                    HYREMOTE_P1_COUNT(QuickD3d11StagingReuse);
                } else {
                    // A driver/format/size change invalidates the one-slot
                    // cache; never keep incompatible staging allocations.
                    s->reusableStaging.Reset();
                    s->reusableDevice.Reset();
                    s->reusableDesc = {};
                    if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr,
                                                         staging.GetAddressOf()))
                        && staging)
                        HYREMOTE_P1_COUNT(QuickD3d11StagingAllocation);
                }
                if (staging) {
                    // Borrow the Qt render target only until this
                    // same-frame CopyResource submission. D3D11 staging owns
                    // its CPU-readable copy independently after return.
                    ComPtr<ID3D11Texture2D> rendered = s->capturedTarget;
                    s->staging = std::move(staging);
                    s->stagingDevice = device;
                    s->stagingDesc = desc;
                    s->pixelWidth = static_cast<int>(desc.Width);
                    s->pixelHeight = static_cast<int>(desc.Height);
                    s->dpr = static_cast<float>(window->effectiveDevicePixelRatio());
                    s->submittedPts = s->capturedPts;
                    s->submitted.swap(s->queued);
                    s->capturedTarget.Reset();
                    s->forcedRenders = 0;
                    context->CopyResource(s->staging.Get(), rendered.Get());
                    pump = true;
                } else {
                    failed.swap(s->queued);
                    failure = "D3D11 staging allocation failed";
                    s->capturedTarget.Reset();
                }
            }
        }

        if (raw) {
            try {
                QThreadPool::globalInstance()->start(
                    [s, generation, raw = std::move(raw), bytes, pitch, rowBytes,
                     width, height, dpr, format, pts] {
                        WorkerDrain drain{s};
                        try {
                            finishCpu(s, generation, raw, bytes, pitch, rowBytes,
                                      width, height, dpr, format, pts);
                        } catch (...) {
                            std::vector<hyremote::CaptureRequest> abandoned;
                            {
                                std::lock_guard<std::mutex> lock(s->mutex);
                                if (s->active && s->generation == generation) {
                                    s->cpuConversionPending = false;
                                    abandoned.swap(s->converting);
                                }
                            }
                            if (!abandoned.empty()) {
                                try {
                                    deliver(s, generation, std::move(abandoned), {},
                                            0, 0, 1.f, {}, "D3D11 conversion worker failed");
                                } catch (...) {}
                            }
                        }
                    });
            } catch (...) {
                std::vector<hyremote::CaptureRequest> unscheduled;
                {
                    std::lock_guard<std::mutex> lock(s->mutex);
                    if (s->active && s->generation == generation) {
                        s->cpuConversionPending = false;
                        unscheduled.swap(s->converting);
                    }
                    --s->cpuWorkersInFlight;
                    s->drained.notify_all();
                }
                if (!unscheduled.empty())
                    deliver(s, generation, std::move(unscheduled), {},
                            0, 0, 1.f, {}, "D3D11 worker scheduling failed");
            }
        }
        if (!failed.empty())
            deliver(s, generation, std::move(failed), {},
                    0, 0, 1.f, {}, failure);
        if (pump)
            requestRetirementRender(s, window, generation);
        if (tryNext)
            QMetaObject::invokeMethod(window, [s, window, generation] {
                requestFallbackRender(s, window, generation);
            }, Qt::QueuedConnection);
    }

    std::shared_ptr<State> m_state;
};

} // namespace

std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickD3d11StagingCaptureSource(QQuickWindow *window)
{
    return std::make_unique<D3d11StagingCaptureSource>(window);
}

} // namespace HyRemote::detail
