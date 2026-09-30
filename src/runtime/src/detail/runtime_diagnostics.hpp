#pragma once

#include "access_types.hpp"

#include <HyRemote/RemoteAccessExport.h>

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

// One bounded formatter shared by every integration route. It renders only the snapshot truth and
// explicit unknown/none placeholders for fields whose authoritative owners have not been wired yet.
// The symbol is exported only so peer frontend modules can call the one Runtime implementation; this
// private header remains uninstalled and therefore does not create an application SDK/ABI promise.
HYREMOTE_REMOTEACCESS_EXPORT QString formatDiagnosticReport(const DiagnosticSnapshot &snapshot);

}  // namespace HyRemote::Runtime
