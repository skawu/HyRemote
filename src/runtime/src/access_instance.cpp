#include "access_instance.hpp"

#include <QAbstractSocket>  // QHostAddress::protocol() answers in this enum, used by the IPv4-only contract
#include <QTimer>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "detail/component_factories.hpp"
#include "detail/listener_binding.hpp"
#include "detail/security_descriptor.hpp"
#include "hyremote/core/session.hpp"

namespace HyRemote::Runtime {
namespace {

AccessState mapState(hyremote::SessionState state)
{
    switch (state) {
    case hyremote::SessionState::Stopped:
        return AccessState::Stopped;
    case hyremote::SessionState::Starting:
        return AccessState::Starting;
    case hyremote::SessionState::Running:
        return AccessState::Running;
    case hyremote::SessionState::Stopping:
        return AccessState::Stopping;
    case hyremote::SessionState::Faulted:
        return AccessState::Faulted;
    }
    return AccessState::Faulted;
}

Error mapError(const hyremote::SessionError &error)
{
    Error result;
    result.message = QString::fromStdString(error.message);
    result.recoverable = error.recoverable;

    switch (error.code) {
    case hyremote::SessionErrorCode::InvalidConfiguration:
    case hyremote::SessionErrorCode::IncompatibleFrameCapabilities:
        result.code = ErrorCode::InvalidConfiguration;
        break;
    case hyremote::SessionErrorCode::CaptureStartFailed:
    case hyremote::SessionErrorCode::TransportStartFailed:
        result.code = ErrorCode::StartFailed;
        break;
    case hyremote::SessionErrorCode::StartCancelled:
        result.code = ErrorCode::Cancelled;
        break;
    case hyremote::SessionErrorCode::TargetLost:
    case hyremote::SessionErrorCode::ComponentFailure:
        result.code = ErrorCode::RuntimeFailure;
        break;
    }

    return result;
}

void decrementConnectedClients(const std::shared_ptr<std::atomic<std::size_t>> &count) noexcept
{
    if (!count)
        return;

    std::size_t current = count->load(std::memory_order_relaxed);
    while (current != 0
           && !count->compare_exchange_weak(current,
                                            current - 1,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
    }
}

// Wraps the transport so the shared Runtime keeps the one authoritative client count and observes the
// *result* of every transport event rather than racing the Core for it: the Core handler runs first,
// and only then does the Runtime read and publish what the Session made of the event.
//
// A run-scoped activity token makes a callback that belongs to a run that has already stopped inert:
// once stop() has entered, a late event can neither change the count nor publish a notification into
// the next run. That is what keeps a queued notification from a previous run from ever overwriting a
// newer run's values - the property #259 freezes instead of a per-notification run generation.
class ClientCountingTransport final : public hyremote::Transport
{
public:
    ClientCountingTransport(std::unique_ptr<hyremote::Transport> transport,
                            std::shared_ptr<std::atomic<std::size_t>> connectedClients,
                            std::shared_ptr<std::atomic<bool>> runActive,
                            std::function<void()> onRuntimeObservation)
        : m_transport(std::move(transport))
        , m_connectedClients(std::move(connectedClients))
        , m_runActive(std::move(runActive))
        , m_onRuntimeObservation(std::move(onRuntimeObservation))
    {
    }

    hyremote::FrameConsumerCapabilities frameCapabilities() const override
    {
        return m_transport->frameCapabilities();
    }

    bool start(hyremote::InputHandler onInput, hyremote::TransportEventHandler onEvent) override
    {
        m_connectedClients->store(0, std::memory_order_relaxed);
        const auto connectedClients = m_connectedClients;
        const auto runActive = m_runActive;
        const auto observe = m_onRuntimeObservation;
        const bool started = m_transport->start(
            // The input path is observed here rather than on the sink because the Core owns the outcome:
            // an InputSink that throws is caught by Core, counted and reported as a recoverable error
            // inside this call, so once it returns the Runtime can read the truth the Core produced
            // instead of asserting one first. That is what makes a repeated recoverable failure
            // observable again after it was acknowledged.
            [runActive, observe, onInput = std::move(onInput)](const hyremote::InputEvent &event) {
                if (onInput)
                    onInput(event);

                if (!runActive->load(std::memory_order_acquire))
                    return;
                if (observe)
                    observe();
            },
            [connectedClients, runActive, observe, onEvent = std::move(onEvent)](
                const hyremote::TransportEvent &event) mutable {
                if (onEvent)
                    onEvent(event);

                if (!runActive->load(std::memory_order_acquire)) {
                    return;
                }

                switch (event.code) {
                case hyremote::TransportEventCode::ClientConnected:
                    connectedClients->fetch_add(1, std::memory_order_relaxed);
                    break;
                case hyremote::TransportEventCode::ClientDisconnected:
                    decrementConnectedClients(connectedClients);
                    break;
                case hyremote::TransportEventCode::AuthenticationRejected:
                case hyremote::TransportEventCode::RecoverableFailure:
                case hyremote::TransportEventCode::FatalFailure:
                    break;
                }

                if (observe)
                    observe();
            });
        if (!started)
            m_connectedClients->store(0, std::memory_order_relaxed);
        return started;
    }

    void stop() noexcept override
    {
        m_transport->stop();
        m_connectedClients->store(0, std::memory_order_relaxed);
    }

    void enqueueFrame(hyremote::RemoteFrame frame) override
    {
        m_transport->enqueueFrame(std::move(frame));
    }

private:
    std::unique_ptr<hyremote::Transport> m_transport;
    std::shared_ptr<std::atomic<std::size_t>> m_connectedClients;
    std::shared_ptr<std::atomic<bool>> m_runActive;
    std::function<void()> m_onRuntimeObservation;
};

// Wraps the capture source for the same reason: target loss and backend failure reach the Core first
// and the Runtime then publishes what the Core made of them. Without this seam the Runtime would have
// to guess the outcome (and could claim Faulted before the Core decided) or poll for it.
class ObservedCaptureSource final : public hyremote::CaptureSource
{
public:
    ObservedCaptureSource(std::unique_ptr<hyremote::CaptureSource> source,
                          std::shared_ptr<std::atomic<bool>> runActive,
                          std::function<void()> onRuntimeObservation)
        : m_source(std::move(source))
        , m_runActive(std::move(runActive))
        , m_onRuntimeObservation(std::move(onRuntimeObservation))
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        return m_source->capabilities();
    }

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler onEvent) override
    {
        const auto runActive = m_runActive;
        const auto observe = m_onRuntimeObservation;
        return m_source->start(
            std::move(onFrame),
            [runActive, observe, onEvent = std::move(onEvent)](const hyremote::CaptureEvent &event) mutable {
                if (onEvent)
                    onEvent(event);

                if (!runActive->load(std::memory_order_acquire)) {
                    return;
                }
                if (observe)
                    observe();
            });
    }

