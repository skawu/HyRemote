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
            counters->observedAuthenticationRequired =
            security.profile == HyRemote::detail::RfbSecurityProfile::VncAuth;
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
    const quint16 accepted_port = remote.port();
    CHECK(!remote.setPort(0));
    CHECK(remote.lastError().has_value());
    CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
    CHECK(remote.lastError()->message.contains(QStringLiteral("port")));
    // The message has to explain the rejection in product terms: the rejected value and the accepted range. A
    // release label is not a user-facing contract, and the V0.0.x labels are retired planning labels under current
    // release authority, so none of them may appear in an error a user can read.
    CHECK(remote.lastError()->message.contains(QStringLiteral("65535")));
    const QStringList retired_labels{QStringLiteral("V0.0.1.0"), QStringLiteral("V0.0.2.0"),
                                     QStringLiteral("V0.0.3.0")};
    for (const QString &retired_label : retired_labels) {
        CHECK(!remote.lastError()->message.contains(retired_label));
    }
    // Rejecting the value must not change the configuration that was already accepted.
    CHECK(remote.port() == accepted_port);
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

// #232/#143: AuthenticatedEncrypted is a declared product API and, since #143, an implemented V0.2 profile whose
// wire contract is VeNCrypt 0.2 + X509Vnc 261 + TLS >= 1.2 with VNC Authentication inside the tunnel. This test owns
// the fail-closed boundary: material that cannot be used is refused deterministically before any listener exists, in
// a build without the transport-security capability and equally in one that has it - the weaker VNC Authentication is
// never served in place of the encrypted profile, and a readable descriptor never makes unusable material valid.
void testAuthenticatedEncryptedFailsClosedBeforeListen()
{
    auto counters = std::make_shared<RuntimeCounters>();
    HyRemote::detail::resetFactories();
    installFakeRuntime(counters);

    // Item 1 of the required evidence - the Insecure loopback baseline still starts as intended - is asserted by
    // testProductLifecycleAndConfigurationForwarding, which owns the running-listener fixture. It is not duplicated
    // here: this test asserts what has to be true *before* a listener exists, so it composes none of its own.

    // 2. Unauthenticated non-loopback still fails before listen: no target and no transport is ever composed for it.
    {
        HyRemote::RemoteAccess remote;
        CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Insecure));
        CHECK(remote.setListenAddress(QHostAddress::AnyIPv4));
        CHECK(!remote.start());
        CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
        CHECK(remote.lastError().has_value());
        CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
        CHECK(counters->targetFactoryCalls.load() == 0);
        CHECK(counters->transportFactoryCalls.load() == 0);
        CHECK(counters->transportStarts.load() == 0);
    }

    // 3./4./5. #143 turns AuthenticatedEncrypted into a real V0.2 profile, so the retired assumption - that the
    // profile can never start - is gone. What a facade test can honestly own is the fail-closed half: material that
    // cannot be used never reaches listen(), and no target or transport is composed for it. The positive half (a real
    // VeNCrypt/TLS session with VNC Authentication inside it on the real transport) is proven by
    // src/runtime/tests/test_rfb_secure_transport.cpp and by the security-enabled hosted lane; fake PEM fixtures
    // cannot prove it here. Descriptor validity, material validity and backend capability are three separate facts.
    {
        QTemporaryDir temp;
        CHECK(temp.isValid());
        auto writeFixture = [](const QString &path, const QByteArray &content) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(content) == content.size();
        };
        CHECK(writeFixture(temp.filePath(QStringLiteral("password.txt")), QByteArray("secret7\n")));
        CHECK(writeFixture(temp.filePath(QStringLiteral("malformed-certificate.pem")),
                           QByteArray("-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----\n")));
        CHECK(writeFixture(temp.filePath(QStringLiteral("malformed-private-key.pem")),
                           QByteArray("-----BEGIN PRIVATE KEY-----\nfixture\n-----END PRIVATE KEY-----\n")));

        struct MaterialCase
        {
            const char *name;
            const char *certificate;
            const char *key;
        };
        const MaterialCase cases[] = {
            {"malformed certificate", "malformed-certificate.pem", "malformed-private-key.pem"},
            {"missing certificate", "absent-certificate.pem", "malformed-private-key.pem"},
            {"missing private key", "malformed-certificate.pem", "absent-private-key.pem"},
        };

        int index = 0;
        for (const MaterialCase &testCase : cases) {
            const QString descriptorPath =
                temp.filePath(QStringLiteral("encrypted-descriptor-") + QString::number(index++) + QStringLiteral(".conf"));
            CHECK(writeFixture(descriptorPath,
                               QByteArray("version=1\n"
                                          "credentialId=operator\n"
                                          "passwordFile=password.txt\n"
                                          "certificateFile=")
                                   + testCase.certificate + QByteArray("\nprivateKeyFile=") + testCase.key
                                   + QByteArray("\n")));

            HyRemote::RemoteAccess remote;
            CHECK(remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::AuthenticatedEncrypted));
            CHECK(remote.setSecurityConfigFile(descriptorPath));

            const int targetsBeforeRefusal = counters->targetFactoryCalls.load();
            const int transportsBeforeRefusal = counters->transportFactoryCalls.load();
            CHECK(!remote.start());
            CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
            CHECK(remote.lastError().has_value());
#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
            // The capability is there, so the refusal has to name the material that was really inspected.
            CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
#else
            // Without the capability the profile is refused before any material is read at all.
            CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::SecurityUnavailable);
#endif
            // The failure discloses neither the credential nor the descriptor location.
            CHECK(!remote.lastError()->message.contains(QStringLiteral("secret7")));
            CHECK(!remote.lastError()->message.contains(descriptorPath));

            // The before-listen proof is explicit rather than inferred from Runtime state: a refused start composes
            // no target and no transport, so no socket is created and nothing can be left listening.
            CHECK(counters->targetFactoryCalls.load() == targetsBeforeRefusal);
            CHECK(counters->transportFactoryCalls.load() == transportsBeforeRefusal);
            CHECK(counters->transportStarts.load() == 0);  // nothing in this test ever opened a listener

            // The refusal is deterministic: retrying, and retrying against a different descriptor, changes nothing.
            CHECK(!remote.start());
            CHECK(counters->transportFactoryCalls.load() == transportsBeforeRefusal);
            CHECK(remote.setSecurityConfigFile(temp.filePath(QStringLiteral("second-descriptor.conf"))));
            CHECK(!remote.start());
            CHECK(counters->transportFactoryCalls.load() == transportsBeforeRefusal);

            remote.stop();
            CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
        }
    }

    HyRemote::detail::resetFactories();
}

int main()
{
    testSafeDefaultsAndNoConstructionSideEffect();
    testMissingTargetAndMissingAdapterFailCleanly();
    testProductLifecycleAndConfigurationForwarding();
    testAuthenticatedEncryptedFailsClosedBeforeListen();
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
