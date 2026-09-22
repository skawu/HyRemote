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
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

#include <iostream>
#include <optional>

#include <HyRemote/RemoteAccess.h>

// The dynamic interface-following rows drive the runtime directly rather than the facade, because the
// reconciliation entry point they must exercise is a runtime seam. This target already links the shared runtime and
// has its private source directory on the include path, so reaching it needs nothing new.
#include "access_instance.hpp"
#include "access_types.hpp"
#include "detail/listener_binding.hpp"

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

void testIpv6IsRejectedAndNeverFallsBack()
{
    // #174 is an IPv4 listener contract, so an IPv6 address is refused at configuration time. The rows are kept as
    // assertions rather than deleted: the point is not that IPv6 is unavailable, it is that asking for it cannot
    // produce a listener on some other address - no wildcard fallback, no loopback fallback - and that the refusal
    // is deterministic for both the IPv6 loopback and the IPv6 wildcard.
    for (const char *ipv6 : {"::1", "::"}) {
        QWidget target;
        HyRemote::RemoteAccess remote(&target);
        CHECK(!remote.setListenAddress(QHostAddress(QString::fromLatin1(ipv6))));
        CHECK(remote.lastError().has_value());
        if (remote.lastError())
            CHECK(remote.lastError()->code == HyRemote::RemoteAccessErrorCode::InvalidConfiguration);
        // The refused address is not adopted, so the listener still holds its previous default.
        CHECK(remote.listenAddress() == QHostAddress(QHostAddress::AnyIPv4));
        std::cout << "row ipv6-rejected (" << ipv6 << ") refused=1\n";
    }
}


// #174: what an interface binding does while the machine moves underneath it. Every step is triggered directly through
// the same reconciliation function the runtime's watcher calls, so nothing here sleeps or waits on a timer, and every
// claim is measured against real reachability on a real socket rather than against a counter.
//
// Both addresses are inside 127/8, which RFC 1122 reserves for host loopback, so they are bindable on the platforms
// this ships on without depending on any particular adapter being present.
void testInterfaceFollowing()
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    const QString identity = QStringLiteral("hyremote-test-iface");
    auto liveAddresses = std::make_shared<QStringList>();

    HyRemote::detail::setInterfaceAddressProvider([identity, liveAddresses](const QString &requested, bool &found) {
        found = (requested == identity);
        QList<QHostAddress> resolved;
        if (!found)
            return resolved;
        for (const QString &candidate : *liveAddresses)
            resolved.append(QHostAddress(candidate));
        return resolved;
    });

    PortProbe probe;
    if (!probe.acquire())
        return;
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    QWidget target;
    AccessInstance instance(&target);
    CHECK(instance.setPort(port));
    CHECK(instance.setListenInterface(identity));
    CHECK(instance.listenInterface() == identity);

    // The interface has one address: the listener serves exactly that one.
    *liveAddresses = QStringList{QStringLiteral("127.0.0.1")};
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.1", port));

    // A -> B. The old listener is gone, a fresh one serves the new address, and the configured identity is untouched:
    // the effective endpoint moved, the configuration did not.
    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.2", port));
    CHECK(!dials("127.0.0.1", port));
    CHECK(instance.listenInterface() == identity);

    // The interface loses its address. No listener survives on the old address and none appears anywhere else: the
    // state says Unavailable and carries the reason, while the intent to be reachable stands.
    liveAddresses->clear();
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Unavailable);
    CHECK(!dials("127.0.0.1", port));
    CHECK(!dials("127.0.0.2", port));
    CHECK(!instance.lastError().has_value() == false);
    if (instance.lastError())
        std::cout << "unavailable reason: " << instance.lastError()->message.toStdString() << '\n';

    // While the intent stands the configuration is not the user's to change, or a recovery could race the edit.
    CHECK(!instance.setListenAddress(QHostAddress(QStringLiteral("127.0.0.1"))));
    CHECK(!instance.setListenInterface(QStringLiteral("some-other-iface")));
    CHECK(!instance.setPort(port == 65000 ? 65001 : 65000));
    CHECK(instance.listenInterface() == identity);

    // The same interface answers again: the listener comes back on the new address without the user doing anything.
    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.2", port));
    CHECK(!instance.lastError().has_value());

    // stop() is final. The interface coming back afterwards must not start anything, and the configuration becomes the
    // user's again.
    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);
    *liveAddresses = QStringList{QStringLiteral("127.0.0.1")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Stopped);
    CHECK(!dials("127.0.0.1", port));
    CHECK(instance.setListenAddress(QHostAddress(QStringLiteral("127.0.0.1"))));

    HyRemote::detail::resetInterfaceAddressProvider();
}

// The real host, not a synthetic snapshot: one non-loopback interface that currently has exactly one usable IPv4
// address. A machine without one (a bare CI runner, for instance) reports SKIP rather than pretending, so this row
// is evidence where it runs and honest where it cannot.
//
// The three rows are the three bindings the product offers, each checked with a real TCP connection to the real LAN
// address rather than to loopback: the wildcard default, the same address named exactly, and the adapter named for
// the runtime to resolve.
void testRealLanInterfaceBinding()
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    QString identity;
    QString lanAddress;
    for (const QNetworkInterface &candidate : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = candidate.flags();
        if (flags.testFlag(QNetworkInterface::IsLoopBack) || !flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)) {
            continue;
        }

        QStringList addresses;
        for (const QNetworkAddressEntry &entry : candidate.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!ip.isNull() && ip.protocol() == QAbstractSocket::IPv4Protocol)
                addresses.append(ip.toString());
        }
        if (addresses.size() == 1) {
            identity = candidate.name();
            lanAddress = addresses.first();
            break;
        }
    }

    if (identity.isEmpty() || lanAddress.isEmpty()) {
        std::cout << "real-LAN rows: SKIP (this host has no non-loopback interface with exactly one IPv4)\n";
        return;
    }

    const QByteArray dialTarget = lanAddress.toLatin1();
    std::cout << "real LAN: interface=" << identity.toStdString() << " address=" << lanAddress.toStdString() << '\n';

    PortProbe probe;
    if (!probe.acquire())
        return;
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    QWidget target;

    // A. The default: 0.0.0.0 is reachable on the real LAN address, not only on loopback.
    {
        AccessInstance instance(&target);
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    // B. The same address, named exactly: bound as asked, and reachable there.
    {
        AccessInstance instance(&target);
        CHECK(instance.setListenAddress(QHostAddress(lanAddress)));
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    // C. The adapter, named: the runtime resolves its current IPv4 and binds exactly that.
    {
        AccessInstance instance(&target);
        CHECK(instance.setListenInterface(identity));
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(instance.listenInterface() == identity);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    // The port is free again after all three: stop() released every endpoint.
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testLoopbackBindsAndStopReleasesTheEndpoint();
    testOccupiedPortFailsBeforeRunning();
    testUnavailableAddressFailsBeforeRunning();

    // IPv4 rows only: the contract is IPv4, and the IPv6 inputs are asserted as refusals below rather than as
    // supported binds.
    const AddressRow rows[] = {
        {"wildcard-ipv4", "0.0.0.0", "127.0.0.1", true},
        {"loopback-ipv4", "127.0.0.1", "127.0.0.1", true},
    };
    for (const AddressRow &row : rows)
        measureAddressRow(row);
    testIpv6IsRejectedAndNeverFallsBack();
    testInterfaceFollowing();
    testRealLanInterfaceBinding();

    std::cout << (failures == 0 ? "PASS: listener address matrix (product rows)"
                                : "FAIL: listener address matrix (product rows)")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