    void stop() noexcept override
    {
        m_source->stop();
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        return m_source->requestFrame(request);
    }

private:
    std::unique_ptr<hyremote::CaptureSource> m_source;
    std::shared_ptr<std::atomic<bool>> m_runActive;
    std::function<void()> m_onRuntimeObservation;
};

}  // namespace

// #174: the interface watcher. Qt has no portable per-interface address-change signal, so a low-frequency poll is
// the reliable mechanism available - it is not a network manager, it does no discovery, and it is never a public API.
// It exists only while an interface binding is active, and the function it calls is the same one the tests call
// directly.
class InterfaceWatcher : public QObject
{
public:
    explicit InterfaceWatcher(std::function<void()> onTick)
        : m_onTick(std::move(onTick))
    {
        m_timer.setInterval(2000);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { m_onTick(); });
    }

    void start() { m_timer.start(); }
    void stop() { m_timer.stop(); }

private:
    QTimer m_timer;
    std::function<void()> m_onTick;
};

struct AccessInstance::Impl
{
    struct SessionErrorRevision
    {
        std::uint64_t inputPostFailures = 0;
    };

    QPointer<QObject> target;
    // 0.0.0.0 is the product default: a first run is reachable on the host's IPv4 interfaces without the user
    // having to find an address first. This is deliberate product behaviour, not accidental widening (#174), and
    // the shipped security state is what tells the user how much to trust that reachability.
    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;
    // #174 interface identity. Empty means the address decides the mode.
    QString listenInterface;
    std::unique_ptr<hyremote::Session> session;
    std::shared_ptr<hyremote::InputSink> inputSink;
    std::optional<Error> error;
    std::optional<hyremote::SessionError> acknowledgedRecoverableError;
    SessionErrorRevision acknowledgedRecoverableRevision;
    std::shared_ptr<std::atomic<std::size_t>> connectedClients =
        std::make_shared<std::atomic<std::size_t>>(0);

