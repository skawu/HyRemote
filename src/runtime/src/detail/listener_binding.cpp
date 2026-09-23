#include "listener_binding.hpp"

#include <QNetworkInterface>
#include <QStringList>

#include <utility>

namespace HyRemote::detail {
namespace {

InterfaceAddressProvider &providerStorage()
{
    // One provider for the process: the runtime is a single shared library, and a test that installs a snapshot must be
    // able to drive every instance it creates.
    static InterfaceAddressProvider provider;
    return provider;
}

// The only place this runtime asks the host what an interface currently has.
QList<QHostAddress> realInterfaceAddresses(const QString &identity, bool &found)
{
    const QNetworkInterface networkInterface = QNetworkInterface::interfaceFromName(identity);
    found = networkInterface.isValid();

    QList<QHostAddress> addresses;
    if (!found)
        return addresses;

    for (const QNetworkAddressEntry &entry : networkInterface.addressEntries()) {
        const QHostAddress address = entry.ip();
        if (!address.isNull() && address.protocol() == QAbstractSocket::IPv4Protocol)
            addresses.append(address);
    }
    return addresses;
}

}  // namespace

void setInterfaceAddressProvider(InterfaceAddressProvider provider)
{
    providerStorage() = std::move(provider);
}

void resetInterfaceAddressProvider()
{
    providerStorage() = nullptr;
}

ListenerMode listenerModeFor(const QHostAddress &address, const QString &interfaceIdentity)
{
    if (!interfaceIdentity.isEmpty())
        return ListenerMode::Interface;
    if (address == QHostAddress(QHostAddress::AnyIPv4))
        return ListenerMode::Wildcard;
    return ListenerMode::Address;
}

InterfaceResolutionResult resolveInterfaceAddress(const QString &identity)
{
    InterfaceResolutionResult result;

    bool found = false;
    const InterfaceAddressProvider &provider = providerStorage();
    const QList<QHostAddress> addresses =
        provider ? provider(identity, found) : realInterfaceAddresses(identity, found);

    if (!found) {
        result.status = InterfaceResolution::NotFound;
        result.error = QStringLiteral("network interface '%1' was not found").arg(identity);
        return result;
    }

    if (addresses.isEmpty()) {
        // No fallback anywhere in this file: an interface without a usable IPv4 is a failure to report, not a reason to
        // listen somewhere else.
        result.status = InterfaceResolution::NoIpv4;
        result.error = QStringLiteral("network interface '%1' currently has no IPv4 address").arg(identity);
        return result;
    }

    if (addresses.size() > 1) {
        // Choosing one here would be inventing a routing preference, so the candidates are named and the choice is
        // handed back to the user, who can switch to the explicit address mode instead.
        result.status = InterfaceResolution::Ambiguous;
        result.candidates = addresses;
        QStringList listed;
        listed.reserve(addresses.size());
        for (const QHostAddress &candidate : addresses)
            listed.append(candidate.toString());
        result.error = QStringLiteral("network interface '%1' has more than one IPv4 address (%2); "
                                      "configure the address you want explicitly instead")
                           .arg(identity, listed.join(QStringLiteral(", ")));
        return result;
    }

    result.status = InterfaceResolution::Found;
    result.address = addresses.first();
    return result;
}

bool isLocalIpv4Address(const QHostAddress &address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    if (address == QHostAddress(QHostAddress::AnyIPv4))
        return true;
    // RFC 1122 reserves the whole 127/8 block for loopback, and the platforms this ships on let a host bind any
    // address in it, so the entire block is local. That keeps 127.0.0.1 legal, as #174 requires, without loosening
    // anything else: a genuinely foreign address like 192.0.2.1 is still refused.
    if (address.isLoopback())
        return true;

    const QList<QHostAddress> localAddresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &local : localAddresses) {
        if (local.protocol() == QAbstractSocket::IPv4Protocol && local == address)
            return true;
    }
    return false;
}

}  // namespace HyRemote::detail
