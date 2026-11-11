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
    if (argc != 2)
        return 2;
    const std::string tracePath = std::string(argv[1]) + ".frames.jsonl";
    if (setenv("HYREMOTE_CPU_PROBE_FILE", argv[1], 1) != 0
        || setenv("HYREMOTE_FRAME_TRACE_FILE", tracePath.c_str(), 1) != 0)
        return 2;

    using namespace HyRemote::detail::p1perf;
    std::uint64_t beforeThread = 0, afterThread = 0;
    std::uint64_t beforeProcess = 0, afterProcess = 0;
    if (!check(threadCpuNs(beforeThread), "thread clock unavailable")
        || !check(processCpuNs(beforeProcess), "process clock unavailable"))
        return 3;

    auto &recorder = Recorder::instance();
    if (!check(recorder.enabled() && recorder.frameTraceEnabled(),
               "opt-in recorder/frame trace did not enable"))
        return 4;
    // Exercise real bounded frame-step event serialization without Qt.
    // The main aggregate JSONL schema=2 remains the first log entry.
    recorder.traceFrame(FrameStep::CaptureRequest, 0, 17, 0, 100, 0);
    recorder.traceFrame(FrameStep::SourceCompleted, 0, 17, 900, 100, 850);
    recorder.traceFrame(FrameStep::CoreFrameQueued, 8, 17, 900, 100, 850);
    recorder.traceFrame(FrameStep::RfbSocketQueued, 8, 17, 900, 100, 850, 2);

    {
        HYREMOTE_P1_SCOPE(RfbCompare);
        burnCpuFor(std::chrono::milliseconds(60));
    }
    HYREMOTE_P1_BYTES(RfbCompare, 8192U);
    HYREMOTE_P1_COUNT(QuickAcceptedRequests);
    HYREMOTE_P1_COUNT(QuickAcceptedRequests);
    HYREMOTE_P1_COUNT(QuickGuiBatches);
    HYREMOTE_P1_COUNT(WatermarkReuse);
    addQuickBackend(QuickGraphicsBackend::Software);
    addQuickBackend(QuickGraphicsBackend::OpenGL);
    {
        HYREMOTE_P1_SCOPE(WatermarkCompose);
        burnCpuFor(std::chrono::milliseconds(12));
    }

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
    if (!check(json.find("\"schema\":2") != std::string::npos,
               "diagnostic schema 2 marker missing")
        || !check(json.find("\"process_cpu_valid\":true") != std::string::npos,
               "process CPU timestamp invalid")
        || !check(json.find("\"rfb_compare\":{\"calls\":1") != std::string::npos,
                  "stage scope not counted")
        || !check(json.find("\"quick_accepted_requests\":{\"calls\":2") != std::string::npos,
                  "two admitted Quick requests not counted")
        || !check(json.find("\"quick_gui_batches\":{\"calls\":1") != std::string::npos,
                  "one GUI queue batch not counted")
        || !check(json.find("\"watermark_compose\":{\"calls\":1") != std::string::npos,
                  "watermark CPU stage not counted")
        || !check(json.find("\"watermark_reuse\":{\"calls\":1") != std::string::npos,
                  "watermark cache hit not counted")
        || !check(json.find("\"software\":1") != std::string::npos
                  || json.find("\"opengl\":1") != std::string::npos,
                  "public Qt scenegraph backend counts absent")
        || !check(json.find("\"bytes\":8192") != std::string::npos,
                  "byte counter not recorded")
        || !check(json.find("\"cpu_samples_missing\":0") != std::string::npos,
                  "CPU stage samples missing"))
        return 7;

    // Legacy CPU parser still sees ONLY schema=2 rows. Frame records
    // must be in their own file and not appended to that old format.
    if (!check(!static_cast<bool>(std::getline(stream, json)),
               "frame steps unexpectedly contaminated aggregate CPU JSONL"))
        return 8;
    std::ifstream frameStream(tracePath);
    std::string trace;
    std::size_t traceEvents = 0;
    bool sawViewer = false;
    while (std::getline(frameStream, trace)) {
        if (trace.find("\"type\":\"frame_step\"") == std::string::npos)
            continue;
        ++traceEvents;
        if (trace.find("\"viewer_id\":2") != std::string::npos
            && trace.find("\"frame_id\":8") != std::string::npos)
            sawViewer = true;
    }
    if (!check(traceEvents == 4 && sawViewer,
               "bounded trace did not flush four correlated frame events"))
        return 8;

    std::cout << "PASS Linux unprivileged CPU clocks, stage attribution, "
                 "bounded per-frame host chronology, viewer identity, JSONL\n";
    return 0;
#endif
}
