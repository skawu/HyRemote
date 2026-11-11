#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>
#include <QWheelEvent>
#include <QSysInfo>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#endif
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

#include "detail/component_factories.hpp"
#include "detail/readback_frame_storage.hpp"
#include "detail/cursor_mapping.hpp"
#include "quick/quick_capture_backend.hpp"
#include "quick/quick_readback_batch.hpp"
#include "quick/quick_adaptive_capture_source.hpp"
#include "quick/quick_image_frame_storage.hpp"
#include "quick/quick_capture_single_flight.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

// A single Qt-free CPU normalization contract is used by the GL provider and
// can also handle pitched D3D staging and Qt5 source rows without QtPrivate.
// These fixtures deliberately avoid any GPU or platform dependency.
// Fake providers have no Qt render workload; they check the common
// CaptureSource identity/fallback/Surface-freshness contract deterministically.
class ControlledQuickSource final : public hyremote::CaptureSource
{
public:
    bool running = false;
    bool startsSuccessfully = true;
    bool acceptsRequests = true;
    std::vector<hyremote::CaptureRequestId> accepted;
    std::mutex callbackMutex;
    hyremote::FrameReadyHandler onFrame;
    hyremote::CaptureEventHandler onEvent;

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }
    bool start(hyremote::FrameReadyHandler frame,
               hyremote::CaptureEventHandler event) override
    {
        if (!startsSuccessfully)
            return false;
        std::lock_guard<std::mutex> lock(callbackMutex);
        running = true;
        accepted.clear();
        onFrame = std::move(frame);
        onEvent = std::move(event);
        return true;
    }
    void stop() noexcept override
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        running = false;
        onFrame = {};
        onEvent = {};
    }
    bool requestFrame(const hyremote::CaptureRequest &req) override
    {
        if (!running || !acceptsRequests)
            return false;
        accepted.push_back(req.id);
        return true;
    }
    void complete(hyremote::CaptureRequestId id, bool valid,
                  std::chrono::milliseconds contentAge = std::chrono::milliseconds{0},
                  hyremote::PtsSource timestampSource = hyremote::PtsSource::Render)
    {
        hyremote::RemoteFrame frame;
        frame.requestId = id;
        frame.timing.ptsSource = timestampSource;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.timing.pts = *frame.timing.completionTime - contentAge;
        if (valid) {
            frame.storage = hyremote::CpuFrameStorage::createSinglePlane(4, 1);
            frame.geometry.size = {1, 1};
            frame.geometry.planeCount = 1;
            frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
            frame.damage = hyremote::Damage::fullFrame();
        }
        hyremote::FrameReadyHandler callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = onFrame; // immutable local copy survives concurrent stop
        }
        if (callback)
            callback(std::move(frame));
    }
};

void testAdaptiveQuickSceneGraphInvalidationFailClosed()
{
    QQuickWindow window;
    auto native = std::make_unique<ControlledQuickSource>();
    auto publicQt = std::make_unique<ControlledQuickSource>();
    auto *primary = native.get();
    auto *backup = publicQt.get();
    HyRemote::detail::AdaptiveQuickCaptureSource source(
        &window, std::move(native), std::move(publicQt));
    std::vector<hyremote::RemoteFrame> frames;
    CHECK(source.start([&](hyremote::RemoteFrame f) {
        frames.push_back(std::move(f));
    }, [](const hyremote::CaptureEvent &) {}));
    CHECK(source.requestFrame({81, hyremote::Clock::now()}));
    CHECK(primary->accepted.size() == 1 && primary->accepted[0] == 81);
    // Qt issues this public signal on scenegraph/device teardown. Delivering
    // it directly tests the shared runtime policy without a real GPU reset.
    const bool invalidated = QMetaObject::invokeMethod(
        &window, "sceneGraphInvalidated", Qt::DirectConnection);
    CHECK(invalidated);
    primary->complete(81, true);
    CHECK(frames.size() == 1 && !frames[0].storage);
    CHECK(source.requestFrame({82, hyremote::Clock::now()}));
    CHECK(backup->accepted.size() == 1 && backup->accepted[0] == 82);
    backup->complete(82, true);
    CHECK(frames.size() == 2 && frames[1].storage);
    source.stop();
}

void testNativeReadbackBatchTimeoutIsOneFailure()
{
    using hyremote::CaptureRequest;
    const auto now = hyremote::Clock::now();
    std::vector<CaptureRequest> submitted{{101, now}, {102, now}};
    std::vector<CaptureRequest> queued{{103, now}};
    std::vector<CaptureRequest> expired;
    CHECK(HyRemote::detail::retireTimedOutReadbackBatch(
        submitted, 102, expired));
    CHECK(submitted.empty() && expired.size() == 2);
    CHECK(expired[0].id == 101 && expired[1].id == 102);
    CHECK(!HyRemote::detail::retireTimedOutReadbackBatch(
        submitted, 101, expired)); // its later timer cannot publish again
    CHECK(expired.size() == 2 && queued.size() == 1);
    CHECK(!HyRemote::detail::retireTimedOutReadbackBatch(
        queued, 102, expired)); // unrelated batch is not revoked
    CHECK(queued.size() == 1);
    CHECK(HyRemote::detail::retireTimedOutReadbackBatch(
        queued, 103, expired));
    CHECK(expired.size() == 3 && queued.empty());
}

void testAdaptiveNativeFailureEventCountsOncePerGpuBatch()
{
    auto native = std::make_unique<ControlledQuickSource>();
    auto publicQt = std::make_unique<ControlledQuickSource>();
    auto *primary = native.get();
    auto *backup = publicQt.get();
    HyRemote::detail::AdaptiveQuickCaptureSource source(
        nullptr, std::move(native), std::move(publicQt));
    std::vector<hyremote::RemoteFrame> frames;
    int observedUnavailableEvents = 0;
    CHECK(source.start([&](hyremote::RemoteFrame frame) {
        frames.push_back(std::move(frame));
    }, [&](const hyremote::CaptureEvent &event) {
        if (event.code == hyremote::CaptureEventCode::TemporarilyUnavailable)
            ++observedUnavailableEvents;
    }));
    CHECK(source.requestFrame({101, hyremote::Clock::now()}));
    CHECK(source.requestFrame({102, hyremote::Clock::now()}));

    // The same unsignaled GPU staging/PBO copy serves BOTH accepted IDs.
    // Its one provider event arrives before the two empty completions.
    hyremote::CaptureEvent unavailable;
    unavailable.code = hyremote::CaptureEventCode::TemporarilyUnavailable;
    unavailable.recoverable = true;
    unavailable.message = "D3D11 staging Map not ready after bounded renders";
    CHECK(static_cast<bool>(primary->onEvent));
    primary->onEvent(unavailable);
    primary->complete(101, false);
    primary->complete(102, false);
    CHECK(observedUnavailableEvents == 1);
    CHECK(frames.size() == 2);
    CHECK(!frames[0].storage && !frames[1].storage);
    CHECK(frames[0].requestId && *frames[0].requestId == 101);
    CHECK(frames[1].requestId && *frames[1].requestId == 102);

    // One failed GPU readback does NOT permanently disable native.
    CHECK(source.requestFrame({103, hyremote::Clock::now()}));
    CHECK(primary->accepted.size() == 3 && primary->accepted.back() == 103);
    CHECK(backup->accepted.empty());

    // A SECOND independent provider event triggers one-way fallback.
    unavailable.message = "D3D11 capture exceeded bounded deadline";
    primary->onEvent(unavailable);
    primary->complete(103, false);
    CHECK(observedUnavailableEvents == 2);
    CHECK(source.requestFrame({104, hyremote::Clock::now()}));
    CHECK(backup->accepted.size() == 1 && backup->accepted[0] == 104);
    backup->complete(104, true);
    CHECK(frames.size() == 4 && frames.back().storage);
    primary->complete(101, true); // retired IDs must never re-enter Core
    CHECK(frames.size() == 4);
    source.stop();
}

void testAdaptiveQuickMalformedNativeFramesTriggerFallback()
{
    auto native = std::make_unique<ControlledQuickSource>();
    auto publicQt = std::make_unique<ControlledQuickSource>();
    auto *primary = native.get();
    auto *backup = publicQt.get();
    HyRemote::detail::AdaptiveQuickCaptureSource source(
        nullptr, std::move(native), std::move(publicQt));
    std::vector<hyremote::RemoteFrame> frames;
    CHECK(source.start([&](hyremote::RemoteFrame f) {
        frames.push_back(std::move(f));
    }, [](const hyremote::CaptureEvent &) {}));

    auto nativeFrame = [](hyremote::CaptureRequestId id) {
        hyremote::RemoteFrame frame;
        frame.requestId = id;
        frame.storage = hyremote::CpuFrameStorage::createSinglePlane(4, 1);
        frame.geometry.size = {1, 1};
        frame.geometry.planeCount = 1;
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.timing.ptsSource = hyremote::PtsSource::Render;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.timing.pts = *frame.timing.completionTime;
        return frame;
    };

    CHECK(source.requestFrame({71, hyremote::Clock::now()}));
    auto malformed = nativeFrame(71);
    malformed.geometry.pixelFormat = hyremote::PixelFormat::Unknown;
    primary->onFrame(std::move(malformed));
    CHECK(frames.size() == 1 && !frames.back().storage);

    CHECK(source.requestFrame({72, hyremote::Clock::now()}));
    malformed = nativeFrame(72);
    malformed.timing.pts += std::chrono::seconds(1);
    primary->onFrame(std::move(malformed));
    CHECK(frames.size() == 2 && !frames.back().storage);

    CHECK(source.requestFrame({73, hyremote::Clock::now()}));
    CHECK(backup->accepted.size() == 1 && backup->accepted[0] == 73);
    backup->complete(73, true);
    CHECK(frames.size() == 3 && frames.back().storage);
    source.stop();
}

