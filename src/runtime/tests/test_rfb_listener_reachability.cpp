// #274 Phase B4: real listener reachability and interface reconciliation are Shared Runtime/RFB
// integration contracts. They use the private Runtime seam directly so the tests do not depend on the
// Embedded C++ facade. #338's deterministic listener-binding unit test remains complementary: it
// validates pure resolution/mode/locality decisions without opening real listeners.

#include "access_instance.hpp"
#include "access_types.hpp"
#include "detail/listener_binding.hpp"

#include <QApplication>
#include <QByteArray>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

#include <iostream>
#include <memory>

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
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired)
        return;
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
    if (port == 0)
        return;

    QWidget target;
    AccessInstance instance(&target);
    const QHostAddress address(QString::fromLatin1(row.address));
    CHECK(!address.isNull());
    CHECK(instance.setListenAddress(address));
    CHECK(instance.setPort(port));
    instance.clearError();

    const bool started = instance.start();
    std::cout << "row " << row.name << " (" << row.address << ") started=" << started;
    if (!started) {
        const auto error = instance.lastError();
        std::cout << " message=" << (error ? error->message.toStdString() : std::string("<none>"));
    }
    std::cout << '\n';

    if (row.expectReachable)
        CHECK(started);

    if (started) {
        CHECK(instance.state() == AccessState::Running);
        const bool connected = dials(row.dial, port);
        std::cout << "    reachable via " << row.dial << " = " << connected
                  << " (expected " << row.expectReachable << ")\n";
        CHECK(connected == row.expectReachable);
        instance.stop();
        CHECK(instance.state() == AccessState::Stopped);
    } else {
        CHECK(!dials("127.0.0.1", port));
    }
}

// #338: drive the exact reconciliation entry point used by the Runtime watcher. The injected interface
// snapshot keeps the transition deterministic while reachability is measured on real sockets.
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
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired) {
        HyRemote::detail::resetInterfaceAddressProvider();
        return;
    }
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
    if (port == 0) {
        HyRemote::detail::resetInterfaceAddressProvider();
        return;
    }

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

// Where a host exposes one usable non-loopback IPv4, prove the three shipped binding modes against the
// actual LAN address. A host without such an interface cannot supply this platform fact, so the row reports
// that absence while the deterministic interface-resolution contract remains covered by the Runtime unit test.
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
        std::cout << "real-LAN rows: unavailable on this host (no non-loopback interface with exactly one IPv4)\n";
        return;
    }

    const QByteArray dialTarget = lanAddress.toLatin1();
    std::cout << "real LAN: interface=" << identity.toStdString() << " address=" << lanAddress.toStdString() << '\n';

    PortProbe probe;
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired)
        return;
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
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
        {"loopback-ipv4", "127.0.0.1", "127.0.0.1", true},
    };
    for (const AddressRow &row : rows)
        measureAddressRow(row);

    testInterfaceFollowing();
    testRealLanInterfaceBinding();

    std::cout << (failures == 0 ? "PASS: Runtime/RFB listener reachability"
                                : "FAIL: Runtime/RFB listener reachability")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
