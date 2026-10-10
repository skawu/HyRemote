// Exercise the actual HyRemoteTool window and public RemoteAccess facade.
// Include the implementation in this one test translation unit so the app
// retains its normal standalone entry point and no test-only public seam is
// added to the installed Tool.
#define main hyremoteToolStandaloneMain
#include "main.cpp"
#undef main

#include <QFile>
#include <QHostAddress>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>

#include <iostream>

namespace {

int testFailures = 0;

#define REQUIRE(condition)                                                              \
    do {                                                                                \
        if (!(condition)) {                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": failed " #condition << '\n'; \
            ++testFailures;                                                             \
        }                                                                               \
    } while (false)

quint16 unusedPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
}

bool canConnect(quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    const bool connected = socket.waitForConnected(1000);
    socket.abort();
    return connected;
}

QString reportFrom(const SupportWindow &window, const QString &path)
{
    if (!window.saveDiagnosticReport(path))
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir reports;
    REQUIRE(reports.isValid());
    if (!reports.isValid())
        return 1;

    const quint16 initial = unusedPort();
    quint16 stoppedPort = unusedPort();
    while (stoppedPort != 0 && stoppedPort == initial)
        stoppedPort = unusedPort();
    quint16 runningPort = unusedPort();
    while (runningPort != 0 && (runningPort == initial || runningPort == stoppedPort))
        runningPort = unusedPort();
    REQUIRE(initial != 0 && stoppedPort != 0 && runningPort != 0);
    if (!initial || !stoppedPort || !runningPort)
        return 1;

    SupportWindow window(initial, false);
    window.show();
    app.processEvents();
    auto *editor = window.findChild<QSpinBox *>(QStringLiteral("hyremoteToolListenerPort"));
    auto *apply = window.findChild<QPushButton *>(QStringLiteral("hyremoteToolApplyListenerPort"));
    REQUIRE(editor != nullptr && apply != nullptr);
    if (!editor || !apply)
        return 1;
    REQUIRE(editor->value() == initial);
    REQUIRE(!apply->isEnabled());

    // A stopped GUI apply must configure the same public runtime without
    // starting a listener, and refresh the bounded diagnostic report.
    editor->setValue(stoppedPort);
    REQUIRE(apply->isEnabled());
    apply->click();
    REQUIRE(!apply->isEnabled());
    REQUIRE(!canConnect(stoppedPort));
    const QString stoppedReport = reportFrom(window, reports.filePath(QStringLiteral("stopped.txt")));
    REQUIRE(stoppedReport.contains(QStringLiteral("STATE=Stopped\n")));
    REQUIRE(stoppedReport.contains(
        QStringLiteral("LISTENER_CONFIGURED=address:0.0.0.0:%1\n").arg(stoppedPort)));

    REQUIRE(window.startRemoteAccess());
    REQUIRE(canConnect(stoppedPort));

    // Editing must NOT disconnect the active listener. Only explicit Apply
    // may stop/configure/restart it on the same RemoteAccess instance.
    editor->setValue(runningPort);
    REQUIRE(apply->isEnabled());
    REQUIRE(canConnect(stoppedPort));
    REQUIRE(!canConnect(runningPort));
    apply->click();
    REQUIRE(!apply->isEnabled());
    REQUIRE(!canConnect(stoppedPort));
    REQUIRE(canConnect(runningPort));

    const QString runningReport = reportFrom(window, reports.filePath(QStringLiteral("running.txt")));
    REQUIRE(runningReport.contains(QStringLiteral("STATE=Running\n")));
    REQUIRE(runningReport.contains(
        QStringLiteral("LISTENER_CONFIGURED=address:0.0.0.0:%1\n").arg(runningPort)));
    REQUIRE(runningReport.contains(
        QStringLiteral("LISTENER_EFFECTIVE=0.0.0.0:%1\n").arg(runningPort)));
    REQUIRE(runningReport.contains(QStringLiteral("REMOTE_INPUT=false\n")));

    if (testFailures)
        std::cerr << testFailures << " HyRemoteTool GUI listener-port checks failed\n";
    return testFailures ? 1 : 0;
}
