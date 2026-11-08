// Developer-only real Qt Quick overlap workload. Do not register as a product CTest.
// Run identical source on both sides of #919, with the same opt-in probe patch.
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QSysInfo>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#endif

#include "detail/component_factories.hpp"
#include "hyremote/core/frame.hpp"

namespace {
using Clock = std::chrono::steady_clock;
using Ms = std::chrono::duration<double, std::milli>;

bool processCpuNs(std::uint64_t &out)
{
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return false;
    ULARGE_INTEGER a{}, b{};
    a.LowPart = kernel.dwLowDateTime; a.HighPart = kernel.dwHighDateTime;
    b.LowPart = user.dwLowDateTime; b.HighPart = user.dwHighDateTime;
    out = (a.QuadPart + b.QuadPart) * 100ULL;
    return true;
#elif defined(__linux__)
    timespec v{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &v) != 0 || v.tv_sec < 0
        || v.tv_nsec < 0 || v.tv_nsec >= 1000000000L)
        return false;
    out = static_cast<std::uint64_t>(v.tv_sec) * 1000000000ULL
        + static_cast<std::uint64_t>(v.tv_nsec);
    return true;
#else
    (void)out;
    return false;
#endif
}

bool pumpUntil(const std::function<bool()> &predicate, int limit = 1200)
{
    for (int i = 0; i < limit; ++i) {
        if (predicate())
            return true;
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t pos = static_cast<std::size_t>(std::ceil(fraction * values.size()));
    return values[std::min(values.size() - 1, pos > 0 ? pos - 1 : 0)];
}

class StageItem final : public QQuickPaintedItem
{
public:
    explicit StageItem(QQuickItem *parent) : QQuickPaintedItem(parent) { setAntialiasing(false); }
    void change(int stage) { m_stage.store(stage, std::memory_order_release); update(); }
    int painted() const { return m_painted.load(std::memory_order_acquire); }
    int paints() const { return m_paints.load(std::memory_order_acquire); }
    void paint(QPainter *p) override
    {
        const int s = m_stage.load(std::memory_order_acquire);
        const QRectF left(0, 0, width() / 2, height());
        const QRectF right(width() / 2, 0, width() / 2, height());
        p->fillRect(left, s >= 1 ? QColor(225,30,40) : QColor(20,60,160));
        p->fillRect(right, s >= 2 ? QColor(225,30,40) : QColor(20,60,160));
        m_painted.store(s, std::memory_order_release);
        m_paints.fetch_add(1, std::memory_order_relaxed);
    }
private:
    std::atomic<int> m_stage{0}, m_painted{-1}, m_paints{0};
};

bool pixelsMatch(const hyremote::RemoteFrame &f, int stage)
{
    if (!f.storage || f.geometry.size.width != 1920 || f.geometry.size.height != 1080
        || f.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888)
        return false;
    const auto plane = f.storage->mapRead(0);
    if (!plane || !plane->data || plane->stride < 1920U * 4U
        || plane->bytes / plane->stride < 1080U)
        return false;
    auto rgb = [&](std::size_t x) {
        const auto *p = reinterpret_cast<const unsigned char *>(plane->data)
            + 540U * plane->stride + x * 4U;
        return (static_cast<std::uint32_t>(p[0]) << 16U)
             | (static_cast<std::uint32_t>(p[1]) << 8U) | p[2];
    };
    return rgb(480U) == (stage >= 1 ? 0xe11e28U : 0x143ca0U)
        && rgb(1440U) == (stage >= 2 ? 0xe11e28U : 0x143ca0U);
}

struct Receipt {
    std::uint64_t id;
    Clock::time_point received;
    hyremote::RemoteFrame frame;
};

int run()
{
    const QString mode = qEnvironmentVariable("HYREMOTE_QUICK_OVERLAP_MODE");
    const bool transition = mode == QStringLiteral("transition");
    if (mode != QStringLiteral("static") && !transition) {
        std::cerr << "INVALID: HYREMOTE_QUICK_OVERLAP_MODE must be static or transition\n";
        return 2;
    }
    if (qEnvironmentVariable("GITHUB_SHA").size() != 40) {
        std::cerr << "INVALID: unstamped source SHA\n";
        return 2;
    }
    constexpr int kWarmup = 8, kMeasured = 40;
    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.resize(1920, 1080);
    StageItem item(window.contentItem());
    item.setWidth(1920); item.setHeight(1080);
    std::atomic<int> observedStage{-1};
    std::atomic<int> rendered{0};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        observedStage.store(item.painted(), std::memory_order_release);
        rendered.fetch_add(1, std::memory_order_relaxed);
    }, Qt::DirectConnection);
    window.show();
    if (!pumpUntil([&] { return window.isVisible() && observedStage.load() == 0; })) {
        std::cerr << "INVALID: no initial Qt render\n";
        return 3;
    }

    const auto *ri = window.rendererInterface();
    const auto api = ri ? ri->graphicsApi() : QSGRendererInterface::Unknown;
    const QString backend = api == QSGRendererInterface::Software
        ? QStringLiteral("software") : api == QSGRendererInterface::OpenGL
        ? QStringLiteral("opengl") : QStringLiteral("other");
    if (backend != qEnvironmentVariable("HYREMOTE_QUICK_OVERLAP_BACKEND")) {
        std::cerr << "INVALID: scenegraph backend mismatch\n";
        return 3;
    }

    auto target = HyRemote::detail::createTargetComponents(&window, false);
    if (!target.capture) {
        std::cerr << "INVALID: real Quick CaptureSource unavailable\n";
        return 3;
    }
    std::vector<Receipt> received;
    int events = 0;
    if (!target.capture->start(
        [&](hyremote::RemoteFrame f) {
            received.push_back({f.requestId.value_or(0U), Clock::now(), std::move(f)});
        },
        [&](const hyremote::CaptureEvent &) { ++events; })) {
        std::cerr << "INVALID: failed to start capture\n";
        return 3;
    }

    std::vector<double> latencies, secondLatencies, transitionToVisible;
    int overlapPairs = 0, orderingErrors = 0, staleSecond = 0;
    int validPairs = 0, validFrames = 0;
    std::uint64_t cpuStart = 0, cpuEnd = 0;
    Clock::time_point wallStart{};
    const int paintBefore = item.paints();
    const int renderBefore = rendered.load();
    bool valid = true;

    for (int index = -kWarmup; index < kMeasured; ++index) {
        if (transition) {
            item.change(0);
            if (!pumpUntil([&] { return observedStage.load() == 0; })) {
                valid = false; break;
            }
        }
        if (index == 0) {
            valid = processCpuNs(cpuStart);
            wallStart = Clock::now();
            if (!valid) break;
        }
        received.clear();
        const auto id = static_cast<std::uint64_t>(100000U + (index + kWarmup) * 2U);
        Clock::time_point firstIssued = Clock::now(), secondIssued{};
        Clock::time_point changedAt{};
        bool secondAccepted = false, callbackRan = false, pressureOverlap = false;
        if (!target.capture->requestFrame({id, hyremote::Clock::now()})) {
            valid = false; break;
        }
        // FIFO GUI turn: first dispatch was queued above; this separate GUI
        // turn requests frame 2 while frame 1 has not yet published.
        if (!QMetaObject::invokeMethod(QCoreApplication::instance(), [&] {
            callbackRan = true;
            pressureOverlap = received.empty();
            if (transition) { changedAt = Clock::now(); item.change(2); }
            secondIssued = Clock::now();
            secondAccepted = target.capture->requestFrame(
                {id + 1U, hyremote::Clock::now()});
        }, Qt::QueuedConnection)) {
            valid = false; break;
        }
        if (!pumpUntil([&] { return callbackRan && received.size() >= 2U; })
            || !secondAccepted || received.size() != 2U) {
            valid = false; break;
        }
        if (index < 0) continue;
        ++validPairs;
        validFrames += 2;
        overlapPairs += pressureOverlap ? 1 : 0;
        if (received[0].id != id || received[1].id != id + 1U)
            ++orderingErrors;
        for (const auto &f : received) {
            const Clock::time_point issued = f.id == id ? firstIssued : secondIssued;
            latencies.push_back(Ms(f.received - issued).count());
        }
        for (const auto &f : received) {
            if (f.id != id + 1U) continue;
            secondLatencies.push_back(Ms(f.received - secondIssued).count());
            if (!pixelsMatch(f.frame, transition ? 2 : 0)) ++staleSecond;
            if (transition) transitionToVisible.push_back(Ms(f.received - changedAt).count());
        }
        // The first asynchronous grab may legitimately observe the later
        // scene; second must always be fresh. Never assert first image == A.
    }

    if (valid && !processCpuNs(cpuEnd)) valid = false;
    const double wallMs = Ms(Clock::now() - wallStart).count();
    target.capture->stop();
    if (!valid || events != 0 || validPairs != kMeasured || validFrames != 2*kMeasured
        || cpuEnd < cpuStart || wallMs <= 0.0 || secondLatencies.size() != kMeasured) {
        std::cerr << "QUICK_OVERLAP_INVALID: missing frame, CPU clock or event\n";
        return 4;
    }
    const double cpuMs = static_cast<double>(cpuEnd - cpuStart) / 1000000.0;
    QJsonObject o;
    o.insert("schema", "hyremote-quick-overlap-profile/1");
    o.insert("context", "HOSTED_QT_REAL_CAPTURE_NOT_GPU_TIMER_OR_RK3588");
    o.insert("source_sha", qEnvironmentVariable("GITHUB_SHA"));
    o.insert("qt", QString::fromLatin1(qVersion()));
    o.insert("platform", QSysInfo::prettyProductName());
    o.insert("backend", backend);
    o.insert("mode", mode);
    o.insert("width", 1920); o.insert("height", 1080);
    o.insert("pairs", kMeasured); o.insert("warmup_pairs", kWarmup);
    o.insert("valid_frames", validFrames);
    o.insert("second_admitted_before_first_published", overlapPairs);
    o.insert("ordering_errors", orderingErrors);
    o.insert("stale_second_frames", staleSecond);
    o.insert("paint_calls", item.paints() - paintBefore);
    o.insert("after_rendering_calls", rendered.load() - renderBefore);
    o.insert("process_cpu_ms", cpuMs); o.insert("wall_ms", wallMs);
    o.insert("cpu_ms_per_frame", cpuMs / validFrames);
    o.insert("published_fps", validFrames * 1000.0 / wallMs);
    o.insert("request_to_publish_p50_ms", percentile(latencies, 0.50));
    o.insert("request_to_publish_p95_ms", percentile(latencies, 0.95));
    o.insert("second_request_p95_ms", percentile(secondLatencies, 0.95));
    o.insert("change_to_visible_p95_ms",
             transition ? percentile(transitionToVisible, 0.95) : 0.0);
    o.insert("render_source_frame_age", "UNKNOWN_COMPLETION_PTS_ONLY");
    o.insert("process_cpu_is_not_gpu_readback_time", true);
    const QByteArray bytes = QJsonDocument(o).toJson(QJsonDocument::Compact);
    std::cout << "QUICK_OVERLAP_PROFILE_JSON=" << bytes.constData() << '\n';
    const QString dest = qEnvironmentVariable("HYREMOTE_QUICK_OVERLAP_FILE");
    if (dest.isEmpty()) return 5;
    QFile file(dest);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(bytes) != bytes.size() || file.write("\n", 1) != 1) {
        std::cerr << "INVALID: could not write JSON\n";
        return 5;
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const int code = run();
    HyRemote::detail::resetFactories();
    return code;
}
