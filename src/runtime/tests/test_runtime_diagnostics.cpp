#include "detail/runtime_diagnostics.hpp"

#include "access_instance.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

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
using HyRemote::Runtime::IntegrationRoute;
using HyRemote::Runtime::SecurityProfile;
using HyRemote::Runtime::UiFamily;
using HyRemote::Runtime::classifyUiFamily;
using HyRemote::Runtime::formatDiagnosticReport;

void checkDefaultReport()
{
    DiagnosticSnapshot snapshot;
    snapshot.configuredListenAddress = QHostAddress::AnyIPv4;
    snapshot.configuredPort = 5920;

    const QString report = formatDiagnosticReport(snapshot);
    // The version token is a build fact: develop uses the 0.0.0 sentinel and
    // reports "unknown", while a versioned release branch reports its four-part
    // version. Assert a well-formed field rather than one branch's literal.
    const QRegularExpression versionLine(
        QStringLiteral("^HYREMOTE_VERSION=(?:unknown|[0-9]+\\.[0-9]+\\.[0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::MultilineOption);
    CHECK(versionLine.match(report).hasMatch());
    CHECK(report.contains(QStringLiteral("BUILD_IDENTITY=source_sha="))
          || report.contains(QStringLiteral("BUILD_IDENTITY=unknown\n")));
    CHECK(report.contains(QStringLiteral("QT_VERSION=%1\n").arg(QString::fromLatin1(QT_VERSION_STR))));
    CHECK(report.contains(QStringLiteral("OS=")));
    CHECK(!report.contains(QStringLiteral("OS=\n")));
    CHECK(report.contains(QStringLiteral("ARCH=")));
    CHECK(!report.contains(QStringLiteral("ARCH=\n")));
    CHECK(report.contains(QStringLiteral("INTEGRATION_ROUTE=unknown\n")));
    CHECK(report.contains(QStringLiteral("UI_FAMILY=unknown\n")));
    CHECK(report.contains(QStringLiteral("STATE=Stopped\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_CONFIGURED=address:0.0.0.0:5920\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_EFFECTIVE=none\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_PROFILE=Insecure\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_ENABLED=false\n")));
    CHECK(report.contains(QStringLiteral("REMOTE_INPUT=false\n")));
    CHECK(report.contains(QStringLiteral("CONNECTED_CLIENTS=0\n")));
    CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE_ORIGIN=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_STATE_DIR=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_STATE_DIR_ORIGIN=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_SECRET_FILE=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_SECRET_FILE_ORIGIN=none\n")));
    CHECK(report.contains(QStringLiteral("HOST_PATHS_ERROR=none\n")));
    CHECK(report.contains(QStringLiteral("LAST_ERROR_CODE=none\n")));
    CHECK(report.endsWith(QStringLiteral("DEPLOYMENT_IDENTITY=unknown")));
}

void checkRunningReport()
{
    DiagnosticSnapshot snapshot;
    snapshot.integrationRoute = IntegrationRoute::Generic;
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
    CHECK(report.contains(QStringLiteral("INTEGRATION_ROUTE=generic\n")));
    CHECK(report.contains(QStringLiteral("STATE=Running\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_CONFIGURED=interface:eth0:5921\n")));
    CHECK(report.contains(QStringLiteral("LISTENER_EFFECTIVE=192.0.2.10:5921\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_PROFILE=Authenticated\n")));
    CHECK(report.contains(QStringLiteral("SECURITY_ENABLED=true\n")));
    CHECK(report.contains(QStringLiteral("REMOTE_INPUT=true\n")));
    CHECK(report.contains(QStringLiteral("CONNECTED_CLIENTS=2\n")));

    snapshot.integrationRoute = IntegrationRoute::Qpa;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("INTEGRATION_ROUTE=qpa\n")));
    snapshot.integrationRoute = IntegrationRoute::Cpp;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("INTEGRATION_ROUTE=cpp\n")));
    snapshot.integrationRoute = IntegrationRoute::Qml;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("INTEGRATION_ROUTE=qml\n")));
}

