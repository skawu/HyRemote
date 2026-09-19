#pragma once

#include <HyRemote/RemoteAccess.h>

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <utility>

namespace HyRemote::detail {

// Private hand-off from the product-level RemoteAccess configuration/provider to the one built-in
// RFB transport. This is deliberately source-private: applications, QML and QPA never receive
// password bytes, backend objects or protocol security-type identifiers.
struct TransportSecurityConfiguration
{
    RemoteSecurityProfile profile = RemoteSecurityProfile::Insecure;
    QString credentialId;
    QByteArray password;
    QString certificateFile;
    QString privateKeyFile;

    TransportSecurityConfiguration() = default;
    TransportSecurityConfiguration(const TransportSecurityConfiguration &) = delete;
    TransportSecurityConfiguration &operator=(const TransportSecurityConfiguration &) = delete;

    TransportSecurityConfiguration(TransportSecurityConfiguration &&other) noexcept
        : profile(other.profile)
        , credentialId(std::move(other.credentialId))
        , password(std::move(other.password))
        , certificateFile(std::move(other.certificateFile))
        , privateKeyFile(std::move(other.privateKeyFile))
    {
        other.profile = RemoteSecurityProfile::Insecure;
    }

    TransportSecurityConfiguration &operator=(TransportSecurityConfiguration &&other) noexcept
    {
        if (this == &other)
            return *this;
        clearSecret();
        profile = other.profile;
        credentialId = std::move(other.credentialId);
        password = std::move(other.password);
        certificateFile = std::move(other.certificateFile);
        privateKeyFile = std::move(other.privateKeyFile);
        other.profile = RemoteSecurityProfile::Insecure;
        return *this;
    }

    ~TransportSecurityConfiguration() { clearSecret(); }

    void clearSecret() noexcept
    {
        if (!password.isEmpty())
            std::fill(password.begin(), password.end(), '\0');
        password.clear();
    }
};

}  // namespace HyRemote::detail
