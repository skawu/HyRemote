include_guard(GLOBAL)

option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" ON)
option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" ON)
option(HYREMOTE_BUILD_CORE "Build the hyremote-core session/frame/dispatch library" ON)
option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the public HyRemote::RemoteAccess C++ facade when Qt is available" ON)
option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)
option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)
option(HYREMOTE_BUILD_SPIKES "Build throwaway architecture spike harnesses (non-production)" OFF)
option(HYREMOTE_WITH_VNC "Enable the first VNC/RFB transport backend when available" ON)
option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)
option(HYREMOTE_WITH_GBM "Enable GBM/DMA-BUF-oriented experimental backends" OFF)
option(HYREMOTE_WITH_RKMPP "Enable Rockchip MPP experimental encoder backend" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Keep platform and hardware optimizations opt-in until their architecture
# spikes have demonstrated a repeatable build and runtime path.
if(HYREMOTE_WITH_RKMPP AND NOT HYREMOTE_WITH_GBM)
    message(STATUS "HYREMOTE_WITH_RKMPP enabled without GBM; the final buffer path is not yet frozen")
endif()