void testAdaptiveQuickStopDrainsForwardedCoreCallback()
{
    auto native = std::make_unique<ControlledQuickSource>();
    auto publicQt = std::make_unique<ControlledQuickSource>();
    auto *primary = native.get();
    HyRemote::detail::AdaptiveQuickCaptureSource source(
        nullptr, std::move(native), std::move(publicQt));
    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool callbackEntered = false;
    bool callbackReleased = false;
    std::atomic<bool> stopped{false};
    CHECK(source.start([&](hyremote::RemoteFrame) {
        std::unique_lock<std::mutex> lock(gateMutex);
        callbackEntered = true;
        gateCv.notify_all();
        gateCv.wait(lock, [&] { return callbackReleased; });
    }, [](const hyremote::CaptureEvent &) {}));
    CHECK(source.requestFrame({9101, hyremote::Clock::now()}));
    std::thread producer([&] {
        primary->complete(9101, true);
    });
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        const bool entered = gateCv.wait_for(
            lock, std::chrono::seconds(3), [&] { return callbackEntered; });
        CHECK(entered);
    }
    std::thread stopper([&] {
        source.stop();
        stopped.store(true, std::memory_order_release);
    });
    // This test must not rely on an unbounded sleep: the callback is already
    // admitted, and release happens only after stop has had a chance to run.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(!stopped.load(std::memory_order_acquire));
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        callbackReleased = true;
    }
    gateCv.notify_all();
    producer.join();
    stopper.join();
    CHECK(stopped.load(std::memory_order_acquire));
}

void testAdaptiveQuickFallbackSurfaceEpochAndFreshness()
{
    QQuickWindow window;
    window.resize(120, 80);
    auto primary = std::make_unique<ControlledQuickSource>();
    auto backup = std::make_unique<ControlledQuickSource>();
    auto *native = primary.get();
    auto *publicQt = backup.get();
    HyRemote::detail::AdaptiveQuickCaptureSource source(
        &window, std::move(primary), std::move(backup));
    std::vector<hyremote::RemoteFrame> frames;
    int events = 0;
    const auto start = [&] {
        return source.start([&](hyremote::RemoteFrame frame) {
            frames.push_back(std::move(frame));
        }, [&](const hyremote::CaptureEvent &) { ++events; });
    };
    auto request = [&](hyremote::CaptureRequestId id) {
        return source.requestFrame({id, hyremote::Clock::now()});
    };

    CHECK(start());
    CHECK(request(1));
    CHECK(native->accepted.size() == 1);
    native->complete(1, false);
    CHECK(frames.size() == 1 && !frames.back().storage);
    CHECK(request(2));
    native->complete(2, false); // two failures arm one-way public fallback
    CHECK(request(3));
    CHECK(publicQt->accepted.size() == 1 && publicQt->accepted[0] == 3);
    publicQt->complete(3, true);
    CHECK(frames.size() == 3 && frames.back().storage);
    native->complete(2, true); // duplicate, already retired: must be ignored
    CHECK(frames.size() == 3);

    CHECK(request(4));
    window.resize(122, 80); // Surface generation changes before GPU completion
    publicQt->complete(4, true);
    CHECK(frames.size() == 4 && !frames.back().storage);
    CHECK(frames.back().requestId && *frames.back().requestId == 4);
    CHECK(request(5));
    publicQt->complete(5, true);
    CHECK(frames.size() == 5 && frames.back().storage);

    source.stop();
    CHECK(!native->running && !publicQt->running);
    CHECK(start()); // a new controlled Session may try native again
    CHECK(request(6));
    CHECK(native->accepted.size() == 1 && native->accepted[0] == 6);
    native->complete(6, true, std::chrono::milliseconds{800});
    CHECK(frames.size() == 6 && !frames.back().storage); // stale render PTS
    CHECK(request(7));
    native->complete(7, true);
    CHECK(frames.size() == 7 && frames.back().storage);
    source.stop();
    CHECK(events == 0);

    // A native GPU request can finish AFTER a newer public fallback frame.
    // Do not replay that older content, even if it is under the 500ms age
    // budget; preserve the Core request's exact terminal identity.
    CHECK(start());
    CHECK(request(10)); // native, deliberately left in flight
    CHECK(request(11));
    CHECK(request(12));
    native->complete(11, false);
    native->complete(12, false);
    CHECK(request(13)); // fallback now active
    CHECK(!publicQt->accepted.empty() && publicQt->accepted.back() == 13);
    publicQt->complete(13, true, std::chrono::milliseconds{0},
                       hyremote::PtsSource::Completion);
    CHECK(frames.back().requestId && *frames.back().requestId == 13
          && frames.back().storage);
    const std::size_t beforeLateNative = frames.size();
    native->complete(10, true, std::chrono::milliseconds{150});
    CHECK(frames.size() == beforeLateNative + 1);
    CHECK(frames.back().requestId && *frames.back().requestId == 10
          && !frames.back().storage);
    source.stop();
}

void testAdaptiveQuickCapabilityStartRejectionAndBoundedAdmission()
{
    {
        auto native = std::make_unique<ControlledQuickSource>();
        auto publicQt = std::make_unique<ControlledQuickSource>();
        auto *backup = publicQt.get();
        native->startsSuccessfully = false; // unsupported native capability
        HyRemote::detail::AdaptiveQuickCaptureSource source(
            nullptr, std::move(native), std::move(publicQt));
        std::vector<hyremote::RemoteFrame> received;
        CHECK(source.start([&](hyremote::RemoteFrame f) {
            received.push_back(std::move(f));
        }, [](const hyremote::CaptureEvent &) {}));
        CHECK(source.requestFrame({41, hyremote::Clock::now()}));
        CHECK(backup->accepted.size() == 1 && backup->accepted.back() == 41);
        backup->complete(41, true);
        CHECK(received.size() == 1 && received.back().storage);
        source.stop();
    }
    {
        auto native = std::make_unique<ControlledQuickSource>();
        auto publicQt = std::make_unique<ControlledQuickSource>();
        auto *primary = native.get();
        auto *backup = publicQt.get();
        HyRemote::detail::AdaptiveQuickCaptureSource source(
            nullptr, std::move(native), std::move(publicQt));
        std::vector<hyremote::RemoteFrame> received;
        CHECK(source.start([&](hyremote::RemoteFrame f) {
            received.push_back(std::move(f));
        }, [](const hyremote::CaptureEvent &) {}));
        primary->acceptsRequests = false;
        CHECK(!source.requestFrame({42, hyremote::Clock::now()}));
        CHECK(!source.requestFrame({43, hyremote::Clock::now()}));
        CHECK(source.requestFrame({44, hyremote::Clock::now()}));
        CHECK(backup->accepted.size() == 1 && backup->accepted.back() == 44);
        backup->complete(44, true);
        CHECK(received.size() == 1 && received.back().storage);
        // Neither source is permitted to accumulate an unbounded backlog.
        for (hyremote::CaptureRequestId id = 50; id < 58; ++id)
            CHECK(source.requestFrame({id, hyremote::Clock::now()}));
        CHECK(!source.requestFrame({58, hyremote::Clock::now()}));
        for (hyremote::CaptureRequestId id = 50; id < 58; ++id)
            backup->complete(id, true);
        CHECK(received.size() == 9);
        source.stop();
    }
}

void testQuickOpaqueRgbxCpuStorageIsZeroCopy()
{
    QImage opaque(3, 2, QImage::Format_RGBX8888);
    opaque.fill(QColor(19, 58, 113));
    const auto *ownerBytes = opaque.constBits();
    CHECK(ownerBytes != nullptr);
    auto storage = HyRemote::detail::QuickImageFrameStorage::create(opaque);
    CHECK(storage != nullptr);
    if (!storage)
        return;
    opaque = QImage(); // FrameStorage must retain the sole lifetime anchor
    auto mapped = storage->mapRead(0);
    CHECK(mapped && mapped->data && mapped->stride >= 12 && mapped->bytes >= 24);
    if (mapped && mapped->data && mapped->bytes >= 4) {
        CHECK(reinterpret_cast<const uchar *>(mapped->data) == ownerBytes);
        CHECK(mapped->data[0] == std::byte{19});
        CHECK(mapped->data[1] == std::byte{58});
        CHECK(mapped->data[2] == std::byte{113});
        CHECK(mapped->data[3] == std::byte{255});
    }
}

