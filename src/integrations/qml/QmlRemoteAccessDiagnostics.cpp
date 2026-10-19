#include "QmlRemoteAccess.h"

#include "access_instance.hpp"
#include "detail/runtime_diagnostics.hpp"

namespace HyRemote::Qml {

QString QmlRemoteAccess::diagnosticReport() const
{
    ::HyRemote::Runtime::DiagnosticSnapshot snapshot =
        m_access ? m_access->diagnosticSnapshot() : ::HyRemote::Runtime::DiagnosticSnapshot{};
    snapshot.integrationRoute = ::HyRemote::Runtime::IntegrationRoute::Qml;
    return ::HyRemote::Runtime::formatDiagnosticReport(snapshot);
}

}  // namespace HyRemote::Qml
