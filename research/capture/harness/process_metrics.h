// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway process metrics helpers (see spike_types.h).

#include <QString>
#include <QtGlobal>

namespace hyremote {
namespace spike {

struct ProcessMetrics
{
    double cpuSeconds = 0.0;   // cumulative user + kernel CPU time
    quint64 rssBytes = 0;      // current resident set size
    quint64 peakRssBytes = 0;  // peak resident set size
};

ProcessMetrics sampleProcessMetrics();

// Empty when the platform is supported by this spike harness.
QString processMetricsUnsupportedReason();

}  // namespace spike
}  // namespace hyremote
