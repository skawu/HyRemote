# HyRemote V0.0.1 Normalized Input Model

Issue: #29

This document fixes the first transport-neutral input vocabulary used by the V0.0.1.0 Embedded C++ product path and reused by the V1 QML/QPA paths. It refines the input boundary left intentionally open by ARCH-01 without introducing Qt, VNC, Windows or Linux protocol values into Core.

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

## Threading and terminal lifecycle

The existing ARCH-01 threading rule remains unchanged: `InputSink::post()` is called from the transport runtime path and the sink must enqueue/marshal to the target UI thread without synchronously blocking transport execution.

View and input authorization remain separate product controls. A target adapter must drop events when input is disabled/not Running and must not log typed text by default.

There are two distinct cleanup boundaries and they must not be conflated:

1. **Viewer disconnect cleanup belongs to the transport.** A transport that tracks protocol-specific held keys/buttons balances the recognized state for that viewer before its client state is discarded. This prevents one vanished viewer from leaking protocol-held state into the shared normalized stream.
2. **HyRemote runtime/target teardown cleanup belongs to the target `InputSink`.** After the shared Session has quiesced transport/Core callbacks, terminal sink shutdown discards normalized input still pending in the target adapter and balances supported key/button state that was already delivered into the Qt target. This prevents an explicit `RemoteAccess::stop()` or QML/QPA teardown from leaving the still-running local application with a synthetic remote press held down.

The second rule is intentionally an internal composition contract. It does not add an application-facing input-reset API. Repeated terminal shutdown is idempotent and may not synthesize duplicate releases.

For Qt target adapters, “pending” and “delivered” are therefore materially different states:

- events accepted into the bounded sink mailbox but not yet processed on the GUI thread are dropped at terminal shutdown;
- held buttons/keys recorded only after actual Qt delivery are balanced on the GUI thread;
- a QWidget press/release lifecycle retains the concrete child receiver where required so terminal release does not jump to a different child merely because focus/hit-testing changed;
- QPA composite teardown propagates terminal shutdown to each qualified child target before the child adapter is retired.

## Deferred to target/protocol adapters

The Core model intentionally does not decide:

- VNC keysym to `KeyCode` mapping;
- Qt key/event construction;
- QWidget vs QQuickWindow focus delivery;
- platform-wide virtual input (`uinput`, virtual HID);
- IME/dead-key full parity;
- transport disconnect key-release synthesis policy;
- framework-specific delivery bookkeeping used by terminal target-input shutdown.

Those are validated by the transport, Widgets, Quick and QPA product paths while preserving this normalized boundary.
