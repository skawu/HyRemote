#pragma once

// #609/#789: optional, developer-only Windows/Linux CPU attribution.
// Never enabled in delivered builds or public SDK. Only aggregate counters;
// no frame contents, input or configuration values enter the trace.
// Disabled builds compile to no-ops.
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#include <unistd.h>
#else
#error "HYREMOTE_ENABLE_CPU_PROBE currently supports Windows and Linux only"
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace HyRemote::detail::p1perf {

enum class Stage : std::size_t {
    WidgetBuffer,
    WidgetPaint,
    QuickAcceptedRequests, // count only: accepted CaptureSource requests
    QuickGuiBatches,       // count only: GUI queue drain batches
    QuickGrabSubmit,
    QuickGrabReadyWall,    // asynchronous elapsed wall time, NEVER CPU
    QuickConvert,
    QuickBuffer,
    QuickCopy,
    QuickGlRenderCallback,    // Qt scenegraph callback CPU+wall cost
    QuickGlGpuMapCopy,        // GL fence-ready map and raw GPU->CPU memcpy
    QuickGlCpuRepack,         // pool worker row normalization
    QuickGlExplicitRender,    // count: source-forced Qt update (not real frames)
    QuickGlRetirementRender,  // count: extra frame to retire a GL fence
    QuickGlGpuSubmit,         // count: GL PBO readback submissions
    QuickGlPboResize,         // count: allocate/reallocate PBO storage
    QuickGlPboReuse,          // count: reuse completed PBO storage
    QuickNativeFallback,     // count: native provider switched to public Qt
    QuickStaleDrop,          // count: epoch/age/reordered frame suppressed
    QuickD3d11StagingReuse, // count: staging resource reused on same device
    QuickD3d11StagingAllocation, // count: D3D staging texture created
    GenericCompose,
    WatermarkCompose,    // CPU copy+paint on RFB worker, one per real composition
    WatermarkReuse,      // count only: immutable-source cache hit
    RfbCompare,
    RfbEncode,
    RfbWrite,
    Count
};

inline constexpr std::array<const char *, static_cast<std::size_t>(Stage::Count)> kNames{
    "widget_buffer", "widget_paint", "quick_accepted_requests", "quick_gui_batches",
    "quick_grab_submit", "quick_grab_ready_wall", "quick_convert",
    "quick_buffer", "quick_copy",
    "quick_gl_render_callback", "quick_gl_gpu_map_copy", "quick_gl_cpu_repack",
    "quick_gl_explicit_render", "quick_gl_retirement_render", "quick_gl_gpu_submit",
    "quick_gl_pbo_resize", "quick_gl_pbo_reuse",
    "quick_native_fallback", "quick_stale_drop",
    "quick_d3d11_staging_reuse", "quick_d3d11_staging_allocation",
    "generic_compose", "watermark_compose",
    "watermark_reuse", "rfb_compare", "rfb_encode", "rfb_write"
};

// Unlike snapshots of a thread enumeration, these bookend the CURRENT thread's
// actual stage execution. A thread that exits between profiler snapshots cannot
// disappear from the accumulated stage total.
#if defined(_WIN32)
inline std::uint64_t filetimeNs(const FILETIME &time) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart * 100ULL;
}

inline bool threadCpuNs(std::uint64_t &out) noexcept
{
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
        return false;
    out = filetimeNs(kernel) + filetimeNs(user);
    return true;
}

inline bool processCpuNs(std::uint64_t &out) noexcept
{
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return false;
    out = filetimeNs(kernel) + filetimeNs(user);
    return true;
}

