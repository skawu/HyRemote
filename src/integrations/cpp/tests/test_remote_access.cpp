#include <HyRemote/RemoteAccess.h>

#include <QFile>
#include <QObject>
#include <QTemporaryDir>

#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
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

struct RuntimeCounters
{
    std::atomic<int> targetFactoryCalls{0};
    std::atomic<int> transportFactoryCalls{0};
    std::atomic<int> captureStarts{0};
    std::atomic<int> captureStops{0};
    std::atomic<int> transportStarts{0};
    std::atomic<int> transportStops{0};
    std::atomic<int> inputPosts{0};
    std::atomic<int> inputShutdowns{0};
    bool transportStartResult = true;
    bool provideInputSink = true;
    bool inputPostThrows = false;
    QHostAddress observedAddress;
    quint16 observedPort = 0;
    // What the composition handed the transport about authentication. The byte count is recorded rather than the
    // password: a test that echoed a secret into its output would be the leak the security model forbids.
    bool observedAuthenticationRequired = false;
    int observedPasswordBytes = -1;
    hyremote::InputHandler transportInputHandler;
    hyremote::TransportEventHandler transportEventHandler;
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

    void post(const hyremote::InputEvent &) override
    {
        ++m_counters->inputPosts;
        if (m_counters->inputPostThrows)
            throw std::runtime_error("fake input post failure");
    }

    void shutdown() noexcept override { ++m_counters->inputShutdowns; }

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
        m_counters->transportInputHandler = m_onInput;
        m_counters->transportEventHandler = std::move(onEvent);
        return m_counters->transportStartResult;
    }

    void stop() noexcept override
    {
        ++m_counters->transportStops;
        m_onInput = {};
        m_counters->transportInputHandler = {};
        m_counters->transportEventHandler = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeCounters> m_counters;
    hyremote::InputHandler m_onInput;
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
        [counters](const QHostAddress &address, quint16 port, const HyRemote::detail::RfbSecurityConfig &security) {
            ++counters->transportFactoryCalls;
            counters->observedAddress = address;
            counters->observedPort = port;
            counters->observedAuthenticationRequired = security.vncAuthenticationRequired;
            counters->observedPasswordBytes = security.password.size();
            HyRemote::detail::TransportComponent result;
            result.transport = std::make_unique<FakeTransport>(counters);
            return result;
        });
}

void emitTransportEvent(const std::shared_ptr<RuntimeCounters> &counters,
                        hyremote::TransportEventCode code)
{
    CHECK(static_cast<bool>(counters->transportEventHandler));
    if (counters->transportEventHandler)
        counters->transportEventHandler(hyremote::TransportEvent{code, "facade diagnostic probe"});
}

