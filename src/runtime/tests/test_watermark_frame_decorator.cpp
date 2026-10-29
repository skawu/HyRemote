// #589 W2b: the transport hook must hand every viewer a composed frame while leaving the captured frame untouched,
// and must expose the exact frame identity the transport's damage bookkeeping depends on.

#include "../src/detail/watermark_frame_decorator.hpp"

#include "hyremote/core/storage.hpp"

#include <QGuiApplication>
#include <QString>
#include <QTextStream>

#include <cstring>

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
    const auto decorator = makeWatermarkFrameDecorator(WatermarkPolicy{}, 1.0);
    check(!decorator, QStringLiteral("a disabled policy installs no decorator, so the capture frame is encoded as is"));
}

void checkCompositionPreservesIdentityAndSource()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy(), 1.0);
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

void checkDevicePixelRatioIsUsed()
{
    const auto at1x = makeWatermarkFrameDecorator(enabledPolicy(), 1.0);
    const auto at2x = makeWatermarkFrameDecorator(enabledPolicy(), 2.0);

    const hyremote::RemoteFrame source = makeFrame();
    hyremote::RemoteFrame one;
    hyremote::RemoteFrame two;
    QString error;
    check(at1x(source, one, error), QStringLiteral("the 1x decorator produces a frame"));
    check(at2x(source, two, error), QStringLiteral("the 2x decorator produces a frame"));
    check(readPixels(one) != readPixels(two),
          QStringLiteral("the target device pixel ratio changes the composed pixels"));
}

void checkUnusableFrameIsRefused()
{
    const auto decorator = makeWatermarkFrameDecorator(enabledPolicy(), 1.0);

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
    const auto decorator = makeWatermarkFrameDecorator(policy, 1.0);

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
    checkDevicePixelRatioIsUsed();
    checkUnusableFrameIsRefused();
    checkTextThatCannotFitIsRefused();

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark frame decorator checks passed\n"
                          : "watermark frame decorator checks failed\n");
    return failures == 0 ? 0 : 2;
}
