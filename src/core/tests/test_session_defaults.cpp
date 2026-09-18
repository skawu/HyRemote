// SPDX-License-Identifier: Apache-2.0
// ADR-0003 documents the defaults a fresh SessionConfig starts from. These are deliberately tunable,
// but a change must update both the implementation and the documented contract rather than drift silently.

#include "hyremote/core/session.hpp"

#include "test_support.hpp"

#include <chrono>

using namespace hyremote;

HYR_TEST(a_fresh_session_config_uses_the_documented_defaults)
{
    const SessionConfig config;

    HYR_CHECK_EQ(config.frameQueue.capacity, std::size_t{2});
    HYR_CHECK(config.frameQueue.policy == BackpressurePolicy::DropOldest);
    HYR_CHECK_EQ(config.capture.maxInFlight, std::size_t{2});
    HYR_CHECK(!config.capture.targetFramesPerSecond.has_value());
    HYR_CHECK_EQ(config.capture.rejectedRequestRetryDelay, std::chrono::milliseconds{50});
    HYR_CHECK(validateSessionConfig(config).empty());

    SessionConfig throttled;
    throttled.frameQueue.policy = BackpressurePolicy::ProducerThrottle;
    HYR_CHECK(throttled.frameQueue.policy != BackpressurePolicy::DropOldest);
    HYR_CHECK(validateSessionConfig(throttled).empty());
}

HYR_TEST_MAIN()