// Both native providers hand their already-owned CPU readback buffer to
// FrameStorage. This prevents a second full-frame allocation/copy and proves
// D3D RowPitch/GL row-order normalization retains the same buffer lifetime.
void testAdoptReadbackStorageNoSecondFullFrame()
{
    using HyRemote::detail::ReadbackRowOrder;
    using HyRemote::detail::ReadbackPixelOrder;
    using HyRemote::detail::adoptReadbackFrameStorage;

    std::shared_ptr<std::byte> pixels(
        new std::byte[24], std::default_delete<std::byte[]>());
    for (std::size_t i = 0; i < 24; ++i)
        pixels.get()[i] = std::byte{0};
    // 2x2 BGRA pixels with 4-byte per-row padding. Row 0 is green, row 1 red.
    pixels.get()[0] = std::byte{0}; pixels.get()[1] = std::byte{200};
    pixels.get()[2] = std::byte{0}; pixels.get()[3] = std::byte{255};
    pixels.get()[12] = std::byte{0}; pixels.get()[13] = std::byte{0};
    pixels.get()[14] = std::byte{222}; pixels.get()[15] = std::byte{255};
    const auto *original = pixels.get();
    auto storage = adoptReadbackFrameStorage(
        pixels, 24, 12, 8, 2, ReadbackRowOrder::BottomUp,
        ReadbackPixelOrder::Bgra8888);
    CHECK(storage != nullptr);
    pixels.reset(); // storage must keep original allocation alive alone
    if (!storage)
        return;
    const auto view = storage->mapRead(0);
    CHECK(view && view->data == original && view->stride == 12
          && view->bytes == 24);
    if (view && view->data) {
        CHECK(view->data[0] == std::byte{222}); // red, from bottom row
        CHECK(view->data[1] == std::byte{0});
        CHECK(view->data[2] == std::byte{0});
        CHECK(view->data[3] == std::byte{255});
        CHECK(view->data[12] == std::byte{0}); // green, from top row
        CHECK(view->data[13] == std::byte{200});
        CHECK(view->data[14] == std::byte{0});
    }
    auto invalid = adoptReadbackFrameStorage(
        std::shared_ptr<std::byte>{}, 24, 12, 8, 2, ReadbackRowOrder::TopDown);
    CHECK(!invalid);
}

void testReadbackFrameStorageRowOrderAndPitch()
{
    using HyRemote::detail::ReadbackRowOrder;
    using HyRemote::detail::copyReadbackFrameStorage;
    std::array<std::byte, 24> raw{};
    for (std::size_t x = 0; x < 8; ++x) {
        raw[x] = std::byte{static_cast<unsigned char>(x + 1)};
        raw[12 + x] = std::byte{static_cast<unsigned char>(x + 11)};
    }
    for (std::size_t x = 8; x < 12; ++x)
        raw[x] = std::byte{0xff}; // deliberate padding that must not leak

    const auto check = [&](ReadbackRowOrder order, bool flipped) {
        auto storage = copyReadbackFrameStorage(
            raw.data(), raw.size(), 12, 8, 2, order);
        CHECK(storage != nullptr);
        if (!storage)
            return;
        const auto mapped = storage->mapRead(0);
        CHECK(mapped && mapped->data && mapped->stride == 8
              && mapped->bytes >= 16);
        if (!mapped || !mapped->data || mapped->bytes < 16)
            return;
        for (std::size_t x = 0; x < 8; ++x) {
            const auto first = flipped ? x + 11 : x + 1;
            const auto second = flipped ? x + 1 : x + 11;
            CHECK(mapped->data[x] == std::byte{static_cast<unsigned char>(first)});
            CHECK(mapped->data[8 + x] == std::byte{static_cast<unsigned char>(second)});
        }
    };
    check(ReadbackRowOrder::TopDown, false);
    check(ReadbackRowOrder::BottomUp, true);

    // Simulate D3D11 BGRA staging with RowPitch padding and verify that
    // the shared CPU contract produces real RGBA bytes, not swapped colors.
    std::array<std::byte, 24> bgra{};
    bgra[0] = std::byte{0x41}; // B
    bgra[1] = std::byte{0x32}; // G
    bgra[2] = std::byte{0x23}; // R
    bgra[3] = std::byte{0xff}; // A
    bgra[12] = std::byte{0x54};
    bgra[13] = std::byte{0x45};
    bgra[14] = std::byte{0x36};
    bgra[15] = std::byte{0xee};
    auto normalized = copyReadbackFrameStorage(
        bgra.data(), bgra.size(), 12, 8, 2, ReadbackRowOrder::TopDown,
        HyRemote::detail::ReadbackPixelOrder::Bgra8888);
    CHECK(normalized != nullptr);
    if (normalized) {
        const auto pixels = normalized->mapRead(0);
        CHECK(pixels && pixels->data && pixels->stride == 8 && pixels->bytes >= 16);
        if (pixels && pixels->data && pixels->bytes >= 16) {
            CHECK(pixels->data[0] == std::byte{0x23});
            CHECK(pixels->data[1] == std::byte{0x32});
            CHECK(pixels->data[2] == std::byte{0x41});
            CHECK(pixels->data[3] == std::byte{0xff});
            CHECK(pixels->data[8] == std::byte{0x36});
            CHECK(pixels->data[9] == std::byte{0x45});
            CHECK(pixels->data[10] == std::byte{0x54});
            CHECK(pixels->data[11] == std::byte{0xee});
        }
    }
    CHECK(!copyReadbackFrameStorage(
        bgra.data(), bgra.size(), 12, 7, 2, ReadbackRowOrder::TopDown,
        HyRemote::detail::ReadbackPixelOrder::Bgra8888));
    CHECK(!copyReadbackFrameStorage(
        raw.data(), 19, 12, 8, 2, ReadbackRowOrder::BottomUp));
    CHECK(!copyReadbackFrameStorage(
        raw.data(), raw.size(), 7, 8, 2, ReadbackRowOrder::TopDown));
    CHECK(!copyReadbackFrameStorage(
        nullptr, raw.size(), 12, 8, 2, ReadbackRowOrder::TopDown));
}

// The backend is data, not permission to try QtPrivate QRhi readback.
// This holds for a software-only Qt Quick plugin, RHI graphics APIs and
// uninitialized/unknown renderers. The route remains the documented Qt
// public grab with its normal bounded retry/failure semantics.
void testQuickRendererCapabilityRouting()
{
    using Api = QSGRendererInterface::GraphicsApi;
    using HyRemote::detail::QuickCaptureBackend;
    using HyRemote::detail::QuickCaptureRoute;
    using HyRemote::detail::QuickRenderingPath;
    using HyRemote::detail::selectQuickCaptureRoute;
    const struct {
        Api api;
        QuickCaptureBackend backend;
    } cases[] = {
        {Api::Unknown, QuickCaptureBackend::Unknown},
        {Api::Software, QuickCaptureBackend::Software},
        {Api::OpenGL, QuickCaptureBackend::OpenGL},
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        {Api::OpenGLRhi, QuickCaptureBackend::OpenGL},
        {Api::Direct3D11Rhi, QuickCaptureBackend::Direct3D11},
        {Api::VulkanRhi, QuickCaptureBackend::Vulkan},
        {Api::MetalRhi, QuickCaptureBackend::Metal},
        {Api::NullRhi, QuickCaptureBackend::Other},
#else
        {Api::Direct3D11, QuickCaptureBackend::Direct3D11},
        {Api::Vulkan, QuickCaptureBackend::Vulkan},
        {Api::Metal, QuickCaptureBackend::Metal},
        {Api::Null, QuickCaptureBackend::Other},
#endif
        {Api::Direct3D12, QuickCaptureBackend::Direct3D12},
        {Api::OpenVG, QuickCaptureBackend::Other},
    };
    for (const auto &scenario : cases) {
        const auto selection = selectQuickCaptureRoute(scenario.api);
        CHECK(selection.backend == scenario.backend);
        CHECK(selection.route == QuickCaptureRoute::PublicQtGrabToImage);
        // An opt-in backend must never turn other render APIs into a PBO
        // route. Qualifying the provider is a Kit/build concern, not an
        // implicit promise that every Qt version or GL driver supports it.
        const auto armed = selectQuickCaptureRoute(scenario.api, true, true);
        CHECK(armed.route ==
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
              QuickCaptureRoute::PublicQtGrabToImage);
        if (scenario.api == Api::OpenGL)
            CHECK(armed.renderingPath == QuickRenderingPath::Qt5Direct);
        if (scenario.api == Api::OpenGLRhi)
            CHECK(armed.renderingPath == QuickRenderingPath::Qt5EarlyRhi);
        // Qt5 direct-GL is opt-in and may use the shared PBO implementation,
        // but never confuse its render phase with Qt5's early RHI path.
        const auto qt5Gl = selectQuickCaptureRoute(scenario.api,
                                                  true, true, true);
        CHECK(qt5Gl.route ==
              (scenario.api == Api::OpenGL
                  ? QuickCaptureRoute::ExperimentalOpenGlPbo
                  : QuickCaptureRoute::PublicQtGrabToImage));
#else
              (scenario.backend == QuickCaptureBackend::OpenGL
               ? QuickCaptureRoute::ExperimentalOpenGlPbo
               : scenario.backend == QuickCaptureBackend::Direct3D11
                 ? QuickCaptureRoute::ExperimentalD3d11Staging
                 : QuickCaptureRoute::PublicQtGrabToImage));
        if (scenario.api == Api::OpenGL)
            CHECK(armed.renderingPath == QuickRenderingPath::Qt6Rhi);
#endif
    }
    if (qEnvironmentVariableIsSet("HYREMOTE_EXPECT_QUICK_OPENGL")) {
        // The dedicated Linux Xvfb/Mesa lane must exercise a REAL OpenGL
        // scenegraph, rather than report success through software fallback.
        QQuickWindow window;
        window.resize(160, 90);
        window.show();
        QCoreApplication::processEvents();
        const auto *renderer = window.rendererInterface();
        CHECK(renderer != nullptr);
        if (renderer) {
            CHECK(renderer->graphicsApi() == Api::OpenGL);
            CHECK(selectQuickCaptureRoute(renderer->graphicsApi()).route
                  == QuickCaptureRoute::PublicQtGrabToImage);
        }
    }
}

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 400)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;

        // processEvents() alone does not wait for a future scene-graph/queued callback. Give the
        // platform event loop a bounded real-time slice on every iteration so Windows and Linux
        // exercise the same asynchronous contract instead of depending on immediate readiness.
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

