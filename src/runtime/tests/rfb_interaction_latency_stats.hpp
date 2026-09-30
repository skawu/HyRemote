#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace HyRemote::test {

struct InteractionLatencySummary
{
    std::size_t sampleCount = 0;
    std::int64_t minUs = 0;
    std::int64_t p50Us = 0;
    std::int64_t p95Us = 0;
    std::int64_t maxUs = 0;
};

inline std::int64_t nearestRankPercentile(const std::vector<std::int64_t> &sortedUs,
                                          std::size_t percentile)
{
    if (sortedUs.empty())
        return 0;

    const std::size_t bounded = std::min<std::size_t>(100, std::max<std::size_t>(1, percentile));
    const std::size_t rank = (bounded * sortedUs.size() + 99) / 100;
    return sortedUs[std::min(sortedUs.size() - 1, rank - 1)];
}

inline InteractionLatencySummary summarizeInteractionLatencies(
    const std::vector<std::chrono::microseconds> &samples)
{
    InteractionLatencySummary summary;
    summary.sampleCount = samples.size();
    if (samples.empty())
        return summary;

    std::vector<std::int64_t> sortedUs;
    sortedUs.reserve(samples.size());
    for (const auto sample : samples)
        sortedUs.push_back(sample.count());
    std::sort(sortedUs.begin(), sortedUs.end());

    summary.minUs = sortedUs.front();
    summary.p50Us = nearestRankPercentile(sortedUs, 50);
    summary.p95Us = nearestRankPercentile(sortedUs, 95);
    summary.maxUs = sortedUs.back();
    return summary;
}

inline void printInteractionLatencySummary(std::ostream &stream,
                                           const char *encoding,
                                           const InteractionLatencySummary &summary)
{
    stream << "HYREMOTE_INTERACTION_LATENCY"
           << " slice=rfb_transport_round_trip"
           << " encoding=" << encoding
           << " samples=" << summary.sampleCount
           << " min_us=" << summary.minUs
           << " p50_us=" << summary.p50Us
           << " p95_us=" << summary.p95Us
           << " max_us=" << summary.maxUs << '\n';
}

}  // namespace HyRemote::test
