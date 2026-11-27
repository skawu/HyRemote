#include "watermark_frame_decorator.hpp"

#include "hyremote/core/storage.hpp"
#include "watermark_compositor.hpp"
#include "perf_probe.hpp"

#include <QByteArray>

#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

namespace HyRemote::detail {

namespace {

// QPainter already composited into its owned QByteArray. Publish THAT same
// allocation through Core's immutable FrameStorage lifetime contract rather
// than allocating a second CpuFrameStorage and copying 8+ MiB again at 1080p.
// The QByteArray is never modified after this ownership transfer, and its
// thread-safe Qt reference count outlives all Core/RFB consumers.
class WatermarkByteArrayFrameStorage final : public hyremote::FrameStorage
{
public:
    static std::shared_ptr<const hyremote::FrameStorage> adopt(
        QByteArray pixels, std::size_t stride, std::size_t height)
    {
        if (stride == 0 || height == 0
            || stride > std::numeric_limits<std::size_t>::max() / height)
            return {};
        const std::size_t expected = stride * height;
        if (pixels.size() <= 0 || static_cast<std::size_t>(pixels.size()) != expected)
            return {};
        return std::shared_ptr<const hyremote::FrameStorage>(
            new WatermarkByteArrayFrameStorage(std::move(pixels), stride, expected));
    }

    hyremote::StorageKind kind() const noexcept override { return hyremote::StorageKind::Cpu; }
    std::size_t planeCount() const noexcept override { return 1U; }

    std::optional<hyremote::PlaneView> mapRead(std::size_t plane) const override
    {
        if (plane != 0U)
            return std::nullopt;
        return hyremote::PlaneView{
            reinterpret_cast<const std::byte *>(m_pixels.constData()),
            m_bytes,
            m_stride,
        };
    }

    std::string_view externalDomain() const noexcept override { return {}; }
    std::shared_ptr<const hyremote::StorageExtension> extension(std::string_view) const override
    {
        return {};
    }

private:
    WatermarkByteArrayFrameStorage(QByteArray pixels, std::size_t stride, std::size_t bytes)
        : m_pixels(std::move(pixels)), m_stride(stride), m_bytes(bytes)
    {
    }

    const QByteArray m_pixels;
    const std::size_t m_stride;
    const std::size_t m_bytes;
};

bool decorate(const WatermarkPolicy &policy, const hyremote::RemoteFrame &source,
              hyremote::RemoteFrame &decorated, QString &error)
{
    if (source.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888 || !source.storage) {
        error = QStringLiteral("the watermark needs an RGBA8888 CPU frame from the capture backend");
        return false;
    }

    const auto plane = source.storage->mapRead(0);
    if (!plane || !plane->data
        || plane->stride < static_cast<std::size_t>(source.geometry.size.width) * 4U) {
        error = QStringLiteral("the watermark could not read the captured frame plane");
        return false;
    }

    WatermarkFrameView view;
    view.rgba = reinterpret_cast<const uchar *>(plane->data);
    view.stride = static_cast<qsizetype>(plane->stride);
    view.width = static_cast<int>(source.geometry.size.width);
    view.height = static_cast<int>(source.geometry.size.height);
    view.premultiplied = source.geometry.alphaMode == hyremote::AlphaMode::Premultiplied;
    view.devicePixelRatio = source.geometry.devicePixelRatio > 0.0F
                                ? static_cast<double>(source.geometry.devicePixelRatio)
                                : 1.0;

    QByteArray composed;
    if (!composeWatermark(policy, view, composed, error)) {
        if (error.isEmpty())
            error = QStringLiteral("the watermark compositor produced no frame");
        return false;
    }

    // The composited QByteArray ALREADY owns the exact final pixels. Moving
    // it into an immutable CPU FrameStorage avoids a redundant allocation and
    // whole-frame memcpy. Invalid output never reaches the transport.
    auto storage = WatermarkByteArrayFrameStorage::adopt(
        std::move(composed), plane->stride, source.geometry.size.height);
    if (!storage) {
        error = QStringLiteral("the watermark compositor produced invalid owned frame storage");
        return false;
    }

    // Identity, geometry and timing remain those of the captured frame.
    // The RFB worker must NOT trust the source's Regions metadata for the
    // recomposed picture: this decorator can change pixels outside the
    // source region, and the worker derives exact delivered RGB damage.
    decorated = source;
    decorated.storage = std::move(storage);
    return true;
}

// This optimisation is intentionally local to a single, immutable watermark
// policy. Qt Quick's public grab batching (#910) may publish two independent
// Core requests which share exactly the same FrameStorage. Repainting that
// same source twice would allocate and copy another whole framebuffer twice,
// despite identical visible content.
//
// Weak references are essential: the worker retains its last *served* frame
// for diffing already; this cache must not pin an additional source or
// decorated 1080p allocation after consumers release it. Never cache by
// requestId, pointer address, frame timestamp or a "no scene render" hint.
struct WatermarkReuseCache
{
    std::mutex mutex;
    std::weak_ptr<const hyremote::FrameStorage> source;
    std::weak_ptr<const hyremote::FrameStorage> decorated;
    hyremote::FrameGeometry geometry;
    bool ready = false;
};

bool sameWatermarkGeometry(const hyremote::FrameGeometry &a,
                           const hyremote::FrameGeometry &b) noexcept
{
    return a.size.width == b.size.width && a.size.height == b.size.height
           && a.pixelFormat == b.pixelFormat && a.alphaMode == b.alphaMode
           && a.planeCount == b.planeCount
           && a.devicePixelRatio == b.devicePixelRatio
           && a.transform.rotation == b.transform.rotation
           && a.transform.mirrorX == b.transform.mirrorX
           && a.transform.mirrorY == b.transform.mirrorY;
}

}  // namespace

RfbFrameDecorator makeWatermarkFrameDecorator(const WatermarkPolicy &policy)
{
    if (!policy.enabled)
        return {};

    // Capture the immutable policy separately for each configured Runtime
    // session. This never reuses pixels between watermark policies, runs, or
    // different source storage/geometry, and introduces no global cache.
    auto cache = std::make_shared<WatermarkReuseCache>();
    return [policy, cache](const hyremote::RemoteFrame &source, hyremote::RemoteFrame &decorated,
                           QString &error) {
        std::lock_guard<std::mutex> guard(cache->mutex);
        if (source.storage && cache->ready
            && sameWatermarkGeometry(cache->geometry, source.geometry)) {
            const auto previousSource = cache->source.lock();
            const auto previousDecorated = cache->decorated.lock();
            if (previousSource && previousSource == source.storage && previousDecorated) {
                // Only the immutable pixels are shared. Always copy THIS
                // frame's id, request time, PTS, damage and metadata instead
                // of replaying an old RemoteFrame as a new completion.
                decorated = source;
                decorated.storage = previousDecorated;
                HYREMOTE_P1_COUNT(WatermarkReuse);
                error.clear();
                return true;
            }
        }

        bool composed = false;
        {
            // CPU-clock duration includes source copy, Qt painter and
            // immutable storage wrapping. Failed paint still costs CPU.
            // This scope excludes the cache hit path entirely.
            HYREMOTE_P1_SCOPE(WatermarkCompose);
            composed = decorate(policy, source, decorated, error);
        }
        if (!composed) {
            cache->ready = false;
            cache->source.reset();
            cache->decorated.reset();
            return false; // fail closed; never serve cached or unmarked pixels
        }

        cache->source = source.storage;
        cache->decorated = decorated.storage;
        cache->geometry = source.geometry;
        cache->ready = true;
        error.clear();
        return true;
    };
}

}  // namespace HyRemote::detail
