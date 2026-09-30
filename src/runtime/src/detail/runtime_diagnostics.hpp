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
// Phase 1 contains only facts already exposed by AccessInstance's existing private Runtime getters.
// Effective listener/security and build/deployment identity are added by later bounded slices from
// their authoritative owners; no frontend is allowed to infer them.
struct DiagnosticSnapshot
{
    AccessState state = AccessState::Stopped;

    QHostAddress configuredListenAddress;
    QString configuredListenInterface;
    quint16 configuredPort = 0;

    SecurityProfile configuredSecurityProfile = SecurityProfile::Insecure;
    bool remoteInputEnabled = false;
    std::size_t connectedClientCount = 0;
    std::optional<Error> lastError;
};

}  // namespace HyRemote::Runtime
