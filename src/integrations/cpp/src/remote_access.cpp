#include <HyRemote/RemoteAccess.h>

#include "access_instance.hpp"

#include <utility>

namespace HyRemote {
namespace {

RemoteAccessState mapState(Runtime::AccessState state)
{
    switch (state) {
    case Runtime::AccessState::Stopped:
        return RemoteAccessState::Stopped;
    case Runtime::AccessState::Starting:
        return RemoteAccessState::Starting;
    case Runtime::AccessState::Running:
        return RemoteAccessState::Running;
    case Runtime::AccessState::Stopping:
        return RemoteAccessState::Stopping;
    case Runtime::AccessState::Faulted:
        return RemoteAccessState::Faulted;
    case Runtime::AccessState::Unavailable:
        return RemoteAccessState::Unavailable;
    }
    return RemoteAccessState::Faulted;
}

Runtime::SecurityProfile mapSecurityProfile(RemoteSecurityProfile profile)
{
    switch (profile) {
    case RemoteSecurityProfile::Insecure:
        return Runtime::SecurityProfile::Insecure;
    case RemoteSecurityProfile::Authenticated:
        return Runtime::SecurityProfile::Authenticated;
    case RemoteSecurityProfile::AuthenticatedEncrypted:
        return Runtime::SecurityProfile::AuthenticatedEncrypted;
    }
    return Runtime::SecurityProfile::Insecure;
}

RemoteSecurityProfile mapSecurityProfile(Runtime::SecurityProfile profile)
{
    switch (profile) {
    case Runtime::SecurityProfile::Insecure:
        return RemoteSecurityProfile::Insecure;
    case Runtime::SecurityProfile::Authenticated:
        return RemoteSecurityProfile::Authenticated;
    case Runtime::SecurityProfile::AuthenticatedEncrypted:
        return RemoteSecurityProfile::AuthenticatedEncrypted;
    }
    return RemoteSecurityProfile::Insecure;
}

RemoteAccessErrorCode mapErrorCode(Runtime::ErrorCode code)
{
    switch (code) {
    case Runtime::ErrorCode::InvalidConfiguration:
        return RemoteAccessErrorCode::InvalidConfiguration;
    case Runtime::ErrorCode::TargetAdapterUnavailable:
        return RemoteAccessErrorCode::TargetAdapterUnavailable;
    case Runtime::ErrorCode::TransportUnavailable:
        return RemoteAccessErrorCode::TransportUnavailable;
    case Runtime::ErrorCode::RemoteInputUnavailable:
        return RemoteAccessErrorCode::RemoteInputUnavailable;
    case Runtime::ErrorCode::SecurityUnavailable:
        return RemoteAccessErrorCode::SecurityUnavailable;
    case Runtime::ErrorCode::StartFailed:
        return RemoteAccessErrorCode::StartFailed;
    case Runtime::ErrorCode::RuntimeFailure:
        return RemoteAccessErrorCode::RuntimeFailure;
    case Runtime::ErrorCode::Cancelled:
        return RemoteAccessErrorCode::Cancelled;
    }
    return RemoteAccessErrorCode::RuntimeFailure;
}

RemoteAccessError mapError(const Runtime::Error &error)
{
    return RemoteAccessError{mapErrorCode(error.code), error.message, error.recoverable};
}

}  // namespace

struct RemoteAccess::Impl
{
    explicit Impl(QObject *target)
        : access(target)
    {
    }

    Runtime::AccessInstance access;
};

RemoteAccess::RemoteAccess(QObject *target)
    : m_impl(std::make_unique<Impl>(target))
{
}

RemoteAccess::~RemoteAccess()
{
    stop();
}

RemoteAccess::RemoteAccess(RemoteAccess &&) noexcept = default;
RemoteAccess &RemoteAccess::operator=(RemoteAccess &&) noexcept = default;

QObject *RemoteAccess::target() const noexcept
{
    return m_impl ? m_impl->access.target() : nullptr;
}

bool RemoteAccess::setTarget(QObject *target)
{
    return m_impl && m_impl->access.setTarget(target);
}

QHostAddress RemoteAccess::listenAddress() const
{
    return m_impl ? m_impl->access.listenAddress() : QHostAddress{};
}

bool RemoteAccess::setListenAddress(const QHostAddress &address)
{
    return m_impl && m_impl->access.setListenAddress(address);
}

QString RemoteAccess::listenInterface() const
{
    return m_impl ? m_impl->access.listenInterface() : QString{};
}

bool RemoteAccess::setListenInterface(const QString &identity)
{
    return m_impl && m_impl->access.setListenInterface(identity);
}

quint16 RemoteAccess::port() const noexcept
{
    return m_impl ? m_impl->access.port() : 0;
}

bool RemoteAccess::setPort(quint16 port)
{
    return m_impl && m_impl->access.setPort(port);
}

bool RemoteAccess::remoteInputEnabled() const noexcept
{
    return m_impl && m_impl->access.remoteInputEnabled();
}

bool RemoteAccess::setRemoteInputEnabled(bool enabled)
{
    return m_impl && m_impl->access.setRemoteInputEnabled(enabled);
}

RemoteSecurityProfile RemoteAccess::securityProfile() const noexcept
{
    return m_impl ? mapSecurityProfile(m_impl->access.securityProfile()) : RemoteSecurityProfile::Insecure;
}

bool RemoteAccess::setSecurityProfile(RemoteSecurityProfile profile)
{
    return m_impl && m_impl->access.setSecurityProfile(mapSecurityProfile(profile));
}

QString RemoteAccess::securityConfigFile() const
{
    return m_impl ? m_impl->access.securityConfigFile() : QString{};
}

bool RemoteAccess::setSecurityConfigFile(const QString &path)
{
    return m_impl && m_impl->access.setSecurityConfigFile(path);
}

bool RemoteAccess::start()
{
    return m_impl && m_impl->access.start();
}

void RemoteAccess::stop() noexcept
{
    if (m_impl)
        m_impl->access.stop();
}

RemoteAccessState RemoteAccess::state() const
{
    return m_impl ? mapState(m_impl->access.state()) : RemoteAccessState::Stopped;
}

std::size_t RemoteAccess::connectedClientCount() const noexcept
{
    return m_impl ? m_impl->access.connectedClientCount() : 0;
}

std::optional<RemoteAccessError> RemoteAccess::lastError() const
{
    if (!m_impl)
        return std::nullopt;
    const std::optional<Runtime::Error> error = m_impl->access.lastError();
    return error ? std::optional<RemoteAccessError>{mapError(*error)} : std::nullopt;
}

void RemoteAccess::clearError()
{
    if (m_impl)
        m_impl->access.clearError();
}

}  // namespace HyRemote
