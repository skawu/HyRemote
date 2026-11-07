// #589 W2b: the transport hook must hand every viewer a composed frame while leaving the captured frame untouched,
// and must expose the exact frame identity the transport's damage bookkeeping depends on.

#include "../src/detail/watermark_frame_decorator.hpp"

#include "hyremote/core/storage.hpp"

#include <QGuiApplication>
#include <QString>
#include <QTextStream>

#include <cstring>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    QTextStream out(stdout);
    if (condition) {
        out << "ok: " << message << '\n';
        return;
    }
    out << "FAIL: " << message << '\n';
    ++failures;
}

using HyRemote::detail::WatermarkPolicy;
using HyRemote::detail::makeWatermarkFrameDecorator;

constexpr int kWidth = 320;
constexpr int kHeight = 200;

hyremote::RemoteFrame makeFrame(hyremote::FrameId id = 7)
{
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(kWidth * 4, kHeight);
    std::byte *plane = storage->mutablePlane(0);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            uchar *pixel = reinterpret_cast<uchar *>(plane) + (y * kWidth + x) * 4;
            pixel[0] = 30;
            pixel[1] = 32;
            pixel[2] = 34;
            pixel[3] = 255;
        }
    }

    hyremote::RemoteFrame frame;
    frame.id = id;
    frame.geometry.size = {kWidth, kHeight};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    frame.geometry.planeCount = 1;
    frame.storage = storage;
    frame.damage = hyremote::Damage::fullFrame();
    return frame;
}

QByteArray readPixels(const hyremote::RemoteFrame &frame)
{
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return {};
    return QByteArray(reinterpret_cast<const char *>(plane->data), static_cast<qsizetype>(plane->bytes));
}

WatermarkPolicy enabledPolicy()
{
    WatermarkPolicy policy;
    policy.enabled = true;
    policy.text = QStringLiteral("MARK");
    return policy;
}

void checkDisabledProducesNoDecorator()
{
    const auto decorator = makeWatermarkFrameDecorator(WatermarkPolicy{});
    check(!decorator, QStringLiteral("a disabled policy installs no decorator, so the capture frame is encoded as is"));
}

void checkCompositionPreservesIdentityAndSource()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy());
    check(static_cast<bool>(decorator), QStringLiteral("an enabled policy installs a decorator"));

    const hyremote::RemoteFrame source = makeFrame(11);
    const QByteArray sourcePixels = readPixels(source);

    hyremote::RemoteFrame decorated;
    QString error;
    check(decorator(source, decorated, error), QStringLiteral("the decorator produces a frame"));
    check(error.isEmpty(), QStringLiteral("a valid decoration reports no error"));

    check(decorated.id == source.id, QStringLiteral("the frame id is preserved for damage bookkeeping"));
    check(decorated.geometry.size.width == source.geometry.size.width
              && decorated.geometry.size.height == source.geometry.size.height,
          QStringLiteral("the geometry is preserved"));
    check(decorated.damage.kind == source.damage.kind,
          QStringLiteral("the capture damage declaration is preserved"));
    check(decorated.storage != source.storage, QStringLiteral("the decorated frame owns a separate buffer"));
    check(readPixels(source) == sourcePixels, QStringLiteral("the captured frame is never modified"));

    const QByteArray decoratedPixels = readPixels(decorated);
    check(decoratedPixels.size() == sourcePixels.size(), QStringLiteral("the decorated frame keeps its size"));
    check(decoratedPixels != sourcePixels, QStringLiteral("the decorated frame carries the watermark"));
}

void checkDevicePixelRatioComesFromTheFrame()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy());

    hyremote::RemoteFrame atOneX = makeFrame();
    atOneX.geometry.devicePixelRatio = 1.0F;
    hyremote::RemoteFrame atTwoX = makeFrame();
    atTwoX.geometry.devicePixelRatio = 2.0F;

    hyremote::RemoteFrame one;
    hyremote::RemoteFrame two;
    QString error;
    check(decorator(atOneX, one, error), QStringLiteral("the 1x frame decorates"));
    check(decorator(atTwoX, two, error), QStringLiteral("the 2x frame decorates"));
    check(readPixels(one) != readPixels(two),
          QStringLiteral("the frame's own device pixel ratio changes the composed pixels, not a startup value"));
    check(two.geometry.devicePixelRatio == 2.0F,
          QStringLiteral("the decorated frame keeps the capture's device pixel ratio"));
}

