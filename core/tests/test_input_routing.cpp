// C7 - input routing boundary.
//
// The frozen part is the boundary, not the keyboard schema: a transport runtime callback must be
// able to hand a normalized event to the InputSink without any synchronous GUI assumption.

#include <thread>

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(transport_input_reaches_the_input_sink)
{
    auto sink = std::make_shared<FakeInputSink>();

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    InputEvent event;
    event.kind = InputEventKind::PointerButton;
    event.x = 12.5F;
    event.y = 40.25F;
    event.button = PointerButton::Left;
    event.pressed = true;
    event.key = 0;
    event.text = "press";

    running.transport->deliverInput(event);

    HYR_CHECK_EQ(sink->count(), std::size_t{1});
    const std::vector<InputEvent> received = sink->events();
    HYR_CHECK(received.at(0).kind == InputEventKind::PointerButton);
    HYR_CHECK_EQ(received.at(0).x, 12.5F);
    HYR_CHECK_EQ(received.at(0).y, 40.25F);
    HYR_CHECK(received.at(0).button == PointerButton::Left);
    HYR_CHECK(received.at(0).pressed);
    HYR_CHECK_EQ(received.at(0).text, std::string{"press"});
    HYR_CHECK_EQ(running.session->stats().inputEventsPosted, std::uint64_t{1});
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(the_routing_boundary_adds_no_synchronous_wait)
{
    auto sink = std::make_shared<FakeInputSink>();

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    InputEvent event;
    event.kind = InputEventKind::Placeholder;

    running.transport->deliverInput(event);

    // The sink was handed the event during the transport call, on the transport thread: Core
    // neither marshals to a GUI thread nor waits for one.
    const std::set<std::thread::id> sinkThreads = sink->threads();
    HYR_CHECK_EQ(sinkThreads.size(), std::size_t{1});
    HYR_CHECK(*sinkThreads.begin() == std::this_thread::get_id());
    HYR_CHECK_EQ(running.session->stats().inputEventsPosted, std::uint64_t{1});
    HYR_CHECK_EQ(running.session->stats().inputEventsDropped, std::uint64_t{0});
}

HYR_TEST(events_are_routed_in_order)
{
    auto sink = std::make_shared<FakeInputSink>();

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    for (int i = 0; i < 3; ++i) {
        InputEvent event;
        event.kind = InputEventKind::Key;
        event.key = static_cast<std::uint32_t>(i);
        event.pressed = (i % 2) == 0;
        running.transport->deliverInput(event);
    }

    HYR_CHECK_EQ(sink->count(), std::size_t{3});
    const std::vector<InputEvent> received = sink->events();
    HYR_CHECK_EQ(received.at(0).key, std::uint32_t{0});
    HYR_CHECK_EQ(received.at(1).key, std::uint32_t{1});
    HYR_CHECK_EQ(received.at(2).key, std::uint32_t{2});
    HYR_CHECK(received.at(0).pressed);
    HYR_CHECK(!received.at(1).pressed);
}

HYR_TEST(input_without_a_sink_is_counted_and_not_fatal)
{
    RunningSession running;
    HYR_CHECK(running.start());

    InputEvent event;
    event.kind = InputEventKind::Placeholder;
    running.transport->deliverInput(event);

    HYR_CHECK_EQ(running.session->stats().inputEventsDropped, std::uint64_t{1});
    HYR_CHECK_EQ(running.session->stats().inputEventsPosted, std::uint64_t{0});
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(input_after_stop_is_ignored_safely)
{
    auto sink = std::make_shared<FakeInputSink>();

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    // A conforming transport runtime stops calling back once it is stopped.
    running.stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK(!running.transport->events().empty());
    running.transport->deliverInput(InputEvent{});
    HYR_CHECK_EQ(sink->count(), std::size_t{0});
    HYR_CHECK_EQ(running.session->stats().callbacksIgnoredAfterStop, std::uint64_t{0});

    // A transport that ignores the quiescence rule is also safe: Core's gate ignores the callback
    // and counts it instead of delivering it to a torn-down Session.
    InputEvent event;
    event.kind = InputEventKind::Placeholder;
    running.transport->forceDeliverInput(event);
    HYR_CHECK_EQ(sink->count(), std::size_t{0});
    HYR_CHECK_EQ(running.session->stats().callbacksIgnoredAfterStop, std::uint64_t{1});
}

HYR_TEST_MAIN()
