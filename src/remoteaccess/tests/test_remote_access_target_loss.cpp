#include <HyRemote/RemoteAccess.h>

#include <QObject>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "detail/component_factories.hpp"
#include "hyremote/core/hyremote_core.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

struct RuntimeState
{
    std::atomic<int> captureStarts{0};
    std::atomic<int> captureStops{0};
    std::atomic<int> transportStarts{0};
    std::atomic<int> transportStops{0};
    std::atomic<int> inputShutdowns{0};
    hyremote::CaptureEventHandler captureEventHandler;
    hyremote::TransportEventHandler transportEventHandler;
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

class FakeInput final : public hyremote::InputSink
{
public:
    explicit FakeInput(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    void post(const hyremote::InputEvent &) override {}
    void shutdown() noexcept override { ++m_state->inputShutdowns; }

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

    bool start(hyremote::InputHandler, hyremote::TransportEventHandler onEvent) override
    {
        ++m_state->transportStarts;
        m_state->transportEventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override
    {
        ++m_state->transportStops;
        m_state->transportEventHandler = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeState> m_state;
};

void installRuntime(const std::shared_ptr<RuntimeState> &state)
{
    HyRemote::detail::setTargetFactory([state](QObject *target, bool remoteInputEnabled) {
        HyRemote::detail::TargetComponents result;
        result.supported = target != nullptr;
        if (!result.supported)
            return result;
        result.capture = std::make_unique<FakeCapture>(state);
        if (remoteInputEnabled)
            result.input = std::make_shared<FakeInput>(state);
        return result;
    });

    HyRemote::detail::setTransportFactory([state](const QHostAddress &, quint16,
                                                  const HyRemote::detail::RfbSecurityConfig &) {
        HyRemote::detail::TransportComponent result;
        result.transport = std::make_unique<FakeTransport>(state);
        return result;
    });
}

void emitCaptureTargetLost(const std::shared_ptr<RuntimeState> &state)
{
    CHECK(static_cast<bool>(state->captureEventHandler));
    if (state->captureEventHandler) {
        state->captureEventHandler(hyremote::CaptureEvent{
            hyremote::CaptureEventCode::TargetLost,
            "target destroyed during facade lifecycle regression",
            false});
    }
}

void emitTransportEvent(const std::shared_ptr<RuntimeState> &state,
                        hyremote::TransportEventCode code)
{
    CHECK(static_cast<bool>(state->transportEventHandler));
    if (state->transportEventHandler)
        state->transportEventHandler(hyremote::TransportEvent{code, "facade target-loss probe"});
}

void testTargetLossFaultStopReplaceRestart()
{
    HyRemote::detail::resetFactories();
    auto state = std::make_shared<RuntimeState>();
    installRuntime(state);

    auto target = std::make_unique<QObject>();
    HyRemote::RemoteAccess remote(target.get());
    CHECK(remote.setRemoteInputEnabled(true));
    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);

    emitTransportEvent(state, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 1);

    // The facade holds the target weakly. Deleting the target must not create a dangling public
    // pointer, while the still-owned runtime remains the authority until its adapter reports loss.
    target.reset();
    CHECK(remote.target() == nullptr);

    emitCaptureTargetLost(state);
    CHECK(remote.state() == HyRemote::RemoteAccessState::Faulted);
    CHECK(remote.connectedClientCount() == 1);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::RuntimeFailure);
    CHECK(!remote.lastError()->recoverable);

    QObject replacement;
    CHECK(!remote.setTarget(&replacement));
    remote.clearError();
    CHECK(remote.lastError().has_value());

    // Faulted still owns the listener/client diagnostic. Explicit stop is the one cleanup boundary:
    // it quiesces Session/transport, clears the client diagnostic and terminally shuts the input sink.
    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.connectedClientCount() == 0);
    CHECK(state->captureStops.load() == 1);
    CHECK(state->transportStops.load() == 1);
    CHECK(state->inputShutdowns.load() == 1);
    CHECK(!state->captureEventHandler);
    CHECK(!state->transportEventHandler);
    CHECK(remote.lastError().has_value());

    // Only after Stopped may the owner replace the target and restart the same public facade.
    CHECK(remote.setTarget(&replacement));
    CHECK(remote.target() == &replacement);
    remote.clearError();
    CHECK(!remote.lastError().has_value());
    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(state->captureStarts.load() == 2);
    CHECK(state->transportStarts.load() == 2);
    CHECK(state->captureStops.load() == 2);
    CHECK(state->transportStops.load() == 2);
    CHECK(state->inputShutdowns.load() == 2);
}

}  // namespace

int main()
{
    testTargetLossFaultStopReplaceRestart();
    HyRemote::detail::resetFactories();

    if (failures != 0) {
        std::cerr << failures << " RemoteAccess target-loss assertion(s) failed\n";
        return 1;
    }

    std::cout << "RemoteAccess target-loss lifecycle tests passed\n";
    return 0;
}
