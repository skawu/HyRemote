# HyRemote V0.0.1 Normalized Input Model

Issue: #29

This document fixes the first transport-neutral input vocabulary used by the V0.0.1.0 Embedded C++ product path. It refines the input boundary left intentionally open by ARCH-01 without introducing Qt, VNC, Windows or Linux protocol values into Core.

## Boundary

```text
viewer/protocol event
  -> transport-specific normalization
  -> hyremote::InputEvent
  -> Core routing
  -> target-specific InputSink
  -> queued delivery on the target UI thread
```

Core does not translate VNC keysyms into Qt keys and does not inject GUI events. Transport adapters normalize protocol data; target adapters translate the normalized event into their UI framework.

## Event categories

`InputEventKind` defines pointer move, pointer button, pointer scroll, key and committed text events. `None` represents an empty/default value and is not emitted as user input by a conforming transport.

Pointer buttons are the V0.0.1.0 acceptance set: left, middle and right. Scroll values use normalized logical wheel steps where `+1` / `-1` means one step; a transport must convert protocol-native units before posting.

## Keyboard and text

`KeyCode` is a backend-neutral logical-key vocabulary for the V0.0.1.0 baseline: navigation/editing keys, digits, Latin A-Z, F1-F12, and the Shift/Control/Alt/Meta/CapsLock/NumLock keys themselves. It is not numerically compatible with Qt keys, VNC keysyms, Windows virtual keys or Linux evdev codes.

Modifier and lock keys have **two complementary representations**: their `KeyCode` identifies the key whose press/release transition is being delivered, while `InputEvent::modifiers` is the state mask that applies to the event. For example, a Shift press is `key=Shift`, `pressed=true`, with Shift present in the mask; its release is `key=Shift`, `pressed=false`, with the post-transition mask cleared. This lets protocol/target adapters preserve actual key-up state instead of inferring it from unrelated keystrokes.

Committed UTF-8 text is carried separately as `InputEventKind::Text`. Target adapters must not infer committed text from `KeyCode`; this avoids embedding keyboard-layout assumptions in Core. IME pre-edit/composition parity remains outside the minimum V0.0.1.0 contract.

Modifier state is an explicit backend-neutral mask containing Shift, Control, Alt, Meta, CapsLock and NumLock.

## Pointer coordinate contract

Pointer `x`/`y` are expressed in the exact pixel grid described by `InputViewport.width` and `InputViewport.height`. The viewport is the frame/remote surface that the viewer actually interacted with.

`devicePixelRatio` records the capture DPR for diagnostics and target-adapter decisions, but source width/height are authoritative for Core mapping. A target adapter supplies its current logical width/height to `mapPointerToTarget()`; Core maps source edge to target edge and clamps out-of-range positions. DPR is therefore not multiplied a second time.

Invalid/zero source geometry, invalid target geometry, non-finite coordinates and non-pointer event kinds fail mapping explicitly with `std::nullopt`.

## Threading and security

The existing ARCH-01 threading rule remains unchanged: `InputSink::post()` is called from the transport runtime path and the sink must enqueue/marshal to the target UI thread without synchronously blocking transport execution.

View and input authorization remain separate product controls. A target adapter must drop events when input is disabled/not Running, drop late events after stop, and must not log typed text by default.

## Deferred to target/protocol adapters

The Core model intentionally does not decide:

- VNC keysym to `KeyCode` mapping;
- Qt key/event construction;
- QWidget vs QQuickWindow focus delivery;
- platform-wide virtual input (`uinput`, virtual HID);
- IME/dead-key full parity;
- transport disconnect key-release synthesis policy.

Those are validated by #27, #6 and #28 while preserving this normalized boundary.
