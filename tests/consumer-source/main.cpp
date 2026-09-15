#include <HyRemote/RemoteAccess.h>

#include <QObject>

int main()
{
    QObject target;
    HyRemote::RemoteAccess remote(&target);
    return remote.target() == &target && remote.state() == HyRemote::RemoteAccessState::Stopped ? 0 : 1;
}
