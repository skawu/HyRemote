#pragma once

#include "transport/rfb_transport.hpp"

#include <QString>

#include <HyRemote/RemoteAccessExport.h>

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

// The private source-tree declaration is exported only in a test-enabled build, so repository tests can link
// against the shared runtime DLL on Windows. It is not installed and not part of any SDK surface.
#if defined(HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS)
#  define HYREMOTE_SECURITY_PREFLIGHT_EXPORT HYREMOTE_REMOTEACCESS_EXPORT
#else
#  define HYREMOTE_SECURITY_PREFLIGHT_EXPORT
#endif

// Only `VeNCryptTlsVncAuth` is prepared here; the other profiles need no TLS backend and are returned as ok.
HYREMOTE_SECURITY_PREFLIGHT_EXPORT SecureTransportPreparation prepareSecureTransport(RfbSecurityConfig &config);

#undef HYREMOTE_SECURITY_PREFLIGHT_EXPORT

}  // namespace HyRemote::detail
