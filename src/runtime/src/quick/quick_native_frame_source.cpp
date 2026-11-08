#include "quick/quick_native_frame_source.hpp"

#if !defined(HYREMOTE_EXPERIMENTAL_QUICK_NATIVE_FRAME)
#  error "Qt Private native-frame source must be compiled only with explicit opt-in"
#endif

#include <QDebug>
#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>

namespace HyRemote::detail {
namespace {

long long monoMs() noexcept
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

struct PendingReadback
{
    QRhiReadbackResult result;
    QSize expectedSize;
    double dpr = 1.0;
    std::uint64_t epoch = 0;
    std::uint64_t renderSerial = 0;
    long long sourceMs = 0;
};

} // namespace

struct QuickNativeFrameSource::State
{
    std::atomic<bool> enabled{false};
    std::atomic<bool> invalidated{false};
    std::atomic<bool> qualified{false};
    std::atomic<bool> requested{false};
    std::atomic<bool> inFlight{false};
    std::atomic<std::uint64_t> afterRenderingSignals{0};
    std::atomic<std::uint64_t> noRhi{0};
    std::atomic<std::uint64_t> noSwapchain{0};
    std::atomic<long long> lastRenderMs{0};
    std::atomic<std::uint64_t> serial{0};
    std::atomic<std::uint64_t> epoch{1};
    std::atomic<int> surfaceWidth{0};
    std::atomic<int> surfaceHeight{0};
    std::atomic<std::uint64_t> backendIneligible{0};
    std::atomic<std::uint64_t> transferSourceUnavailable{0};
    std::atomic<std::uint64_t> eligibleRenders{0};
    std::atomic<std::uint64_t> readbacksSubmitted{0};
    std::atomic<std::uint64_t> readbacksCompleted{0};
    std::atomic<std::uint64_t> samplesTaken{0};