class PaintedProbeItem final : public QQuickPaintedItem
{
public:
    explicit PaintedProbeItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void paint(QPainter *painter) override
    {
        painter->fillRect(boundingRect(), QColor(20, 120, 220));
    }
};

// #567/H4: a real QQuickPaintedItem can render an intentional intermediate
// A/S/B scene. Track which scene was actually painted, not a timer guess.
class StagedQuickProbeItem final : public QQuickPaintedItem
{
public:
    explicit StagedQuickProbeItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    void setStage(int stage)
    {
        m_requestedStage.store(stage, std::memory_order_release);
        update();
    }

    int paintedStage() const { return m_paintedStage.load(std::memory_order_acquire); }

    void paint(QPainter *painter) override
    {
        const int stage = m_requestedStage.load(std::memory_order_acquire);
        const QRectF left(0.0, 0.0, width() / 2.0, height());
        const QRectF right(width() / 2.0, 0.0, width() / 2.0, height());
        const QColor a(20, 60, 160);
        const QColor b(225, 30, 40);
        painter->fillRect(left, stage >= 1 ? b : a);
        painter->fillRect(right, stage >= 2 ? b : a);
        m_paintedStage.store(stage, std::memory_order_release);
    }

private:
    std::atomic<int> m_requestedStage{0};
    std::atomic<int> m_paintedStage{-1};
};

class EventProbe final : public QObject
{
public:
    int mousePresses = 0;
    int mouseMoves = 0;
    int wheels = 0;
    int keyPresses = 0;
    int keyReleases = 0;
    int inputMethods = 0;
    QPointF lastLocal;
    QPoint lastAngleDelta;
    int lastKey = 0;
    Qt::KeyboardModifiers lastModifiers = Qt::NoModifier;
    QString committedText;

    void reset()
    {
        mousePresses = 0;
        mouseMoves = 0;
        wheels = 0;
        keyPresses = 0;
        keyReleases = 0;
        inputMethods = 0;
        lastLocal = {};
        lastAngleDelta = {};
        lastKey = 0;
        lastModifiers = Qt::NoModifier;
        committedText.clear();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++mousePresses;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = mouse->position();
#else
            lastLocal = mouse->localPos();
#endif
            lastModifiers = mouse->modifiers();
            break;
        }
        case QEvent::MouseMove: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++mouseMoves;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = mouse->position();
#else
            lastLocal = mouse->localPos();
#endif
            lastModifiers = mouse->modifiers();
            break;
        }
        case QEvent::Wheel: {
            auto *wheel = static_cast<QWheelEvent *>(event);
            ++wheels;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            lastLocal = wheel->position();
#else
            lastLocal = wheel->posF();
#endif
            lastAngleDelta = wheel->angleDelta();
            lastModifiers = wheel->modifiers();
            break;
        }
        case QEvent::KeyPress: {
            auto *key = static_cast<QKeyEvent *>(event);
            ++keyPresses;
            lastKey = key->key();
            lastModifiers = key->modifiers();
            break;
        }
        case QEvent::KeyRelease: {
            auto *key = static_cast<QKeyEvent *>(event);
            ++keyReleases;
            lastKey = key->key();
            lastModifiers = key->modifiers();
            break;
        }
        case QEvent::InputMethod: {
            auto *input = static_cast<QInputMethodEvent *>(event);
            ++inputMethods;
            committedText += input->commitString();
            break;
        }
        default:
            break;
        }
        return false;
    }
};

void testQuickFactoryAndOwnedFrame()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(160, 90);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    window.show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input == nullptr);

    const hyremote::CaptureCapabilities caps = components.capture->capabilities();
    CHECK(caps.asynchronous);
    CHECK(caps.cpuReadable);
    CHECK(caps.cpuFormats.size() == 1);
    CHECK(caps.cpuFormats.front() == hyremote::PixelFormat::Rgba8888);

    std::optional<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    hyremote::CaptureRequest request;
    request.id = 21;
    request.requestTime = hyremote::Clock::now();
    CHECK(components.capture->requestFrame(request));
    CHECK(!received.has_value());
    CHECK(pumpUntil([&] { return received.has_value(); }));
    CHECK(events == 0);

    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 21);
        CHECK(received->geometry.size.width > 0);
        CHECK(received->geometry.size.height > 0);

        // #162 criterion 4, Quick half: the same "DPR applied exactly once" contract as the Widgets test.
        // Registered a second time with QT_SCALE_FACTOR=1.5 + HYREMOTE_EXPECT_DPR=1.5; the gate keeps a
        // plugin that silently stays at 1.0 from turning these assertions into decoration.
        const qreal dpr = window.devicePixelRatio();
        const QString expectedDpr = qEnvironmentVariable("HYREMOTE_EXPECT_DPR");
        if (!expectedDpr.isEmpty()) {
            CHECK(qAbs(dpr - expectedDpr.toDouble()) < 0.01);
        }
        CHECK(received->geometry.size.width == qRound(160.0 * dpr));
        CHECK(received->geometry.size.height == qRound(90.0 * dpr));

        // The same physical-to-logical mapping is required for the Quick cursor path.
        const std::uint32_t sourceWidth = static_cast<std::uint32_t>(qRound(160.0 * dpr));
        const std::uint32_t sourceHeight = static_cast<std::uint32_t>(qRound(90.0 * dpr));
        const QPointF mappedCentre = HyRemote::detail::mapFramebufferCursorPoint(
            static_cast<int>(sourceWidth / 2U),
            static_cast<int>(sourceHeight / 2U),
            sourceWidth,
            sourceHeight,
            160.0,
            90.0);
        CHECK(qAbs(mappedCentre.x() - 79.5) <= 0.6);
        CHECK(qAbs(mappedCentre.y() - 44.5) <= 0.6);
        const QPointF mappedEdge = HyRemote::detail::mapFramebufferCursorPoint(
            static_cast<int>(sourceWidth), static_cast<int>(sourceHeight), sourceWidth, sourceHeight, 160.0, 90.0);
        CHECK(qAbs(mappedEdge.x() - 159.0) < 0.01);
        CHECK(qAbs(mappedEdge.y() - 89.0) < 0.01);

        // Resize transition on the DPR in force.
        window.resize(320, 180);
        content.setWidth(320);
        content.setHeight(180);
        QCoreApplication::processEvents();
        received.reset();
        hyremote::CaptureRequest resizedRequest{22, hyremote::Clock::now()};
        CHECK(components.capture->requestFrame(resizedRequest));
        CHECK(pumpUntil([&] { return received.has_value(); }));
        if (received) {
            CHECK(received->requestId.has_value());
            CHECK(received->requestId.value() == 22);
            CHECK(received->geometry.size.width == qRound(320.0 * dpr));
            CHECK(received->geometry.size.height == qRound(180.0 * dpr));
        }
        CHECK(received->geometry.pixelFormat == hyremote::PixelFormat::Rgba8888);
        CHECK(received->geometry.alphaMode == hyremote::AlphaMode::Premultiplied);
        CHECK(received->geometry.planeCount == 1);
        CHECK(received->damage.kind == hyremote::DamageKind::FullFrame);
        CHECK(received->timing.requestTime.has_value());
        CHECK(received->timing.completionTime.has_value());
        CHECK(received->storage != nullptr);
        if (received->storage) {
            const auto plane = received->storage->mapRead(0);
            CHECK(plane.has_value());
            if (plane) {
                CHECK(plane->data != nullptr);
                CHECK(plane->bytes > 0);
                CHECK(plane->stride == static_cast<std::size_t>(received->geometry.size.width) * 4U);
            }
        }
    }

    components.capture->stop();
}