    // #259 private notification seam. `transitionState` is the only state this class projects on its
    // own, and only while a lifecycle call is in flight: Core performs its own Starting/Stopping window
    // synchronously inside start()/stop(), so an observer would otherwise never see those two states.
    // Every other state is read from the Core Session, so no second state machine exists here.
    RuntimeNotificationSink notifications;
    std::optional<AccessState> transitionState;
    std::shared_ptr<std::atomic<bool>> runActive;
    std::uint64_t lastFailureRevision = 0;

    // ---------------------------------------------------------------- #174 binding lifecycle
    //
    // `desiredActive` is the user's intent to be reachable, and it outlives any single listener: when the selected
    // interface loses its address the Session is gone but the intent is not, and only stop() clears it. That is what
    // keeps the configuration immutable while an interface listener is waiting to come back, so a recovery can never
    // race an edit the user made in the meantime. `effectiveAddress` is where the current Session actually listens -
    // an observation, never a setting; the configured identity stays `listenInterface`.
    bool desiredActive = false;
    std::optional<QHostAddress> effectiveAddress;
    // The transport security derived from configuration. It is kept only so the rebind path composes exactly what
    // the current run composed: while desiredActive is true the configuration cannot change, so it cannot go stale.
    std::optional<detail::RfbSecurityConfig> activeTransportSecurity;
    std::unique_ptr<InterfaceWatcher> watcher;

    // The single composition path (#174). The initial start, a rebind after the interface moved and a recovery all
    // come through here, so there is exactly one place that builds a Session and one that tears the previous one
    // down - never two ways to build a listener, and never a weaker one for the recovery path.
    bool composeAndStart(const QHostAddress &address, const detail::RfbSecurityConfig &security);
    // A -> B for the same configured identity.
    bool rebindTo(const QHostAddress &address);
    // The reconciliation the watcher and the tests share.
    void reconcileBinding();
    void startInterfaceWatcher();
    void stopInterfaceWatcher();

    ~Impl() { shutdownRuntime(); }

    bool isConfigurable() const
    {
        // #174: the Session is no longer the whole truth. After an interface loses its address the Session is gone,
        // yet the user has still not stopped and a watcher is waiting to bring the listener back - so accepting a
        // configuration change here would race the recovery that is about to happen. Only stop() ends the intent.
        if (desiredActive)
            return false;
        return !session || session->state() == hyremote::SessionState::Stopped;
    }

    void setError(ErrorCode code, QString message, bool recoverable = false)
    {
        error = Error{code, std::move(message), recoverable};
    }

    SessionErrorRevision currentSessionErrorRevision() const
    {
        if (!session)
            return {};
        const hyremote::SessionStats stats = session->stats();
        return SessionErrorRevision{stats.inputPostFailures};
    }

    bool isAcknowledgedRecoverableError(const hyremote::SessionError &candidate) const
    {
        if (!candidate.recoverable || !acknowledgedRecoverableError || !session)
            return false;

        const hyremote::SessionError &acknowledged = *acknowledgedRecoverableError;
        if (acknowledged.code != candidate.code || acknowledged.message != candidate.message
            || acknowledged.recoverable != candidate.recoverable) {
            return false;
        }

        const SessionErrorRevision current = currentSessionErrorRevision();
        return current.inputPostFailures == acknowledgedRecoverableRevision.inputPostFailures;
    }

    void resetErrorAcknowledgement() noexcept
    {
        acknowledgedRecoverableError.reset();
        acknowledgedRecoverableRevision = {};
    }

