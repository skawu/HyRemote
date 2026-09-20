#pragma once

#include "automatic/automatic_access_config.hpp"

#include <HyRemote/RemoteAccessExport.h>

#include <memory>

namespace HyRemote::Runtime::Automatic {

// Private cross-frontend application-level zero-code controller. Generic Plugin and QPA own one
// instance and differ only in bootstrap/config parsing; application-surface discovery, composition,
// input routing and AccessInstance lifecycle are implemented once here.
class HYREMOTE_REMOTEACCESS_EXPORT AccessController
{
public:
    explicit AccessController(AccessConfig config);
    ~AccessController();

    AccessController(const AccessController &) = delete;
    AccessController &operator=(const AccessController &) = delete;
    AccessController(AccessController &&) noexcept;
    AccessController &operator=(AccessController &&) noexcept;

    // Generic Plugin is loaded after native GUI initialization and may request an initial refresh.
    // QPA is created earlier: it arms observation with scheduleInitialRefresh=false so platform
    // factory creation never depends on a timer/event dispatcher; later real window events trigger
    // the same refresh path after the native integration is established.
    bool start(bool scheduleInitialRefresh = true);
    void stop() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote::Runtime::Automatic