void emitTransportInput(const std::shared_ptr<RuntimeCounters> &counters,
                        const hyremote::InputEvent &event)
{
    CHECK(static_cast<bool>(counters->transportInputHandler));
    if (counters->transportInputHandler)
        counters->transportInputHandler(event);
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
    CHECK(remote.port() == HYREMOTE_DEFAULT_PORT);
    CHECK(!remote.remoteInputEnabled());
    CHECK(remote.securityProfile() == HyRemote::RemoteSecurityProfile::Insecure);
    CHECK(remote.securityConfigFile().isEmpty());
    CHECK(remote.connectedClientCount() == 0);
    CHECK(counters->targetFactoryCalls.load() == 0);
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(counters->captureStarts.load() == 0);
    CHECK(counters->transportStarts.load() == 0);
    CHECK(counters->inputShutdowns.load() == 0);

    // #170 security configuration is frozen before the real #143 transport step: a selected secure
    // profile must fail before any target/transport/listener is composed, never downgrade to None.
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(!remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(counters->targetFactoryCalls.load() == 0);
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::SecurityUnavailable);
    CHECK(remote.lastError()->message.contains(QStringLiteral("descriptor")));

    // A descriptor path that cannot be loaded is now a configuration error rather than "unavailable": the
    // descriptor is really parsed. Whether the mechanism exists at all is a property of the build, and a build
    // without it refuses before reading the descriptor.
    const QString descriptor = QStringLiteral("support-security.conf");
    CHECK(remote.setSecurityConfigFile(descriptor));
    CHECK(!remote.start());
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(remote.lastError().has_value());
#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
#else
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::SecurityUnavailable);
#endif
    CHECK(!remote.lastError()->message.isEmpty());

    // Insecure compatibility mode is loopback-only. This absorbs the former #153 guard without
    // changing the normal default listener or pretending an unavailable secure transport exists.
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Insecure));
    CHECK(remote.setListenAddress(QHostAddress::AnyIPv4));
    CHECK(!remote.start());
    CHECK(counters->targetFactoryCalls.load() == 0);
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
    CHECK(remote.lastError()->message.contains(QStringLiteral("non-loopback")));
    CHECK(remote.setListenAddress(QHostAddress::LocalHost));

    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    CHECK(!remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(!remote.setSecurityConfigFile(QStringLiteral("other.conf")));
    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::AuthenticatedEncrypted));
    CHECK(remote.setSecurityConfigFile(QString()));
}

