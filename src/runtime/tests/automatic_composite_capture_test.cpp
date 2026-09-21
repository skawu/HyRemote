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
    explicit SolidCapture(QRgb color) : m_color(color) {}

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
                bytes[offset + 3] = 255;
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
                result.capture = std::make_unique<SolidCapture>(qRgba(255, 0, 0, 255));
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
    if (!check(image.size() == QSize(4, 2), "surface union becomes one logical canvas")
        || !check(image.pixelColor(0, 0) == QColor(255, 0, 0, 255), "left surface is composed")
        || !check(image.pixelColor(3, 0) == QColor(0, 255, 0, 255), "right surface is composed")) {
        return 4;
    }

    components.capture->stop();
    std::cout << "PASS: automatic runtime composes multiple application surfaces into one capture target\n";
    return 0;
}
