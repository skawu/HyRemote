#pragma once

// The Shared Runtime's one listener contract (#174): every frontend configures the same two facts, and this file turns
// them into one mode plus one effective IPv4 address. It is deliberately not a binding framework - there is no public
// type, no route metric, no gateway inference and no "pick the most likely adapter" heuristic. It answers exactly two
// questions: which mode a configuration means, and which single IPv4 an interface identity currently resolves to.

#include <HyRemote/RemoteAccessExport.h>

#include <QHostAddress>
#include <QList>
#include <QString>

#include <functional>

// The private source-tree declarations are exported only in a test-enabled build, so repository tests can drive the
// resolver with a snapshot instead of the host's real adapters. Not installed, not part of any SDK surface.
#if defined(HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS)
#  define HYREMOTE_LISTENER_BINDING_EXPORT HYREMOTE_REMOTEACCESS_EXPORT
#else
#  define HYREMOTE_LISTENER_BINDING_EXPORT
#endif

namespace HyRemote::detail {

enum class ListenerMode
{
    // 0.0.0.0: every IPv4 interface of the host, and the product default.
    Wildcard,
    // One exact local IPv4 address.
    Address,
    // One named network interface, whose current IPv4 is resolved at start and followed afterwards.
    Interface
};

// A non-empty interface identity outranks the address, because configuring an interface is the more specific
// statement; the wildcard is the wildcard; everything else is one explicit address.
HYREMOTE_LISTENER_BINDING_EXPORT ListenerMode listenerModeFor(const QHostAddress &address, const QString &interfaceIdentity);

enum class InterfaceResolution
{
    Found,
    NotFound,
    NoIpv4,
    Ambiguous
};

struct InterfaceResolutionResult
{
    InterfaceResolution status = InterfaceResolution::NotFound;
    // Only set for Found.
    QHostAddress address;
    // Only set for Ambiguous, so the error can name what it refused to choose between.
    QList<QHostAddress> candidates;
    QString error;
};

// Reads the host's own interfaces. Tests replace it with a snapshot so an address change can be driven deterministically
// without reconfiguring a runner's network; production code never replaces it.
using InterfaceAddressProvider = std::function<QList<QHostAddress>(const QString &identity, bool &found)>;

HYREMOTE_LISTENER_BINDING_EXPORT void setInterfaceAddressProvider(InterfaceAddressProvider provider);
HYREMOTE_LISTENER_BINDING_EXPORT void resetInterfaceAddressProvider();

// Resolves one interface identity to exactly one IPv4 address, or states precisely why it cannot. Identity is
// QNetworkInterface::name(): the human-readable name is never accepted as identity, and there is no fuzzy matching.
HYREMOTE_LISTENER_BINDING_EXPORT InterfaceResolutionResult resolveInterfaceAddress(const QString &identity);

// True when the address is the IPv4 wildcard or currently belongs to this host. Loopback stays legal: the default
// became the wildcard, which is not a reason to forbid 127.0.0.1.
HYREMOTE_LISTENER_BINDING_EXPORT bool isLocalIpv4Address(const QHostAddress &address);

#undef HYREMOTE_LISTENER_BINDING_EXPORT

}  // namespace HyRemote::detail
