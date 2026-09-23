// The listener binding contract of #174, decided without touching a real adapter.
//
// The resolver reads the host through one injectable provider, so interface identity, an absent interface, an
// interface without IPv4 and an ambiguous one are all driven from a snapshot. Nothing here sleeps, polls or waits on a
// timer: the decisions under test are pure functions of the snapshot, which is what makes them deterministic on CI.

#include "detail/listener_binding.hpp"

#include <QCoreApplication>
#include <QHostAddress>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace {

int failures = 0;

#define CHECK(condition)                                                                          \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            ++failures;                                                                           \
            std::printf("CHECK failed: %s (line %d)\n", #condition, __LINE__);                     \
        }                                                                                         \
    } while (false)

// Installs a provider that only knows one identity.
void installSnapshot(const QString &knownIdentity, const QStringList &addresses)
{
    HyRemote::detail::setInterfaceAddressProvider(
        [knownIdentity, addresses](const QString &identity, bool &found) {
            found = (identity == knownIdentity);
            QList<QHostAddress> resolved;
            if (!found)
                return resolved;
            for (const QString &address : addresses)
                resolved.append(QHostAddress(address));
            return resolved;
        });
}

using HyRemote::detail::InterfaceResolution;
using HyRemote::detail::ListenerMode;

void testSingleAddress()
{
    installSnapshot(QStringLiteral("hyremote-test-iface"), {QStringLiteral("192.168.10.5")});

    const auto result = HyRemote::detail::resolveInterfaceAddress(QStringLiteral("hyremote-test-iface"));
    CHECK(result.status == InterfaceResolution::Found);
    CHECK(result.address == QHostAddress(QStringLiteral("192.168.10.5")));
    CHECK(result.candidates.isEmpty());
}

void testNotFound()
{
    installSnapshot(QStringLiteral("hyremote-test-iface"), {QStringLiteral("192.168.10.5")});

    const auto result = HyRemote::detail::resolveInterfaceAddress(QStringLiteral("hyremote-absent-iface"));
    CHECK(result.status == InterfaceResolution::NotFound);
    CHECK(result.error.contains(QStringLiteral("not found")));
    // An unknown identity must not resolve to anything at all, least of all the interface that does exist.
    CHECK(result.address.isNull());
}

void testNoIpv4()
{
    installSnapshot(QStringLiteral("hyremote-test-iface"), {});

    const auto result = HyRemote::detail::resolveInterfaceAddress(QStringLiteral("hyremote-test-iface"));
    CHECK(result.status == InterfaceResolution::NoIpv4);
    CHECK(result.address.isNull());
    CHECK(result.error.contains(QStringLiteral("no IPv4")));
}

void testAmbiguousListsCandidates()
{
    installSnapshot(QStringLiteral("hyremote-test-iface"),
                    {QStringLiteral("192.168.10.5"), QStringLiteral("10.0.0.7")});

    const auto result = HyRemote::detail::resolveInterfaceAddress(QStringLiteral("hyremote-test-iface"));
    CHECK(result.status == InterfaceResolution::Ambiguous);
    CHECK(result.candidates.size() == 2);
    // The refusal has to name what it refused to choose between, or the user cannot act on it.
    CHECK(result.error.contains(QStringLiteral("192.168.10.5")));
    CHECK(result.error.contains(QStringLiteral("10.0.0.7")));
    // Ambiguity is never resolved by picking one.
    CHECK(result.address.isNull());
}

void testModeInference()
{
    const QHostAddress wildcard(QHostAddress::AnyIPv4);
    const QHostAddress explicitAddress(QStringLiteral("192.168.1.4"));

    CHECK(HyRemote::detail::listenerModeFor(wildcard, QString{}) == ListenerMode::Wildcard);
    CHECK(HyRemote::detail::listenerModeFor(explicitAddress, QString{}) == ListenerMode::Address);
    CHECK(HyRemote::detail::listenerModeFor(QHostAddress(QHostAddress::LocalHost), QString{})
          == ListenerMode::Address);
    // A configured identity outranks the address, whatever the address happens to be.
    CHECK(HyRemote::detail::listenerModeFor(explicitAddress, QStringLiteral("hyremote-test-iface"))
          == ListenerMode::Interface);
    CHECK(HyRemote::detail::listenerModeFor(wildcard, QStringLiteral("hyremote-test-iface"))
          == ListenerMode::Interface);
}

void testLocalityContract()
{
    CHECK(HyRemote::detail::isLocalIpv4Address(QHostAddress(QHostAddress::AnyIPv4)));
    // #174 keeps loopback legal now that the default is the wildcard; the whole 127/8 block is host loopback.
    CHECK(HyRemote::detail::isLocalIpv4Address(QHostAddress(QStringLiteral("127.0.0.1"))));
    // TEST-NET-1 belongs to no interface, and refusing it is what makes "no fallback" meaningful.
    CHECK(!HyRemote::detail::isLocalIpv4Address(QHostAddress(QStringLiteral("192.0.2.1"))));
    // The contract is IPv4: an IPv6 address is not a local IPv4 address and is refused as an address.
    CHECK(!HyRemote::detail::isLocalIpv4Address(QHostAddress(QStringLiteral("::1"))));
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testSingleAddress();
    testNotFound();
    testNoIpv4();
    testAmbiguousListsCandidates();
    testModeInference();
    testLocalityContract();

    HyRemote::detail::resetInterfaceAddressProvider();

    std::printf("%s: listener binding (%d checks failed)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
