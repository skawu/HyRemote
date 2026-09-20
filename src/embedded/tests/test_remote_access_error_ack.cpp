#include <HyRemote/RemoteAccess.h>

#include <QObject>

#include <iostream>
#include <memory>
#include <stdexcept>
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

struct RuntimeProbe
{
    hyremote::InputHandler inputHandler;
    hyremote::TransportEventHandler eventHandler;
    int inputPosts = 0;
};

class FakeCapture final : public hyremote::CaptureSource
{
public:
    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler, hyremote::CaptureEventHandler) override { return true; }
    void stop() noexcept override {}
    bool requestFrame(const hyremote::CaptureRequest &) override { return false; }
};

class ThrowingInput final : public hyremote::InputSink
{
public:
    explicit ThrowingInput(std::shared_ptr<RuntimeProbe> probe)
        : m_probe(std::move(probe))
    {
    }

    void post(const hyremote::InputEvent &) override
    {
        ++m_probe->inputPosts;
        throw std::runtime_error("deterministic input failure");
    }

private:
    std::shared_ptr<RuntimeProbe> m_probe;
};

class FakeTransport final : public hyremote::Transport
{
public:
    explicit FakeTransport(std::shared_ptr<RuntimeProbe> probe)
        : m_probe(std::move(probe))
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
        m_probe->inputHandler = std::move(onInput);
        m_probe->eventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override
    {
        m_probe->inputHandler = {};
        m_probe->eventHandler = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeProbe> m_probe;
};

void installRuntime(const std::shared_ptr<RuntimeProbe> &probe)
{
    HyRemote::detail::setTargetFactory(
        [probe](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            result.supported = target != nullptr;
            result.capture = std::make_unique<FakeCapture>();
            if (remoteInputEnabled)
                result.input = std::make_shared<ThrowingInput>(probe);
            return result;
        });

    HyRemote::detail::setTransportFactory(
        [probe](const QHostAddress &, quint16) {
            HyRemote::detail::TransportComponent result;
            result.transport = std::make_unique<FakeTransport>(probe);
            return result;
        });
}

void emitInput(const std::shared_ptr<RuntimeProbe> &probe)
{
    CHECK(static_cast<bool>(probe->inputHandler));
    if (!probe->inputHandler)
        return;

    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::Key;
    event.key = hyremote::KeyCode::A;
    event.pressed = true;
    probe->inputHandler(event);
}

void emitEvent(const std::shared_ptr<RuntimeProbe> &probe, hyremote::TransportEventCode code)
{
    CHECK(static_cast<bool>(probe->eventHandler));
    if (probe->eventHandler)
        probe->eventHandler(hyremote::TransportEvent{code, "unrelated transport event"});
}

void testAcknowledgedRecoverableErrorSurvivesUnrelatedTransportEvents()
{
    HyRemote::detail::resetFactories();
    const auto probe = std::make_shared<RuntimeProbe>();
    installRuntime(probe);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setRemoteInputEnabled(true));
    CHECK(remote.start());

    emitInput(probe);
    CHECK(probe->inputPosts == 1);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->recoverable);

    remote.clearError();
    CHECK(!remote.lastError().has_value());

    // These events advance ordinary transport diagnostics but do not represent a new occurrence of
    // the acknowledged recoverable input failure. The old error must stay cleared.
    emitEvent(probe, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 1);
    CHECK(!remote.lastError().has_value());

    emitEvent(probe, hyremote::TransportEventCode::RecoverableFailure);
    CHECK(!remote.lastError().has_value());

    emitEvent(probe, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(remote.connectedClientCount() == 0);
    CHECK(!remote.lastError().has_value());

    // A genuinely new occurrence of the same recoverable failure advances inputPostFailures and
    // must become observable again even though code/message are identical.
    emitInput(probe);
    CHECK(probe->inputPosts == 2);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->recoverable);

    remote.clearError();
    CHECK(!remote.lastError().has_value());
    remote.stop();
    CHECK(!remote.lastError().has_value());

    HyRemote::detail::resetFactories();
}

}  // namespace

int main()
{
    testAcknowledgedRecoverableErrorSurvivesUnrelatedTransportEvents();
    return failures == 0 ? 0 : 1;
}
