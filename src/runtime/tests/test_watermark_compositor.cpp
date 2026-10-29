// #589 W2: the compositor must paint the shared policy onto a copy of the frame - never onto the capture buffer -
// and must reproduce the Owner decision (bottom-right single line by default, tiled only on request, 0.35 opacity).

#include "../src/detail/watermark_compositor.hpp"

#include <QByteArray>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QString>
#include <QTextStream>

#include <algorithm>
#include <cmath>
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

using HyRemote::detail::WatermarkFrameView;
using HyRemote::detail::WatermarkPolicy;
using HyRemote::detail::WatermarkPosition;
using HyRemote::detail::composeWatermark;

QByteArray makeFrame(int width, int height, qsizetype *stride = nullptr)
{
    const qsizetype rowBytes = static_cast<qsizetype>(width) * 4;
    QByteArray frame(rowBytes * height, '\0');
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uchar *pixel = reinterpret_cast<uchar *>(frame.data()) + y * rowBytes + x * 4;
            pixel[0] = 30;
            pixel[1] = 32;
            pixel[2] = 34;
            pixel[3] = 255;
        }
    }
    if (stride)
        *stride = rowBytes;
    return frame;
}

WatermarkFrameView viewOf(const QByteArray &frame, int width, int height, qsizetype stride, double dpr = 1.0)
{
    WatermarkFrameView view;
    view.rgba = reinterpret_cast<const uchar *>(frame.constData());
    view.width = width;
    view.height = height;
    view.stride = stride;
    view.premultiplied = true;
    view.devicePixelRatio = dpr;
    return view;
}

qsizetype changedPixels(const QByteArray &before, const QByteArray &after)
{
    if (before.size() != after.size())
        return -1;
    qsizetype changed = 0;
    for (qsizetype offset = 0; offset + 4 <= before.size(); offset += 4) {
        if (std::memcmp(before.constData() + offset, after.constData() + offset, 4) != 0)
            ++changed;
    }
    return changed;
}

qsizetype changedPixelsInRect(const QByteArray &before, const QByteArray &after, qsizetype stride,
                              int x0, int y0, int x1, int y1)
{
    qsizetype changed = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const qsizetype offset = y * stride + static_cast<qsizetype>(x) * 4;
            if (std::memcmp(before.constData() + offset, after.constData() + offset, 4) != 0)
                ++changed;
        }
    }
    return changed;
}

int brightestChannel(const QByteArray &frame)
{
    int brightest = 0;
    for (qsizetype offset = 0; offset + 4 <= frame.size(); offset += 4) {
        brightest = std::max({brightest,
                              static_cast<int>(static_cast<uchar>(frame.at(offset))),
                              static_cast<int>(static_cast<uchar>(frame.at(offset + 1))),
                              static_cast<int>(static_cast<uchar>(frame.at(offset + 2)))});
    }
    return brightest;
}

WatermarkPolicy enabledPolicy(const QString &text)
{
    WatermarkPolicy policy;
    policy.enabled = true;
    policy.text = text;
    return policy;
}

void checkDisabledDoesNothing()
{
    const int width = 160;
    const int height = 100;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);
    QByteArray destination;
    QString error;

    WatermarkPolicy policy;  // disabled
    check(!composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
          QStringLiteral("a disabled policy composes nothing"));
    check(destination.isEmpty(), QStringLiteral("a disabled policy produces no buffer"));
    check(error.isEmpty(), QStringLiteral("a disabled policy is not an error"));
}