    // One latest sample, for one Core request. No samples are taken at idle;
    // any unclaimed completed image is dropped on the next request or stop.
    std::mutex sampleMutex;
    std::optional<QuickNativeSample> sample;
};

QuickNativeFrameSource::QuickNativeFrameSource(QQuickWindow *window)
    : m_window(window), m_state(std::make_shared<State>())
{
}

QuickNativeFrameSource::~QuickNativeFrameSource() { stop(); }

void QuickNativeFrameSource::start()
{
    if (!m_window || m_afterRendering)
        return;
    auto state = m_state;
    state->invalidated.store(false, std::memory_order_release);
    state->enabled.store(true, std::memory_order_release);

    m_invalidated = QObject::connect(
        m_window, &QQuickWindow::sceneGraphInvalidated, m_window,
        [state] {
            state->invalidated.store(true, std::memory_order_release);
            state->qualified.store(false, std::memory_order_release);
            state->lastRenderMs.store(0, std::memory_order_release);
            state->requested.store(false, std::memory_order_release);
            state->epoch.fetch_add(1, std::memory_order_acq_rel);
            std::lock_guard<std::mutex> lock(state->sampleMutex);
            state->sample.reset();
        }, Qt::DirectConnection);
    m_initialized = QObject::connect(
        m_window, &QQuickWindow::sceneGraphInitialized, m_window,
        [state] {
            state->invalidated.store(false, std::memory_order_release);
            state->epoch.fetch_add(1, std::memory_order_acq_rel);
        }, Qt::DirectConnection);

    m_afterRendering = QObject::connect(
        m_window, &QQuickWindow::afterRendering, m_window,
        [window = m_window, state] {
            if (!state->enabled.load(std::memory_order_acquire)
                || state->invalidated.load(std::memory_order_acquire))
                return;
            state->afterRenderingSignals.fetch_add(1, std::memory_order_relaxed);
            // Qt 6.6+ exposes both resources through QQuickWindow public
            // accessors. Only the QRhi readback protocol is QtGuiPrivate;
            // QtQuickPrivate's version-sensitive QQuickWindowPrivate layout
            // must never be a production capture dependency.
            QRhi *rhi = window->rhi();
            QRhiSwapChain *swapchain = window->swapChain();
            if (!rhi) {
                state->noRhi.fetch_add(1, std::memory_order_relaxed);
                state->qualified.store(false, std::memory_order_release);
                return;
            }
            if (!swapchain) {
                state->noSwapchain.fetch_add(1, std::memory_order_relaxed);
                state->qualified.store(false, std::memory_order_release);
                return;
            }
            // A backend is a qualified *adapter*, not a product platform.
            // D3D11 is the single already-measured backend; others fall back
            // until proven with their exact Qt Kit and framebuffer semantics.
            // Crucially, an arbitrary Qt Quick swapchain MUST NOT be read
            // without its creation-time UsedAsTransferSource flag.
            const bool supportedBackend = rhi->backend() == QRhi::D3D11;
            const bool transferSource = swapchain->flags().testFlag(
                QRhiSwapChain::UsedAsTransferSource);
            if (!supportedBackend)
                state->backendIneligible.fetch_add(1, std::memory_order_relaxed);
            if (supportedBackend && !transferSource)
                state->transferSourceUnavailable.fetch_add(1, std::memory_order_relaxed);
            // Orientation must match the top-down Core RGBA contract.
            // Do not silently publish a vertically inverted native frame.
            // An upside-down backend needs a separately qualified row-flip
            // adapter, not a generalization of the D3D11 observation.
            const bool eligible = supportedBackend && transferSource
                && swapchain->flags().testFlag(QRhiSwapChain::SurfaceHasPreMulAlpha)
                && !rhi->isYUpInFramebuffer();
            state->qualified.store(eligible, std::memory_order_release);
            if (!eligible)
                return;
            state->eligibleRenders.fetch_add(1, std::memory_order_relaxed);

            const QSize physical = swapchain->surfacePixelSize();
            if (physical.width() <= 0 || physical.height() <= 0)
                return;
            const int prevW = state->surfaceWidth.exchange(physical.width());
            const int prevH = state->surfaceHeight.exchange(physical.height());
            if (prevW != 0 && (prevW != physical.width() || prevH != physical.height())) {
                state->epoch.fetch_add(1, std::memory_order_acq_rel);
                std::lock_guard<std::mutex> lock(state->sampleMutex);
                state->sample.reset();
            }
            state->serial.fetch_add(1, std::memory_order_acq_rel);
            state->lastRenderMs.store(monoMs(), std::memory_order_release);

            if (!state->requested.exchange(false, std::memory_order_acq_rel))
                return;
            if (state->inFlight.exchange(true, std::memory_order_acq_rel)) {
                state->requested.store(true, std::memory_order_release);
                return;
            }
            QRhiCommandBuffer *cb = swapchain->currentFrameCommandBuffer();
            if (!cb) {
                state->inFlight.store(false, std::memory_order_release);
                return;
            }
            QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
            if (!batch) {
                state->inFlight.store(false, std::memory_order_release);
                return;
            }
            // No render-pass restart, no blocking finish(), no GUI/Core call.
            // The callback runs in Qt's QRhi thread and owns its bytes until
            // the GUI-side adapter polls them.
            auto *pending = new PendingReadback;
            pending->expectedSize = physical;
            pending->dpr = window->width() > 0
                ? static_cast<double>(physical.width()) / static_cast<double>(window->width())
                : 1.0;
            pending->epoch = state->epoch.load(std::memory_order_acquire);
            pending->renderSerial = state->serial.load(std::memory_order_acquire);
            pending->sourceMs = monoMs();
            pending->result.completed = [state, pending] {
                const int width = pending->result.pixelSize.width();
                const int height = pending->result.pixelSize.height();
                const std::size_t bytes = static_cast<std::size_t>(pending->result.data.size());
                const bool sizeOk = width > 0 && height > 0
                    && width == pending->expectedSize.width()
                    && height == pending->expectedSize.height()
                    && static_cast<std::size_t>(width) <=
                        std::numeric_limits<std::size_t>::max() / 4U
                    && static_cast<std::size_t>(height) <=
                        std::numeric_limits<std::size_t>::max()
                        / (static_cast<std::size_t>(width) * 4U)
                    && bytes == static_cast<std::size_t>(width) * height * 4U;
                const bool formatOk = pending->result.format == QRhiTexture::RGBA8
                    || pending->result.format == QRhiTexture::BGRA8;
                if (sizeOk && formatOk
                    && state->enabled.load(std::memory_order_acquire)
                    && !state->invalidated.load(std::memory_order_acquire)
                    && state->epoch.load(std::memory_order_acquire) == pending->epoch) {
                    QuickNativeSample sample;
                    sample.pixels = std::move(pending->result.data);
                    sample.width = width;
                    sample.height = height;
                    sample.format = static_cast<int>(pending->result.format);
                    sample.dpr = pending->dpr;
                    sample.epoch = pending->epoch;
                    sample.renderSerial = pending->renderSerial;
                    sample.sourceMs = pending->sourceMs;
                    std::lock_guard<std::mutex> lock(state->sampleMutex);
                    state->sample = std::move(sample);
                }
                state->readbacksCompleted.fetch_add(1, std::memory_order_relaxed);
                state->inFlight.store(false, std::memory_order_release);
                delete pending;
            };
            state->readbacksSubmitted.fetch_add(1, std::memory_order_relaxed);
            state->inFlight.store(true, std::memory_order_release);
            batch->readBackTexture(QRhiReadbackDescription{}, &pending->result);
            cb->resourceUpdate(batch);
        }, Qt::DirectConnection);
}

void QuickNativeFrameSource::stop() noexcept
{
    if (!m_state->enabled.exchange(false, std::memory_order_acq_rel))
        return;
    m_state->requested.store(false, std::memory_order_release);
    m_state->qualified.store(false, std::memory_order_release);
    m_state->epoch.fetch_add(1, std::memory_order_acq_rel);
    QObject::disconnect(m_afterRendering);
    QObject::disconnect(m_invalidated);
    QObject::disconnect(m_initialized);
    m_afterRendering = {};
    m_invalidated = {};
    m_initialized = {};
    std::lock_guard<std::mutex> lock(m_state->sampleMutex);
    m_state->sample.reset();
    if (qEnvironmentVariable("HYREMOTE_EXPECT_NATIVE_HIT") == "1")
        qWarning().nospace() << "P3_RHI_DIAGNOSTIC after_rendering="
            << m_state->afterRenderingSignals.load(std::memory_order_relaxed)
            << " no_rhi=" << m_state->noRhi.load(std::memory_order_relaxed)
            << " no_swapchain=" << m_state->noSwapchain.load(std::memory_order_relaxed)
            << " unsupported_backend=" << m_state->backendIneligible.load(std::memory_order_relaxed)
            << " missing_transfer_source=" << m_state->transferSourceUnavailable.load(std::memory_order_relaxed)
            << " eligible_render=" << m_state->eligibleRenders.load(std::memory_order_relaxed)
            << " readbacks_completed=" << m_state->readbacksCompleted.load(std::memory_order_relaxed);
    qInfo().nospace()
        << "HyRemote P3 native pilot: eligible_render="
        << m_state->eligibleRenders.load(std::memory_order_relaxed)
        << " after_rendering="
        << m_state->afterRenderingSignals.load(std::memory_order_relaxed)
        << " no_rhi="
        << m_state->noRhi.load(std::memory_order_relaxed)
        << " no_swapchain="
        << m_state->noSwapchain.load(std::memory_order_relaxed)
        << " unsupported_backend="
        << m_state->backendIneligible.load(std::memory_order_relaxed)
        << " missing_transfer_source="
        << m_state->transferSourceUnavailable.load(std::memory_order_relaxed)
        << " readback_submitted="
        << m_state->readbacksSubmitted.load(std::memory_order_relaxed)
        << " readback_completed="
        << m_state->readbacksCompleted.load(std::memory_order_relaxed)
        << " sample_taken="
        << m_state->samplesTaken.load(std::memory_order_relaxed);
}

bool QuickNativeFrameSource::renderActive(long long nowMs, long long maxAgeMs) const noexcept
{
    const long long last = m_state->lastRenderMs.load(std::memory_order_acquire);
    return m_state->enabled.load(std::memory_order_acquire)
        && m_state->qualified.load(std::memory_order_acquire)
        && last > 0 && nowMs >= last && nowMs - last <= maxAgeMs;
}

std::uint64_t QuickNativeFrameSource::renderSerial() const noexcept
{
    return m_state->serial.load(std::memory_order_acquire);
}

std::uint64_t QuickNativeFrameSource::epoch() const noexcept
{
    return m_state->epoch.load(std::memory_order_acquire);
}

void QuickNativeFrameSource::requestNextRender() noexcept
{
    m_state->requested.store(true, std::memory_order_release);
}

void QuickNativeFrameSource::cancelRequest() noexcept
{
    m_state->requested.store(false, std::memory_order_release);
}

bool QuickNativeFrameSource::takeSample(QuickNativeSample &out)
{
    std::lock_guard<std::mutex> lock(m_state->sampleMutex);
    if (!m_state->sample)
        return false;
    out = std::move(*m_state->sample);
    m_state->sample.reset();
    m_state->samplesTaken.fetch_add(1, std::memory_order_relaxed);
    return true;
}

} // namespace HyRemote::detail