inline unsigned long processId() noexcept
{
    return static_cast<unsigned long>(GetCurrentProcessId());
}
#else
// Linux CLOCK_THREAD_CPUTIME_ID / CLOCK_PROCESS_CPUTIME_ID work on normal
// unprivileged desktop, ARM64 and Buildroot processes. They count CPU time,
// NOT wall time, and no /proc, CAP_PERFMON or perf_event permission is needed.
inline bool cpuClockNs(clockid_t id, std::uint64_t &out) noexcept
{
    timespec sample{};
    if (clock_gettime(id, &sample) != 0 || sample.tv_sec < 0
        || sample.tv_nsec < 0 || sample.tv_nsec >= 1000000000L)
        return false;
    out = static_cast<std::uint64_t>(sample.tv_sec) * 1000000000ULL
          + static_cast<std::uint64_t>(sample.tv_nsec);
    return true;
}

inline bool threadCpuNs(std::uint64_t &out) noexcept
{
    return cpuClockNs(CLOCK_THREAD_CPUTIME_ID, out);
}

inline bool processCpuNs(std::uint64_t &out) noexcept
{
    return cpuClockNs(CLOCK_PROCESS_CPUTIME_ID, out);
}

inline unsigned long processId() noexcept
{
    return static_cast<unsigned long>(getpid());
}
#endif

// RendererInterface::graphicsApi() is Qt's public API. Counting the actual
// grab backend proves which API the submitted public capture used, without
// importing Qt headers into this standalone (also rootless Linux/ARM) probe.
// Unknown and Other remain explicit rather than being guessed as OpenGL.
enum class QuickGraphicsBackend : std::size_t {
    Unknown, Software, OpenGL, Direct3D11, Direct3D12, Vulkan, Metal, Other, Count
};
inline constexpr std::array<const char *,
                            static_cast<std::size_t>(QuickGraphicsBackend::Count)> kBackendNames{
    "unknown", "software", "opengl", "d3d11", "d3d12", "vulkan", "metal", "other"
};

// Correlate the capture request, Core-assigned frame and each RFB viewer
// without modifying the RFB wire format or making Core depend on Qt.
// These records are local steady-clock timestamps, NOT Viewer presentation.
enum class FrameStep : std::uint8_t {
    QtActivityHint,
    CaptureRequest,
    QuickGrabSubmitted,
    GpuReadbackSubmitted,
    GpuReadbackReady,
    SourceCompleted,
    CompositeCompleted,
    CoreFrameQueued,
    RfbObserved,
    RfbCompared,
    RfbEncoded,
    RfbSocketQueued,
    Count
};

inline constexpr std::array<const char *, static_cast<std::size_t>(FrameStep::Count)>
    kFrameStepNames{"qt_activity_hint", "capture_request", "quick_grab_submitted",
                    "gpu_readback_submitted", "gpu_readback_ready",
                    "source_completed", "composite_completed",
                    "core_frame_queued", "rfb_observed", "rfb_compared",
                    "rfb_encoded", "rfb_socket_queued"};

inline std::uint64_t monotonicNs(std::chrono::steady_clock::time_point time) noexcept
{
    if (time == std::chrono::steady_clock::time_point{})
        return 0;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
    return ns > 0 ? static_cast<std::uint64_t>(ns) : 0;
}

struct FrameTraceRecord {
    std::uint64_t atNs = 0;
    std::uint64_t frameId = 0;
    std::uint64_t requestId = 0;
    std::uint64_t contentPtsNs = 0;
    std::uint64_t requestNs = 0;
    std::uint64_t completedNs = 0;
    std::uint64_t viewerId = 0;
    std::uint64_t updateId = 0; // per-viewer RFB framebuffer update sequence
    std::uint8_t ptsSource = 0; // Core PtsSource: completion=0, render=1, presentation=2
    FrameStep step = FrameStep::CaptureRequest;
};

struct Counters {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> cpuNs{0};
    std::atomic<std::uint64_t> wallNs{0};
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> cpuSamplesMissing{0};
};

