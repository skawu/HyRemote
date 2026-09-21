# HyRemote Normalized Input Model

This document defines HyRemote's transport-neutral remote-input contract. It describes product behavior shared by the C++ API, QML API, Generic Plugin, and QPA frontends.

The goal is to keep protocol values, Qt event details, and platform-specific input mechanisms out of Core.

## Input boundary

```text
viewer/protocol event
  -> transport-specific normalization
  -> hyremote::InputEvent
  -> Core routing
  -> target-specific InputSink
  -> queued delivery on the target UI thread
```

Core does not translate VNC keysyms into Qt keys and does not inject GUI events directly. Transport adapters normalize protocol data; target adapters translate normalized events into the target UI framework.

## Event categories

`InputEventKind` covers:

- pointer movement;
- pointer button transitions;
- pointer scrolling;
- key press/release;
- committed text.

`None` is an empty/default value and is not emitted as user input by a conforming transport.

The baseline pointer-button vocabulary includes left, middle, and right buttons. Scroll values use normalized logical wheel steps so transports convert protocol-native units before posting them into Core.

## Keyboard and committed text

`KeyCode` is a backend-neutral logical-key vocabulary. It is not numerically compatible with Qt keys, VNC keysyms, Windows virtual keys, Linux evdev codes, or another protocol/platform key representation.

Modifier and lock keys have two complementary representations:

- `KeyCode` identifies the key whose transition is being delivered;
- `InputEvent::modifiers` carries the modifier state applicable to the event.

Committed UTF-8 text is carried separately as a text event. Target adapters do not infer committed text from `KeyCode`, avoiding keyboard-layout assumptions in Core.

Current modifier-state vocabulary includes Shift, Control, Alt, Meta, CapsLock, and NumLock.

Full IME pre-edit/composition parity is not part of the current baseline.

## Pointer coordinate contract

Pointer `x`/`y` values are expressed in the exact pixel grid described by `InputViewport.width` and `InputViewport.height`. The viewport represents the remote surface that the viewer interacted with.

`devicePixelRatio` is diagnostic/context information. The source width/height remain authoritative for Core mapping so DPR is not applied twice.

A target adapter supplies its current logical target dimensions to the mapping operation. Core maps source edge to target edge and clamps out-of-range positions.

Invalid source geometry, invalid target geometry, non-finite coordinates, or a non-pointer event fail mapping explicitly rather than being guessed.

## Threading

Remote-input delivery must not synchronously block the transport path on Qt GUI processing.

`InputSink::post()` queues or marshals target work onto the appropriate UI thread. Widgets and Qt Quick remain responsible for actual framework event delivery on the Qt GUI thread.

Remote-view and remote-input policy are separate. If remote input is disabled or the Runtime is not in a state that accepts input, events are dropped rather than delivered later after policy changes.

Typed text is not written to normal diagnostic logs by default.

## Disconnect cleanup

A viewer may disappear while holding keys or pointer buttons. HyRemote must not leave the local Qt application in a synthetic remote-held state.

Cleanup occurs at two boundaries:

1. **Viewer disconnect** — the transport balances recognized held state contributed by that viewer before discarding its protocol/client state.
2. **Runtime/target teardown** — after transport/Core callbacks are quiesced, the target adapter discards undelivered queued input and balances supported held key/button state that has already reached the Qt target.

These rules apply regardless of whether the Runtime was entered through C++, QML, Generic, or QPA.

## Multiple viewers

Multiple remote viewers feed one logical application input target unless a future product capability explicitly introduces independent control ownership.

Per-viewer protocol state remains separate, but overlapping holds of the same normalized key/button are combined safely:

- the first holder produces the target press;
- another viewer holding the same logical input does not create a second physical held transition;
- one viewer releasing/disconnecting removes only its contribution;
- the target release occurs when the final holder releases/disconnects.

Current HyRemote does not expose per-viewer cursors, independent focus contexts, or a public per-client control-owner API.

> **TODO V0.2/V1.x:** richer authenticated-session and control-policy APIs may add explicit ownership/admission semantics without changing the normalized input vocabulary.

## Bounded input delivery

Remote input is bounded. A slow or stalled GUI must not create an unbounded queue.

Widgets and Qt Quick adapters distinguish ordinary bounded input from release delivery needed to prevent a previously accepted held state from becoming stuck.

A release is protected only when its corresponding press was accepted by the target adapter. If a press was rejected by backpressure, the later unmatched release is dropped rather than synthesizing a release for a press Qt never accepted.

This keeps overload behavior bounded while preserving input neutrality.

## Terminal teardown behavior

At terminal target shutdown:

- queued input not yet delivered to Qt is discarded;
- accepted/delivered held key/button state is balanced on the target UI thread;
- repeated teardown is idempotent and does not synthesize duplicate releases;
- QWidget delivery may retain the concrete receiver needed to balance a prior press correctly;
- composite QPA/automatic targets retire child input adapters only after their terminal cleanup.

This is an internal product guarantee, not a separate application-facing reset API.

## Deferred to adapters

The normalized Core model intentionally does not define:

- VNC keysym-to-`KeyCode` mapping;
- Qt key/event construction;
- QWidget versus QQuickWindow focus policy details;
- OS-wide virtual input such as Linux `uinput` or Windows virtual HID;
- full IME/dead-key composition parity;
- backend-specific protocol bookkeeping.

Those remain behind transport and target adapters while preserving the same public remote-input behavior.

## Current product limits

The current product does not promise:

- full IME/pre-edit parity;
- every international keyboard-layout edge case;
- OS-desktop-wide input injection;
- independent per-viewer input devices;
- arbitrary native/foreign-window input targeting.

Unsupported input cases are documented rather than approximated silently. See [`known-limitations.md`](known-limitations.md) and [`compatibility.md`](compatibility.md).
