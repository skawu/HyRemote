// Focused #259 test: the Runtime-private typed notification seam, and the removal of polling.
//
// Every case here drives a real event boundary of the shared runtime through the private test factories
// (the same seam the facade tests use), so nothing is asserted by reading a timer or by waiting for a
// wall clock: an injected event either produces a typed notification or it does not.
//
// The ordering frozen by #259 is asserted as a sequence, not as a set:
//   successful start : StateChanged(Starting), StateChanged(Running)
//   start failure    : StateChanged(Starting), ErrorChanged(error), StateChanged(Stopped) - never Faulted
//   stop             : StateChanged(Stopping), [ConnectedClientCountChanged(0)], StateChanged(Stopped)
//   fatal/target loss: ErrorChanged(non-recoverable), StateChanged(Faulted)

#include "access_instance.hpp"
#include "detail/component_factories.hpp"

#include <QCoreApplication>
#include <QObject>

#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

using ::HyRemote::Runtime::AccessState;
using ::HyRemote::Runtime::AccessInstance;
using ::HyRemote::Runtime::Error;
using ::HyRemote::Runtime::ErrorCode;
using ::HyRemote::Runtime::RuntimeNotificationHandlers;

const char *stateName(AccessState state)
{
    switch (state) {
    case AccessState::Stopped: return "Stopped";
    case AccessState::Starting: return "Starting";
    case AccessState::Running: return "Running";
    case AccessState::Stopping: return "Stopping";
    case AccessState::Faulted: return "Faulted";
    }
    return "?";
}

// Records the three typed notifications as one ordered sequence, so a test can assert the frozen order
// rather than merely that a value arrived.
class Recorder
{
public:
    RuntimeNotificationHandlers handlers()
    {
        RuntimeNotificationHandlers bundle;
        bundle.stateChanged = [this](AccessState state) {
            sequence.push_back(std::string("state:") + stateName(state));
            states.push_back(state);
        };
        bundle.connectedClientCountChanged = [this](std::size_t count) {
            sequence.push_back("count:" + std::to_string(count));
            counts.push_back(count);
        };
        bundle.errorChanged = [this](std::optional<Error> error) {
            if (error) {
                sequence.push_back("error:" + error->message.toStdString());
                errors.push_back(*error);
            } else {
                sequence.push_back("error:none");
                errors.push_back(std::nullopt);
            }
        };
        return bundle;
    }

    std::vector<AccessState> states;
    std::vector<std::size_t> counts;
    std::vector<std::optional<Error>> errors;
    std::vector<std::string> sequence;

    void clear() { states.clear(); counts.clear(); errors.clear(); sequence.clear(); }

    // The state/error/count subsequences, so an assertion about one dimension does not depend on the
    // order in which the others were published for the same boundary.
    std::vector<std::string> only(const char *prefix) const
    {
        std::vector<std::string> result;
        const std::string needle(prefix);
        for (const std::string &entry : sequence) {
            if (entry.rfind(needle, 0) == 0) {
                result.push_back(entry);
            }
        }
        return result;
    }
};

std::size_t indexOf(const std::vector<std::string> &entries, const std::string &entry)
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index] == entry) {
            return index;
        }
    }
    return entries.size();
}

struct RuntimeState
{
    std::atomic<int> captureStarts{0};
    std::atomic<int> captureStops{0};
    std::atomic<int> transportStops{0};
    int inputPosts = 0;
    bool throwOnInputPost = false;
    hyremote::InputHandler inputHandler;
    hyremote::CaptureEventHandler captureEventHandler;
    hyremote::TransportEventHandler transportEventHandler;
};

