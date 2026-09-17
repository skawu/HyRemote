// ADR-0003 documents the defaults a fresh SessionConfig starts from ("Initial defaults",
// docs/adr/0003-threading-backpressure.md:93-101): max capture in flight 2, completed mailbox
// capacity 2, completed-frame policy DropOldest.
//
// Those defaults are deliberately tunable ("v0.1 defaults/candidates"; #18 may retune them for
// RK3588/EGLFS), so this case intentionally fixes the *current* contract rather than freezing it:
// tuning the defaults now means editing this test and the ADR together, which is precisely the point -
// without it a silent change to a documented default would be invisible to the suite.

#include "hyremote/core/session.hpp"

#include "test_support.hpp"

#include <chrono>

using namespace hyremote;

HYR_TEST(a_fresh_session_config_uses_the_documented_defaults)
{
    const SessionConfig config;

    // ADR-0003: completed mailbox capacity counts frames waiting for dispatch; the dispatch-worker-owned
    // frame is additional, so the documented default is 2 (session.hpp FrameQueueConfig).
    HYR_CHECK_EQ(config.frameQueue.capacity, std::size_t{2});
    HYR_CHECK(config.frameQueue.policy == BackpressurePolicy::DropOldest);

    HYR_CHECK_EQ(config.capture.maxInFlight, std::size_t{2});

    // No pacing by default: the scheduler stays independent of transport backlog (ADR-0003:91).
    HYR_CHECK(!config.capture.targetFramesPerSecond.has_value());
    HYR_CHECK_EQ(config.capture.rejectedRequestRetryDelay, std::chrono::milliseconds{50});

    // The documented defaults must also be a valid configuration.
    HYR_CHECK(validateSessionConfig(config).empty());

    // ProducerThrottle stays an explicit opt-in and is never silently substituted for DropOldest.
    SessionConfig throttled;
    throttled.frameQueue.policy = BackpressurePolicy::ProducerThrottle;
    HYR_CHECK(throttled.frameQueue.policy != BackpressurePolicy::DropOldest);
    HYR_CHECK(validateSessionConfig(throttled).empty());
}

HYR_TEST_MAIN()
