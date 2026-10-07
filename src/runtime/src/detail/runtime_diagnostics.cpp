#include "detail/runtime_diagnostics.hpp"

#include <QtGlobal>

#include <QStringList>

namespace HyRemote::Runtime {
namespace {

QString integrationRouteName(IntegrationRoute route)
{
    switch (route) {
    case IntegrationRoute::Unknown: return QStringLiteral("unknown");
    case IntegrationRoute::Cpp: return QStringLiteral("cpp");
    case IntegrationRoute::Qml: return QStringLiteral("qml");
    case IntegrationRoute::Generic: return QStringLiteral("generic");
    case IntegrationRoute::Qpa: return QStringLiteral("qpa");
    }
    return QStringLiteral("unknown");
}

QString uiFamilyName(UiFamily family)
{
    switch (family) {
    case UiFamily::Unknown: return QStringLiteral("unknown");
    case UiFamily::Widgets: return QStringLiteral("widgets");
    case UiFamily::Quick: return QStringLiteral("quick");
    case UiFamily::Mixed: return QStringLiteral("mixed");
    }
    return QStringLiteral("unknown");
}

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

QString hyremoteVersion()
{
    const QString version = QString::fromLatin1(HYREMOTE_DIAGNOSTIC_VERSION);
    return version == QStringLiteral("0.0.0") || version.isEmpty() ? QStringLiteral("unknown") : version;
}

QString buildIdentity()
{
    const QString sourceSha = QString::fromLatin1(HYREMOTE_DIAGNOSTIC_SOURCE_SHA);
    const QString buildType = QString::fromLatin1(HYREMOTE_DIAGNOSTIC_BUILD_TYPE);
    if (sourceSha.isEmpty() || sourceSha == QStringLiteral("unknown"))
        return QStringLiteral("unknown");
    return QStringLiteral("source_sha=%1,build_type=%2")
        .arg(sourceSha, buildType.isEmpty() ? QStringLiteral("unknown") : buildType);
}

QString buildFact(const char *value)
{
    const QString fact = QString::fromLatin1(value);
    return fact.isEmpty() ? QStringLiteral("unknown") : fact;
}

}  // namespace

QString formatDiagnosticReport(const DiagnosticSnapshot &snapshot)
{
    QStringList lines;
    lines.reserve(18);

    lines << QStringLiteral("HYREMOTE_VERSION=%1").arg(hyremoteVersion())
          << QStringLiteral("BUILD_IDENTITY=%1").arg(buildIdentity())
          << QStringLiteral("QT_VERSION=%1").arg(QString::fromLatin1(QT_VERSION_STR))
          << QStringLiteral("OS=%1").arg(buildFact(HYREMOTE_DIAGNOSTIC_OS))
          << QStringLiteral("ARCH=%1").arg(buildFact(HYREMOTE_DIAGNOSTIC_ARCH))
          << QStringLiteral("INTEGRATION_ROUTE=%1").arg(integrationRouteName(snapshot.integrationRoute))
          << QStringLiteral("UI_FAMILY=%1").arg(uiFamilyName(snapshot.uiFamily))
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
