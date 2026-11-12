#include "automatic/composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>

#include <iostream>
#include <memory>
#include <vector>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/storage.hpp"

namespace {

class SolidCapture final : public hyremote::CaptureSource
{
public:
    explicit SolidCapture(QRgb color, int pixelWidth = 2, int pixelHeight = 2, float dpr = 1.0F,
                          std::shared_ptr<const hyremote::FrameStorage> *observedStorage = nullptr)
        : m_color(color), m_pixelWidth(pixelWidth), m_pixelHeight(pixelHeight), m_dpr(dpr),
          m_observedStorage(observedStorage)
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities caps;
        caps.asynchronous = true;
        caps.cpuReadable = true;
        caps.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return caps;
    }

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler) override
    {
        m_onFrame = std::move(onFrame);
        return true;
    }

    void stop() noexcept override { m_onFrame = {}; }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        if (!m_onFrame)
            return false;
        const int width = m_pixelWidth;
        const int height = m_pixelHeight;
        const std::size_t stride = static_cast<std::size_t>(width) * 4U;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(stride, height);
        if (!storage || !storage->mutablePlane(0))
            return false;

        auto *bytes = reinterpret_cast<unsigned char *>(storage->mutablePlane(0));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset = static_cast<std::size_t>(y) * stride
                                           + static_cast<std::size_t>(x) * 4U;
                const int alpha = qAlpha(m_color);
                bytes[offset + 0] = static_cast<unsigned char>((qRed(m_color) * alpha + 127) / 255);
                bytes[offset + 1] = static_cast<unsigned char>((qGreen(m_color) * alpha + 127) / 255);
                bytes[offset + 2] = static_cast<unsigned char>((qBlue(m_color) * alpha + 127) / 255);
                bytes[offset + 3] = static_cast<unsigned char>(alpha);
            }
        }

        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(width),
                               static_cast<std::uint32_t>(height)};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.geometry.devicePixelRatio = m_dpr;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        if (m_observedStorage)
            *m_observedStorage = frame.storage;
        m_onFrame(std::move(frame));
        return true;
    }

