#include "../composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>

#include <iostream>
#include <memory>
#include <vector>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/storage.hpp"

namespace {

struct Counters
{
    int starts = 0;
    int stops = 0;
    int requests = 0;
};

class SolidCapture final : public hyremote::CaptureSource
{
public:
    SolidCapture(QRgb color, std::shared_ptr<Counters> counters)
        : m_color(color)
        , m_counters(std::move(counters))
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
        ++m_counters->starts;
        m_onFrame = std::move(onFrame);
        return true;
    }

    void stop() noexcept override
    {
        ++m_counters->stops;
        m_onFrame = {};
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        ++m_counters->requests;
        if (!m_onFrame)
            return false;

        constexpr int width = 2;
        constexpr int height = 2;
        constexpr std::size_t stride = width * 4U;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(stride, height);
        if (!storage || !storage->mutablePlane(0))
            return false;

        auto *bytes = reinterpret_cast<unsigned char *>(storage->mutablePlane(0));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset = static_cast<std::size_t>(y) * stride
                                           + static_cast<std::size_t>(x) * 4U;
                bytes[offset + 0] = static_cast<unsigned char>(qRed(m_color));
                bytes[offset + 1] = static_cast<unsigned char>(qGreen(m_color));
                bytes[offset + 2] = static_cast<unsigned char>(qBlue(m_color));
                bytes[offset + 3] = static_cast<unsigned char>(qAlpha(m_color));
            }
        }

        hyremote::RemoteFrame frame;
        frame.geometry.size = {width, height};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        m_onFrame(std::move(frame));
        return true;
    }

private:
    QRgb m_color;
    std::shared_ptr<Counters> m_counters;
    hyremote::FrameReadyHandler m_onFrame;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForFrameCount(std::vector<hyremote::RemoteFrame> &frames, std::size_t count)
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

    QObject leftTarget;
    QObject rightTarget;
    auto leftCounters = std::make_shared<Counters>();
    auto rightCounters = std::make_shared<Counters>();

    HyRemote::Qpa::CompositeTarget composite;
    composite.upsertSurface(1, &leftTarget, QRect(-2, 0, 2, 2), true);
    composite.upsertSurface(2, &rightTarget, QRect(0, 0, 2, 2), true);

    const HyRemote::detail::BuiltinTargetResolver resolver =
        [&](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            if (remoteInputEnabled)
                return result;
            if (target == &leftTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(255, 0, 0, 255), leftCounters);
            } else if (target == &rightTarget) {
                result.supported = true;
                result.capture = std::make_unique<SolidCapture>(qRgba(0, 255, 0, 255), rightCounters);
            }
            return result;
        };

    HyRemote::detail::TargetComponents components = composite.createTargetComponents(false, resolver);
    if (!check(components.supported && components.capture != nullptr,
               "composite target supplies one capture source")) {
        return 1;
    }

    std::vector<hyremote::RemoteFrame> frames;
    if (!check(components.capture->start(
                   [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
                   [](const hyremote::CaptureEvent &) {}),
               "composite capture source starts")) {
        return 2;
    }

    hyremote::CaptureRequest request1{1, hyremote::Clock::now()};
    if (!check(components.capture->requestFrame(request1), "first composite request is accepted")
        || !check(waitForFrameCount(frames, 1), "first composite frame arrives")) {
        return 3;
    }

    QImage first = imageView(frames[0]);
    if (!check(first.size() == QSize(4, 2), "negative/global surfaces form a 4x2 canvas")
        || !check(first.pixelColor(0, 0) == QColor(255, 0, 0, 255), "left surface is composited")
        || !check(first.pixelColor(3, 0) == QColor(0, 255, 0, 255), "right surface is composited")) {
        return 4;
    }

    // Moving one surface changes RemoteFrame geometry inside the same capture-source lifetime.
    composite.setSurfaceGeometry(2, QRect(2, 0, 2, 2));
    hyremote::CaptureRequest request2{2, hyremote::Clock::now()};
    if (!check(components.capture->requestFrame(request2), "geometry-change request is accepted")
        || !check(waitForFrameCount(frames, 2), "geometry-change frame arrives")) {
        return 5;
    }

    QImage second = imageView(frames[1]);
    if (!check(second.size() == QSize(6, 2), "canvas expands without recreating the capture source")
        || !check(second.pixelColor(0, 0) == QColor(255, 0, 0, 255), "left content remains")
        || !check(second.pixelColor(2, 0).alpha() == 0, "gap in the logical canvas stays transparent")
        || !check(second.pixelColor(5, 0) == QColor(0, 255, 0, 255), "moved right surface is composited")) {
        return 6;
    }

    // Removing one surface retires only that child adapter; the composite capture source remains
    // started and immediately serves the remaining application surface.
    composite.removeSurface(1);
    hyremote::CaptureRequest request3{3, hyremote::Clock::now()};
    if (!check(components.capture->requestFrame(request3), "surface-removal request is accepted")
        || !check(waitForFrameCount(frames, 3), "surface-removal frame arrives")) {
        return 7;
    }

    QImage third = imageView(frames[2]);
    if (!check(third.size() == QSize(2, 2), "canvas contracts to the remaining surface")
        || !check(third.pixelColor(0, 0) == QColor(0, 255, 0, 255), "remaining surface stays live")
        || !check(leftCounters->starts == 1 && leftCounters->stops >= 1,
                  "removed child adapter is stopped without stopping the composite source")
        || !check(rightCounters->starts == 1,
                  "unchanged child adapter is reused across dynamic canvas frames")) {
        return 8;
    }

    components.capture->stop();
    if (!check(rightCounters->stops >= 1, "composite stop drains the remaining child adapter"))
        return 9;

    std::cout << "PASS: one composite CaptureSource follows multi-surface canvas churn\n";
    return 0;
}
