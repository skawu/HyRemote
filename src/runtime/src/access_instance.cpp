#include "access_instance.hpp"

#include <QObject>
#include <QPointer>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "detail/component_factories.hpp"
#include "detail/security_descriptor.hpp"
#include "detail/security_preflight.hpp"
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

struct AccessInstance::Impl
{
    struct SessionErrorRevision
    {
        std::uint64_t inputPostFailures = 0;
    };

    QPointer<QObject> target;
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;
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

    ~Impl() { shutdownRuntime(); }

    bool isConfigurable() const
    {
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
        if (!session)
            return AccessState::Stopped;
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
    if (address.isNull()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen address must not be null"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->listenAddress = address;
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
        transportSecurity.password = descriptor->password;

        // The private profile is closed, so a frontend profile maps onto exactly one wire profile.
        if (m_impl->securityProfile == SecurityProfile::AuthenticatedEncrypted) {
            // Prepared exactly as the final implementation prepares it - explicit OpenSSL backend selection,
            // readable material, matching certificate and key - so these pre-listen guarantees cannot be bypassed
            // when the VeNCrypt/TLS wire slice lands, and none of them can be satisfied after a listener exists.
            transportSecurity.profile = detail::RfbSecurityProfile::VeNCryptTlsVncAuth;
            transportSecurity.certificateFile = descriptor->certificateFile;
            transportSecurity.privateKeyFile = descriptor->privateKeyFile;

            const detail::SecureTransportPreparation preparation =
                detail::prepareSecureTransport(transportSecurity);
            if (!preparation.ok) {
                m_impl->setError(preparation.unavailable ? ErrorCode::SecurityUnavailable
                                                        : ErrorCode::InvalidConfiguration,
                                 preparation.error);
                return false;
            }
        } else {
            transportSecurity.profile = detail::RfbSecurityProfile::VncAuth;
        }
#endif
    }

    if (!authenticationEnabled && !m_impl->listenAddress.isLoopback()) {
        m_impl->setError(
            ErrorCode::InvalidConfiguration,
            QStringLiteral("refusing a non-loopback listener while no authentication mode is enabled"));
        return false;
    }

    QObject *targetObject = m_impl->target.data();
    if (targetObject == nullptr) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("no live Qt target is attached"));
        return false;
    }

    detail::TargetComponents targetComponents =
        detail::createTargetComponents(targetObject, m_impl->remoteInputEnabled);
    if (!targetComponents.supported || !targetComponents.capture) {
        const QString message = targetComponents.error.isEmpty()
                                    ? QStringLiteral("no HyRemote adapter supports the attached Qt target")
                                    : targetComponents.error;
        m_impl->setError(ErrorCode::TargetAdapterUnavailable, message);
        return false;
    }

    if (m_impl->remoteInputEnabled && !targetComponents.input) {
        m_impl->setError(ErrorCode::RemoteInputUnavailable,
                         targetComponents.error.isEmpty()
                             ? QStringLiteral("remote input was enabled but this target adapter has no input sink")
                             : targetComponents.error);
        return false;
    }

    detail::TransportComponent transport =
        detail::createDefaultTransport(m_impl->listenAddress, m_impl->port, transportSecurity);
    if (!transport.transport) {
        const QString message = transport.error.isEmpty()
                                    ? QStringLiteral("no default HyRemote transport is available")
                                    : transport.error;
        m_impl->setError(ErrorCode::TransportUnavailable, message);
        return false;
    }

    // The run's activity token is created before the components are composed so both wrappers can hold
    // it. It is activated immediately before the Session starts and cleared by every teardown.
    m_impl->runActive = std::make_shared<std::atomic<bool>>(false);
    const auto observeRuntime = [impl = m_impl.get()] { impl->publishSnapshotNoexcept(); };

    transport.transport = std::make_unique<ClientCountingTransport>(std::move(transport.transport),
                                                                    m_impl->connectedClients,
                                                                    m_impl->runActive,
                                                                    observeRuntime);

    auto session = std::make_unique<hyremote::Session>();
    auto observedCapture = std::make_unique<ObservedCaptureSource>(
        std::move(targetComponents.capture), m_impl->runActive, observeRuntime);
    if (!session->setCaptureSource(std::move(observedCapture))
        || !session->setTransport(std::move(transport.transport))) {
        m_impl->setError(ErrorCode::RuntimeFailure,
                         QStringLiteral("failed to compose the internal HyRemote session"));
        return false;
    }

    std::shared_ptr<hyremote::InputSink> inputSink = targetComponents.input;
    if (inputSink)
        session->setInputSink(inputSink);

    m_impl->runActive->store(true, std::memory_order_release);
    if (!session->start()) {
        m_impl->runActive->store(false, std::memory_order_release);
        const std::optional<hyremote::SessionError> coreError = session->lastError();
        if (coreError)
            m_impl->error = mapError(*coreError);
        else
            m_impl->setError(ErrorCode::StartFailed,
                             QStringLiteral("HyRemote runtime failed to start"));

        session->stop();
        return false;
    }

    m_impl->inputSink = std::move(inputSink);
    m_impl->session = std::move(session);
    return true;
}

void AccessInstance::stop() noexcept
{
    if (!m_impl || !m_impl->session)
        return;

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

}  // namespace HyRemote::Runtime
