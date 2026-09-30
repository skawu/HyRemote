#include "detail/runtime_diagnostics.hpp"

#include <QStringList>

namespace HyRemote::Runtime {
namespace {

QString stateName(AccessState state)
{
    switch (state) {
    case AccessState::Stopped: return QStringLiteral("Stopped");
    case AccessState::Starting: return QStringLiteral("Starting");
    case AccessState::Running: return QStringLiteral("Running");
    case AccessState::Stopping: return QStringLiteral("Stopping");
    case AccessState::Faulted: return QStringLiteral("Faulted");
    case AccessState::Unavailable: return QStringLiteral("Unavailable");
    }
    return QStringLiteral("unknown");
}

QString securityName(SecurityProfile profile)
{
    switch (profile) {
    case SecurityProfile::Insecure: return QStringLiteral("Insecure");
    case SecurityProfile::Authenticated: return QStringLiteral("Authenticated");
    case SecurityProfile::AuthenticatedEncrypted: return QStringLiteral("AuthenticatedEncrypted");
    }
    return QStringLiteral("unknown");
}

QString errorCodeName(ErrorCode code)
{
    switch (code) {
    case ErrorCode::InvalidConfiguration: return QStringLiteral("InvalidConfiguration");
    case ErrorCode::TargetAdapterUnavailable: return QStringLiteral("TargetAdapterUnavailable");
    case ErrorCode::TransportUnavailable: return QStringLiteral("TransportUnavailable");
    case ErrorCode::RemoteInputUnavailable: return QStringLiteral("RemoteInputUnavailable");
    case ErrorCode::SecurityUnavailable: return QStringLiteral("SecurityUnavailable");
    case ErrorCode::StartFailed: return QStringLiteral("StartFailed");
    case ErrorCode::RuntimeFailure: return QStringLiteral("RuntimeFailure");
    case ErrorCode::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("unknown");
}

QString boundedOneLine(QString value)
{
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    value.replace(QLatin1Char('\t'), QLatin1Char(' '));
    constexpr qsizetype maxLength = 512;
    if (value.size() > maxLength)
        value = value.left(maxLength - 3) + QStringLiteral("...");
    return value;
}

QString configuredListener(const DiagnosticSnapshot &snapshot)
{
    if (!snapshot.configuredListenInterface.isEmpty()) {
        return QStringLiteral("interface:%1:%2")
            .arg(snapshot.configuredListenInterface)
            .arg(snapshot.configuredPort);
    }
    return QStringLiteral("address:%1:%2")
        .arg(snapshot.configuredListenAddress.toString())
        .arg(snapshot.configuredPort);
}

QString effectiveListener(const DiagnosticSnapshot &snapshot)
{
    if (!snapshot.effectiveListenAddress || !snapshot.effectivePort)
        return QStringLiteral("none");
    return QStringLiteral("%1:%2")
        .arg(snapshot.effectiveListenAddress->toString())
        .arg(*snapshot.effectivePort);
}

}  // namespace

QString formatDiagnosticReport(const DiagnosticSnapshot &snapshot)
{
    QStringList lines;
    lines.reserve(18);

    lines << QStringLiteral("HYREMOTE_VERSION=unknown")
          << QStringLiteral("BUILD_IDENTITY=unknown")
          << QStringLiteral("QT_VERSION=unknown")
          << QStringLiteral("OS=unknown")
          << QStringLiteral("ARCH=unknown")
          << QStringLiteral("INTEGRATION_ROUTE=unknown")
          << QStringLiteral("UI_FAMILY=unknown")
          << QStringLiteral("STATE=%1").arg(stateName(snapshot.state))
          << QStringLiteral("LISTENER_CONFIGURED=%1").arg(configuredListener(snapshot))
          << QStringLiteral("LISTENER_EFFECTIVE=%1").arg(effectiveListener(snapshot))
          << QStringLiteral("SECURITY_PROFILE=%1").arg(securityName(snapshot.configuredSecurityProfile))
          << QStringLiteral("SECURITY_ENABLED=%1")
                 .arg(snapshot.configuredSecurityProfile == SecurityProfile::Insecure
                          ? QStringLiteral("false")
                          : QStringLiteral("true"))
          << QStringLiteral("REMOTE_INPUT=%1")
                 .arg(snapshot.remoteInputEnabled ? QStringLiteral("true") : QStringLiteral("false"))
          << QStringLiteral("CONNECTED_CLIENTS=%1").arg(snapshot.connectedClientCount);

    if (snapshot.lastError) {
        lines << QStringLiteral("LAST_ERROR_CODE=%1").arg(errorCodeName(snapshot.lastError->code))
              << QStringLiteral("LAST_ERROR_MESSAGE=%1").arg(boundedOneLine(snapshot.lastError->message))
              << QStringLiteral("LAST_ERROR_RECOVERABLE=%1")
                     .arg(snapshot.lastError->recoverable ? QStringLiteral("true") : QStringLiteral("false"));
    } else {
        lines << QStringLiteral("LAST_ERROR_CODE=none")
              << QStringLiteral("LAST_ERROR_MESSAGE=none")
              << QStringLiteral("LAST_ERROR_RECOVERABLE=none");
    }

    lines << QStringLiteral("DEPLOYMENT_IDENTITY=unknown");
    return lines.join(QLatin1Char('\n'));
}

}  // namespace HyRemote::Runtime