    void acknowledgeCurrentRecoverableError()
    {
        resetErrorAcknowledgement();
        if (!session)
            return;

        const std::optional<hyremote::SessionError> coreError = session->lastError();
        if (!coreError || !coreError->recoverable)
            return;

        acknowledgedRecoverableError = *coreError;
        acknowledgedRecoverableRevision = currentSessionErrorRevision();
    }

    void shutdownRuntime() noexcept
    {
        // The run stops being active before anything is torn down, so a callback that belongs to this
        // run can no longer change the client count or publish a notification - including the last
        // one, which the caller publishes itself after the teardown.
        if (runActive)
            runActive->store(false, std::memory_order_release);

        if (!session) {
            inputSink.reset();
            return;
        }

        // Session becomes quiescent before the target input sink discards pending work and balances
        // already-delivered held state. This lifecycle ordering is shared by every integration frontend.
        session->stop();
        if (inputSink)
            inputSink->shutdown();
        session.reset();
        inputSink.reset();
        resetErrorAcknowledgement();
    }

    // ---------------------------------------------------------------- #259 publication of the truth
    //
    // Every notification is a snapshot of the Runtime truth, taken after the Core has already handled
    // whatever caused it. That is what makes a stale event harmless: a callback belonging to a previous
    // run either is inert (its activity token was cleared) or publishes the *current* truth, and the
    // sink drops a value the consumers already hold. A queued notification can therefore never
    // overwrite a newer run's values, which is the property the preflight froze as "stale generation
    // rejected".

    AccessState projectedState() const
    {
        if (transitionState)
            return *transitionState;
        if (!session) {
            // No Session, but the user still asked to be reachable: there is simply no usable address for the
            // interface right now. Anything non-recoverable still arrives as Faulted from the Session itself, so
            // that state keeps meaning what it always meant.
            return desiredActive ? AccessState::Unavailable : AccessState::Stopped;
        }
        return mapState(session->state());
    }

    std::optional<Error> effectiveError() const
    {
        if (session) {
            if (const std::optional<hyremote::SessionError> coreError = session->lastError()) {
                if (!isAcknowledgedRecoverableError(*coreError))
                    return mapError(*coreError);
            }
        }
        return error;
    }

    // Moves only when the Session actually produced a failure, so a second real occurrence of the same
    // recoverable error is still delivered after the first one was acknowledged (see publishSnapshot).
    std::uint64_t failureRevision() const
    {
        if (!session)
            return 0;
        const hyremote::SessionStats stats = session->stats();
        return stats.inputPostFailures + stats.captureEventsNonRecoverable + stats.transportEventsFatal;
    }

    void publishSnapshot()
    {
        const std::uint64_t revision = failureRevision();
        const bool newErrorOccurrence = revision != lastFailureRevision;
        lastFailureRevision = revision;

        // Order: client count, error, state. An observer that receives a state already holds the value
        // that explains it, which is the ordering #259 freezes (a Faulted/Stopped notification arrives
        // with a meaningful lastError).
        notifications.publishConnectedClientCount(connectedClients->load(std::memory_order_relaxed));
        notifications.publishError(effectiveError(), newErrorOccurrence);
        notifications.publishState(projectedState());
    }

    void publishSnapshotNoexcept() noexcept
    {
        try {
            publishSnapshot();
        } catch (...) {
            // Publishing is diagnostics; it must never break a lifecycle transition or a teardown.
        }
    }
};

AccessInstance::AccessInstance(QObject *target)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->target = target;
}

AccessInstance::~AccessInstance()
{
    stop();
}

AccessInstance::AccessInstance(AccessInstance &&) noexcept = default;
AccessInstance &AccessInstance::operator=(AccessInstance &&) noexcept = default;

QObject *AccessInstance::target() const noexcept
{
    return m_impl ? m_impl->target.data() : nullptr;
}

bool AccessInstance::setTarget(QObject *target)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->target = target;
    return true;
}

QHostAddress AccessInstance::listenAddress() const
{
    return m_impl ? m_impl->listenAddress : QHostAddress{};
}