class Recorder final
{
public:
    static Recorder &instance()
    {
        // Intentionally process-lifetime storage: the 1 Hz reporter may still
        // be running as the application exits; no DLL/global destructor races.
        static Recorder *const recorder = new Recorder;
        return *recorder;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool frameTraceEnabled() const noexcept { return m_traceEnabled; }

    // One constant-size lock-protected insertion, no heap allocation or
    // file I/O on Qt GUI/render, Core or RFB worker threads. When full,
    // count dropped TRACE records rather than blocking the product.
    void traceFrame(FrameStep step, std::uint64_t frameId,
                    std::uint64_t requestId, std::uint64_t ptsNs,
                    std::uint64_t requestNs, std::uint64_t completedNs,
                    std::uint64_t viewerId = 0,
                    std::uint8_t ptsSource = 0,
                    std::uint64_t updateId = 0) noexcept
    {
        if (!m_traceEnabled)
            return;
        const FrameTraceRecord record{
            monotonicNs(Clock::now()), frameId, requestId,
            ptsNs, requestNs, completedNs, viewerId, updateId, ptsSource, step};
        std::lock_guard<std::mutex> lock(m_traceMutex);
        const auto active = m_traceActive;
        if (m_traceCounts[active] == kTraceCapacity)
            ++m_traceDropped;
        else
            m_traceBuffers[active][m_traceCounts[active]++] = record;
    }

    void record(Stage stage, std::uint64_t wallNs,
                std::uint64_t cpuNs, bool cpuValid) noexcept
    {
        if (!m_enabled)
            return;
        Counters &counter = m_counters[static_cast<std::size_t>(stage)];
        counter.calls.fetch_add(1, std::memory_order_relaxed);
        counter.wallNs.fetch_add(wallNs, std::memory_order_relaxed);
        if (cpuValid)
            counter.cpuNs.fetch_add(cpuNs, std::memory_order_relaxed);
        else
            counter.cpuSamplesMissing.fetch_add(1, std::memory_order_relaxed);
    }

    void recordCount(Stage stage) noexcept
    {
        if (m_enabled)
            m_counters[static_cast<std::size_t>(stage)].calls.fetch_add(
                1, std::memory_order_relaxed);
    }

    void recordQuickBackend(QuickGraphicsBackend backend) noexcept
    {
        if (m_enabled && static_cast<std::size_t>(backend) < m_quickBackends.size())
            m_quickBackends[static_cast<std::size_t>(backend)].fetch_add(
                1, std::memory_order_relaxed);
    }

    void addBytes(Stage stage, std::uint64_t bytes) noexcept
    {
        if (m_enabled)
            m_counters[static_cast<std::size_t>(stage)].bytes.fetch_add(
                bytes, std::memory_order_relaxed);
    }

    void recordAsyncWall(Stage stage, std::uint64_t wallNs) noexcept
    {
        // The async Qt Quick grab-ready span is WALL LATENCY ONLY.
        // It must never be summed into attributed CPU time.
        record(stage, wallNs, 0, false);
    }

    void snapshot()
    {
        if (!m_enabled)
            return;
        std::lock_guard<std::mutex> lock(m_writeMutex);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - m_start).count();
        // Several AccessInstance instances may coexist. Allow only one sample
        // per ~second even when multiple object-owned reporters tick together.
        if (elapsed - m_lastReportMs < 750)
            return;
        m_lastReportMs = elapsed;
        writeSnapshotLocked(elapsed);
        flushFrameTraceLocked();
    }

private:
    Recorder()
    {
        const char *value = std::getenv("HYREMOTE_CPU_PROBE_FILE");
        if (value == nullptr || *value == '\0')
            return;
        m_path = value;
        m_enabled = true;
        // Keep legacy aggregate CPU JSONL untouched. Per-frame records
        // use a separate opt-in file so existing Windows/local-owner parsers
        // never encounter an unexpected schema-3 event line.
        const char *trace = std::getenv("HYREMOTE_FRAME_TRACE_FILE");
        if (trace && *trace) {
            m_tracePath = trace;
            m_traceEnabled = true;
        }
        m_start = Clock::now();
        (void)processCpuNs(m_startCpuNs);
    }