void checkSingleCornerAndSourceIntegrity()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);
    const QByteArray sourceCopy = frame;

    QByteArray destination;
    QString error;
    const WatermarkPolicy policy = enabledPolicy(QStringLiteral("MARK"));

    check(composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
          QStringLiteral("an enabled policy composes a frame"));
    check(error.isEmpty(), QStringLiteral("a valid composition reports no error"));
    check(destination.size() == stride * height, QStringLiteral("the watermarked frame keeps the geometry"));
    check(frame == sourceCopy, QStringLiteral("the capture buffer is never modified"));

    const qsizetype changed = changedPixels(frame, destination);
    check(changed > 0, QStringLiteral("the watermarked frame differs from the capture"));

    const qsizetype leftHalf = changedPixelsInRect(frame, destination, stride, 0, 0, width / 2, height);
    const qsizetype topHalf = changedPixelsInRect(frame, destination, stride, 0, 0, width, height / 2);
    check(leftHalf == 0, QStringLiteral("the default single line stays out of the left half"));
    check(topHalf == 0, QStringLiteral("the default single line stays out of the top half"));
    check(changedPixelsInRect(frame, destination, stride, width / 2, height / 2, width, height) > 0,
          QStringLiteral("the default single line is drawn in the bottom-right corner"));
}

void checkExplicitTopLeft()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);

    WatermarkPolicy policy = enabledPolicy(QStringLiteral("TOP LEFT"));
    policy.position = WatermarkPosition::TopLeft;

    QByteArray destination;
    QString error;
    check(composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
          QStringLiteral("an explicit top-left policy composes"));
    check(changedPixelsInRect(frame, destination, stride, 0, 0, width / 2, height / 2) > 0,
          QStringLiteral("the top-left quadrant carries the watermark"));
    check(changedPixelsInRect(frame, destination, stride, width / 2, height / 2, width, height) == 0,
          QStringLiteral("the bottom-right quadrant is untouched"));
}

void checkTiled()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);

    WatermarkPolicy policy = enabledPolicy(QStringLiteral("DO NOT SHARE"));
    policy.position = WatermarkPosition::Tiled;

    QByteArray destination;
    QString error;
    check(composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
          QStringLiteral("a tiled policy composes"));

    const int halfWidth = width / 2;
    const int halfHeight = height / 2;
    check(changedPixelsInRect(frame, destination, stride, 0, 0, halfWidth, halfHeight) > 0,
          QStringLiteral("tiling covers the top-left quadrant"));
    check(changedPixelsInRect(frame, destination, stride, halfWidth, 0, width, halfHeight) > 0,
          QStringLiteral("tiling covers the top-right quadrant"));
    check(changedPixelsInRect(frame, destination, stride, 0, halfHeight, halfWidth, height) > 0,
          QStringLiteral("tiling covers the bottom-left quadrant"));
    check(changedPixelsInRect(frame, destination, stride, halfWidth, halfHeight, width, height) > 0,
          QStringLiteral("tiling covers the bottom-right quadrant"));
}

void checkOpacityIsHonoured()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);

    WatermarkPolicy faint = enabledPolicy(QStringLiteral("OPACITY"));
    faint.opacity = 0.35;
    WatermarkPolicy solid = enabledPolicy(QStringLiteral("OPACITY"));
    solid.opacity = 1.0;

    QByteArray faintFrame;
    QByteArray solidFrame;
    QString error;
    check(composeWatermark(faint, viewOf(frame, width, height, stride), faintFrame, error),
          QStringLiteral("the default-opacity watermark composes"));
    check(composeWatermark(solid, viewOf(frame, width, height, stride), solidFrame, error),
          QStringLiteral("the full-opacity watermark composes"));

    check(changedPixels(frame, faintFrame) <= changedPixels(frame, solidFrame),
          QStringLiteral("a fainter watermark never paints more pixels than a solid one"));
    check(brightestChannel(faintFrame) < brightestChannel(solidFrame),
          QStringLiteral("the default 0.35 opacity is visibly blended, not opaque"));
}

void checkDevicePixelRatio()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);
    const WatermarkPolicy policy = enabledPolicy(QStringLiteral("DPR"));

    QByteArray at1x;
    QByteArray at2x;
    QString error;
    check(composeWatermark(policy, viewOf(frame, width, height, stride, 1.0), at1x, error),
          QStringLiteral("the 1x frame composes"));
    check(composeWatermark(policy, viewOf(frame, width, height, stride, 2.0), at2x, error),
          QStringLiteral("the 2x frame composes"));
    check(changedPixels(frame, at2x) > changedPixels(frame, at1x),
          QStringLiteral("a 2x device pixel ratio keeps the same physical size by painting more device pixels"));
}