// Public Qt 5/6 API only: two Core requests queued before the same GUI event
// turn must share ONE actual offscreen grab and its immutable CPU storage,
// while each accepted request receives its own terminal identity and timing.
// A later page transition must cause a NEW image, never replay the old batch.
void testQuickQueuedRequestsCoalesceWithoutReplayingStalePixels()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++events; }));
    content.setStage(0);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 0; }));

    const auto firstTime = hyremote::Clock::now();
    const auto secondTime = hyremote::Clock::now();
    // Deliberately no processEvents between the two calls.
    CHECK(components.capture->requestFrame({701U, firstTime}));
    CHECK(components.capture->requestFrame({702U, secondTime}));
    CHECK(received.empty());
    CHECK(pumpUntil([&] { return received.size() == 2U; }));
    CHECK(events == 0);
    if (received.size() == 2U) {
        CHECK(received[0].requestId && *received[0].requestId == 701U);
        CHECK(received[1].requestId && *received[1].requestId == 702U);
        CHECK(received[0].timing.requestTime
              && *received[0].timing.requestTime == firstTime);
        CHECK(received[1].timing.requestTime
              && *received[1].timing.requestTime == secondTime);
        CHECK(received[0].storage && received[1].storage);
        CHECK(received[0].storage == received[1].storage);
        CHECK(received[0].timing.completionTime == received[1].timing.completionTime);
        CHECK(received[0].geometry.size.width == received[1].geometry.size.width);
        CHECK(received[0].geometry.size.height == received[1].geometry.size.height);

        if (received[0].storage) {
            const auto plane = received[0].storage->mapRead(0);
            CHECK(plane && plane->data);
            if (plane && plane->data) {
                const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                                   + (received[0].geometry.size.height / 2U) * plane->stride
                                   + (received[0].geometry.size.width / 4U) * 4U;
                CHECK(pixel[0] == 20U && pixel[1] == 60U && pixel[2] == 160U);
            }
        }
    }

    // A new, actually rendered scene after the prior batch has completed
    // must NOT borrow its source image or report stale page A as page B.
    content.setStage(2);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 2; }));
    CHECK(components.capture->requestFrame({703U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 3U; }));
    if (received.size() == 3U) {
        CHECK(received[2].requestId && *received[2].requestId == 703U);
        CHECK(received[2].storage != received[0].storage);
        const auto plane = received[2].storage ? received[2].storage->mapRead(0)
                                               : std::nullopt;
        CHECK(plane && plane->data);
        if (plane && plane->data) {
            const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                               + (received[2].geometry.size.height / 2U) * plane->stride
                               + (received[2].geometry.size.width / 4U) * 4U;
            CHECK(pixel[0] == 225U && pixel[1] == 30U && pixel[2] == 40U);
        }
    }
    CHECK(events == 0);
    components.capture->stop();
}

// The single-flight admission contract is independent of operating system,
// Qt Quick's rendering backend or a particular QQuickItemGrabResult timing.
// This model test fails if an in-flight batch accidentally becomes eligible
// for another grab when a later Core request is admitted.
void testQuickSingleFlightDispatchPolicy()
{
    HyRemote::detail::QuickCaptureSingleFlight gate;
    CHECK(gate.needsDispatch());
    gate.dispatchPosted();
    CHECK(!gate.needsDispatch());
    CHECK(gate.beginDispatchedGrab());
    CHECK(gate.grabInProgress());
    // The newer request may be queued but must NOT submit a second grab.
    CHECK(!gate.needsDispatch());
    gate.grabCompleted();
    CHECK(!gate.grabInProgress());
    CHECK(gate.needsDispatch());
    gate.dispatchPosted();
    CHECK(gate.beginDispatchedGrab());
    CHECK(!gate.needsDispatch());
    // Stop / restart invalidates both the old dispatch and active grab.
    gate.reset();
    CHECK(gate.needsDispatch());
    CHECK(!gate.grabInProgress());
}

// A later request in a different GUI turn must get a NEW Qt scene capture,
// but may not overtake the earlier asynchronous result. The old adapter
// began multiple offscreen grabs concurrently; scene/driver readiness could
// reorder completions and publish an older page after a newer page.
void testQuickDeferredRequestsKeepOrderAndFreshPixels()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();
    content.setStage(0);
    CHECK(pumpUntil([&] { return painted.load(std::memory_order_acquire) == 0; }));

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> frames;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    const auto firstTime = hyremote::Clock::now();
    CHECK(components.capture->requestFrame({811U, firstTime}));
    bool queuedSecondAccepted = false;
    const auto secondTime = hyremote::Clock::now();
    // The first GUI-dispatch event was queued BEFORE this callback. Thus the
    // second request is admitted in a distinct GUI turn while the first
    // grab may still be waiting for its asynchronous ready signal.
    CHECK(QMetaObject::invokeMethod(
        QCoreApplication::instance(), [&] {
            content.setStage(2);
            queuedSecondAccepted = components.capture->requestFrame(
                {812U, secondTime});
        }, Qt::QueuedConnection));
    CHECK(pumpUntil([&] { return frames.size() == 2U; }, 600));
    CHECK(queuedSecondAccepted);
    CHECK(events == 0);
    if (frames.size() == 2U) {
        CHECK(frames[0].requestId && *frames[0].requestId == 811U);
        CHECK(frames[1].requestId && *frames[1].requestId == 812U);
        CHECK(frames[0].timing.requestTime
              && *frames[0].timing.requestTime == firstTime);
        CHECK(frames[1].timing.requestTime
              && *frames[1].timing.requestTime == secondTime);
        CHECK(frames[0].storage && frames[1].storage);
        CHECK(frames[0].storage != frames[1].storage);
        const auto leftRgb = [](const hyremote::RemoteFrame &frame) {
            if (!frame.storage)
                return std::uint32_t(0);
            const auto plane = frame.storage->mapRead(0);
            if (!plane || !plane->data)
                return std::uint32_t(0);
            const auto *pixel = reinterpret_cast<const std::uint8_t *>(plane->data)
                                + (frame.geometry.size.height / 2U) * plane->stride
                                + (frame.geometry.size.width / 4U) * 4U;
            return (static_cast<std::uint32_t>(pixel[0]) << 16U)
                   | (static_cast<std::uint32_t>(pixel[1]) << 8U)
                   | static_cast<std::uint32_t>(pixel[2]);
        };
        // Qt grabToImage is asynchronous: submission at stage A does
        // not promise an A pixel snapshot. By the time its offscreen render
        // takes place the application may already have painted stage B.
        // Either is valid for request 811; request 812, whose NEW grab starts
        // strictly after 811 completes, must observe the updated B page.
        const auto firstRgb = leftRgb(frames[0]);
        CHECK(firstRgb == 0x143ca0U || firstRgb == 0xe11e28U);
        CHECK(leftRgb(frames[1]) == 0xe11e28U);
    }
    components.capture->stop();
}

// A hidden target exercises the same single-flight gate across bounded
// retry timers. Requests arriving during the previous retry cannot launch
// an independent Qt grab or hold a Core slot forever; every accepted request
// must have a terminal callback, then a visible target must recover.
void testQuickSingleFlightHiddenRetryQueuesAndDrains()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(96, 64);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(96);
    content.setHeight(64);
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::vector<hyremote::RemoteFrame> frames;
    int unavailableEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { frames.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &event) {
            if (event.code == hyremote::CaptureEventCode::TemporarilyUnavailable)
                ++unavailableEvents;
        }));
    CHECK(components.capture->requestFrame({820U, hyremote::Clock::now()}));
    QCoreApplication::processEvents(); // first GUI batch enters bounded retry
    CHECK(components.capture->requestFrame({821U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return frames.size() == 2U; }, 600));
    CHECK(unavailableEvents == 2); // each independently retried batch
    if (frames.size() == 2U) {
        CHECK(frames[0].requestId && *frames[0].requestId == 820U);
        CHECK(frames[1].requestId && *frames[1].requestId == 821U);
        CHECK(!frames[0].storage && !frames[1].storage);
    }
    window.show();
    CHECK(components.capture->requestFrame({822U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return frames.size() == 3U; }, 500));
    if (frames.size() == 3U) {
        CHECK(frames[2].requestId && *frames[2].requestId == 822U);
        CHECK(frames[2].storage != nullptr);
    }
    components.capture->stop();
}

// A queued grab from a stopped capture lifetime must not complete using the
// handlers or framebuffer identity of a newly started lifetime.
void testQuickQueuedRequestsDoNotCrossStopRestart()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(120, 80);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(120);
    content.setHeight(80);
    window.show();
    QCoreApplication::processEvents();

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    int oldCallbacks = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++oldCallbacks; },
        [&](const hyremote::CaptureEvent &) { ++oldCallbacks; }));
    CHECK(components.capture->requestFrame({710U, hyremote::Clock::now()}));
    CHECK(components.capture->requestFrame({711U, hyremote::Clock::now()}));
    components.capture->stop(); // no GUI pump: both belong to old generation

    std::vector<hyremote::CaptureRequestId> newCallbacks;
    int newEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId)
                newCallbacks.push_back(*frame.requestId);
        },
        [&](const hyremote::CaptureEvent &) { ++newEvents; }));
    CHECK(components.capture->requestFrame({712U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return newCallbacks.size() == 1U; }));
    CHECK(oldCallbacks == 0);
    CHECK(newCallbacks.size() == 1U && newCallbacks[0] == 712U);
    CHECK(newEvents == 0);
    components.capture->stop();
}

// #609/#790: each successful asynchronous Qt Quick grab used to leave a
// two-second singleShot callback alive after the image was delivered. Verify
// the replacement cancels its owned timeout promptly instead of maintaining
// a growing set of dormant Qt timers for high-frequency captures.
void testCompletedQuickGrabsCancelTimeouts()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(128, 72);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(128);
    content.setHeight(72);
    window.show();
    QCoreApplication::processEvents();

    auto *dispatcher = QCoreApplication::instance();
    CHECK(dispatcher != nullptr);
    if (!dispatcher)
        return;
    // The grab deadlines have the dispatcher as their direct QObject parent.
    // Count after warmup to ignore unrelated Qt platform timers.
    const auto deadlineCount = [dispatcher] {
        return dispatcher->findChildren<QTimer *>(QString(),
                                                  Qt::FindDirectChildrenOnly).size();
    };
    const int baseline = deadlineCount();
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported && components.capture != nullptr);
    if (!components.capture)
        return;

    hyremote::CaptureRequestId latest = 0;
    int successes = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) {
            if (frame.requestId && frame.storage) {
                latest = *frame.requestId;
                ++successes;
            }
        },
        [&](const hyremote::CaptureEvent &) {}));
    constexpr int count = 20;
    for (int index = 0; index < count; ++index) {
        const auto id = static_cast<hyremote::CaptureRequestId>(3000 + index);
        const bool accepted = components.capture->requestFrame(
            {id, hyremote::Clock::now()});
        CHECK(accepted);
        if (!accepted || !pumpUntil([&] { return latest == id; }))
            break;
        // Flush just Qt's deferred deletion, never wait 2s for the watchdog:
        // cancellation is part of the ready callback itself.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK(deadlineCount() <= baseline + 1);
    }
    CHECK(successes == count);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(deadlineCount() <= baseline + 1);
    components.capture->stop();
}

