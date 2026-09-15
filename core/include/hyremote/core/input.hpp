#pragma once

// Transport-neutral remote-input contract.
//
// Protocol adapters normalize backend-specific pointer/key data into this schema before handing
// events to Core. Target adapters consume the same schema and marshal delivery to the target UI
// thread. No VNC keysym, Qt key, Windows VK or Linux evdev value is part of this contract.

#include <cstdint>
#include <optional>
#include <string>

namespace hyremote {

enum class InputEventKind {
    None,
    PointerMove,
    PointerButton,
    PointerScroll,
    Key,
    Text,
};

enum class PointerButton {
    None,
    Left,
    Middle,
    Right,
};

enum class InputModifier : std::uint16_t {
    Shift = 1U << 0U,
    Control = 1U << 1U,
    Alt = 1U << 2U,
    Meta = 1U << 3U,
    CapsLock = 1U << 4U,
    NumLock = 1U << 5U,
};

using InputModifiers = std::uint16_t;

constexpr InputModifiers modifierMask(InputModifier modifier) noexcept
{
    return static_cast<InputModifiers>(modifier);
}

constexpr bool hasModifier(InputModifiers modifiers, InputModifier modifier) noexcept
{
    return (modifiers & modifierMask(modifier)) != 0U;
}

// Logical key identity shared by transports and target adapters. Printable text is carried
// separately through InputEventKind::Text, so keyboard-layout/IME output is not inferred from a
// physical or logical key code.
enum class KeyCode : std::uint16_t {
    Unknown,
    Enter,
    Escape,
    Tab,
    Backspace,
    DeleteForward,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    ArrowLeft,
    ArrowUp,
    ArrowRight,
    ArrowDown,
    Space,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

// Geometry of the frame/viewport the remote viewer is interacting with. x/y below are expressed
// in this exact pixel grid. devicePixelRatio records capture metadata; source width/height remain
// authoritative for coordinate mapping so a target adapter never applies DPR twice.
struct InputViewport
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float devicePixelRatio = 1.0F;
};

struct MappedInputPoint
{
    float x = 0.0F;
    float y = 0.0F;
};

struct InputEvent
{
    InputEventKind kind = InputEventKind::None;

    // Pointer coordinates in sourceViewport pixel coordinates. Backends may provide values outside
    // the frame during edge transitions; target mapping clamps them predictably.
    InputViewport sourceViewport;
    float x = 0.0F;
    float y = 0.0F;

    PointerButton button = PointerButton::None;

    // Normalized wheel distance. +1/-1 means one logical wheel step; transports convert their
    // protocol-specific units before posting the event.
    float scrollX = 0.0F;
    float scrollY = 0.0F;

    KeyCode key = KeyCode::Unknown;
    bool pressed = false;
    InputModifiers modifiers = 0U;

    // UTF-8 committed text for InputEventKind::Text. Target adapters must not log typed text by
    // default. Composition/IME pre-edit is outside the V0.0.1.0 minimum contract.
    std::string textUtf8;
};

bool isValidInputViewport(const InputViewport &viewport) noexcept;
bool isPointerInputEvent(InputEventKind kind) noexcept;

// Maps source-frame pointer coordinates to target logical coordinates. targetWidth/targetHeight
// are the current logical dimensions exposed by the target adapter (for example QWidget::width()
// or QQuickWindow::width()). Edge coordinates map edge-to-edge and out-of-range source positions
// are clamped. Invalid geometry or non-pointer events return std::nullopt.
std::optional<MappedInputPoint> mapPointerToTarget(const InputEvent &event,
                                                   float targetWidth,
                                                   float targetHeight) noexcept;

class InputSink
{
public:
    virtual ~InputSink() = default;

    // Called on the transport runtime thread. Implementations must enqueue/marshal to the thread
    // the target requires and must not synchronously wait for GUI execution.
    //
    // Failure policy: an exception thrown here is caught by Core inside the callback, never
    // unwinds through the transport runtime, is counted in `SessionStats::inputPostFailures`, and
    // is reported as a recoverable `SessionError`. Remote input is not on the capture path, so a
    // failing sink does not fault the Session or interrupt frame delivery.
    virtual void post(const InputEvent &event) = 0;
};

}  // namespace hyremote
