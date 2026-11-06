// #789: rootless Linux/ARM CPU clock and diagnostic JSON smoke test.
// Intentionally standalone, no Qt/SDK linkage or private customer source.
#include "detail/perf_probe.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void burnCpuFor(std::chrono::milliseconds duration)
{
    volatile std::uint64_t checksum = 1;
    const auto until = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < until)
        checksum = checksum * 1664525ULL + 1013904223ULL;
    (void)checksum;
}

bool check(bool value, const char *message)
{
    if (!value)
        std::cerr << "cpu probe smoke FAILED: " << message << '\n';
    return value;
}

} // namespace

int main(int argc, char **argv)
{
#if !defined(__linux__) || !defined(HYREMOTE_ENABLE_CPU_PROBE) || !HYREMOTE_ENABLE_CPU_PROBE
    std::cerr << "requires opt-in Linux probe compilation\n";
    return 1;
#else
    if (argc != 2 || setenv("HYREMOTE_CPU_PROBE_FILE", argv[1], 1) != 0)
        return 2;

    using namespace HyRemote::detail::p1perf;
    std::uint64_t beforeThread = 0, afterThread = 0;
    std::uint64_t beforeProcess = 0, afterProcess = 0;
    if (!check(threadCpuNs(beforeThread), "thread clock unavailable")
        || !check(processCpuNs(beforeProcess), "process clock unavailable"))
        return 3;

    auto &recorder = Recorder::instance();
    if (!check(recorder.enabled(), "opt-in recorder did not enable"))
        return 4;

    {
        HYREMOTE_P1_SCOPE(RfbCompare);
        burnCpuFor(std::chrono::milliseconds(60));
    }
    HYREMOTE_P1_BYTES(RfbCompare, 8192U);

    if (!check(threadCpuNs(afterThread), "thread clock failed after work")
        || !check(processCpuNs(afterProcess), "process clock failed after work")
        || !check(afterThread > beforeThread + 1000000ULL, "thread time did not advance")
        || !check(afterProcess > beforeProcess + 1000000ULL, "process time did not advance"))
        return 5;

    recorder.snapshot();
    std::ifstream stream(argv[1]);
    std::string json;
    if (!check(static_cast<bool>(std::getline(stream, json)), "missing JSON snapshot"))
        return 6;
    if (!check(json.find("\"process_cpu_valid\":true") != std::string::npos,
               "process CPU timestamp invalid")
        || !check(json.find("\"rfb_compare\":{\"calls\":1") != std::string::npos,
                  "stage scope not counted")
        || !check(json.find("\"bytes\":8192") != std::string::npos,
                  "byte counter not recorded")
        || !check(json.find("\"cpu_samples_missing\":0") != std::string::npos,
                  "CPU stage samples missing"))
        return 7;

    std::cout << "PASS Linux unprivileged process/thread clocks, "
                 "actual RfbCompare stage CPU and privacy-safe JSON\n";
    return 0;
#endif
}