void testMissingTargetAndMissingAdapterFailCleanly()
{
    HyRemote::detail::resetFactories();

    HyRemote::RemoteAccess noTarget;
    CHECK(!noTarget.start());
    CHECK(noTarget.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(noTarget.connectedClientCount() == 0);
    CHECK(noTarget.lastError().has_value());
    CHECK(noTarget.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);

    QObject target;
    HyRemote::RemoteAccess noAdapter(&target);
    CHECK(!noAdapter.start());
    CHECK(noAdapter.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(noAdapter.connectedClientCount() == 0);
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
    CHECK(remote.connectedClientCount() == 0);
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
    CHECK(!remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(!remote.setSecurityConfigFile(QStringLiteral("support.conf")));

    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.connectedClientCount() == 0);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStops.load() == 1);
    CHECK(counters->inputShutdowns.load() == 1);

    // Stop is idempotent: a terminal target-input reset belongs to one runtime only.
    remote.stop();
    CHECK(counters->inputShutdowns.load() == 1);

    // Configuration becomes mutable again after stop.
    CHECK(remote.setPort(5901));
    CHECK(remote.setRemoteInputEnabled(false));
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(remote.setSecurityConfigFile(QStringLiteral("support.conf")));
}

void testMoveTransfersOwnershipAndQuiescesReplacedRuntime()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess destination(&target);
    HyRemote::RemoteAccess source(&target);
    CHECK(destination.setRemoteInputEnabled(true));
    CHECK(source.setRemoteInputEnabled(true));

    CHECK(destination.start());
    CHECK(source.start());
    CHECK(counters->captureStarts.load() == 2);
    CHECK(counters->transportStarts.load() == 2);
    CHECK(counters->captureStops.load() == 0);
    CHECK(counters->transportStops.load() == 0);
    CHECK(counters->inputShutdowns.load() == 0);

    // Move assignment must destroy/quiesce the destination's old running Impl before it takes
    // ownership of the source runtime. The old target input sink is terminally shut down as part of
    // the same replacement; the source runtime itself must remain Running afterwards.
    destination = std::move(source);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStops.load() == 1);
    CHECK(counters->inputShutdowns.load() == 1);
    CHECK(destination.state() == HyRemote::RemoteAccessState::Running);

    // A moved-from facade is intentionally inert/null-safe rather than retaining runtime ownership.
    CHECK(source.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(source.connectedClientCount() == 0);
    CHECK(!source.start());
    CHECK(!source.lastError().has_value());

    destination.stop();
    CHECK(destination.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(counters->captureStops.load() == 2);
    CHECK(counters->transportStops.load() == 2);
    CHECK(counters->inputShutdowns.load() == 2);

    // Move construction transfers a stopped facade without creating/stopping another runtime/sink.
    HyRemote::RemoteAccess moved(std::move(destination));
    CHECK(moved.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(destination.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(counters->captureStops.load() == 2);
    CHECK(counters->transportStops.load() == 2);
    CHECK(counters->inputShutdowns.load() == 2);
}

void testConnectedClientCountUsesTransportNeutralEvents()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.connectedClientCount() == 0);
    CHECK(remote.start());
    CHECK(remote.connectedClientCount() == 0);

    emitTransportEvent(counters, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 1);
    emitTransportEvent(counters, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 2);

    emitTransportEvent(counters, hyremote::TransportEventCode::RecoverableFailure);
    emitTransportEvent(counters, hyremote::TransportEventCode::AuthenticationRejected);
    CHECK(remote.connectedClientCount() == 2);

    emitTransportEvent(counters, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(remote.connectedClientCount() == 1);
    emitTransportEvent(counters, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(remote.connectedClientCount() == 0);

    // A malformed/duplicate disconnect diagnostic cannot underflow the product count.
    emitTransportEvent(counters, hyremote::TransportEventCode::ClientDisconnected);
    CHECK(remote.connectedClientCount() == 0);

    emitTransportEvent(counters, hyremote::TransportEventCode::ClientConnected);
    emitTransportEvent(counters, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 2);

    remote.stop();
    CHECK(remote.connectedClientCount() == 0);
    // The fake transport obeys the quiescence contract by removing its callback on stop. This pins
    // the public invariant that no late connection event can resurrect a Stopped facade count.
    CHECK(!counters->transportEventHandler);
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
    CHECK(counters->inputShutdowns.load() == 0);

    HyRemote::RemoteAccess control(&target);
    CHECK(control.setRemoteInputEnabled(true));
    CHECK(!control.start());
    CHECK(control.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(control.lastError().has_value());
    CHECK(control.lastError()->code == HyRemote::RemoteAccessErrorCode::RemoteInputUnavailable);
    CHECK(counters->inputShutdowns.load() == 0);
}

void testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    counters->inputPostThrows = true;
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setRemoteInputEnabled(true));
    CHECK(remote.start());

    hyremote::InputEvent input;
    input.kind = hyremote::InputEventKind::Key;
    input.key = hyremote::KeyCode::A;
    input.pressed = true;

    emitTransportInput(counters, input);
    CHECK(counters->inputPosts.load() == 1);
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::RuntimeFailure);
    CHECK(remote.lastError()->recoverable);

    remote.clearError();
    CHECK(!remote.lastError().has_value());

    // An identical recoverable failure is a new diagnostic occurrence and must become visible again
    // rather than being hidden forever merely because code/message match the acknowledged error.
    emitTransportInput(counters, input);
    CHECK(counters->inputPosts.load() == 2);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->recoverable);

    remote.clearError();
    CHECK(!remote.lastError().has_value());
    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(!remote.lastError().has_value()); // acknowledged recoverable error must not resurrect
    CHECK(counters->inputShutdowns.load() == 1);
}

void testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic()
{
    HyRemote::detail::resetFactories();
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setRemoteInputEnabled(true));
    CHECK(remote.start());

    emitTransportEvent(counters, hyremote::TransportEventCode::ClientConnected);
    CHECK(remote.connectedClientCount() == 1);
    emitTransportEvent(counters, hyremote::TransportEventCode::FatalFailure);

    CHECK(remote.state() == HyRemote::RemoteAccessState::Faulted);
    CHECK(remote.connectedClientCount() == 1); // runtime remains owned until explicit stop()
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::RuntimeFailure);
    CHECK(!remote.lastError()->recoverable);

    // clearError() acknowledges transient/recoverable diagnostics only. An active fatal error is the
    // explanation for Faulted and remains visible until the owner performs the documented cleanup.
    remote.clearError();
    CHECK(remote.lastError().has_value());
    CHECK(!remote.setPort(5901));
    CHECK(!remote.setRemoteInputEnabled(false));

    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(remote.connectedClientCount() == 0);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStops.load() == 1);
    CHECK(counters->inputShutdowns.load() == 1);
    CHECK(remote.lastError().has_value()); // preserve the fatal reason across cleanup

    remote.clearError();
    CHECK(!remote.lastError().has_value());
    CHECK(remote.setPort(5901));
    CHECK(remote.setRemoteInputEnabled(false));
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
    CHECK(remote.connectedClientCount() == 0);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::StartFailed);
    CHECK(counters->captureStarts.load() == 1);
    CHECK(counters->captureStops.load() == 1);
    CHECK(counters->transportStarts.load() == 1);
    CHECK(counters->inputShutdowns.load() == 0);
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

bool writeTestFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(content) == content.size();
}

// #143 bind policy (docs/security-model.md section 10.1): a listener may only be widened beyond loopback while an
// authentication mode is genuinely enabled, and the credential that protects it has to be in hand - that is what
// makes accepting the widened bind defensible rather than merely configured. Both directions of the rule are
// asserted here so they read together.
void testBindPolicyFollowsAuthentication()
{
    auto counters = std::make_shared<RuntimeCounters>();
    installFakeRuntime(counters);

    QObject target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setListenAddress(QHostAddress(QHostAddress::AnyIPv4)));

    // The default profile carries no authentication, so the widened listener is refused and nothing is composed.
    CHECK(!remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    CHECK(counters->targetFactoryCalls.load() == 0);
    CHECK(counters->transportFactoryCalls.load() == 0);
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
    CHECK(remote.lastError()->message.contains(QStringLiteral("non-loopback")));

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
    QTemporaryDir temp;
    CHECK(temp.isValid());
    CHECK(writeTestFile(temp.filePath(QStringLiteral("password.txt")), QByteArray("secret7\n")));
    CHECK(writeTestFile(temp.filePath(QStringLiteral("authenticated.conf")),
                        QByteArray("version=1\n"
                                   "credentialId=maintenance-console\n"
                                   "passwordFile=password.txt\n")));

    // With a profile that really produced a credential the same address is accepted, and the credential is what the
    // transport is told to require.
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(remote.setSecurityConfigFile(temp.filePath(QStringLiteral("authenticated.conf"))));
    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    CHECK(counters->transportFactoryCalls.load() == 1);
    CHECK(counters->observedAddress == QHostAddress(QHostAddress::AnyIPv4));
    CHECK(counters->observedAuthenticationRequired);
    CHECK(counters->observedPasswordBytes == 7);  // "secret7"
    remote.stop();
#else
    // Without the capability in this build the same configuration is refused rather than served unauthenticated,
    // even though a descriptor is named.
    CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated));
    CHECK(remote.setSecurityConfigFile(QStringLiteral("support-security.conf")));
    CHECK(!remote.start());
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::SecurityUnavailable);
#endif

    HyRemote::detail::resetFactories();
}

int main()
{
    testSafeDefaultsAndNoConstructionSideEffect();
    testMissingTargetAndMissingAdapterFailCleanly();
    testProductLifecycleAndConfigurationForwarding();
    testMoveTransfersOwnershipAndQuiescesReplacedRuntime();
    testConnectedClientCountUsesTransportNeutralEvents();
    testRemoteInputIsIndependentAndOffByDefault();
    testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure();
    testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic();
    testBackendStartFailureIsMappedAndCleanedUp();
    testInvalidPublicConfigurationIsProductLevel();
    testBindPolicyFollowsAuthentication();

    HyRemote::detail::resetFactories();
    if (failures != 0) {
        std::cerr << failures << " RemoteAccess test assertion(s) failed\n";
        return 1;
    }

    std::cout << "RemoteAccess facade tests passed\n";
    return 0;
}