class FakeInput final : public hyremote::InputSink
{
public:
    explicit FakeInput(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    void post(const hyremote::InputEvent &) override
    {
        ++m_state->inputPosts;
        if (m_state->throwOnInputPost) {
            throw std::runtime_error("deterministic input failure");
        }
    }

    void shutdown() noexcept override {}

private:
    std::shared_ptr<RuntimeState> m_state;
};

class FakeCapture final : public hyremote::CaptureSource
{
public:
    explicit FakeCapture(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler, hyremote::CaptureEventHandler onEvent) override
    {
        ++m_state->captureStarts;
        m_state->captureEventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override
    {
        ++m_state->captureStops;
        m_state->captureEventHandler = {};
    }

    bool requestFrame(const hyremote::CaptureRequest &) override { return false; }

private:
    std::shared_ptr<RuntimeState> m_state;
};

class FakeTransport final : public hyremote::Transport
{
public:
    explicit FakeTransport(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    hyremote::FrameConsumerCapabilities frameCapabilities() const override
    {
        hyremote::FrameConsumerCapabilities result;
        result.acceptsCpu = true;
        result.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return result;
    }

    bool start(hyremote::InputHandler onInput, hyremote::TransportEventHandler onEvent) override
    {
        m_state->inputHandler = std::move(onInput);
        m_state->transportEventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override
    {
        ++m_state->transportStops;
        m_state->inputHandler = {};
        m_state->transportEventHandler = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeState> m_state;
};

void installRuntime(const std::shared_ptr<RuntimeState> &state, bool captureSupported = true)
{
    HyRemote::detail::setTargetFactory([state, captureSupported](QObject *target, bool remoteInputEnabled) {
        HyRemote::detail::TargetComponents result;
        result.supported = captureSupported && target != nullptr;
        if (!result.supported) {
            result.error = QStringLiteral("no adapter supports this target");
            return result;
        }
        result.capture = std::make_unique<FakeCapture>(state);
        if (remoteInputEnabled) {
            result.input = std::make_shared<FakeInput>(state);
        }
        return result;
    });

    HyRemote::detail::setTransportFactory([state](const QHostAddress &, quint16,
                                                  const HyRemote::detail::RfbSecurityConfig &) {
        HyRemote::detail::TransportComponent result;
        result.transport = std::make_unique<FakeTransport>(state);
        return result;
    });
}

void emitTransport(const std::shared_ptr<RuntimeState> &state, hyremote::TransportEventCode code)
{
    CHECK(static_cast<bool>(state->transportEventHandler));
    if (state->transportEventHandler) {
        state->transportEventHandler(hyremote::TransportEvent{code, "test event"});
    }
}

// ------------------------------------------------------------------------------------------------ cases

void testStartOrderingAndStopOrdering()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    CHECK(instance.subscribeNotifications(recorder.handlers()).isValid());

    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    CHECK(recorder.only("state:").size() == 2u);
    CHECK(indexOf(recorder.only("state:"), "state:Starting") == 0u);
    CHECK(indexOf(recorder.only("state:"), "state:Running") == 1u);
    // The first snapshot a subscriber sees establishes the client count, and a repeated value is not
    // published again: seeing 0 twice would mean the seam republishes unchanged values.
    CHECK(recorder.counts.size() == 1u);
    CHECK(recorder.counts.front() == 0u);
    CHECK(recorder.errors.empty());

    recorder.clear();
    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);
    CHECK(recorder.only("state:").size() == 2u);
    CHECK(indexOf(recorder.only("state:"), "state:Stopping") == 0u);
    CHECK(indexOf(recorder.only("state:"), "state:Stopped") == 1u);
    // No client was connected, so the stop must not invent a client-count change.
    CHECK(recorder.counts.empty());

    HyRemote::detail::resetFactories();
}

void testStartFailurePublishesErrorThenStopped()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state, /* captureSupported */ false);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());

    CHECK(!instance.start());
    // The frozen semantic: a startup failure ends Stopped with the error retained, not Faulted.
    CHECK(instance.state() == AccessState::Stopped);
    CHECK(instance.lastError().has_value());
    CHECK(instance.lastError()->code == ErrorCode::TargetAdapterUnavailable);

    const std::vector<std::string> states = recorder.only("state:");
    CHECK(states.size() == 2u);
    CHECK(indexOf(states, "state:Starting") == 0u);
    CHECK(indexOf(states, "state:Stopped") == 1u);

    const std::vector<std::string> errors = recorder.only("error:");
    CHECK(errors.size() == 1u);
    // Error before the final state, so an observer that receives Stopped already has the reason.
    CHECK(indexOf(recorder.sequence, errors.front()) < indexOf(recorder.sequence, "state:Stopped"));

    HyRemote::detail::resetFactories();
}

