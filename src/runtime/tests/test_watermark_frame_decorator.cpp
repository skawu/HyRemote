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
        bool paddingUntouched = true;
        for (int y = 0; y < kHeight; ++y) {
            const auto pad = reinterpret_cast<const unsigned char *>(plane->data)
                             + static_cast<std::size_t>(y) * paddedStride
                             + static_cast<std::size_t>(kWidth) * 4U;
            for (std::size_t j = 0; j < 16; ++j)
                paddingUntouched = paddingUntouched && pad[j] == 0xA5;
        }
        check(paddingUntouched,
              QStringLiteral("compositor preserves all 200 padded row sentinels"));
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

// #609/#910: two independent Core requests can share ONE already captured
// immutable Qt Quick frame. That is not permission to duplicate an 8 MiB
// watermark copy/paint, nor permission to replay its older frame metadata.
void checkIdenticalSourceStorageReusesOnlyComposedPixels()
{
    auto decorator = makeWatermarkFrameDecorator(enabledPolicy());
    auto original = makeFrame(70);
    original.timing.requestTime = hyremote::Clock::now();
    original.requestId = 700U;
    hyremote::RemoteFrame first;
    QString error;
    check(decorator(original, first, error),
          QStringLiteral("first shared-grab frame composes successfully"));

    auto secondSource = original; // SAME immutable pixel owner, NEW Core identity
    secondSource.id = 71;
    secondSource.requestId = 701U;
    secondSource.timing.requestTime = hyremote::Clock::now();
    secondSource.timing.completionTime = hyremote::Clock::now();
    secondSource.damage = hyremote::Damage::unknown();
    hyremote::RemoteFrame second;
    check(decorator(secondSource, second, error),
          QStringLiteral("second shared-grab frame reuses composition"));
    check(first.storage && first.storage == second.storage,
          QStringLiteral("duplicate immutable input does not allocate a second watermarked framebuffer"));
    check(second.storage != secondSource.storage,
          QStringLiteral("cached output remains marked instead of exposing the source"));
    check(second.id == secondSource.id && second.requestId == secondSource.requestId
              && second.timing.requestTime == secondSource.timing.requestTime
              && second.timing.completionTime == secondSource.timing.completionTime
              && second.damage.kind == secondSource.damage.kind,
          QStringLiteral("reuse copies fresh ID, times and damage rather than the old frame"));
    check(!secondSource.storage || readPixels(secondSource) == readPixels(original),
          QStringLiteral("watermark cache does not modify source pixels"));

    // Equal visual bytes in a DISTINCT allocation must be composed again:
    // there is no hash/dirty-signal shortcut that can hide page changes.
    auto independent = makeFrame(72);
    hyremote::RemoteFrame third;
    check(decorator(independent, third, error),
          QStringLiteral("new source allocation composes independently"));
    check(third.storage && third.storage != second.storage,
          QStringLiteral("equal-looking but independently owned frames are not reused"));
}

void checkReuseInvalidatesOnGeometryAndFailsClosed()
{
    auto decorator = makeWatermarkFrameDecorator(enabledPolicy());
    auto source = makeFrame(80);
    hyremote::RemoteFrame first;
    QString error;
    check(decorator(source, first, error),
          QStringLiteral("source with first DPR is watermarked"));

    auto newDpr = source;
    newDpr.id = 81;
    newDpr.geometry.devicePixelRatio = 2.0F;
    hyremote::RemoteFrame second;
    check(decorator(newDpr, second, error),
          QStringLiteral("same pixels at different DPR are recomposed"));
    check(first.storage && second.storage && first.storage != second.storage,
          QStringLiteral("DPR change invalidates cached watermarked storage"));
    if (first.storage && second.storage)
        check(readPixels(first) != readPixels(second),
              QStringLiteral("new DPR changes actual watermark pixels"));

    auto invalid = newDpr;
    invalid.id = 82;
    invalid.geometry.pixelFormat = hyremote::PixelFormat::Bgra8888;
    hyremote::RemoteFrame badOutput;
    check(!decorator(invalid, badOutput, error),
          QStringLiteral("unsupported source cannot reuse a previously marked image"));
    check(!error.isEmpty(),
          QStringLiteral("invalid same-storage frame retains a failure diagnostic"));
    auto recovered = newDpr;
    recovered.id = 83;
    hyremote::RemoteFrame recoveredOutput;
    check(decorator(recovered, recoveredOutput, error),
          QStringLiteral("valid source still recovers after rejected frame"));
    check(error.isEmpty(), QStringLiteral("successful recovery clears stale error"));
    check(recoveredOutput.storage && recoveredOutput.storage != second.storage,
          QStringLiteral("recovery composes new pixels after fail-closed invalidation"));

    auto differentPolicy = enabledPolicy();
    differentPolicy.colorRgb = 0x00FF00U;
    auto independentDecorator = makeWatermarkFrameDecorator(differentPolicy);
    hyremote::RemoteFrame otherPolicyOutput;
    check(independentDecorator(recovered, otherPolicyOutput, error),
          QStringLiteral("independent watermark policy composes"));
    check(otherPolicyOutput.storage && otherPolicyOutput.storage != recoveredOutput.storage,
          QStringLiteral("two separate policy instances do not share cache identity"));
    if (otherPolicyOutput.storage && recoveredOutput.storage)
        check(readPixels(otherPolicyOutput) != readPixels(recoveredOutput),
              QStringLiteral("watermark colour policy changes the actual served pixels"));
}

// No retained strong ownership: an idle/closed RFB worker must not keep the
// extra captured 1080p source or decorated frame pinned solely for caching.
void checkReuseCacheDoesNotPinFrameMemory()
{
    auto decorator = makeWatermarkFrameDecorator(enabledPolicy());
    std::weak_ptr<const hyremote::FrameStorage> captured;
    std::weak_ptr<const hyremote::FrameStorage> decorated;
    {
        auto source = makeFrame(90);
        hyremote::RemoteFrame painted;
        QString error;
        check(decorator(source, painted, error),
              QStringLiteral("weak ownership test composes frame"));
        captured = source.storage;
        decorated = painted.storage;
        check(!captured.expired() && !decorated.expired(),
              QStringLiteral("sources are alive while a consumer holds them"));
    }
    check(captured.expired() && decorated.expired(),
          QStringLiteral("per-run decorator cache pins neither full-frame allocation"));

    auto replacement = makeFrame(91);
    hyremote::RemoteFrame second;
    QString error;
    check(decorator(replacement, second, error),
          QStringLiteral("a new source composes after weak cache anchors expired"));
    check(second.id == replacement.id && second.storage != replacement.storage,
          QStringLiteral("expired caches never publish an unmarked or stale frame"));
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
    checkIdenticalSourceStorageReusesOnlyComposedPixels();
    checkReuseInvalidatesOnGeometryAndFailsClosed();
    checkReuseCacheDoesNotPinFrameMemory();
    checkUnusableFrameIsRefused();
    checkTextThatCannotFitIsRefused();

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark frame decorator checks passed\n"
                          : "watermark frame decorator checks failed\n");
    return failures == 0 ? 0 : 2;
}
