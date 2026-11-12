#include <HyRemote/RemoteAccess.h>
#include "../src/quick/quick_gl_pbo_capture_source.hpp"

extern "C" __declspec(dllexport) void *hyremote_test_start(QObject *target, quint16 port)
{
    auto access = std::make_unique<HyRemote::RemoteAccess>(target);
    if (!access->setListenAddress(QHostAddress::LocalHost)
        || !access->setPort(port) || !access->start())
        return nullptr;
    return access.release();
}

extern "C" __declspec(dllexport) bool hyremote_test_stop(void *instance)
{
    std::unique_ptr<HyRemote::RemoteAccess> access(static_cast<HyRemote::RemoteAccess *>(instance));
    access->stop();
    return access->state() == HyRemote::RemoteAccessState::Stopped;
}

// 1 requires a real PBO and worker to have been used. 2 additionally requires
// their synchronous retirement and deletion, before any further GUI dispatch.
extern "C" __declspec(dllexport) int hyremote_test_gl_resources()
{
    const auto c = HyRemote::detail::quickGlPboResourceCounts();
    if (c.buffersCreated == 0 || c.workersCreated == 0)
        return 0;
    return c.buffersCreated == c.buffersDeleted && c.workersCreated == c.workersRetired
        ? 2 : 1;
}
