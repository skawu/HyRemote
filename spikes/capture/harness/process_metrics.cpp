#include "harness/process_metrics.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#elif defined(Q_OS_LINUX)
#  include <sys/resource.h>
#  include <unistd.h>
#  include <cstdio>
#endif

namespace hyremote {
namespace spike {

ProcessMetrics sampleProcessMetrics()
{
    ProcessMetrics metrics;

#ifdef Q_OS_WIN
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        const auto toSeconds = [](const FILETIME &value) {
            ULARGE_INTEGER ticks;
            ticks.LowPart = value.dwLowDateTime;
            ticks.HighPart = value.dwHighDateTime;
            return static_cast<double>(ticks.QuadPart) / 1.0e7;  // 100 ns ticks
        };
        metrics.cpuSeconds = toSeconds(kernelTime) + toSeconds(userTime);
    }

    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters))) {
        metrics.rssBytes = static_cast<quint64>(counters.WorkingSetSize);
        metrics.peakRssBytes = static_cast<quint64>(counters.PeakWorkingSetSize);
    }
#elif defined(Q_OS_LINUX)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        metrics.cpuSeconds = double(usage.ru_utime.tv_sec) + double(usage.ru_utime.tv_usec) / 1.0e6
                           + double(usage.ru_stime.tv_sec) + double(usage.ru_stime.tv_usec) / 1.0e6;
        metrics.peakRssBytes = quint64(usage.ru_maxrss) * 1024u;
    }

    if (FILE *statm = std::fopen("/proc/self/statm", "r")) {
        unsigned long totalPages = 0;
        unsigned long residentPages = 0;
        if (std::fscanf(statm, "%lu %lu", &totalPages, &residentPages) == 2) {
            const long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize > 0)
                metrics.rssBytes = quint64(residentPages) * quint64(pageSize);
        }
        std::fclose(statm);
    }
#endif

    return metrics;
}

QString processMetricsUnsupportedReason()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return QString();
#else
    return QStringLiteral("process CPU/RSS sampling is only implemented for Windows and Linux in this spike");
#endif
}

}  // namespace spike
}  // namespace hyremote