// H4 mechanism check: distinguish a source that really paints A/S/B from an
// adapter which invents mixed pixels. A and B are complete pages, while S is
// deliberately half of each, representing an application-staged transition.
// The Qt rendering signal is a stage barrier; no sleep is used for ordering.
void testStagedQuickSourceCaptureFollowsPaintedScene()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(160, 90);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(160);
    content.setHeight(90);

    std::atomic<int> completedPaintStage{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        completedPaintStage.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.supported && components.capture != nullptr);
    if (!components.capture)
        return;

    std::optional<hyremote::RemoteFrame> received;
    int captureEvents = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++captureEvents; }));

    for (int stage = 0; stage <= 2; ++stage) {
        // afterRendering is emitted only after the Quick scene graph has drawn
        // the stage; requestFrame is issued strictly after that confirmation.
        content.setStage(stage);
        const bool rendered = pumpUntil([&] {
            return completedPaintStage.load(std::memory_order_acquire) == stage;
        });
        CHECK(rendered);
        if (!rendered)
            break;

        received.reset();
        hyremote::CaptureRequest request;
        request.id = static_cast<hyremote::CaptureRequestId>(500U + stage);
        request.requestTime = hyremote::Clock::now();
        CHECK(components.capture->requestFrame(request));
        const bool captured = pumpUntil([&] { return received.has_value(); });
        CHECK(captured);
        if (!captured)
            break;

        CHECK(received->requestId.has_value() && *received->requestId == request.id);
        CHECK(received->storage != nullptr);
        if (!received->storage)
            break;
        CHECK(received->geometry.pixelFormat == hyremote::PixelFormat::Rgba8888);
        CHECK(received->geometry.alphaMode == hyremote::AlphaMode::Premultiplied);
        const auto plane = received->storage->mapRead(0);
        CHECK(plane.has_value() && plane->data != nullptr);
        if (!plane || !plane->data)
            break;

        const auto width = static_cast<std::size_t>(received->geometry.size.width);
        const auto height = static_cast<std::size_t>(received->geometry.size.height);
        CHECK(width >= 4U && height >= 4U);
        CHECK(plane->stride >= width * 4U && height <= plane->bytes / plane->stride);
        if (width < 4U || height < 4U || plane->stride < width * 4U
            || height > plane->bytes / plane->stride)
            break;

        const auto sample = [&](std::size_t x) {
            const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                                + (height / 2U) * plane->stride + x * 4U;
            return (static_cast<std::uint32_t>(pixel[0]) << 16U)
                   | (static_cast<std::uint32_t>(pixel[1]) << 8U)
                   | static_cast<std::uint32_t>(pixel[2]);
        };
        constexpr std::uint32_t aRgb = 0x143ca0U;
        constexpr std::uint32_t bRgb = 0xe11e28U;
        CHECK(sample(width / 4U) == (stage >= 1 ? bRgb : aRgb));
        CHECK(sample(3U * width / 4U) == (stage >= 2 ? bRgb : aRgb));
    }
    CHECK(captureEvents == 0);
    components.capture->stop();
}

void testDestroyedQuickTargetReportsTargetLost()
{
    HyRemote::detail::resetFactories();

    auto *window = new QQuickWindow;
    window->resize(80, 40);
    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(window, false);
    CHECK(components.capture != nullptr);

    int frames = 0;
    std::optional<hyremote::CaptureEvent> lastEvent;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame) { ++frames; },
        [&](const hyremote::CaptureEvent &event) { lastEvent = event; }));

    hyremote::CaptureRequest request{22, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(request));
    delete window;

    CHECK(pumpUntil([&] { return lastEvent.has_value(); }));
    CHECK(frames == 0);
    CHECK(lastEvent.has_value());
    if (lastEvent) {
        CHECK(lastEvent->code == hyremote::CaptureEventCode::TargetLost);
        CHECK(!lastEvent->recoverable);
    }
    components.capture->stop();
}

void testUnavailableQuickCaptureHasBoundedRetryAndRecovers()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(120, 60);
    PaintedProbeItem content(window.contentItem());
    content.setWidth(120);
    content.setHeight(60);
    // Intentionally leave the window hidden. The adapter accepts the asynchronous request, but a
    // permanently unavailable public Quick grab must not retain Core's in-flight slot forever.

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);

    std::optional<hyremote::RemoteFrame> received;
    std::optional<hyremote::CaptureEvent> lastEvent;
    int eventCount = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &event) {
            lastEvent = event;
            ++eventCount;
        }));

    hyremote::CaptureRequest hiddenRequest{31, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(hiddenRequest));
    CHECK(pumpUntil([&] { return received.has_value() && lastEvent.has_value(); }));
    CHECK(eventCount == 1);
    CHECK(lastEvent.has_value());
    if (lastEvent) {
        CHECK(lastEvent->code == hyremote::CaptureEventCode::TemporarilyUnavailable);
        CHECK(lastEvent->recoverable);
    }
    CHECK(received.has_value());
    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 31);
        CHECK(received->storage == nullptr);
        CHECK(received->geometry.size.width == 0);
        CHECK(received->geometry.size.height == 0);
        CHECK(received->timing.requestTime.has_value());
        CHECK(received->timing.completionTime.has_value());
    }

    // A later request must remain usable. Showing the same target after the bounded unavailable
    // completion proves the adapter did not enter a terminal state or keep retry state across requests.
    received.reset();
    lastEvent.reset();
    eventCount = 0;
    window.show();
    QCoreApplication::processEvents();
    hyremote::CaptureRequest recoveryRequest{32, hyremote::Clock::now()};
    CHECK(components.capture->requestFrame(recoveryRequest));
    CHECK(pumpUntil([&] { return received.has_value(); }));
    CHECK(eventCount == 0);
    if (received) {
        CHECK(received->requestId.has_value());
        CHECK(received->requestId.value() == 32);
        CHECK(received->storage != nullptr);
        CHECK(received->geometry.size.width > 0);
        CHECK(received->geometry.size.height > 0);
    }

    components.capture->stop();
}

// If a same-turn Qt grab cannot complete after the finite retry budget,
// every accepted Core slot must receive exactly one terminal frame, not just
// the first request in the batch. A later visible grab remains recoverable.
void testQuickHiddenBatchCompletesEveryAcceptedRequest()
{
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.resize(96, 64);
    PaintedProbeItem item(window.contentItem());
    item.setWidth(96);
    item.setHeight(64);
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;

    std::vector<hyremote::RemoteFrame> received;
    int unavailable = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received.push_back(std::move(frame)); },
        [&](const hyremote::CaptureEvent &event) {
            if (event.code == hyremote::CaptureEventCode::TemporarilyUnavailable)
                ++unavailable;
        }));
    // No GUI event processing: the hidden requests necessarily share a batch.
    CHECK(components.capture->requestFrame({720U, hyremote::Clock::now()}));
    CHECK(components.capture->requestFrame({721U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 2U; }));
    CHECK(unavailable == 1);
    if (received.size() == 2U) {
        CHECK(received[0].requestId && *received[0].requestId == 720U);
        CHECK(received[1].requestId && *received[1].requestId == 721U);
        CHECK(!received[0].storage && !received[1].storage);
        CHECK(received[0].timing.requestTime && received[1].timing.requestTime);
        CHECK(received[0].timing.completionTime && received[1].timing.completionTime);
    }

    window.show();
    QCoreApplication::processEvents();
    CHECK(components.capture->requestFrame({722U, hyremote::Clock::now()}));
    CHECK(pumpUntil([&] { return received.size() == 3U; }));
    if (received.size() == 3U) {
        CHECK(received[2].requestId && *received[2].requestId == 722U);
        CHECK(received[2].storage != nullptr);
    }
    CHECK(unavailable == 1);
    components.capture->stop();
}

