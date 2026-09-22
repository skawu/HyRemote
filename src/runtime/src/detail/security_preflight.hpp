#pragma once

#include "transport/rfb_transport.hpp"

#include <QString>

namespace HyRemote::detail {

// Pre-listen preparation for a secure transport profile.
//
// A secure profile must never be discovered to be unusable *after* a listener exists, so this runs before the
// listener is created: it selects the TLS backend, loads the credential material and proves the certificate and key
// belong together. On success the (moved-from-safe) config carries the loaded material, and the transport never has to
// touch the file system again.
//
// `error` is a user-facing diagnostic: it describes which input is wrong, never its content.
struct SecureTransportPreparation
{
    bool ok = false;

    // True when the failure means "this build/host cannot provide the profile" (reported as SecurityUnavailable)
    // rather than "the configuration is wrong" (reported as InvalidConfiguration).
    bool unavailable = false;

    QString error;

    // Non-secret label of the credential in use, for diagnostics that must not carry secrets.
    QString credentialId;
};

// Only `VeNCryptTlsVncAuth` is prepared here; the other profiles need no TLS backend and are returned as ok.
SecureTransportPreparation prepareSecureTransport(RfbSecurityConfig &config);

}  // namespace HyRemote::detail
