// #174 Runtime/RFB listener reachability contract.
//
// This is the real-listener companion to test_listener_binding.cpp. The binding unit test decides mode,
// interface resolution and locality from an injected snapshot; this integration test proves that the resulting
// Runtime configuration reaches the requested IPv4 scope through the production RFB listener, and that interface
// reconciliation moves or removes that real listener without depending on the public C++ RemoteAccess facade.

#include <QApplication>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

#include <iostream>
#include <memory>

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

bool dials(const char *address, quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QString::fromLatin1(address)), port);
    const bool connected = socket.waitForConnected(2000);
    socket.abort();
    return connected;
}

struct AddressRow
{
    const char *name;
    const char *address;
    const char *dial;
    bool expectReachable;
};

void measureAddressRow(const AddressRow &row)
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    PortProbe probe;
    if (!probe.acquire())
        return;
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    QWidget target;
    AccessInstance instance(&target);
    const QHostAddress address(QString::fromLatin1(row.address));
    CHECK(!address.isNull());
    CHECK(instance.setListenAddress(address));
    CHECK(instance.setPort(port));

    const bool started = instance.start();
    std::cout << "row " << row.name << " (" << row.address << ") started=" << started;
    if (!started) {
        const auto error = instance.lastError();
        std::cout << " message=" << (error ? error->message.toStdString() : std::string("<none>"));
    }
    std::cout << '\n';

    if (started) {
        CHECK(instance.state() == AccessState::Running);
        const bool connected = dials(row.dial, port);
        std::cout << "    reachable via " << row.dial << " = " << connected
                  << " (expected " << row.expectReachable << ")\n";
        CHECK(connected == row.expectReachable);
        instance.stop();
        CHECK(instance.state() == AccessState::Stopped);
    } else {
        // Preserve the old matrix's fail-closed assertion: failure must not expose a listener on loopback.
        CHECK(!dials("127.0.0.1", port));
    }
}

// #174: what an interface binding does while the machine moves underneath it. Every step is triggered directly through
// the same reconciliation function the runtime's watcher calls, so nothing here sleeps or waits on a timer, and every
// claim is measured against real reachability on a real socket rather than against a counter.
//
// Both addresses are inside 127/8, which is host loopback, so the rows do not depend on a particular adapter existing.
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

    *liveAddresses = QStringList{QStringLiteral("127.0.0.1")};
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.1", port));

    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.2", port));
    CHECK(!dials("127.0.0.1", port));
    CHECK(instance.listenInterface() == identity);

    liveAddresses->clear();
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Unavailable);
    CHECK(!dials("127.0.0.1", port));
    CHECK(!dials("127.0.0.2", port));
    CHECK(instance.lastError().has_value());
    if (instance.lastError())
        std::cout << "unavailable reason: " << instance.lastError()->message.toStdString() << '\n';

    CHECK(!instance.setListenAddress(QHostAddress(QStringLiteral("127.0.0.1"))));
    CHECK(!instance.setListenInterface(QStringLiteral("some-other-iface")));
    CHECK(!instance.setPort(port == 65000 ? 65001 : 65000));
    CHECK(instance.listenInterface() == identity);

    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    CHECK(dials("127.0.0.2", port));
    CHECK(!instance.lastError().has_value());

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
// address. A machine without one reports an honest SKIP for these host-dependent rows rather than manufacturing one.
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

    {
        AccessInstance instance(&target);
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    {
        AccessInstance instance(&target);
        CHECK(instance.setListenAddress(QHostAddress(lanAddress)));
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

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
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const AddressRow rows[] = {
        {"wildcard-ipv4", "0.0.0.0", "127.0.0.1", true},
        {"explicit-ipv4", "127.0.0.1", "127.0.0.1", true},
    };
    for (const AddressRow &row : rows)
        measureAddressRow(row);

    testInterfaceFollowing();
    testRealLanInterfaceBinding();

    std::cout << (failures == 0 ? "PASS: RFB listener reachability (Runtime rows)"
                                : "FAIL: RFB listener reachability (Runtime rows)")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
