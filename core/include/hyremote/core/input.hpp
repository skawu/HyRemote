#pragma once

// Input routing boundary.
//
// ARCH-01 deliberately does not freeze the full normalized keyboard/pointer schema
// (docs/core-architecture.md section 6). What is frozen is the boundary and its threading rule:
// `InputSink::post()` must schedule/marshal work and must never make the transport runtime wait
// on synchronous GUI execution.

#include <cstdint>
#include <memory>
#include <string>

#include "hyremote/core/types.hpp"

namespace hyremote {

enum class InputEventKind {
    PointerMove,
    PointerButton,
    PointerScroll,
    Key,
    Placeholder,  // for transports that only need to prove the routing boundary
};

enum class PointerButton {
    None,
    Left,
    Middle,
    Right,
};

// Placeholder normalized event. Coordinates are in frame/root coordinate space of the shared
// target. Key identity is intentionally not a frozen schema yet.
struct InputEvent
{
    InputEventKind kind = InputEventKind::Placeholder;

    // Normalized pointer position in target coordinates (meaningful for pointer kinds).
    float x = 0.0F;
    float y = 0.0F;

    PointerButton button = PointerButton::None;
    bool pressed = false;

    // Backend-neutral key identity placeholder until a key-schema task refines it.
    std::uint32_t key = 0;

    // Free-form diagnostic payload supplied by a transport adapter.
    std::string text;
};

class InputSink
{
public:
    virtual ~InputSink() = default;

    // Called on the transport runtime thread. Implementations must enqueue/marshal to the thread
    // the target requires and must not synchronously wait for GUI execution.
    virtual void post(const InputEvent &event) = 0;
};

}  // namespace hyremote
