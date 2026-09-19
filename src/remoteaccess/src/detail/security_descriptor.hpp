#pragma once

#include <HyRemote/RemoteAccess.h>

#include <QByteArray>
#include <QString>

#include <optional>

namespace HyRemote::detail {

// Private material loaded from the V1 security descriptor. Secret bytes never cross the public
// RemoteAccess/QML/QPA surfaces. The object is move-only so password storage is not accidentally
// shared/copied, and destruction overwrites the in-memory password buffer before release.
struct SecurityDescriptor
{
    QString credentialId;
    QByteArray password;
    QString certificateFile;
    QString privateKeyFile;

    SecurityDescriptor() = default;
    SecurityDescriptor(const SecurityDescriptor &) = delete;
    SecurityDescriptor &operator=(const SecurityDescriptor &) = delete;
    SecurityDescriptor(SecurityDescriptor &&other) noexcept;
    SecurityDescriptor &operator=(SecurityDescriptor &&other) noexcept;
    ~SecurityDescriptor();

    void clearSecret() noexcept;
};

// Loads and validates the descriptor and every material file required by the selected product
// profile. The parser is deliberately strict and bounded. On failure, `error` contains only
// non-secret configuration diagnostics; password/private-key contents are never included.
std::optional<SecurityDescriptor> loadSecurityDescriptor(const QString &descriptorFile,
                                                         RemoteSecurityProfile profile,
                                                         QString &error);

}  // namespace HyRemote::detail
