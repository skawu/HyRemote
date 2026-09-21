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
