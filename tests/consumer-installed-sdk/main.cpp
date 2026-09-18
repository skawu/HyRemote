// SPDX-License-Identifier: Apache-2.0
#include <HyRemote/RemoteAccess.h>

#include <QObject>

int main()
{
    QObject target;
    HyRemote::RemoteAccess remote(&target);

    if (remote.target() != &target)
        return 1;
    if (remote.state() != HyRemote::RemoteAccessState::Stopped)
        return 2;
    if (remote.listenAddress() != QHostAddress(QHostAddress::LocalHost))
        return 3;
    if (remote.port() != 5900)
        return 4;
    if (remote.remoteInputEnabled())
        return 5;

    // Deliberately do not call start(): this fixture verifies that a completely external project
    // can discover/link/run the installed public facade and that construction remains side-effect
    // free. End-to-end network operation belongs to #6/#28/#27/#30.
    return 0;
}