// The compositor's QImage and temporary QByteArray have already gone out
// of scope when transport reads this FrameStorage. Check the moved pixels
// survive independently of the source and are never exposed as mutable data.
void checkOwnedComposedStorageOutlivesSourceAndKeepsPaddedRows()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy());
    hyremote::RemoteFrame composed;
    std::weak_ptr<const hyremote::FrameStorage> oldSource;
    QByteArray referencePixels;
    constexpr std::size_t paddedStride = static_cast<std::size_t>(kWidth) * 4U + 16U;
    {
        auto original = makeFrame(43);
        auto padded = hyremote::CpuFrameStorage::createSinglePlane(paddedStride, kHeight);
        check(padded && padded->mutablePlane(0),
              QStringLiteral("padded source storage allocates"));
        if (!padded || !padded->mutablePlane(0))
            return;
        const auto src = original.storage->mapRead(0);
        auto *dst = padded->mutablePlane(0);
        if (!src || !src->data)
            return;
        for (int y = 0; y < kHeight; ++y) {
            std::memcpy(dst + static_cast<std::size_t>(y) * paddedStride,
                        src->data + static_cast<std::size_t>(y) * src->stride,
                        static_cast<std::size_t>(kWidth) * 4U);
            std::memset(dst + static_cast<std::size_t>(y) * paddedStride
                              + static_cast<std::size_t>(kWidth) * 4U,
                        0xA5, 16);
        }
        original.storage = std::move(padded);
        oldSource = original.storage;
        QString error;
        check(decorator(original, composed, error),
              QStringLiteral("padded watermark composes into owned storage"));
        check(error.isEmpty(), QStringLiteral("padded watermark has no error"));
        if (!composed.storage)
            return;
        const auto plane = composed.storage->mapRead(0);
        check(plane.has_value(), QStringLiteral("owned composited plane can be read"));
        if (!plane)
            return;
        check(plane->stride == paddedStride
                  && plane->bytes == paddedStride * static_cast<std::size_t>(kHeight),
              QStringLiteral("owned storage keeps source stride and byte count"));
        referencePixels = readPixels(composed);
        for (int y = 0; y < kHeight; ++y) {
            const auto pad = reinterpret_cast<const unsigned char *>(plane->data)
                             + static_cast<std::size_t>(y) * paddedStride
                             + static_cast<std::size_t>(kWidth) * 4U;
            for (std::size_t j = 0; j < 16; ++j)
                check(pad[j] == 0xA5,
                      QStringLiteral("compositor does not overwrite row padding"));
        }
    }
    check(oldSource.expired(),
          QStringLiteral("composed frame retains no source frame allocation"));
    check(composed.storage->kind() == hyremote::StorageKind::Cpu,
          QStringLiteral("composed storage presents a plain CPU plane"));
    check(!composed.storage->mapRead(1).has_value(),
          QStringLiteral("composed storage refuses a nonexistent second plane"));
    check(readPixels(composed) == referencePixels,
          QStringLiteral("final watermarked bytes survive source and compositor destruction"));

    hyremote::RemoteFrame shared = composed;
    composed.storage.reset();
    check(readPixels(shared) == referencePixels,
          QStringLiteral("Core frame copies preserve the immutable composed pixel owner"));
}

void checkUnusableFrameIsRefused()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy());

    hyremote::RemoteFrame unsupported = makeFrame();
    unsupported.geometry.pixelFormat = hyremote::PixelFormat::Bgra8888;
    hyremote::RemoteFrame decorated;
    QString error;
    check(!decorator(unsupported, decorated, error),
          QStringLiteral("a non-RGBA8888 capture is refused instead of streamed unwatermarked"));
    check(!error.isEmpty(), QStringLiteral("the refusal explains itself"));

    hyremote::RemoteFrame noStorage = makeFrame();
    noStorage.storage.reset();
    QString missingError;
    check(!decorator(noStorage, decorated, missingError), QStringLiteral("a frame without storage is refused"));
    check(!missingError.isEmpty(), QStringLiteral("the missing-storage refusal explains itself"));
}

void checkTextThatCannotFitIsRefused()
{
    auto policy = enabledPolicy();
    policy.text = QString(128, QLatin1Char('W'));
    const auto decorator = makeWatermarkFrameDecorator(policy);

    const hyremote::RemoteFrame source = makeFrame();
    hyremote::RemoteFrame decorated;
    QString error;
    check(!decorator(source, decorated, error),
          QStringLiteral("text that cannot fit the frame refuses composition"));
    check(error.contains(QStringLiteral("does not fit")), QStringLiteral("the refusal names the reason"));
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);

    checkDisabledProducesNoDecorator();
    checkCompositionPreservesIdentityAndSource();
    checkOwnedComposedStorageOutlivesSourceAndKeepsPaddedRows();
    checkDevicePixelRatioComesFromTheFrame();
    checkUnusableFrameIsRefused();
    checkTextThatCannotFitIsRefused();

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark frame decorator checks passed\n"
                          : "watermark frame decorator checks failed\n");
    return failures == 0 ? 0 : 2;
}
