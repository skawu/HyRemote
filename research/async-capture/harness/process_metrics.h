// SPDX-License-Identifier: Apache-2.0
#pragma once

// Async capture spike throwaway process metrics helpers (issue #16).

#include <QString>
#include <QtGlobal>

namespace hyremote {
namespace asyncspike {

struct ProcessMetrics
{
    double cpuSeconds = 0.0;   // cumulative user + kernel CPU time
    quint64 rssBytes = 0;      // current resident set size
    quint64 peakRssBytes = 0;  // peak resident set size
};

ProcessMetrics sampleProcessMetrics();

// Empty when the platform is supported by this spike harness.
QString processMetricsUnsupportedReason();

}  // namespace asyncspike
}  // namespace hyremote