void testQuickInputIsQueuedToWindow()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(200, 100);
    window.show();
    QCoreApplication::processEvents();

    EventProbe probe;
    window.installEventFilter(&probe);
    probe.reset();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.supported);
    CHECK(components.capture != nullptr);
    CHECK(components.input != nullptr);

    hyremote::InputEvent press;
    press.kind = hyremote::InputEventKind::PointerButton;
    press.sourceViewport = {200U, 100U, 1.0F};
    press.x = 50.0F;
    press.y = 25.0F;
    press.button = hyremote::PointerButton::Left;
    press.pressed = true;
    press.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(press);

    CHECK(probe.mousePresses == 0);
    CHECK(pumpUntil([&] { return probe.mousePresses == 1; }));
    CHECK(std::fabs(probe.lastLocal.x() - 50.0) <= 1.0);
    CHECK(std::fabs(probe.lastLocal.y() - 25.0) <= 1.0);
    CHECK(probe.lastModifiers.testFlag(Qt::ShiftModifier));

    hyremote::InputEvent wheel;
    wheel.kind = hyremote::InputEventKind::PointerScroll;
    wheel.sourceViewport = {200U, 100U, 1.0F};
    wheel.x = 50.0F;
    wheel.y = 25.0F;
    wheel.scrollY = -1.0F;
    components.input->post(wheel);
    CHECK(pumpUntil([&] { return probe.wheels == 1; }));
    CHECK(probe.lastAngleDelta.y() == -120);

    hyremote::InputEvent key;
    key.kind = hyremote::InputEventKind::Key;
    key.key = hyremote::KeyCode::A;
    key.pressed = true;
    key.modifiers = hyremote::modifierMask(hyremote::InputModifier::Control);
    components.input->post(key);
    CHECK(pumpUntil([&] { return probe.keyPresses == 1; }));
    CHECK(probe.lastKey == Qt::Key_A);
    CHECK(probe.lastModifiers.testFlag(Qt::ControlModifier));

    key.pressed = false;
    components.input->post(key);
    CHECK(pumpUntil([&] { return probe.keyReleases == 1; }));

    hyremote::InputEvent text;
    text.kind = hyremote::InputEventKind::Text;
    text.textUtf8 = "A";
    components.input->post(text);
    CHECK(pumpUntil([&] { return probe.inputMethods == 1; }));
    CHECK(probe.committedText == QStringLiteral("A"));
}

void testQueuedQuickInputIsDroppedWhenSinkIsDestroyed()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(100, 50);
    EventProbe probe;
    window.installEventFilter(&probe);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.input != nullptr);

    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::PointerMove;
    event.sourceViewport = {100U, 50U, 1.0F};
    event.x = 10.0F;
    event.y = 10.0F;
    components.input->post(event);
    components.input.reset();

    QCoreApplication::processEvents();
    CHECK(probe.mouseMoves == 0);
}

// This is a process CPU clock, not a stopwatch. MSVC std::clock() returns
// elapsed wall time and would mislabel GUI event-loop waits as "CPU". Match
// the rootless CPU probe's OS clocks; unsupported OS builds can run ordinary
// Quick CTest but must not emit synthetic CPU qualification data.
bool quickProfileProcessCpuNs(std::uint64_t &out) noexcept
{
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return false;
    ULARGE_INTEGER kernelTicks{}, userTicks{};
    kernelTicks.LowPart = kernel.dwLowDateTime;
    kernelTicks.HighPart = kernel.dwHighDateTime;
    userTicks.LowPart = user.dwLowDateTime;
    userTicks.HighPart = user.dwHighDateTime;
    out = (kernelTicks.QuadPart + userTicks.QuadPart) * 100ULL;
    return true;
#elif defined(__linux__)
    timespec sample{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &sample) != 0
        || sample.tv_sec < 0 || sample.tv_nsec < 0
        || sample.tv_nsec >= 1000000000L)
        return false;
    out = static_cast<std::uint64_t>(sample.tv_sec) * 1000000000ULL
          + static_cast<std::uint64_t>(sample.tv_nsec);
    return true;
#else
    (void)out;
    return false; // no guessed CPU-time semantics for other OS toolchains
#endif
}

// Test the semantic distinction responsible for review P2 on a real
// Windows/Linux runner, not only that the OS function compiles. A 120ms
// sleep must advance wall time but not consume roughly 120ms of process CPU.
// Do this before creating any scenegraph rendering threads.
void testQuickProfileClockDoesNotMeasureSleepAsCpu()
{
#if defined(_WIN32) || defined(__linux__)
    std::uint64_t before = 0, after = 0;
    const bool okBefore = quickProfileProcessCpuNs(before);
    CHECK(okBefore);
    if (!okBefore)
        return;
    const auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    const auto elapsedWallMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const bool okAfter = quickProfileProcessCpuNs(after);
    CHECK(okAfter && after >= before);
    if (!okAfter || after < before)
        return;
    const double processCpuMs = static_cast<double>(after - before) / 1000000.0;
    CHECK(elapsedWallMs >= 100.0);
    CHECK(processCpuMs < elapsedWallMs * 0.70);
#endif
}

// Advisory, opt-in production PUBLIC Qt Quick capture baseline. Never runs
// as part of ordinary CTest: a hosted Xvfb/Mesa graphics environment cannot
// establish release CPU benefit, especially not for RK3588 EGLFS or D3D11
// private QRhi. Bracket one animated case with equivalent static A1/A2
// workloads so large host drift is visible, rather than averaging it away.
struct PublicQuickProfileCase
{
    QString name;
    int validFrames = 0;
    double processCpuMs = 0.0;
    double wallMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    int observedWidth = 0;
    int observedHeight = 0;
};

double profilePercentile(std::vector<double> times, double fraction)
{
    if (times.empty())
        return 0.0;
    std::sort(times.begin(), times.end());
    const auto position = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(times.size())));
    return times[std::min(times.size() - 1U,
                          position > 0U ? position - 1U : 0U)];
}

bool profileHasStagePixels(const hyremote::RemoteFrame &frame, int stage)
{
    if (!frame.storage || frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || frame.geometry.size.width < 8U || frame.geometry.size.height < 8U)
        return false;
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return false;
    const std::size_t width = frame.geometry.size.width;
    const std::size_t height = frame.geometry.size.height;
    if (plane->stride < width * 4U || height > plane->bytes / plane->stride)
        return false;

    const auto sample = [&](std::size_t x) {
        const auto *pixel = reinterpret_cast<const unsigned char *>(plane->data)
                            + (height / 2U) * plane->stride + x * 4U;
        return (static_cast<std::uint32_t>(pixel[0]) << 16U)
               | (static_cast<std::uint32_t>(pixel[1]) << 8U)
               | static_cast<std::uint32_t>(pixel[2]);
    };
    constexpr std::uint32_t a = 0x143ca0U;
    constexpr std::uint32_t b = 0xe11e28U;
    return sample(width / 4U) == (stage >= 1 ? b : a)
           && sample(3U * width / 4U) == (stage >= 2 ? b : a);
}

PublicQuickProfileCase runPublicQuickProfileCase(
    hyremote::CaptureSource &source, StagedQuickProbeItem &content,
    const std::atomic<int> &renderedStage, std::optional<hyremote::RemoteFrame> &received,
    QString name, bool animate, int &lastStage, std::uint64_t &nextRequestId)
{
    constexpr int warmup = 8;
    constexpr int measured = 24;
    using Clock = std::chrono::steady_clock;
    PublicQuickProfileCase result;
    result.name = std::move(name);
    std::vector<double> latencies;
    latencies.reserve(measured);
    std::uint64_t startCpuNs = 0;
    bool cpuClockValid = false;
    Clock::time_point startWall{};
    bool started = false;

    for (int index = -warmup; index < measured; ++index) {
        if (index == 0) {
            cpuClockValid = quickProfileProcessCpuNs(startCpuNs);
            CHECK(cpuClockValid);
            startWall = Clock::now();
            started = true;
        }
        // Public A1 and A2 are truly static after the first paint. Dynamic
        // switches A -> split -> B -> A and waits for the actual scene paint.
        const int stage = animate ? ((index + warmup) % 3) : 0;
        if (stage != lastStage) {
            content.setStage(stage);
            lastStage = stage;
            const bool painted = pumpUntil([&] {
                return renderedStage.load(std::memory_order_acquire) == stage;
            });
            CHECK(painted);
            if (!painted)
                break;
        }

        received.reset();
        const auto issuedAt = Clock::now();
        const auto id = ++nextRequestId;
        const bool accepted = source.requestFrame({id, hyremote::Clock::now()});
        CHECK(accepted);
        if (!accepted)
            break;
        const bool completed = pumpUntil([&] { return received.has_value(); }, 500);
        CHECK(completed);
        if (!completed)
            break;
        const auto completedAt = Clock::now();
        CHECK(received->requestId && *received->requestId == id);
        CHECK(profileHasStagePixels(*received, stage));
        if (!received->requestId || *received->requestId != id
            || !profileHasStagePixels(*received, stage))
            break;

        if (index >= 0) {
            ++result.validFrames;
            result.observedWidth = static_cast<int>(received->geometry.size.width);
            result.observedHeight = static_cast<int>(received->geometry.size.height);
            latencies.push_back(std::chrono::duration<double, std::milli>(
                                    completedAt - issuedAt).count());
        }
    }

    if (started) {
        std::uint64_t endCpuNs = 0;
        const bool cpuValid = cpuClockValid && quickProfileProcessCpuNs(endCpuNs)
                              && endCpuNs >= startCpuNs;
        CHECK(cpuValid);
        result.processCpuMs = cpuValid
                                  ? static_cast<double>(endCpuNs - startCpuNs) / 1000000.0
                                  : 0.0;
        result.wallMs = std::chrono::duration<double, std::milli>(
                            Clock::now() - startWall).count();
    }
    result.p50Ms = profilePercentile(latencies, 0.50);
    result.p95Ms = profilePercentile(latencies, 0.95);
    CHECK(result.validFrames == measured);
    return result;
}