bool AccessInstance::setListenAddress(const QHostAddress &address)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    // The listener contract is IPv4 (#174): the wildcard, one explicit local IPv4, or an interface. Anything else
    // is refused here, at the one place every frontend goes through, rather than being rediscovered per frontend.
    if (address.isNull() || address.protocol() != QAbstractSocket::IPv4Protocol) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         address.isNull()
                             ? QStringLiteral("listen address must not be null")
                             : QStringLiteral("listen address must be an IPv4 address"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->listenAddress = address;
    // Setting an address is a statement about the address, so it replaces any interface selection: the two
    // outrank each other in no other way, and silently keeping both would make the mode ambiguous.
    m_impl->listenInterface.clear();
    return true;
}

QString AccessInstance::listenInterface() const
{
    return m_impl ? m_impl->listenInterface : QString{};
}

bool AccessInstance::setListenInterface(const QString &identity)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (identity.isEmpty()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen interface must not be empty; set an address to use the address modes"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    // The identity is recorded as configuration. It is deliberately not resolved here: the current address is an
    // observation, not a setting, and the whole point of interface mode is that it may change while the
    // configuration does not (#174).
    m_impl->listenInterface = identity;
    return true;
}

quint16 AccessInstance::port() const noexcept
{
    return m_impl ? m_impl->port : 0;
}

bool AccessInstance::setPort(quint16 port)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (port == 0) {
        // A version-neutral, factual statement: the rejected value, the accepted range and nothing about a release
        // label. A retired planning label has no place in a user-visible product error.
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("port 0 is invalid: a configured listener port must be between 1 and 65535"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->port = port;
    return true;
}

bool AccessInstance::remoteInputEnabled() const noexcept
{
    return m_impl && m_impl->remoteInputEnabled;
}

bool AccessInstance::setRemoteInputEnabled(bool enabled)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->remoteInputEnabled = enabled;
    return true;
}

SecurityProfile AccessInstance::securityProfile() const noexcept
{
    return m_impl ? m_impl->securityProfile : SecurityProfile::Insecure;
}

bool AccessInstance::setSecurityProfile(SecurityProfile profile)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityProfile = profile;
    return true;
}

QString AccessInstance::securityConfigFile() const
{
    return m_impl ? m_impl->securityConfigFile : QString{};
}

bool AccessInstance::setSecurityConfigFile(const QString &path)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityConfigFile = path;
    return true;
}

