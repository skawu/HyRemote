#pragma once

#include <cstddef>
#include <cstdint>

struct HyRemoteVncProbeEvent {
    std::uint32_t kind;
    std::uint64_t client_id;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t button_mask;
    std::uint32_t key;
    std::uint32_t pressed;
};

using HyRemoteVncProbeAbiVersionFn = std::uint32_t (*)();
using HyRemoteVncProbeCreateFn = void *(*)(std::uint16_t, std::uint16_t);
using HyRemoteVncProbeUpdateRgbaFn = int (*)(void *, const std::uint8_t *, std::size_t);
using HyRemoteVncProbeStartFn = int (*)(void *, std::uint16_t);
using HyRemoteVncProbeRunningFn = int (*)(void *);
using HyRemoteVncProbePollEventFn = int (*)(void *, HyRemoteVncProbeEvent *);
using HyRemoteVncProbeStopFn = int (*)(void *);
using HyRemoteVncProbeDestroyFn = void (*)(void *);
