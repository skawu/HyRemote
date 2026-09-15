#include <HyRemote/RemoteAccess.h>

#include <QObject>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>

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

struct RuntimeCounters
{
    std::atomic<int> targetFactoryCalls{0};
    std::atomic<int> transportFactoryCalls{0};
    std::atomic<int> captureStarts{0};
    std::atomic<int> captureStops{0};
    std::atomic<int> transportStarts{0};
    std::atomic<int> transportStops{0};
    std::atomic<int> inputPosts{0};
    bool transportStartResult = true;
    bool provideInputSink = true;
    QHostAddress observedAddress;
    quint16 observedPort = 0;
};

class FakeCapture final : public hyremote::CaptureSource
{
public:
    explicit FakeCapture(std::shared_ptr<RuntimeCounters> counters)
        : m_counters(std::move(counters))
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

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler onEvent) override
    {
        m_onFrame = std::move(onFrame);
        m_onEvent = std::move(onEvent);
        ++m_counters->captureStarts;
        return true;
    }

    void stop() noexcept override
    {
        ++m_counters->captureStops;
        m_onFrame = {};
        m_onEvent = {};
    }

    bool requestFrame(const hyremote::CaptureRequest &) override
    {
        // The facade tests exercise lifecycle/composition, not target capture. Refusing the request
        // is a valid recoverable Core path and keeps the test independent of GUI/rendering.
        return false;
    }

private:
    std::shared_ptr<RuntimeCounters> m_counters;
    hyremote::FrameReadyHandler m_onFrame;
    hyremote::CaptureEventHandler m_onEvent;
};

class FakeInput final : public hyremote::InputSink
{
public:
    explicit FakeInput(std::shared_ptr<RuntimeCounters> counters)
        : m_counters(std::move(counters))
    {
    }

    void post(const hyremote::InputEvent &) override { ++m_counters->inputPosts; }

private:
    std::shared_ptr<RuntimeCounters> m_counters;
};

class FakeTransport final : public hyremote::Transport
{
public:
    explicit FakeTransport(std::shared_ptr<RuntimeCounters> counters)
        : m_counters(std::move(counters))
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
        ++m_counters->transportStarts;
        m_onInput = std::move(onInput);
        m_onEvent = std::move(onEvent);
        return m_counters->transportStartResult;
    }

    void stop() noexcept override
    {
        ++m_counters->transportStops;
        m_onInput = {};
        m_onEvent = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeCounters> m_counters;
    hyremote::InputHandler m_onInput;
    hyremote::TransportEventHandler m_onEvent;
};

void installFakeRuntime(const std::shared_ptr<RuntimeCounters> &counters)
{
    HyRemote::detail::setTargetFactory(
        [counters](QObject *target, bool remoteInputEnabled) {
            ++counters->targetFactoryCalls;
            HyRemote::detail::TargetComponents result;
            result.supported = target != nullptr;
            result.capture = std::make_unique<FakeCapture>(counters);
            if (remoteInputEnabled && counters->provideInputSink)
                result.input = std::make_shared<FakeInput>(counters);
            return result;
        });

    HyRemote::detail::setTransportFactory(
        [counters](const QHostAddress &address, quint16 port) {
            ++counters->transportFactoryCalls;
            counters->observedAddress = address;
            counters->observedPort = port;
            HyRemote::detail::TransportComponent result;
            result.transport = std::make_unique<FakeTransport>(counters);
            return result;
        });
}

void testSafeDefaultsAndNoConstructionSideEffect()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);

    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.listenAddress() == QHostAddress(QHostAddress::LocalHost));
    CHECK(remote.port() == 5900);
    CHECK(!remote.remoteInputEnabled());
    CHECK(counters->targetFactoryCalls.load() == 0);
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(counters->captureStarts.load() == 0);
    CHECK(counters->transportStarts.load() == 0);
}

void testMissingTargetAndMissingAdapterFailCleanly()
{
    HyRemote::detail::resetFactories();

    HyRemote::RemoteAccess noTarget;
    CHECK(!noTarget.start());
    CHECK(noTarget.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(noTarget.lastError().has_value());
    CHECK(noTarget.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);

    QObject target;
    HyRemote::RemoteAccess noAdapter(&target);
    CHECK(!noAdapter.start());
    CHECK(noAdapter.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(noAdapter.lastError().has_value());
    CHECK(noAdapter.lastError()->code == HyRemote::RemoteAccessErrorCode::TargetAdapterUnavailable);
}

void testProductLifecycleAndConfigurationForwarding()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setListenAddress(QHostAddress(QStringLiteral("127.0.0.2"))));
    CHECK(remote.setPort(5999));
    CHECK(remote.setRemoteInputEnabled(true));

    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    CHECK(counters->targetFactoryCalls.load() == 1);
    CHECK(counters->transportFactoryCalls.load() == 1);
    CHECK(counters->captureStarts.load() == 1);
    CHECK(counters->transportStarts.load() == 1);
    CHECK(counters->observedAddress == QHostAddress(QStringLiteral("127.0.0.2")));
    CHECK(counters->observedPort == 5999);

    // Public configuration cannot mutate a live Session.
    CHECK(!remote.setPort(5901));
    CHECK(!remote.setTarget(nullptr));
    CHECK(!remote.setRemoteInputEnabled(false));

    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStops.load() == 1);

    // Configuration becomes mutable again after stop.
    CHECK(remote.setPort(5901));
    CHECK(remote.setRemoteInputEnabled(false));
}

void testRemoteInputIsIndependentAndOffByDefault()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    counters->provideInputSink = false;
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess viewOnly(&target);
    CHECK(viewOnly.start());  // input sink is not required for view-only mode
    CHECK(viewOnly.state() == HyRemote::RemoteAccessState::Running);
    viewOnly.stop();

    HyRemote::RemoteAccess control(&target);
    CHECK(control.setRemoteInputEnabled(true));
    CHECK(!control.start());
    CHECK(control.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(control.lastError().has_value());
    CHECK(control.lastError()->code == HyRemote::RemoteAccessErrorCode::RemoteInputUnavailable);
}

void testBackendStartFailureIsMappedAndCleanedUp()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    counters->transportStartResult = false;
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(!remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::StartFailed);
    CHECK(counters->captureStarts.load() == 1);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStarts.load() == 1);
}

void testInvalidPublicConfigurationIsProductLevel()
{
    HyRemote::detail::resetFactories();
    QObject target;
    HyRemote::RemoteAccess remote(&target);

    CHECK(!remote.setListenAddress(QHostAddress{}));
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);

    remote.clearError();
    CHECK(!remote.lastError().has_value());
    CHECK(!remote.setPort(0));
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->message.contains(QStringLiteral("port")));
}

}  // namespace

int main()
{
    testSafeDefaultsAndNoConstructionSideEffect();
    testMissingTargetAndMissingAdapterFailCleanly();
    testProductLifecycleAndConfigurationForwarding();
    testRemoteInputIsIndependentAndOffByDefault();
    testBackendStartFailureIsMappedAndCleanedUp();
    testInvalidPublicConfigurationIsProductLevel();

    HyRemote::detail::resetFactories();
    if (failures != 0) {
        std::cerr << failures << " RemoteAccess test assertion(s) failed\n";
        return 1;
    }

    std::cout << "RemoteAccess facade tests passed\n";
    return 0;
}