    void writeSnapshotLocked(std::int64_t elapsed)
    {
        std::FILE *file = std::fopen(m_path.c_str(), "ab");
        if (!file)
            return;
        std::uint64_t processCpu = 0;
        const bool valid = processCpuNs(processCpu);
        std::fprintf(file,
                     "{\"schema\":2,\"pid\":%lu,\"elapsed_ms\":%lld,"
                     "\"process_cpu_ns\":%llu,\"process_cpu_valid\":%s,\"stages\":{",
                     processId(),
                     static_cast<long long>(elapsed),
                     static_cast<unsigned long long>(valid && processCpu >= m_startCpuNs
                                                         ? processCpu - m_startCpuNs : 0),
                     valid ? "true" : "false");
        for (std::size_t i = 0; i < m_counters.size(); ++i) {
            const auto &c = m_counters[i];
            std::fprintf(file,
                         "%s\"%s\":{\"calls\":%llu,\"cpu_ns\":%llu,"
                         "\"wall_ns\":%llu,\"bytes\":%llu,\"cpu_samples_missing\":%llu}",
                         i ? "," : "", kNames[i],
                         static_cast<unsigned long long>(c.calls.load(std::memory_order_relaxed)),
                         static_cast<unsigned long long>(c.cpuNs.load(std::memory_order_relaxed)),
                         static_cast<unsigned long long>(c.wallNs.load(std::memory_order_relaxed)),
                         static_cast<unsigned long long>(c.bytes.load(std::memory_order_relaxed)),
                         static_cast<unsigned long long>(c.cpuSamplesMissing.load(
                             std::memory_order_relaxed)));
        }
        std::fputs("},\"quick_grab_backend_counts\":{", file);
        for (std::size_t i = 0; i < m_quickBackends.size(); ++i)
            std::fprintf(file, "%s\"%s\":%llu", i ? "," : "", kBackendNames[i],
                         static_cast<unsigned long long>(
                             m_quickBackends[i].load(std::memory_order_relaxed)));
        std::fputs("}}\n", file);
        std::fclose(file);
    }

    // Flip active/inactive buffers under a short lock, then write the
    // detached buffer on the existing 1Hz reporter thread. A bounded
    // overflow record makes missing diagnostic evidence explicit.
    void flushFrameTraceLocked()
    {
        if (!m_traceEnabled)
            return;
        std::size_t count = 0;
        std::uint64_t dropped = 0;
        std::size_t ready = 0;
        {
            std::lock_guard<std::mutex> lock(m_traceMutex);
            ready = m_traceActive;
            m_traceActive = 1 - ready;
            count = m_traceCounts[ready];
            m_traceCounts[ready] = 0;
            dropped = m_traceDropped;
            m_traceDropped = 0;
        }
        if (count == 0 && dropped == 0)
            return;
        std::FILE *file = std::fopen(m_tracePath.c_str(), "ab");
        if (!file)
            return;
        for (std::size_t i = 0; i < count; ++i) {
            const auto &r = m_traceBuffers[ready][i];
            std::fprintf(file,
                "{\"schema\":3,\"type\":\"frame_step\",\"pid\":%lu,"
                "\"step\":\"%s\",\"at_ns\":%llu,\"frame_id\":%llu,"
                "\"request_id\":%llu,\"content_pts_ns\":%llu,"
                "\"request_ns\":%llu,\"completion_ns\":%llu,"
                "\"viewer_id\":%llu,\"update_id\":%llu,\"pts_source\":%u}\n",
                processId(), kFrameStepNames[static_cast<std::size_t>(r.step)],
                static_cast<unsigned long long>(r.atNs),
                static_cast<unsigned long long>(r.frameId),
                static_cast<unsigned long long>(r.requestId),
                static_cast<unsigned long long>(r.contentPtsNs),
                static_cast<unsigned long long>(r.requestNs),
                static_cast<unsigned long long>(r.completedNs),
                static_cast<unsigned long long>(r.viewerId),
                static_cast<unsigned long long>(r.updateId),
                static_cast<unsigned int>(r.ptsSource));
        }
        if (dropped != 0)
            std::fprintf(file,
                "{\"schema\":3,\"type\":\"frame_trace_overflow\",\"pid\":%lu,"
                "\"dropped\":%llu}\n", processId(),
                static_cast<unsigned long long>(dropped));
        std::fclose(file);
    }