void checkCjkTextAndDeterminism()
{
    const int width = 320;
    const int height = 200;
    qsizetype stride = 0;
    const QByteArray frame = makeFrame(width, height, &stride);
    const WatermarkPolicy policy = enabledPolicy(QStringLiteral("内部资料 请勿外传"));

    QByteArray first;
    QByteArray second;
    QString error;
    check(composeWatermark(policy, viewOf(frame, width, height, stride), first, error),
          QStringLiteral("CJK text composes"));
    check(changedPixels(frame, first) > 0, QStringLiteral("CJK text produces visible pixels"));
    check(composeWatermark(policy, viewOf(frame, width, height, stride), second, error),
          QStringLiteral("a second composition succeeds"));
    check(first == second, QStringLiteral("composition is deterministic for the same policy and frame"));
}

void checkDegenerateInputs()
{
    {
        // A watermark that cannot fit is refused instead of being clipped, elided or silently shrunk.
        const int width = 24;
        const int height = 12;
        qsizetype stride = 0;
        const QByteArray frame = makeFrame(width, height, &stride);
        QByteArray destination;
        QString error;
        const WatermarkPolicy policy = enabledPolicy(QStringLiteral("tiny"));
        check(!composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
              QStringLiteral("a frame too small for the text is refused"));
        check(error.contains(QStringLiteral("does not fit")),
              QStringLiteral("the refusal explains that the text does not fit"));
        check(destination.isEmpty(), QStringLiteral("a refused composition produces no buffer"));
    }
    {
        const int width = 320;
        const int height = 200;
        qsizetype stride = 0;
        const QByteArray frame = makeFrame(width, height, &stride);
        QByteArray destination;
        QString error;
        const WatermarkPolicy policy = enabledPolicy(QString(128, QLatin1Char('W')));
        check(!composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
              QStringLiteral("accepted-but-too-wide text is refused at composition time"));
    }
    {
        // Tiling needs one whole instance to fit; a partial repeating pattern is still a visible watermark.
        const int width = 320;
        const int height = 200;
        qsizetype stride = 0;
        const QByteArray frame = makeFrame(width, height, &stride);
        WatermarkPolicy policy = enabledPolicy(QStringLiteral("WP"));
        policy.position = WatermarkPosition::Tiled;
        QByteArray destination;
        QString error;
        check(composeWatermark(policy, viewOf(frame, width, height, stride), destination, error),
              QStringLiteral("a tiled watermark that fits composes without error"));
    }
    {
        const int width = 320;
        const int height = 200;
        qsizetype stride = 0;
        const QByteArray frame = makeFrame(width, height, &stride);
        WatermarkFrameView broken = viewOf(frame, width, height, stride);
        broken.stride = stride - 4;  // narrower than width * 4
        QByteArray destination;
        QString error;
        const WatermarkPolicy policy = enabledPolicy(QStringLiteral("broken"));
        check(!composeWatermark(policy, broken, destination, error),
              QStringLiteral("a malformed plane is refused"));
        check(!error.isEmpty(), QStringLiteral("a malformed plane explains itself"));
        check(destination.isEmpty(), QStringLiteral("a malformed plane produces no buffer"));
    }
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);

    // A platform plugin without a font database cannot draw text at all. Say so explicitly instead of reporting
    // "no changed pixels", which would look like a compositor defect.
    if (QFontDatabase::families().isEmpty()) {
        QTextStream(stdout) << "FAIL: the platform plugin exposes no font family; text cannot be rendered\n";
        return 3;
    }

    checkDisabledDoesNothing();
    checkSingleCornerAndSourceIntegrity();
    checkExplicitTopLeft();
    checkTiled();
    checkOpacityIsHonoured();
    checkDevicePixelRatio();
    checkCjkTextAndDeterminism();
    checkDegenerateInputs();

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark compositor checks passed\n" : "watermark compositor checks failed\n");
    return failures == 0 ? 0 : 2;
}
