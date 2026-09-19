// #174: the listener-address contract has to be measured, not asserted in prose. The repository had no
// coverage for anything beyond binding loopback - no occupied-port row, no invalid-address row, no
// wildcard or IPv6 row - while the contract itself (loopback default, explicit widening, no silent
// fallback) is a V1 blocker.
//
// Every port in this file is chosen by the operating system: a probe QTcpServer binds port 0 and reports
// the port it got, and the product is then told to use exactly that number. Nothing here picks a port, and
// no fixed port is needed, so the rows stay deterministic and do not collide with a developer's machine.
//
// These are the product-level (RemoteAccess) rows. The transport-level rows - wildcard, IPv6 loopback,
// IPv6 wildcard and dual-stack reachability - are the next increment.

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

#include <iostream>
#include <optional>

#include "hyremote/RemoteAccess.h"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

void pump()
{
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

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

// The remaining address-family rows. Two rules hold whatever the platform reports, and those are what is
// asserted: a bind that succeeds must be reachable on the address that was asked for, and a bind that fails
// must not silently fall back to some other scope. Which rows the platform supports is recorded, not
// assumed - #174 allows an explicit "IPv4-only in V1" answer but not an ambiguous one.
struct AddressRow
{
    const char *name;
    const char *address;
    const char *dial;
    bool expectReachable;
};

bool dials(const char *address, quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QString::fromLatin1(address)), port);
    const bool connected = socket.waitForConnected(2000);
    socket.abort();
    return connected;
}

void measureAddressRow(const AddressRow &row)
{
    PortProbe probe;
    if (!probe.acquire())
        return;
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    QWidget target;
    HyRemote::RemoteAccess remote(&target);
    const QHostAddress address(QString::fromLatin1(row.address));
    CHECK(!address.isNull());
    CHECK(remote.setListenAddress(address));
    CHECK(remote.setPort(port));
    remote.clearError();

    const bool started = remote.start();
    std::cout << "row " << row.name << " (" << row.address << ") started=" << started;
    if (!started) {
        const std::optional<HyRemote::RemoteAccessError> error = remote.lastError();
        std::cout << " message=" << (error ? error->message.toStdString() : std::string("<none>"));
    }
    std::cout << '\n';

    if (started) {
        CHECK(remote.state() == HyRemote::RemoteAccessState::Running);
        const bool connected = dials(row.dial, port);
        std::cout << "    reachable via " << row.dial << " = " << connected
                  << " (expected " << row.expectReachable << ")\n";
        CHECK(connected == row.expectReachable);
        remote.stop();
        CHECK(remote.state() == HyRemote::RemoteAccessState::Stopped);
    } else {
        // No silent fallback: a rejected row must also refuse a connection on the address it was denied.
        CHECK(!dials("127.0.0.1", port));
    }
}

void measureIpv6WildcardIsNotDualStack()
{
    // Measured on this platform: binding "::" accepts IPv6 and does NOT accept an IPv4 loopback
    // connection, so "::" is IPv6-only here rather than a dual-stack listener. #174 accepts an explicit
    // evidence-backed answer like this, but not an ambiguous one, so the negative case is pinned too.
    PortProbe probe;
    if (!probe.acquire())
        return;
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    QWidget target;
    HyRemote::RemoteAccess remote(&target);
    CHECK(remote.setListenAddress(QHostAddress(QStringLiteral("::"))));
    CHECK(remote.setPort(port));
    remote.clearError();
    if (!remote.start())
        return;

    CHECK(dials("::1", port));
    std::cout << "row wildcard-ipv6 negative: reachable via 127.0.0.1 = " << dials("127.0.0.1", port)
              << " (expected 0)\n";
    CHECK(!dials("127.0.0.1", port));
    remote.stop();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testLoopbackBindsAndStopReleasesTheEndpoint();
    testOccupiedPortFailsBeforeRunning();
    testUnavailableAddressFailsBeforeRunning();

    const AddressRow rows[] = {
        {"wildcard-ipv4", "0.0.0.0", "127.0.0.1", true},
        {"loopback-ipv6", "::1", "::1", true},
        {"wildcard-ipv6", "::", "::1", true},
    };
    for (const AddressRow &row : rows)
        measureAddressRow(row);
    measureIpv6WildcardIsNotDualStack();

    std::cout << (failures == 0 ? "PASS: listener address matrix (product rows)"
                                : "FAIL: listener address matrix (product rows)")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