    static constexpr std::size_t kTraceCapacity = 2048;
    using Clock = std::chrono::steady_clock;
    bool m_enabled = false;
    bool m_traceEnabled = false;
    std::string m_path;
    std::string m_tracePath;
    Clock::time_point m_start{};
    std::uint64_t m_startCpuNs = 0;
    std::int64_t m_lastReportMs = -1000;
    std::array<Counters, static_cast<std::size_t>(Stage::Count)> m_counters{};
    std::array<std::atomic<std::uint64_t>,
               static_cast<std::size_t>(QuickGraphicsBackend::Count)> m_quickBackends{};
    std::mutex m_writeMutex;
    std::mutex m_traceMutex;
    std::array<FrameTraceRecord, kTraceCapacity> m_traceBuffers[2]{};
    std::size_t m_traceCounts[2]{};
    std::size_t m_traceActive = 0;
    std::uint64_t m_traceDropped = 0;
};

// One object-owned reporter per live AccessInstance. Its worker is JOINED
// before the runtime DLL can unload: no detached callback into unloaded code.
class Reporter final
{
public:
    Reporter()
    {
        if (!Recorder::instance().enabled())
            return;
        m_thread = std::thread([this] {
            std::unique_lock<std::mutex> lock(m_mutex);
            for (int second = 0; second < 300 && !m_stop; ++second) {
                if (m_changed.wait_for(lock, std::chrono::seconds(1),
                                       [this] { return m_stop; }))
                    break;
                lock.unlock();
                Recorder::instance().snapshot();
                lock.lock();
            }
        });
    }

    ~Reporter()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_changed.notify_all();
        if (m_thread.joinable())
            m_thread.join();
        Recorder::instance().snapshot();
    }

    Reporter(const Reporter &) = delete;
    Reporter &operator=(const Reporter &) = delete;

private:
    std::mutex m_mutex;
    std::condition_variable m_changed;
    bool m_stop = false;
    std::thread m_thread;
};

class Scope final
{
public:
    explicit Scope(Stage stage) noexcept
        : m_stage(stage), m_enabled(Recorder::instance().enabled())
    {
        if (m_enabled) {
            m_startWall = std::chrono::steady_clock::now();
            m_validCpu = threadCpuNs(m_startCpu);
        }
    }

    ~Scope()
    {
        if (!m_enabled)
            return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - m_startWall).count();
        std::uint64_t endCpu = 0;
        const bool valid = m_validCpu && threadCpuNs(endCpu) && endCpu >= m_startCpu;
        Recorder::instance().record(m_stage, static_cast<std::uint64_t>(elapsed),
                                    valid ? endCpu - m_startCpu : 0, valid);
    }

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

private:
    Stage m_stage;
    bool m_enabled = false;
    bool m_validCpu = false;
    std::uint64_t m_startCpu = 0;
    std::chrono::steady_clock::time_point m_startWall{};
};

template <typename Request>
inline void traceRequestFrame(FrameStep step, const Request &request) noexcept
{
    auto &recorder = Recorder::instance();
    if (!recorder.frameTraceEnabled())
        return;
    recorder.traceFrame(step, 0, request.id, 0,
                        monotonicNs(request.requestTime), 0);
}

