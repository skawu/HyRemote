#pragma once

#include "access_types.hpp"

#include <QHostAddress>
#include <QString>

#include <cstddef>
#include <optional>

namespace HyRemote::Runtime {

// #335 private one-Runtime diagnostic truth. This type is intentionally not installed and is not a
// public SDK/ABI promise. Frontends may project/format this snapshot, but must not maintain their own
// copies of the represented Runtime state.
//
// Configured facts describe user intent. Effective facts are observations of a successfully running
// listener only; they are absent while stopped/starting/unavailable or after a failed start. Frontends
// must never infer effective values from configured values on their own.
struct DiagnosticSnapshot
{
    AccessState state = AccessState::Stopped;

    QHostAddress configuredListenAddress;
    QString configuredListenInterface;
    quint16 configuredPort = 0;
    std::optional<QHostAddress> effectiveListenAddress;
    std::optional<quint16> effectivePort;

    SecurityProfile configuredSecurityProfile = SecurityProfile::Insecure;
    std::optional<SecurityProfile> effectiveSecurityProfile;
    bool remoteInputEnabled = false;
    std::size_t connectedClientCount = 0;
    std::optional<Error> lastError;
};

}  // namespace HyRemote::Runtime
