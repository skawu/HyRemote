#pragma once

// #609: optional, developer-only CPU attribution. Never enabled in delivered
// builds or public SDK. Only aggregate counters; no frame contents, input or
// configuration values enter the trace. Disabled builds compile to no-ops.
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE

#ifndef _WIN32
#error "HYREMOTE_ENABLE_CPU_PROBE currently supports Windows only"
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
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
    QuickGrabSubmit,
    QuickGrabReadyWall,
    QuickConvert,
    QuickBuffer,
    QuickCopy,
    GenericCompose,
    RfbCompare,
    RfbEncode,
    RfbWrite,
    Count
};

inline constexpr std::array<const char *, static_cast<std::size_t>(Stage::Count)> kNames{
    "widget_buffer", "widget_paint", "quick_grab_submit", "quick_grab_ready_wall",
    "quick_convert", "quick_buffer", "quick_copy", "generic_compose",
    "rfb_compare", "rfb_encode", "rfb_write"
};

inline std::uint64_t filetimeNs(const FILETIME &time) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart * 100ULL;
}

// Unlike snapshots of a thread enumeration, these bookend the CURRENT thread's
// actual stage execution. A thread that exits between profiler snapshots cannot
// disappear from the accumulated stage total.
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

private:
    Recorder()
    {
        const char *value = std::getenv("HYREMOTE_CPU_PROBE_FILE");
        if (value == nullptr || *value == '\0')
            return;
        m_path = value;
        m_enabled = true;
        m_start = Clock::now();
        (void)processCpuNs(m_startCpuNs);
        // Reporter is CPU-probe-only and sleeps outside any measured stage.
        // It also samples B1 when no Viewer/capture work is occurring.
        std::thread([this] {
            for (int second = 1; second <= 300; ++second) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                snapshot();
            }
        }).detach();
    }

    void snapshot()
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        std::FILE *file = std::fopen(m_path.c_str(), "ab");
        if (!file)
            return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - m_start).count();
        std::uint64_t processCpu = 0;
        const bool valid = processCpuNs(processCpu);
        std::fprintf(file,
                     "{\"schema\":1,\"pid\":%lu,\"elapsed_ms\":%lld,"
                     "\"process_cpu_ns\":%llu,\"process_cpu_valid\":%s,\"stages\":{",
                     static_cast<unsigned long>(GetCurrentProcessId()),
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
        std::fputs("}}\n", file);
        std::fclose(file);
    }

    using Clock = std::chrono::steady_clock;
    bool m_enabled = false;
    std::string m_path;
    Clock::time_point m_start{};
    std::uint64_t m_startCpuNs = 0;
    std::array<Counters, static_cast<std::size_t>(Stage::Count)> m_counters{};
    std::mutex m_writeMutex;
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
#define HYREMOTE_P1_BYTES(stage, bytes) \
    ::HyRemote::detail::p1perf::addBytes(::HyRemote::detail::p1perf::Stage::stage, \
                                        static_cast<std::uint64_t>(bytes))
#else
#define HYREMOTE_P1_SCOPE(stage) ((void)0)
#define HYREMOTE_P1_BYTES(stage, bytes) ((void)0)
#endif