bool AccessInstance::start()
{
    if (!m_impl)
        return false;

    // #259: every exit from this call publishes the resulting truth exactly once. The guard clears the
    // projected Starting state and publishes whatever the call produced, so no early return has to
    // remember to notify, and the frozen failure order (ErrorChanged, then StateChanged(Stopped))
    // falls out of the snapshot itself instead of being repeated in every branch.
    struct ExitPublication
    {
        Impl *impl;

        ~ExitPublication()
        {
            impl->transitionState.reset();
            impl->publishSnapshotNoexcept();
        }
    } exitPublication{m_impl.get()};

    if (!m_impl->isConfigurable()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("runtime start requires the Stopped state"));
        return false;
    }

    m_impl->transitionState = AccessState::Starting;
    m_impl->lastFailureRevision = 0;
    m_impl->error.reset();
    m_impl->resetErrorAcknowledgement();
    m_impl->publishSnapshotNoexcept();  // StateChanged(Starting)

    // #174 listener contract. The mode is decided from configuration, and the effective IPv4 is settled here -
    // before any transport exists - so an address that does not belong to this host, an unknown interface, an
    // interface without a usable IPv4 or an ambiguous one all fail without leaving a half-composed listener. None
    // of these paths fall back: not to the wildcard, not to loopback, not to another interface.
    QHostAddress effectiveAddress = m_impl->listenAddress;
    const detail::ListenerMode bindingMode =
        detail::listenerModeFor(m_impl->listenAddress, m_impl->listenInterface);
    if (bindingMode == detail::ListenerMode::Address
        && !detail::isLocalIpv4Address(m_impl->listenAddress)) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen address %1 does not belong to this host")
                             .arg(m_impl->listenAddress.toString()));
        return false;
    }
    if (bindingMode == detail::ListenerMode::Interface) {
        const detail::InterfaceResolutionResult resolution =
            detail::resolveInterfaceAddress(m_impl->listenInterface);
        if (resolution.status != detail::InterfaceResolution::Found) {
            m_impl->setError(ErrorCode::TransportUnavailable, resolution.error);
            return false;
        }
        effectiveAddress = resolution.address;
    }

    detail::RfbSecurityConfig transportSecurity;
    bool authenticationEnabled = false;
    if (m_impl->securityProfile != SecurityProfile::Insecure) {
        // AuthenticatedEncrypted is a declared product API, but its wire contract (VeNCrypt 0.2 + X509Vnc +
        // TLS >= 1.2) does not exist yet. It therefore has to fail closed deterministically and
        // *independently of the transport-security capability*: a build that can perform VNC Authentication
        // must not accept the encrypted profile and then serve the weaker mechanism, and a readable
        // certificate/private-key descriptor must not make the profile look implemented. Descriptor validity
        // and backend capability are separate facts; this one is about the backend. The refusal happens here,
        // before any listener or transport is composed, and it never falls back to Authenticated or Insecure.
        if (m_impl->securityProfile == SecurityProfile::AuthenticatedEncrypted) {
            m_impl->setError(
                ErrorCode::SecurityUnavailable,
                QStringLiteral("AuthenticatedEncrypted requires the final VeNCrypt/TLS transport, which this "
                               "release does not provide; refusing to start instead of falling back to a weaker "
                               "listener"));
            return false;
        }

        if (m_impl->securityConfigFile.trimmed().isEmpty()) {
            m_impl->setError(ErrorCode::SecurityUnavailable,
                             QStringLiteral("the selected security profile requires a security descriptor"));
            return false;
        }

#ifndef HYREMOTE_HAS_TRANSPORT_SECURITY
        m_impl->setError(ErrorCode::SecurityUnavailable,
                         QStringLiteral("the selected security profile needs the transport-security capability, "
                                        "which is not compiled into this build"));
        return false;
#else
        QString descriptorError;
        std::optional<detail::SecurityDescriptor> descriptor =
            detail::loadSecurityDescriptor(m_impl->securityConfigFile, m_impl->securityProfile, descriptorError);
        if (!descriptor) {
            m_impl->setError(ErrorCode::InvalidConfiguration,
                             descriptorError.isEmpty()
                                 ? QStringLiteral("the security descriptor could not be loaded")
                                 : descriptorError);
            return false;
        }

        authenticationEnabled = true;
        transportSecurity.vncAuthenticationRequired = true;
        transportSecurity.password = descriptor->password;
#endif
    }

    // There is deliberately no "unauthenticated implies loopback" rule here any more. It was a product
    // restriction that no longer matches the decision in #143/#174: a BASIC_TRUSTED_LAN build is allowed to listen
    // on the LAN, and what keeps that honest is the reported security state and the documentation, not a silently
    // narrowed bind. Encrypting or authenticating the stream is a separate question and stays where it is.

    m_impl->activeTransportSecurity = transportSecurity;

    if (!m_impl->composeAndStart(effectiveAddress, transportSecurity))
        return false;

    // Only now is the user's intent to be reachable recorded, so a start that failed anywhere above leaves no
    // desired-active state behind - and therefore no watcher that could quietly start a listener seconds later.
    // From here the configuration is fixed until stop(): the listener is either running, or waiting for the
    // interface it was told to use.
    m_impl->desiredActive = true;
    m_impl->effectiveAddress = effectiveAddress;
    if (bindingMode == detail::ListenerMode::Interface)
        m_impl->startInterfaceWatcher();
    return true;
}

void AccessInstance::reconcileInterfaceBinding()
{
    if (m_impl)
        m_impl->reconcileBinding();
}

