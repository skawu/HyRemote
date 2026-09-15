#include "probe_api.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

class DynamicLibrary
{
public:
    explicit DynamicLibrary(const char *path)
    {
#ifdef _WIN32
        m_handle = LoadLibraryA(path);
#else
        m_handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }

    ~DynamicLibrary()
    {
#ifdef _WIN32
        if (m_handle)
            FreeLibrary(m_handle);
#else
        if (m_handle)
            dlclose(m_handle);
#endif
    }

    DynamicLibrary(const DynamicLibrary &) = delete;
    DynamicLibrary &operator=(const DynamicLibrary &) = delete;

    bool valid() const { return m_handle != nullptr; }

    template <typename Function>
    Function symbol(const char *name) const
    {
#ifdef _WIN32
        return reinterpret_cast<Function>(GetProcAddress(m_handle, name));
#else
        return reinterpret_cast<Function>(dlsym(m_handle, name));
#endif
    }

    std::string error() const
    {
#ifdef _WIN32
        return "LoadLibrary/GetProcAddress failed with Win32 error "
               + std::to_string(GetLastError());
#else
        const char *message = dlerror();
        return message ? message : "dlopen/dlsym failed";
#endif
    }

private:
#ifdef _WIN32
    HMODULE m_handle = nullptr;
#else
    void *m_handle = nullptr;
#endif
};

template <typename Function>
bool loadRequired(const DynamicLibrary &library, Function &out, const char *name)
{
    out = library.symbol<Function>(name);
    if (out)
        return true;

    std::cerr << "missing symbol: " << name << " (" << library.error() << ")\n";
    return false;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: hyremote_vnc_ffi_smoke <probe-shared-library>\n";
        return 2;
    }

    DynamicLibrary library(argv[1]);
    if (!library.valid()) {
        std::cerr << "failed to load probe library: " << library.error() << "\n";
        return 3;
    }

    HyRemoteVncProbeAbiVersionFn abiVersion = nullptr;
    HyRemoteVncProbeCreateFn create = nullptr;
    HyRemoteVncProbeUpdateRgbaFn update = nullptr;
    HyRemoteVncProbeStartFn start = nullptr;
    HyRemoteVncProbeRunningFn running = nullptr;
    HyRemoteVncProbePollEventFn pollEvent = nullptr;
    HyRemoteVncProbeStopFn stop = nullptr;
    HyRemoteVncProbeDestroyFn destroy = nullptr;

    if (!loadRequired(library, abiVersion, "hyremote_vnc_probe_abi_version")
        || !loadRequired(library, create, "hyremote_vnc_probe_create")
        || !loadRequired(library, update, "hyremote_vnc_probe_update_rgba")
        || !loadRequired(library, start, "hyremote_vnc_probe_start")
        || !loadRequired(library, running, "hyremote_vnc_probe_running")
        || !loadRequired(library, pollEvent, "hyremote_vnc_probe_poll_event")
        || !loadRequired(library, stop, "hyremote_vnc_probe_stop")
        || !loadRequired(library, destroy, "hyremote_vnc_probe_destroy")) {
        return 4;
    }

    if (abiVersion() != 1) {
        std::cerr << "unexpected probe ABI version: " << abiVersion() << "\n";
        return 5;
    }

    constexpr std::uint16_t kWidth = 4;
    constexpr std::uint16_t kHeight = 3;
    void *handle = create(kWidth, kHeight);
    if (!handle) {
        std::cerr << "probe create failed\n";
        return 6;
    }

    std::vector<std::uint8_t> rgba(kWidth * kHeight * 4, 0);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        rgba[i + 0] = 0x33;
        rgba[i + 1] = 0x66;
        rgba[i + 2] = 0x99;
        rgba[i + 3] = 0xff;
    }

    if (update(handle, rgba.data(), rgba.size()) != 0) {
        std::cerr << "framebuffer update failed\n";
        destroy(handle);
        return 7;
    }

    // Port 0 is intentionally used only by this feasibility smoke test. The upstream API binds
    // 0.0.0.0 internally; production HyRemote adoption remains blocked until an explicit safe bind
    // address/pre-bound listener API exists.
    if (start(handle, 0) != 0) {
        std::cerr << "listener start scheduling failed\n";
        destroy(handle);
        return 8;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (running(handle) != 1) {
        std::cerr << "listener task terminated during startup\n";
        destroy(handle);
        return 9;
    }

    HyRemoteVncProbeEvent event{};
    const int pollResult = pollEvent(handle, &event);
    if (pollResult < 0) {
        std::cerr << "event poll failed\n";
        destroy(handle);
        return 10;
    }

    if (stop(handle) != 0) {
        std::cerr << "probe stop failed\n";
        destroy(handle);
        return 11;
    }

    if (running(handle) != 0) {
        std::cerr << "listener still marked running after stop\n";
        destroy(handle);
        return 12;
    }

    destroy(handle);
    std::cout << "PASS: C++ loaded Rust VNC probe ABI, updated RGBA frame, and exercised lifecycle\n";
    return 0;
}