void checkUiFamilyClassification()
{
    CHECK(classifyUiFamily(false, false) == UiFamily::Unknown);
    CHECK(classifyUiFamily(true, false) == UiFamily::Widgets);
    CHECK(classifyUiFamily(false, true) == UiFamily::Quick);
    CHECK(classifyUiFamily(true, true) == UiFamily::Mixed);

    DiagnosticSnapshot snapshot;
    snapshot.uiFamily = UiFamily::Widgets;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("UI_FAMILY=widgets\n")));
    snapshot.uiFamily = UiFamily::Quick;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("UI_FAMILY=quick\n")));
    snapshot.uiFamily = UiFamily::Mixed;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("UI_FAMILY=mixed\n")));
    snapshot.uiFamily = UiFamily::Unknown;
    CHECK(formatDiagnosticReport(snapshot).contains(QStringLiteral("UI_FAMILY=unknown\n")));
}

// #591: the report carries the one shared host-path resolution with its provenance. A route never describes
// its own raw selection, so the same deployment answers identically from C++/QML/Generic/QPA.
void checkHostPathFacts()
{
    HyRemote::Runtime::AccessInstance instance;

    // 1. Nothing selected: no configuration or secret reference is implied, and the state directory falls
    // back to the platform default that is reported rather than hidden.
    {
        const QString report = formatDiagnosticReport(instance.diagnosticSnapshot());
        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE=none\n")));
        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE_ORIGIN=none\n")));
        CHECK(report.contains(QStringLiteral("HOST_STATE_DIR_ORIGIN=platform-default\n")));
        CHECK(report.contains(QStringLiteral("HOST_SECRET_FILE=none\n")));
        CHECK(report.contains(QStringLiteral("HOST_PATHS_ERROR=none\n")));
    }

    // 2. The environment selector is reported as an environment selection, and the state directory it
    // derives is anchored to the configuration file instead of the process working directory.
    QTemporaryDir root;
    CHECK(root.isValid());
    if (root.isValid()) {
        const QString configPath = root.filePath(QStringLiteral("hyremote.conf"));
        QFile config(configPath);
        CHECK(config.open(QIODevice::WriteOnly));
        config.write("version=1\n");
        config.close();

        qputenv("HYREMOTE_CONFIG_FILE", configPath.toUtf8());
        const QString report = formatDiagnosticReport(instance.diagnosticSnapshot());
        qunsetenv("HYREMOTE_CONFIG_FILE");

        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE=%1\n")
                                  .arg(QFileInfo(configPath).absoluteFilePath())));
        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE_ORIGIN=environment\n")));
        CHECK(report.contains(QStringLiteral("HOST_STATE_DIR_ORIGIN=config-relative\n")));
        CHECK(report.contains(QStringLiteral("HOST_PATHS_ERROR=none\n")));
    }

    // 3. An explicit selection that cannot be used is reported instead of being replaced by a silently
    // chosen writable location.
    CHECK(instance.setSecurityConfigFile(
        QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("hyremote-absent-descriptor.conf"))));
    {
        const QString report = formatDiagnosticReport(instance.diagnosticSnapshot());
        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE=none\n")));
        CHECK(report.contains(QStringLiteral("HOST_CONFIG_FILE_ORIGIN=none\n")));
        CHECK(report.contains(QStringLiteral("HOST_PATHS_ERROR=the explicit configuration file")));
    }
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
    checkUiFamilyClassification();
    checkHostPathFacts();
    checkErrorIsBoundedAndSingleLine();

    if (failures == 0) {
        std::cout << "Runtime diagnostic formatter tests passed\n";
        return 0;
    }

    std::cerr << failures << " runtime diagnostic formatter check(s) failed\n";
    return 1;
}