void AccessInstance::stop() noexcept
{
    if (!m_impl)
        return;

    // #174: stop() is the absolute cancellation boundary. The desired-active intent and the watcher are cleared
    // first, so a tick that is already queued - or a reconciliation a timer ran moments earlier - finds an instance
    // that no longer wants to be reachable and cannot restart anything. After this returns, only a new start() can
    // compose a Session again.
    m_impl->desiredActive = false;
    m_impl->stopInterfaceWatcher();
    m_impl->effectiveAddress.reset();
    m_impl->activeTransportSecurity.reset();

    if (!m_impl->session) {
        // Nothing is running. The state still has to be published: leaving Unavailable behind would tell the user
        // they are still waiting for an interface to come back when they have explicitly stopped.
        m_impl->transitionState.reset();
        m_impl->publishSnapshotNoexcept();
        return;
    }

    // #259 frozen stop order: Stopping, then a client count of 0 if it was not already 0, then Stopped.
    m_impl->transitionState = AccessState::Stopping;
    m_impl->publishSnapshotNoexcept();

    try {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
            if (!m_impl->isAcknowledgedRecoverableError(*coreError))
                m_impl->error = mapError(*coreError);
        }
    } catch (...) {
        // Diagnostics are best effort during noexcept teardown; quiescence is unconditional.
    }

    m_impl->shutdownRuntime();

    m_impl->transitionState.reset();
    m_impl->publishSnapshotNoexcept();
}

AccessState AccessInstance::state() const
{
    if (!m_impl)
        return AccessState::Stopped;
    // Same truth as before, except while a lifecycle call is in flight, where the projected
    // Starting/Stopping state is the one an observer was just notified about.
    return m_impl->projectedState();
}

std::size_t AccessInstance::connectedClientCount() const noexcept
{
    if (!m_impl || !m_impl->connectedClients)
        return 0;
    return m_impl->connectedClients->load(std::memory_order_relaxed);
}

std::optional<Error> AccessInstance::lastError() const
{
    if (!m_impl)
        return std::nullopt;

    return m_impl->effectiveError();
}

void AccessInstance::clearError()
{
    if (!m_impl)
        return;

    m_impl->error.reset();
    m_impl->acknowledgeCurrentRecoverableError();
    m_impl->publishSnapshotNoexcept();  // ErrorChanged(std::nullopt)
}

RuntimeNotificationToken AccessInstance::subscribeNotifications(RuntimeNotificationHandlers handlers)
{
    if (!m_impl)
        return RuntimeNotificationToken{};
    return m_impl->notifications.subscribe(std::move(handlers));
}

void AccessInstance::unsubscribeNotifications(RuntimeNotificationToken token) noexcept
{
    if (!m_impl)
        return;
    m_impl->notifications.unsubscribe(token);
}

// The single composition path (#174): the initial start, an interface rebind and a recovery all arrive here. Every
// call tears the previous run down first and then builds a fresh activity token and a fresh Session, so the old token
// is never revived and a late callback from the previous listener or capture cannot change the new run's client count
// or publish on its behalf. At most one Session is alive at any moment.
bool AccessInstance::Impl::composeAndStart(const QHostAddress &address, const detail::RfbSecurityConfig &security)
{
    shutdownRuntime();

    QObject *targetObject = target.data();
    if (targetObject == nullptr) {
        setError(ErrorCode::InvalidConfiguration, QStringLiteral("no live Qt target is attached"));
        return false;
    }

    detail::TargetComponents targetComponents = detail::createTargetComponents(targetObject, remoteInputEnabled);
    if (!targetComponents.supported || !targetComponents.capture) {
        const QString message = targetComponents.error.isEmpty()
                                    ? QStringLiteral("no HyRemote adapter supports the attached Qt target")
                                    : targetComponents.error;
        setError(ErrorCode::TargetAdapterUnavailable, message);
        return false;
    }

    if (remoteInputEnabled && !targetComponents.input) {
        setError(ErrorCode::RemoteInputUnavailable,
                 targetComponents.error.isEmpty()
                     ? QStringLiteral("remote input was enabled but this target adapter has no input sink")
                     : targetComponents.error);
        return false;
    }

    detail::TransportComponent transport = detail::createDefaultTransport(address, port, security);
    if (!transport.transport) {
        const QString message = transport.error.isEmpty()
                                    ? QStringLiteral("no default HyRemote transport is available")
                                    : transport.error;
        setError(ErrorCode::TransportUnavailable, message);
        return false;
    }

    // The run's activity token is created before the components are composed so both wrappers can hold it. It is
    // activated immediately before the Session starts and cleared by every teardown.
    runActive = std::make_shared<std::atomic<bool>>(false);
    const auto observeRuntime = [impl = this] { impl->publishSnapshotNoexcept(); };

    transport.transport = std::make_unique<ClientCountingTransport>(std::move(transport.transport),
                                                                    connectedClients,
                                                                    runActive,
                                                                    observeRuntime);

    auto freshSession = std::make_unique<hyremote::Session>();
    auto observedCapture =
        std::make_unique<ObservedCaptureSource>(std::move(targetComponents.capture), runActive, observeRuntime);
    if (!freshSession->setCaptureSource(std::move(observedCapture))
        || !freshSession->setTransport(std::move(transport.transport))) {
        setError(ErrorCode::RuntimeFailure, QStringLiteral("failed to compose the internal HyRemote session"));
        return false;
    }

    std::shared_ptr<hyremote::InputSink> freshInputSink = targetComponents.input;
    if (freshInputSink)
        freshSession->setInputSink(freshInputSink);

    runActive->store(true, std::memory_order_release);
    if (!freshSession->start()) {
        runActive->store(false, std::memory_order_release);
        if (const std::optional<hyremote::SessionError> coreError = freshSession->lastError())
            error = mapError(*coreError);
        else
            setError(ErrorCode::StartFailed, QStringLiteral("HyRemote runtime failed to start"));

        freshSession->stop();
        return false;
    }

    inputSink = std::move(freshInputSink);
    session = std::move(freshSession);
    return true;
}

