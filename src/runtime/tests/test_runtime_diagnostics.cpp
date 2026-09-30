#include "detail/runtime_diagnostics.hpp"

#include <QCoreApplication>

#include <iostream>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

using HyRemote::Runtime::AccessState;
using HyRemote::Runtime::DiagnosticSnapshot;
using HyRemote::Runtime::Error;
using HyRemote::Runtime::ErrorCode;
using HyRemote::Runtime::SecurityProfile;
using HyRemote::Runtime::formatDiagnosticReport;

void checkDefaultReport()
{
    DiagnosticSnapshot snapshot;
    snapshot.configuredListenAddress = QHostAddress::AnyIPv4;
    snapshot.configuredPort = 5920;

    const QString report = formatDiagnosticReport(snapshot);
    CHECK(report.contains(QStringLiteral("HYREMOTE_VERSION=unknown\n")));
    CHECK(report.contains(QStringLiteral("STATE=Stopped\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_CONFIGURED=address:0.0.0.0:5920\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_EFFECTIVE=none\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_PROFILE=Insecure\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_ENABLED=false\n")));
    CHECK(report.contains(QStringLiteral("REMOTE_INPUT=false\n")));
    CHECK(report.contains(QStringLiteral("CONNECTED_CLIENTS=0\n")));
    CHECK(report.contains(QStringLiteral("LAST_ERROR_CODE=none\n")));
    CHECK(report.endsWith(QStringLiteral("DEPLOYMENT_IDENTITY=unknown")));
}

void checkRunningReport()
{
    DiagnosticSnapshot snapshot;
    snapshot.state = AccessState::Running;
    snapshot.configuredListenInterface = QStringLiteral("eth0");
    snapshot.configuredPort = 5921;
    snapshot.effectiveListenAddress = QHostAddress(QStringLiteral("192.0.2.10"));
    snapshot.effectivePort = 5921;
    snapshot.configuredSecurityProfile = SecurityProfile::Authenticated;
    snapshot.effectiveSecurityProfile = SecurityProfile::Authenticated;
    snapshot.remoteInputEnabled = true;
    snapshot.connectedClientCount = 2;

    const QString report = formatDiagnosticReport(snapshot);
    CHECK(report.contains(QStringLiteral("STATE=Running\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_CONFIGURED=interface:eth0:5921\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_EFFECTIVE=192.0.2.10:5921\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_PROFILE=Authenticated\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_ENABLED=true\n")));
    CHECK(report.contains(QStringLiteral("REMOTE_INPUT=true\n")));
    CHECK(report.contains(QStringLiteral("CONNECTED_CLIENTS=2\n")));
}

void checkErrorIsBoundedAndSingleLine()
{
    DiagnosticSnapshot snapshot;
    snapshot.lastError = Error{ErrorCode::TransportUnavailable,
                               QString(600, QLatin1Char('x')) + QStringLiteral("\nsecret-next-line"),
                               true};

    const QString report = formatDiagnosticReport(snapshot);
    CHECK(report.contains(QStringLiteral("LAST_ERROR_CODE=TransportUnavailable\n")));
    CHECK(report.contains(QStringLiteral("LAST_ERROR_RECOVERABLE=true\n")));

    const QString prefix = QStringLiteral("LAST_ERROR_MESSAGE=");
    const qsizetype start = report.indexOf(prefix);
    CHECK(start >= 0);
    const qsizetype lineEnd = report.indexOf(QLatin1Char('\n'), start);
    CHECK(lineEnd > start);
    if (start >= 0 && lineEnd > start) {
        const QString value = report.mid(start + prefix.size(), lineEnd - start - prefix.size());
        CHECK(value.size() <= 512);
        CHECK(!value.contains(QLatin1Char('\n')));
        CHECK(!value.contains(QLatin1Char('\r')));
        CHECK(value.endsWith(QStringLiteral("...")));
    }
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    checkDefaultReport();
    checkRunningReport();
    checkErrorIsBoundedAndSingleLine();

    if (failures == 0) {
        std::cout << "Runtime diagnostic formatter tests passed\n";
        return 0;
    }

    std::cerr << failures << " runtime diagnostic formatter check(s) failed\n";
    return 1;
}
