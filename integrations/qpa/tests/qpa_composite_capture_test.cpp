// SPDX-License-Identifier: Apache-2.0
#include "../composite_target.hpp"

#include <QColor>
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

struct Counters
{
    int starts = 0;
    int stops = 0;
    int requests = 0;
    bool holdRequests = false;
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
        if (m_counters->holdRequests)
            return true;

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

bool waitForRequestCount(const std::shared_ptr<Counters> &counters, int count)
{
    QElapsedTimer timer;
    timer.start();
    while (counters->requests < count && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return counters->requests >= count;
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

    // QPA composite mode uses one logical-pixel remote canvas. A child adapter may capture at a
    // higher device-pixel ratio; drawImage scales that child frame into this logical geometry so
    // global Qt coordinates and later input routing stay in the same coordinate space.
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

    // Reproduce the Quick visibility race deterministically: one child accepts request #3 but does
    // not complete it. When that surface leaves the visible composite, the already-admitted parent
    // request must stop waiting for it. The composite runtime stays started, releases Core's
    // in-flight slot, and the next request observes the contracted secondary-only canvas.
    leftCounters->holdRequests = true;
    hyremote::CaptureRequest request3{3, hyremote::Clock::now()};
    if (!check(components.capture->requestFrame(request3), "pending child request is accepted")
        || !check(waitForRequestCount(leftCounters, 3), "left child accepted the pending request")
        || !check(waitForRequestCount(rightCounters, 3), "right child accepted the pending request")
        || !check(frames.size() == 2, "composite waits while a visible child request is outstanding")) {
        return 7;
    }

    // RemoteController uses upsertSurface(..., visible=false) for a hidden QQuickWindow, so exercise
    // that exact path rather than a test-only removal shortcut.
    composite.upsertSurface(1, &leftTarget, QRect(-2, 0, 2, 2), false);
    if (!check(waitForFrameCount(frames, 3),
               "hiding an expected child releases the pending composite request")) {
        return 8;
    }

    QImage released = imageView(frames[2]);
    if (!check(released.size() == QSize(6, 2),
               "the released request retains its original snapshot geometry")
        || !check(released.pixelColor(0, 0).alpha() == 0,
                  "the hidden child is omitted from the released request")
        || !check(released.pixelColor(5, 0) == QColor(0, 255, 0, 255),
                  "the completed child remains in the released request")) {
        return 9;
    }

    hyremote::CaptureRequest request4{4, hyremote::Clock::now()};
    if (!check(components.capture->requestFrame(request4), "post-hide request is accepted")
        || !check(waitForFrameCount(frames, 4), "post-hide frame arrives")) {
        return 10;
    }

    QImage contracted = imageView(frames[3]);
    if (!check(contracted.size() == QSize(2, 2), "canvas contracts to the remaining visible surface")
        || !check(contracted.pixelColor(0, 0) == QColor(0, 255, 0, 255), "remaining surface stays live")
        || !check(leftCounters->starts == 1 && leftCounters->stops >= 1,
                  "hidden child adapter is retired without stopping the composite source")
        || !check(rightCounters->starts == 1,
                  "unchanged child adapter is reused across dynamic canvas frames")) {
        return 11;
    }

    components.capture->stop();
    if (!check(rightCounters->stops >= 1, "composite stop drains the remaining child adapter"))
        return 12;

    std::cout << "PASS: one composite CaptureSource follows multi-surface canvas churn\n";
    return 0;
}
