// #174 public C++ listener configuration/error contract.
//
// Every port in this file is chosen by the operating system: a probe QTcpServer binds port 0 and reports
// the port it got, and the public RemoteAccess facade is then told to use exactly that number. The real
// Runtime/RFB reachability and interface-reconciliation rows live in the Runtime-owned B4 companion test.

#include <QApplication>
#include <QTcpServer>
#include <QWidget>

#include <iostream>
#include <memory>
#include <optional>

#include <HyRemote/RemoteAccess.h>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

// Binds port 0, reports the number the operating system handed out, then either keeps holding it (to
// simulate an occupied port) or releases it (to hand a free, OS-chosen port to the product).
class PortProbe
{
public:
    bool acquire()
    {
        m_server = std::make_unique<QTcpServer>();
        if (!m_server->listen(QHostAddress::LocalHost, 0))
            return false;
        m_port = m_server->serverPort();
        return m_port != 0;
    }

    void release()
    {
        if (m_server)
            m_server->close();
        m_server.reset();
    }

    quint16 port() const { return m_port; }

private:
    std::unique_ptr<QTcpServer> m_server;
    quint16 m_port = 0;
};

void testLoopbackBindsAndStopReleasesTheEndpoint()
{
    PortProbe probe;
    CHECK(probe.acquire());
    if (probe.port() == 0)
        return;
    probe.release();

    QWidget target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setListenAddress(QHostAddress(QHostAddress::LocalHost)));
    CHECK(remote.setPort(probe.port()));

    CHECK(remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
    remote.stop();
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);

    // Stop must actually release the endpoint: the same OS-chosen port has to be bindable again at once.
    QTcpServer rebind;
    CHECK(rebind.listen(QHostAddress::LocalHost, probe.port()));
    rebind.close();
}

void testOccupiedPortFailsBeforeRunning()
{
    PortProbe probe;
    CHECK(probe.acquire());
    if (probe.port() == 0)
        return;

    QWidget target;
    HyRemote::RemoteAccess remote(&target);
    // The probe and the listener must contend for the same endpoint, so the address is stated explicitly instead of
    // relying on how a wildcard and a loopback bind happen to overlap on a given OS.
    CHECK(remote.setListenAddress(QHostAddress(QHostAddress::LocalHost)));
    CHECK(remote.setPort(probe.port()));
    remote.clearError();

    CHECK(!remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    const std::optional<HyRemote::RemoteAccessError> error = remote.lastError();
    CHECK(error.has_value());
    if (error) {
        CHECK(!error->message.isEmpty());
        std::cout << "occupied-port error code=" << static_cast<int>(error->code) << '\n';
    }

    probe.release();
}

void testUnavailableAddressFailsBeforeRunning()
{
    // 192.0.2.0/24 is TEST-NET-1 and is not assigned to any interface, so binding to it must fail on any
    // host. The port comes from the OS, so the address is the only reason the bind can fail.
    PortProbe probe;
    CHECK(probe.acquire());
    if (probe.port() == 0)
        return;
    probe.release();

    QWidget target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setListenAddress(QHostAddress(QStringLiteral("192.0.2.1"))));
    CHECK(remote.setPort(probe.port()));
    remote.clearError();

    CHECK(!remote.start());
    CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    const std::optional<HyRemote::RemoteAccessError> error = remote.lastError();
    CHECK(error.has_value());
    if (error) {
        CHECK(!error->message.isEmpty());
        std::cout << "unavailable-address error code=" << static_cast<int>(error->code)
                  << " message=" << error->message.toStdString() << '\n';
    }
}

void testIpv6IsRejectedAndNeverFallsBack()
{
    // #174 is an IPv4 listener contract, so an IPv6 address is refused at configuration time. Keeping this at the
    // public facade proves both the error mapping and that the last accepted address remains unchanged.
    for (const char *ipv6 : {"::1", "::"}) {
        QWidget target;
        HyRemote::RemoteAccess remote(&target);
        CHECK(!remote.setListenAddress(QHostAddress(QString::fromLatin1(ipv6))));
        CHECK(remote.lastError().has_value());
        if (remote.lastError())
            CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
        CHECK(remote.listenAddress() == QHostAddress(QHostAddress::AnyIPv4));
        std::cout << "row ipv6-rejected (" << ipv6 << ") refused=1\n";
    }
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testLoopbackBindsAndStopReleasesTheEndpoint();
    testOccupiedPortFailsBeforeRunning();
    testUnavailableAddressFailsBeforeRunning();
    testIpv6IsRejectedAndNeverFallsBack();

    std::cout << (failures == 0 ? "PASS: listener address matrix (facade rows)"
                                : "FAIL: listener address matrix (facade rows)")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