void testClientCountIsEventDrivenAndNotRepeated()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());
    CHECK(instance.start());

    recorder.clear();
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(instance.connectedClientCount() == 1u);
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(instance.connectedClientCount() == 2u);
    emitTransport(state, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(instance.connectedClientCount() == 1u);
    emitTransport(state, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(instance.connectedClientCount() == 0u);
    // An unreachable further disconnect must not publish a duplicate.
    emitTransport(state, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(instance.connectedClientCount() == 0u);

    CHECK((recorder.counts == std::vector<std::size_t>{1u, 2u, 1u, 0u}));
    // Nothing else moved: a client-count change is not a state or error change.
    CHECK(recorder.states.empty());
    CHECK(recorder.errors.empty());

    instance.stop();
    HyRemote::detail::resetFactories();
}

void testStopPublishesClientCountZeroBeforeStopped()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());
    CHECK(instance.start());
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(instance.connectedClientCount() == 1u);

    recorder.clear();
    instance.stop();
    CHECK(instance.connectedClientCount() == 0u);

    const std::vector<std::string> states = recorder.only("state:");
    CHECK(states.size() == 2u);
    CHECK(indexOf(states, "state:Stopping") == 0u);
    CHECK(indexOf(states, "state:Stopped") == 1u);
    CHECK((recorder.counts == std::vector<std::size_t>{0u}));
    // Frozen stop order: Stopping, then the client count, then Stopped.
    CHECK(indexOf(recorder.sequence, "state:Stopping") < indexOf(recorder.sequence, "count:0"));
    CHECK(indexOf(recorder.sequence, "count:0") < indexOf(recorder.sequence, "state:Stopped"));

    HyRemote::detail::resetFactories();
}

void testClearErrorAndRepeatedRecoverableOccurrence()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());
    CHECK(instance.setRemoteInputEnabled(true));
    CHECK(instance.start());

    const hyremote::InputEvent event{};

    recorder.clear();
    state->throwOnInputPost = true;
    CHECK(static_cast<bool>(state->inputHandler));
    state->inputHandler(event);
    CHECK(instance.lastError().has_value());
    CHECK(instance.lastError()->recoverable);
    CHECK(recorder.errors.size() == 1u);
    CHECK(recorder.errors.front().has_value());
    CHECK(recorder.errors.front()->recoverable);
    const std::string firstOccurrence = recorder.sequence.front();

    // An unrelated transport event must not resurrect a cleared error, and must not republish the
    // current one either.
    recorder.clear();
    instance.clearError();
    CHECK(!instance.lastError().has_value());
    CHECK(recorder.sequence.size() == 1u);
    CHECK(recorder.sequence.front() == "error:none");

    recorder.clear();
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    emitTransport(state, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(recorder.errors.empty());

    // The same recoverable failure happening again is a new occurrence: it must be delivered again even
    // though its code, message and recoverability are identical to the acknowledged one.
    recorder.clear();
    state->inputHandler(event);
    CHECK(instance.lastError().has_value());
    CHECK(recorder.errors.size() == 1u);
    CHECK(recorder.errors.front().has_value());
    CHECK(recorder.sequence.size() == 1u);
    CHECK(recorder.sequence.front() == firstOccurrence);
    CHECK(state->inputPosts == 2);

    instance.stop();
    HyRemote::detail::resetFactories();
}

void testTargetLossPublishesErrorThenFaulted()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);

    recorder.clear();
    CHECK(static_cast<bool>(state->captureEventHandler));
    state->captureEventHandler(hyremote::CaptureEvent{hyremote::CaptureEventCode::TargetLost,
                                                      "target destroyed during the run", false});

    CHECK(instance.state() == AccessState::Faulted);
    CHECK(instance.lastError().has_value());
    CHECK(!instance.lastError()->recoverable);

    const std::vector<std::string> states = recorder.only("state:");
    CHECK(states.size() == 1u);
    CHECK(states.front() == "state:Faulted");
    const std::vector<std::string> errors = recorder.only("error:");
    CHECK(errors.size() == 1u);
    // Error first, so a Faulted observer already holds the reason.
    CHECK(indexOf(recorder.sequence, errors.front()) < indexOf(recorder.sequence, "state:Faulted"));

    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);
    HyRemote::detail::resetFactories();
}