template <typename Frame>
inline void traceAcceptedFrame(FrameStep step, const Frame &frame,
                               std::uint64_t viewerId = 0,
                               std::uint64_t updateId = 0) noexcept
{
    auto &recorder = Recorder::instance();
    if (!recorder.frameTraceEnabled())
        return;
    recorder.traceFrame(
        step, frame.id, frame.requestId.value_or(0),
        monotonicNs(frame.timing.pts),
        frame.timing.requestTime ? monotonicNs(*frame.timing.requestTime) : 0,
        frame.timing.completionTime ? monotonicNs(*frame.timing.completionTime) : 0,
        viewerId, static_cast<std::uint8_t>(frame.timing.ptsSource), updateId);
}

inline void addCount(Stage stage) noexcept
{
    Recorder::instance().recordCount(stage);
}

inline void addQuickBackend(QuickGraphicsBackend backend) noexcept
{
    Recorder::instance().recordQuickBackend(backend);
}

inline void addBytes(Stage stage, std::uint64_t bytes) noexcept
{
    Recorder::instance().addBytes(stage, bytes);
}

inline void addAsyncWall(Stage stage, std::chrono::steady_clock::duration duration) noexcept
{
    Recorder::instance().recordAsyncWall(
        stage, static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()));
}

} // namespace HyRemote::detail::p1perf

#define HYREMOTE_P1_PROBE_CAT2(a, b) a##b
#define HYREMOTE_P1_PROBE_CAT(a, b) HYREMOTE_P1_PROBE_CAT2(a, b)
#define HYREMOTE_P1_SCOPE(stage) \
    ::HyRemote::detail::p1perf::Scope HYREMOTE_P1_PROBE_CAT(_hyremoteP1Scope, __COUNTER__)( \
        ::HyRemote::detail::p1perf::Stage::stage)
#define HYREMOTE_P1_COUNT(stage) \
    ::HyRemote::detail::p1perf::addCount(::HyRemote::detail::p1perf::Stage::stage)
#define HYREMOTE_P1_QUICK_BACKEND(backend) \
    ::HyRemote::detail::p1perf::addQuickBackend(backend)
#define HYREMOTE_P1_BYTES(stage, bytes) \
    ::HyRemote::detail::p1perf::addBytes(::HyRemote::detail::p1perf::Stage::stage, \
                                        static_cast<std::uint64_t>(bytes))
#define HYREMOTE_P1_TRACE_HINT(stage) \
    ::HyRemote::detail::p1perf::Recorder::instance().traceFrame( \
        ::HyRemote::detail::p1perf::FrameStep::stage, 0, 0, 0, 0, 0)
#define HYREMOTE_P1_TRACE_REQUEST(stage, request) \
    ::HyRemote::detail::p1perf::traceRequestFrame( \
        ::HyRemote::detail::p1perf::FrameStep::stage, request)
#define HYREMOTE_P1_TRACE_FRAME(stage, frame, viewer) \
    ::HyRemote::detail::p1perf::traceAcceptedFrame( \
        ::HyRemote::detail::p1perf::FrameStep::stage, frame, viewer)
#define HYREMOTE_P1_TRACE_VIEWER_UPDATE(stage, frame, viewer, update) \
    ::HyRemote::detail::p1perf::traceAcceptedFrame( \
        ::HyRemote::detail::p1perf::FrameStep::stage, frame, viewer, update)
#else
#define HYREMOTE_P1_SCOPE(stage) ((void)0)
#define HYREMOTE_P1_COUNT(stage) ((void)0)
#define HYREMOTE_P1_QUICK_BACKEND(backend) ((void)0)
#define HYREMOTE_P1_BYTES(stage, bytes) ((void)0)
#define HYREMOTE_P1_TRACE_HINT(stage) ((void)0)
#define HYREMOTE_P1_TRACE_REQUEST(stage, request) ((void)0)
#define HYREMOTE_P1_TRACE_FRAME(stage, frame, viewer) ((void)0)
#define HYREMOTE_P1_TRACE_VIEWER_UPDATE(stage, frame, viewer, update) ((void)0)
#endif
