#include "watermark_frame_decorator.hpp"

#include "hyremote/core/storage.hpp"
#include "watermark_compositor.hpp"

#include <QByteArray>

#include <cstring>

namespace HyRemote::detail {

namespace {

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

    auto storage = hyremote::CpuFrameStorage::createSinglePlane(plane->stride, source.geometry.size.height);
    if (!storage) {
        error = QStringLiteral("the watermark compositor could not allocate frame storage");
        return false;
    }
    std::byte *target = storage->mutablePlane(0);
    if (!target || static_cast<std::size_t>(composed.size()) != storage->totalBytes()) {
        error = QStringLiteral("the watermark compositor produced a frame of the wrong size");
        return false;
    }
    std::memcpy(target, composed.constData(), static_cast<std::size_t>(composed.size()));

    // Everything except the pixels is the captured frame's own truth: id, geometry, timing and damage stay
    // identical so the transport's damage/refresh bookkeeping keeps working against the decorated picture.
    decorated = source;
    decorated.storage = std::move(storage);
    return true;
}

}  // namespace

RfbFrameDecorator makeWatermarkFrameDecorator(const WatermarkPolicy &policy)
{
    if (!policy.enabled)
        return {};

    return [policy](const hyremote::RemoteFrame &source, hyremote::RemoteFrame &decorated,
                    QString &error) { return decorate(policy, source, decorated, error); };
}

}  // namespace HyRemote::detail