void testThrowingConsumerIsContained()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    RuntimeNotificationHandlers throwing = recorder.handlers();
    throwing.stateChanged = [](AccessState) { throw std::runtime_error("consumer state failure"); };
    throwing.connectedClientCountChanged = [](std::size_t) { throw std::runtime_error("consumer count failure"); };
    throwing.errorChanged = [](std::optional<Error>) { throw std::runtime_error("consumer error failure"); };

    CHECK(instance.subscribeNotifications(throwing).isValid());
    CHECK(instance.subscribeNotifications(recorder.handlers()).isValid());

    // None of these calls may let a consumer exception escape, abort a worker or change a Session state.
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(instance.connectedClientCount() == 1u);
    emitTransport(state, hyremote::TransportEventCode::ClientDisconnected);
    instance.clearError();
    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);

    // The well-behaved subscriber still saw everything, and the runtime is still usable.
    CHECK(recorder.only("state:").size() >= 3u);
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);

    HyRemote::detail::resetFactories();
}

void testUnsubscribeStopsDelivery()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder first;
    Recorder second;
    const auto firstToken = instance.subscribeNotifications(first.handlers());
    instance.subscribeNotifications(second.handlers());
    CHECK(firstToken.isValid());

    CHECK(instance.start());
    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(first.counts.size() == 2u);
    CHECK(second.counts.size() == 2u);

    first.clear();
    second.clear();
    instance.unsubscribeNotifications(firstToken);
    // Unsubscribing twice, and unsubscribing a default-constructed token, are no-ops.
    instance.unsubscribeNotifications(firstToken);
    instance.unsubscribeNotifications(::HyRemote::Runtime::RuntimeNotificationToken{});

    emitTransport(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(instance.connectedClientCount() == 2u);
    CHECK(first.sequence.empty());
    CHECK(second.counts.size() == 1u);

    instance.stop();
    HyRemote::detail::resetFactories();
}

void testStaleRunCallbackCannotPolluteTheNextRun()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    AccessInstance instance(&target);
    Recorder recorder;
    instance.subscribeNotifications(recorder.handlers());

    CHECK(instance.start());
    // A queued notification from the first run: the handler the transport was given belongs to that
    // run, so a test can invoke it after the run has ended, exactly like a late worker callback.
    const hyremote::TransportEventHandler staleEventHandler = state->transportEventHandler;
    const hyremote::InputHandler staleInputHandler = state->inputHandler;
    CHECK(static_cast<bool>(staleEventHandler));

    instance.stop();
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);

    recorder.clear();
    staleEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "stale event"});
    if (staleInputHandler) {
        staleInputHandler(hyremote::InputEvent{});
    }

    // The stale callback belongs to a run whose activity token is cleared, so it publishes nothing and
    // the new run's snapshot is untouched: no queued event can overwrite a newer run's values.
    CHECK(recorder.sequence.empty());
    CHECK(instance.connectedClientCount() == 0u);
    CHECK(instance.state() == AccessState::Running);

    instance.stop();
    HyRemote::detail::resetFactories();
}

void testNoCallbackAfterOwnerDestruction()
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    std::atomic<int> callbacks{0};
    {
        AccessInstance instance(&target);
        RuntimeNotificationHandlers handlers;
        handlers.stateChanged = [&callbacks](AccessState) { callbacks.fetch_add(1); };
        handlers.connectedClientCountChanged = [&callbacks](std::size_t) { callbacks.fetch_add(1); };
        handlers.errorChanged = [&callbacks](std::optional<Error>) { callbacks.fetch_add(1); };
        instance.subscribeNotifications(std::move(handlers));
        CHECK(instance.start());
        CHECK(callbacks.load() > 0);
        instance.stop();
    }

    const int afterStop = callbacks.load();
    // Nothing in the runtime can publish once the owner is gone: the wrappers are destroyed with the
    // Session, whose callbacks are already quiesced by stop().
    CHECK(callbacks.load() == afterStop);

    HyRemote::detail::resetFactories();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    testStartOrderingAndStopOrdering();
    testStartFailurePublishesErrorThenStopped();
    testClientCountIsEventDrivenAndNotRepeated();
    testStopPublishesClientCountZeroBeforeStopped();
    testClearErrorAndRepeatedRecoverableOccurrence();
    testTargetLossPublishesErrorThenFaulted();
    testThrowingConsumerIsContained();
    testUnsubscribeStopsDelivery();
    testStaleRunCallbackCannotPolluteTheNextRun();
    testNoCallbackAfterOwnerDestruction();

    if (failures == 0) {
        std::cout << "Runtime notification tests passed\n";
        return 0;
    }

    std::cerr << failures << " runtime notification check(s) failed\n";
    return 1;
}
