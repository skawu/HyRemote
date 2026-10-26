#include <QApplication>

#include <HyRemote/RemoteAccess.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    HyRemote::RemoteAccess access;
    return access.target() == nullptr ? 0 : 0;
}