bool AccessInstance::Impl::rebindTo(const QHostAddress &address)
{
    // A real composition, not a patch: the configured identity is untouched and only the effective endpoint moves,
    // with the security the current run was composed with.
    transitionState = AccessState::Starting;
    lastFailureRevision = 0;
    error.reset();
    resetErrorAcknowledgement();
    publishSnapshotNoexcept();  // StateChanged(Starting)

    const detail::RfbSecurityConfig security =
        activeTransportSecurity ? *activeTransportSecurity : detail::RfbSecurityConfig{};

    if (composeAndStart(address, security)) {
        effectiveAddress = address;
        error.reset();
    } else {
        // The interface answered but a listener for it could not be composed. That is reported as unavailable
        // rather than guessed at, and the watcher keeps the same identity so the next tick retries the same
        // endpoint. Nothing here moves to another address.
        effectiveAddress.reset();
    }

    transitionState.reset();
    publishSnapshotNoexcept();
    return effectiveAddress.has_value() && *effectiveAddress == address;
}

void AccessInstance::Impl::reconcileBinding()
{
    // Everything below is a no-op unless an interface listener is actually wanted, which is what makes a tick that
    // arrives after stop() harmless.
    if (!desiredActive)
        return;
    if (detail::listenerModeFor(listenAddress, listenInterface) != detail::ListenerMode::Interface)
        return;

    const detail::InterfaceResolutionResult resolution = detail::resolveInterfaceAddress(listenInterface);

    if (resolution.status == detail::InterfaceResolution::Found) {
        if (effectiveAddress && *effectiveAddress == resolution.address)
            return;  // A -> A: nothing changed, so nothing is torn down
        rebindTo(resolution.address);
        return;
    }

    // The interface is gone, or no longer answers with exactly one address. The listener stops - it does not keep
    // serving the address it used to have, and it does not move to the wildcard, to loopback or to another
    // interface - but the user has not stopped, so the intent stays and the watcher keeps following this identity.
    shutdownRuntime();
    effectiveAddress.reset();
    setError(ErrorCode::TransportUnavailable, resolution.error);
    publishSnapshotNoexcept();  // StateChanged(Unavailable), with the reason visible
}

void AccessInstance::Impl::startInterfaceWatcher()
{
    if (!watcher)
        watcher = std::make_unique<InterfaceWatcher>([this] { reconcileBinding(); });
    watcher->start();
}

void AccessInstance::Impl::stopInterfaceWatcher()
{
    if (watcher)
        watcher->stop();
}

}  // namespace HyRemote::Runtime