QJsonObject publicProfileToJson(const PublicQuickProfileCase &sample)
{
    QJsonObject result;
    result.insert(QStringLiteral("name"), sample.name);
    result.insert(QStringLiteral("valid_frames"), sample.validFrames);
    result.insert(QStringLiteral("observed_width"), sample.observedWidth);
    result.insert(QStringLiteral("observed_height"), sample.observedHeight);
    result.insert(QStringLiteral("process_cpu_ms"), sample.processCpuMs);
    result.insert(QStringLiteral("wall_ms"), sample.wallMs);
    result.insert(QStringLiteral("request_p50_ms"), sample.p50Ms);
    result.insert(QStringLiteral("request_p95_ms"), sample.p95Ms);
    return result;
}

void runPublicQuickProfile()
{
    constexpr int requestedWidth = 1920;
    constexpr int requestedHeight = 1080;
    constexpr int measuredFrames = 24;
    constexpr double maxStableSpreadPercent = 15.0;
    HyRemote::detail::resetFactories();
    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.resize(requestedWidth, requestedHeight);
    StagedQuickProbeItem content(window.contentItem());
    content.setWidth(requestedWidth);
    content.setHeight(requestedHeight);
    std::atomic<int> painted{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        painted.store(content.paintedStage(), std::memory_order_release);
    }, Qt::DirectConnection);
    window.show();
    const auto visible = pumpUntil([&] {
        return window.isVisible() && painted.load(std::memory_order_acquire) >= 0;
    });
    CHECK(visible);
    if (!visible)
        return;

    const auto *renderer = window.rendererInterface();
    const auto selected = HyRemote::detail::selectQuickCaptureRoute(
        renderer ? renderer->graphicsApi() : QSGRendererInterface::Unknown);
    CHECK(selected.route == HyRemote::detail::QuickCaptureRoute::PublicQtGrabToImage);
    const QString backend = [&] {
        using B = HyRemote::detail::QuickCaptureBackend;
        switch (selected.backend) {
        case B::Software: return QStringLiteral("software");
        case B::OpenGL: return QStringLiteral("opengl");
        case B::Direct3D11: return QStringLiteral("d3d11");
        case B::Direct3D12: return QStringLiteral("d3d12");
        case B::Vulkan: return QStringLiteral("vulkan");
        case B::Metal: return QStringLiteral("metal");
        case B::Other: return QStringLiteral("other");
        default: return QStringLiteral("unknown");
        }
    }();
    const QString expectedBackend =
        qEnvironmentVariable("HYREMOTE_PUBLIC_PROFILE_EXPECT_BACKEND");
    if (!expectedBackend.isEmpty())
        CHECK(backend == expectedBackend);

    auto components = HyRemote::detail::createTargetComponents(&window, false);
    CHECK(components.capture != nullptr);
    if (!components.capture)
        return;
    std::optional<hyremote::RemoteFrame> received;
    int events = 0;
    CHECK(components.capture->start(
        [&](hyremote::RemoteFrame frame) { received = std::move(frame); },
        [&](const hyremote::CaptureEvent &) { ++events; }));

    int lastStage = content.paintedStage();
    std::uint64_t nextRequestId = 50000U;
    const auto a1 = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_static_a1"), false, lastStage, nextRequestId);
    const auto dynamic = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_a_split_b_a"), true, lastStage, nextRequestId);
    const auto a2 = runPublicQuickProfileCase(
        *components.capture, content, painted, received,
        QStringLiteral("public_static_a2"), false, lastStage, nextRequestId);
    CHECK(events == 0);
    components.capture->stop();

    const double meanCpu = (a1.processCpuMs + a2.processCpuMs) / 2.0;
    const double cpuSpread = meanCpu > 0.0
        ? 100.0 * std::abs(a1.processCpuMs - a2.processCpuMs) / meanCpu
        : -1.0;
    const bool fullResolution = a1.observedWidth == requestedWidth
        && a1.observedHeight == requestedHeight
        && dynamic.observedWidth == requestedWidth
        && dynamic.observedHeight == requestedHeight
        && a2.observedWidth == requestedWidth
        && a2.observedHeight == requestedHeight;
    if (qEnvironmentVariableIsSet("HYREMOTE_EXPECT_PHYSICAL_1080P"))
        CHECK(fullResolution);
    CHECK(a1.validFrames == measuredFrames
          && dynamic.validFrames == measuredFrames
          && a2.validFrames == measuredFrames);
    CHECK(a1.processCpuMs > 0.0 && a2.processCpuMs > 0.0);
    const bool stable = cpuSpread >= 0.0 && cpuSpread <= maxStableSpreadPercent;

    // A failed pixel, stage, backend, source or clock assertion must never
    // leave an apparently qualified JSON artifact behind.
    if (failures != 0) {
        std::cerr << "QUICK_PUBLIC_PROFILE_INVALID=YES (no JSON evidence)\n";
        return;
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema"),
                    QStringLiteral("hyremote-quick-public-profile/1"));
    document.insert(QStringLiteral("context"),
                    QStringLiteral("HOSTED_SYNTHETIC_NOT_DEVICE_OR_NATIVE_GAIN"));
    document.insert(QStringLiteral("route"), QStringLiteral("public_grabToImage"));
    document.insert(QStringLiteral("qt"), QString::fromLatin1(qVersion()));
    document.insert(QStringLiteral("platform"), QSysInfo::prettyProductName());
    document.insert(QStringLiteral("renderer"), backend);
    document.insert(QStringLiteral("probe_mode"),
                    qEnvironmentVariable("HYREMOTE_PUBLIC_PROFILE_PROBE_MODE",
                                         QStringLiteral("unspecified")));
    document.insert(QStringLiteral("requested_width"), requestedWidth);
    document.insert(QStringLiteral("requested_height"), requestedHeight);
    document.insert(QStringLiteral("actual_1080p"), fullResolution);
    document.insert(QStringLiteral("warmup_per_case"), 8);
    document.insert(QStringLiteral("measured_per_case"), measuredFrames);
    document.insert(QStringLiteral("whole_process_cpu_not_gpu_time"), true);
    document.insert(QStringLiteral("source_sha"),
                    qEnvironmentVariable("GITHUB_SHA", QStringLiteral("unstamped")));
    document.insert(QStringLiteral("public_a1_a2_cpu_spread_percent"), cpuSpread);
    document.insert(QStringLiteral("stable_reference_max_spread_percent"),
                    maxStableSpreadPercent);
    document.insert(QStringLiteral("public_reference_stability"),
                    stable ? QStringLiteral("STABLE")
                           : QStringLiteral("UNSTABLE_BASELINE"));
    document.insert(QStringLiteral("native_performance_qualification"),
                    QStringLiteral("NOT_MEASURED_DO_NOT_PROMOTE"));
    QJsonArray cases;
    cases.append(publicProfileToJson(a1));
    cases.append(publicProfileToJson(dynamic));
    cases.append(publicProfileToJson(a2));
    document.insert(QStringLiteral("cases"), cases);
    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Compact);
    std::cout << "QUICK_PUBLIC_PROFILE_JSON=" << bytes.constData() << '\n';
    const QString destination =
        qEnvironmentVariable("HYREMOTE_QUICK_PUBLIC_PROFILE_FILE");
    if (!destination.isEmpty()) {
        QFile file(destination);
        const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        CHECK(opened);
        if (opened) {
            CHECK(file.write(bytes) == bytes.size());
            file.write("\n", 1);
        }
    }
    // An unstable hosted baseline is useful evidence, not a failing
    // correctness test and NEVER permission to cherry-pick a better run.
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    testQuickProfileClockDoesNotMeasureSleepAsCpu();
    if (qEnvironmentVariableIsSet("HYREMOTE_QUICK_PUBLIC_PROFILE")) {
        runPublicQuickProfile();
        HyRemote::detail::resetFactories();
        return failures == 0 ? 0 : 1;
    }

    testAdaptiveQuickSceneGraphInvalidationFailClosed();
    testNativeReadbackBatchTimeoutIsOneFailure();
    testAdaptiveNativeFailureEventCountsOncePerGpuBatch();
    testAdaptiveQuickMalformedNativeFramesTriggerFallback();
    testAdaptiveQuickStopDrainsForwardedCoreCallback();
    testAdaptiveQuickFallbackSurfaceEpochAndFreshness();
    testAdaptiveQuickCapabilityStartRejectionAndBoundedAdmission();
    testQuickOpaqueRgbxCpuStorageIsZeroCopy();
    testAdoptReadbackStorageNoSecondFullFrame();
    testReadbackFrameStorageRowOrderAndPitch();
    testQuickRendererCapabilityRouting();
    testQuickFactoryAndOwnedFrame();
    testQuickQueuedRequestsCoalesceWithoutReplayingStalePixels();
    testQuickSingleFlightDispatchPolicy();
    testQuickDeferredRequestsKeepOrderAndFreshPixels();
    testQuickSingleFlightHiddenRetryQueuesAndDrains();
    testQuickQueuedRequestsDoNotCrossStopRestart();
    testCompletedQuickGrabsCancelTimeouts();
    testStagedQuickSourceCaptureFollowsPaintedScene();
    testDestroyedQuickTargetReportsTargetLost();
    testUnavailableQuickCaptureHasBoundedRetryAndRecovers();
    testQuickHiddenBatchCompletesEveryAcceptedRequest();
    testQuickInputIsQueuedToWindow();
    testQueuedQuickInputIsDroppedWhenSinkIsDestroyed();

    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Quick adapter checks failed\n";
    return failures == 0 ? 0 : 1;
}
