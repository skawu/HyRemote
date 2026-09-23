#pragma once

#include <QString>

namespace HyRemote::Runtime {

// Internal product-runtime vocabulary shared by integration frontends. These types deliberately do
// not reuse the Embedded C++ facade enums: frontends map their public vocabulary onto this private
// contract so no integration frontend becomes the architectural parent of another.
enum class AccessState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Faulted,
    // #174: the user asked to be reachable and the selected interface currently has no single usable IPv4 address.
    // There is no listener and no Session, but this is neither of the two states it could be confused with:
    // not Stopped, because the user has not stopped and a watcher is still following the same identity; and not
    // Faulted, because nothing failed irrecoverably and the listener returns by itself once the interface answers
    // again. Appended rather than inserted so the existing numeric values keep their meaning for observers.
    Unavailable,
};

enum class SecurityProfile {
    Insecure,
    Authenticated,
    AuthenticatedEncrypted,
};

enum class ErrorCode {
    InvalidConfiguration,
    TargetAdapterUnavailable,
    TransportUnavailable,
    RemoteInputUnavailable,
    SecurityUnavailable,
    StartFailed,
    RuntimeFailure,
    Cancelled,
};

struct Error
{
    ErrorCode code = ErrorCode::RuntimeFailure;
    QString message;
    bool recoverable = false;
};

}  // namespace HyRemote::Runtime