private:
    QRgb m_color;
    int m_pixelWidth;
    int m_pixelHeight;
    float m_dpr;
    std::shared_ptr<const hyremote::FrameStorage> *m_observedStorage = nullptr;
    hyremote::FrameReadyHandler m_onFrame;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForFrame(std::vector<hyremote::RemoteFrame> &frames, std::size_t count)
{
    QElapsedTimer timer;
    timer.start();
    while (frames.size() < count && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return frames.size() >= count;
}

QImage imageView(const hyremote::RemoteFrame &frame)
{
    if (!frame.storage)
        return {};
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return {};
    return QImage(reinterpret_cast<const uchar *>(plane->data),
                  static_cast<int>(frame.geometry.size.width),
                  static_cast<int>(frame.geometry.size.height),
                  static_cast<qsizetype>(plane->stride),
                  QImage::Format_RGBA8888_Premultiplied);
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using HyRemote::Runtime::Automatic::CompositeTarget;
    QObject leftTarget;
    QObject rightTarget;
    CompositeTarget composite;
    composite.upsertSurface(1, &leftTarget, QRect(-2, 0, 2, 2), true);
    composite.upsertSurface(2, &rightTarget, QRect(0, 0, 2, 2), true);

    const HyRemote::detail::BuiltinTargetResolver resolver =
        [&](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            if (remoteInputEnabled)
                return result;
            if (target == &leftTarget) {
                result.supported = true;
                // High-DPI child: four physical pixels per logical two-unit surface.
                result.capture = std::make_unique<SolidCapture>(qRgba(255, 0, 0, 255),
                                                                 4, 4, 2.0F);
            } else if (target == &rightTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(0, 255, 0, 255));
            }
            return result;
        };

    auto components = composite.createTargetComponents(false, resolver);
    if (!check(components.supported && components.capture, "automatic composite creates capture source"))
        return 1;

    std::vector<hyremote::RemoteFrame> frames;
    if (!check(components.capture->start(
                   [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "automatic composite capture starts")) {
        return 2;
    }

    if (!check(components.capture->requestFrame({1, hyremote::Clock::now()}), "frame request accepted")
        || !check(waitForFrame(frames, 1), "composite frame arrives")) {
        return 3;
    }

    const QImage image = imageView(frames.front());
    // #806 regression: left capture is 4x4 physical pixels with DPR=2 and
    // right capture is 2x2 with DPR=1; both are rendered into a 4x2 logical
    // output. A max(child DPR)=2 result wrongly enlarges watermark font pixels.
    if (!check(frames.front().geometry.devicePixelRatio == 1.0F,
               "composite output DPR follows its logical canvas, not its 2x child")
        || !check(image.size() == QSize(4, 2), "surface union becomes one logical canvas")
        || !check(image.pixelColor(0, 0) == QColor(255, 0, 0, 255), "left surface is composed")
        || !check(image.pixelColor(3, 0) == QColor(0, 255, 0, 255), "right surface is composed")) {
        return 4;
    }

    // #567: once all child surfaces disappear, do not re-label the last
    // colorful frame as new pixels. Keep the old geometry to avoid forcing
    // a DesktopSize extension on legacy Viewers, but clear every pixel.
    composite.removeSurface(1);
    composite.removeSurface(2);
    if (!check(components.capture->requestFrame({2, hyremote::Clock::now()}),
               "empty composite request accepted")
        || !check(waitForFrame(frames, 2), "empty composite frame arrives"))
        return 9;
    const QImage emptyImage = imageView(frames.at(1));
    if (!check(emptyImage.size() == QSize(4, 2), "empty scene retains geometry")
        || !check(emptyImage.pixelColor(0, 0) == QColor(0, 0, 0, 0),
                  "empty scene does not replay left stale pixels")
        || !check(emptyImage.pixelColor(3, 0) == QColor(0, 0, 0, 0),
                  "empty scene does not replay right stale pixels")
        || !check(frames.at(1).requestId == hyremote::CaptureRequestId{2},
                  "empty scene completes current request without old-frame metadata"))
        return 10;

    // A visible but unsupported child must also clear, rather than return a
    // stale frame stamped as a new successful capture.
    QObject missingTarget;
    composite.upsertSurface(5, &missingTarget, QRect(-2, 0, 4, 2), true);
    if (!check(components.capture->requestFrame({3, hyremote::Clock::now()}),
               "unavailable-child composite request accepted")
        || !check(waitForFrame(frames, 3), "unavailable-child clear frame arrives"))
        return 11;
    const QImage unavailableImage = imageView(frames.at(2));
    if (!check(unavailableImage.size() == QSize(4, 2), "unavailable child retains canvas bounds")
        || !check(unavailableImage.pixelColor(0, 0) == QColor(0, 0, 0, 0),
                  "unavailable child does not replay prior source")
        || !check(frames.at(2).requestId == hyremote::CaptureRequestId{3},
                  "unavailable child completes its own request"))
        return 12;
    components.capture->stop();

    // #609: one full-canvas, DPR1 premultiplied child is already the exact
    // composite output: reuse its immutable FrameStorage without allocating,
    // clearing or drawing a second full-size picture. Verify semi-transparent
    // premultiplied samples, negative-origin canvas and per-request metadata.
    QObject singleTarget;
    CompositeTarget single;
    single.upsertSurface(3, &singleTarget, QRect(-7, 3, 3, 2), true);
    std::shared_ptr<const hyremote::FrameStorage> childStorage;
    const HyRemote::detail::BuiltinTargetResolver singleResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (!input && target == &singleTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(
                    qRgba(64, 32, 16, 128), 3, 2, 1.0F, &childStorage);
            }
            return result;
        };
    auto singleComponents = single.createTargetComponents(false, singleResolver);
    std::vector<hyremote::RemoteFrame> singleFrames;
    if (!check(singleComponents.capture->start(
                   [&](hyremote::RemoteFrame frame) { singleFrames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "single full-canvas capture starts")
        || !check(singleComponents.capture->requestFrame({42, hyremote::Clock::now()}),
                  "single full-canvas frame requested")
        || !check(waitForFrame(singleFrames, 1), "single full-canvas frame delivered")) {
        return 5;
    }
    const auto &singleResult = singleFrames.front();
    const QImage singleImage = imageView(singleResult);
    const uchar *singleBytes = singleImage.constBits();
    if (!check(childStorage && singleResult.storage == childStorage,
               "full-canvas fast path shares immutable child storage")
        || !check(singleResult.geometry.devicePixelRatio == 1.0F
                      && singleImage.size() == QSize(3, 2),
                  "full-canvas output has correct logical geometry")
        || !check(singleBytes && singleBytes[0] == 32 && singleBytes[1] == 16
                      && singleBytes[2] == 8 && singleBytes[3] == 128,
                  "full-canvas fast path preserves premultiplied transparent pixels")
        || !check(singleResult.requestId == hyremote::CaptureRequestId{42}
                      && singleResult.damage.kind == hyremote::DamageKind::FullFrame,
                  "full-canvas fast path retains composite request/damage metadata")) {
        return 6;
    }
    singleComponents.capture->stop();

    // A single 2x-DPR child needs scaling into the logical canvas. It must
    // retain the old compositor (no direct storage alias) and #806 DPR=1.
    QObject scaledTarget;
    CompositeTarget scaled;
    scaled.upsertSurface(4, &scaledTarget, QRect(5, 5, 2, 2), true);
    std::shared_ptr<const hyremote::FrameStorage> highDprStorage;
    const HyRemote::detail::BuiltinTargetResolver scaledResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (!input && target == &scaledTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(
                    qRgba(0, 255, 0, 255), 4, 4, 2.0F, &highDprStorage);
            }
            return result;
        };
    auto scaledComponents = scaled.createTargetComponents(false, scaledResolver);
    std::vector<hyremote::RemoteFrame> scaledFrames;
    if (!check(scaledComponents.capture->start(
                   [&](hyremote::RemoteFrame frame) { scaledFrames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "scaled child capture starts")
        || !check(scaledComponents.capture->requestFrame({43, hyremote::Clock::now()}),
                  "scaled child frame requested")
        || !check(waitForFrame(scaledFrames, 1), "scaled child frame delivered")) {
        return 7;
    }
    const auto &scaledResult = scaledFrames.front();
    const QImage scaledImage = imageView(scaledResult);
    if (!check(highDprStorage && scaledResult.storage != highDprStorage,
               "DPR2 child must retain pixel-scaling compositor")
        || !check(scaledResult.geometry.devicePixelRatio == 1.0F
                      && scaledImage.size() == QSize(2, 2),
                  "DPR2 child output stays logical DPR1")
        || !check(scaledImage.pixelColor(0, 0) == QColor(0, 255, 0, 255),
                  "DPR2 child's pixels are still composited correctly")) {
        return 8;
    }
    scaledComponents.capture->stop();

    // #958 final-output ownership: two distinct presentable surfaces overlap.
    // Capture each whole surface exactly once, then blend in application
    // stacking order. This is deliberately NOT an item-by-item Qt Quick grab,
    // nor proof that one GPU framebuffer covers both independent windows.
    QObject baseWindow;
    QObject translucentPopup;
    CompositeTarget layered;
    layered.upsertSurface(10, &baseWindow, QRect(0, 0, 4, 4), true);
    layered.upsertSurface(11, &translucentPopup, QRect(1, 1, 2, 2), true);
    const HyRemote::detail::BuiltinTargetResolver layeredResolver =
        [&](QObject *target, bool input) {
            HyRemote::detail::TargetComponents result;
            if (input)
                return result;
            if (target == &baseWindow) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(255, 0, 0, 255), 4, 4);
            } else if (target == &translucentPopup) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(0, 0, 255, 128), 2, 2);
            }
            return result;
        };
    auto layeredComponents = layered.createTargetComponents(false, layeredResolver);
    std::vector<hyremote::RemoteFrame> layeredFrames;
    if (!check(layeredComponents.supported && layeredComponents.capture,
               "layered top-level surface capture is available")
        || !check(layeredComponents.capture->start(
                      [&](hyremote::RemoteFrame frame) {
                          layeredFrames.push_back(std::move(frame));
                      },
                      [](const hyremote::CaptureEvent &) {}),
                  "layered capture starts")
        || !check(layeredComponents.capture->requestFrame({51, hyremote::Clock::now()}),
                  "layered first frame admitted")
        || !check(waitForFrame(layeredFrames, 1), "layered first frame ready"))
        return 13;
    const QImage blend = imageView(layeredFrames.at(0));
    const QColor blended = blend.pixelColor(1, 1);
    if (!check(blend.size() == QSize(4, 4), "popup must not change base canvas size")
        || !check(blend.pixelColor(0, 0) == QColor(255, 0, 0, 255),
                  "uncovered base pixels preserved")
        || !check(blended.alpha() == 255 && qAbs(blended.red() - 127) <= 1
                      && blended.green() == 0 && qAbs(blended.blue() - 128) <= 1,
                  "translucent popup is composed over its base exactly once"))
        return 14;

    // A real native-window raise changes the final ordering without creating
    // or resurrecting any old source snapshot; the next accepted request must
    // represent the NEW layering relationship.
    layered.raiseSurface(10);
    if (!check(layeredComponents.capture->requestFrame({52, hyremote::Clock::now()}),
               "raised surface frame admitted")
        || !check(waitForFrame(layeredFrames, 2), "raised surface frame ready"))
        return 15;
    const QImage raised = imageView(layeredFrames.at(1));
    if (!check(raised.pixelColor(1, 1) == QColor(255, 0, 0, 255),
               "raised opaque window obscures translucent popup")
        || !check(layeredFrames.at(1).requestId == hyremote::CaptureRequestId{52},
                  "raised composition has current request identity"))
        return 16;

    // Hide the base after the raise: a previously composed red pixel may not
    // survive as stale data behind the remaining translucent popup.
    layered.setSurfaceVisible(10, false);
    if (!check(layeredComponents.capture->requestFrame({53, hyremote::Clock::now()}),
               "hidden base frame admitted")
        || !check(waitForFrame(layeredFrames, 3), "hidden base frame ready"))
        return 17;
    const QImage popupOnly = imageView(layeredFrames.at(2));
    const QColor popupPixel = popupOnly.pixelColor(0, 0);
    if (!check(popupOnly.size() == QSize(2, 2),
               "hidden base is excluded from visible canvas")
        || !check(popupPixel.alpha() == 128 && popupPixel.red() == 0
                      && popupPixel.green() == 0 && popupPixel.blue() == 255,
                  "popup output retains its own alpha with no stale base pixels")
        || !check(layeredFrames.at(2).requestId == hyremote::CaptureRequestId{53},
                  "new composition has new request identity"))
        return 18;
    layeredComponents.capture->stop();
    std::cout << "PASS: composite multi-surface, no stale-frame fallback, single-canvas immutable fast path, DPR fallback, and #958 overlapped alpha/Z/hide output truth\\n";
    return 0;
}
