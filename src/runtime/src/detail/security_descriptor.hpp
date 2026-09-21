#pragma once

#include "access_types.hpp"

#include <QByteArray>
#include <QString>

#include <optional>

namespace HyRemote::detail {

// Private material loaded from the security descriptor. Secret bytes never cross integration
// frontend surfaces. The object is move-only so password storage is not accidentally shared/copied,
// and destruction overwrites the in-memory password buffer before release.
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

// Loads and validates the descriptor and every material file required by the selected internal
// runtime profile. Public/front-end profile enums are mapped before entering this layer.
std::optional<SecurityDescriptor> loadSecurityDescriptor(const QString &descriptorFile,
                                                         Runtime::SecurityProfile profile,
                                                         QString &error);

}  // namespace HyRemote::detail
