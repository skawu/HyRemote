#include <HyRemote/RemoteAccess.h>

#include <QHostAddress>
#include <QObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace {

using HyRemote::RemoteAccess;
using HyRemote::RemoteAccessError;
using HyRemote::RemoteAccessState;

static_assert(!std::is_copy_constructible_v<RemoteAccess>);
static_assert(!std::is_copy_assignable_v<RemoteAccess>);
static_assert(std::is_nothrow_move_constructible_v<RemoteAccess>);
static_assert(std::is_nothrow_move_assignable_v<RemoteAccess>);

static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().target()), QObject *>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().setTarget(nullptr)), bool>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().listenAddress()), QHostAddress>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().setListenAddress(QHostAddress{})), bool>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().port()), quint16>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().setPort(quint16{})), bool>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().remoteInputEnabled()), bool>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().setRemoteInputEnabled(false)), bool>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().start()), bool>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().stop()), void>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().state()), RemoteAccessState>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().connectedClientCount()), std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<const RemoteAccess &>().lastError()), std::optional<RemoteAccessError>>);
static_assert(std::is_same_v<decltype(std::declval<RemoteAccess &>().clearError()), void>);

}  // namespace

int main()
{
    // Construction must remain inert and preserve the documented product defaults without needing
    // any backend type in application code.
    RemoteAccess remote;
    if (remote.state() != RemoteAccessState::Stopped)
        return 1;
    if (!remote.listenAddress().isLoopback())
        return 2;
    if (remote.remoteInputEnabled())
        return 3;
    if (remote.connectedClientCount() != 0)
        return 4;
    return 0;
}
