# Qt Widgets Capture and Input Baseline

> Language / 语言: **English** | [中文](../widgets-capture.md)

This document describes the current HyRemote product behavior for qualified Qt Widgets targets.

Applications attach a `QWidget` through the public HyRemote frontend they selected. They do not construct capture sources or input sinks directly.

## Product boundary

For the programmable C++ API:

```cpp
HyRemote::RemoteAccess remote(&widget);
remote.start();
```

Generic Plugin and QPA may discover supported application-owned Widget surfaces automatically through the shared Runtime.

The Widgets adapter is an internal Runtime implementation detail. Applications do not select it explicitly.

## Capture path

The portable correctness path:

```text
Core scheduler
    -> Widgets target adapter
    -> queued work on the Qt GUI thread
    -> QWidget::render()
    -> owned CPU-readable frame storage
    -> RemoteFrame
    -> bounded Core handoff
    -> transport
```

Current baseline properties:

- QWidget access occurs on the Qt GUI thread;
- each published frame owns its pixel storage for the required frame lifetime;
- no borrowed or reused `QImage` pointer is exposed as a remote frame;
- device-pixel ratio is applied to captured dimensions once;
- completion time is used as the safe baseline content timestamp;
- damage may be represented conservatively as full-frame;
- capture-to-transport handoff remains bounded;
- stop and target teardown prevent stale queued capture from being published after the Runtime is no longer active.

This is a correctness baseline, not the final performance ceiling.

## Remote input path

When remote input is enabled, normalized input follows:

```text
transport normalization
    -> Core InputEvent routing
    -> Widgets InputSink
    -> queued GUI-thread delivery
    -> QWidget or focused descendant
```

The baseline supports the normalized product input categories in [`../input-model.md`](../input-model.md): pointer movement and buttons, wheel steps, logical key transitions, modifiers, and committed text where the transport provides it.

Pointer coordinates map from the remote frame viewport to the current QWidget target geometry without applying DPR twice.

Input delivery is asynchronous and bounded. Transport callbacks do not wait synchronously for QWidget event processing.

## Window and surface scope

A single explicit QWidget target does not mean “capture the entire desktop”. HyRemote targets the application.

Depending on the selected frontend, supported top-level dialogs, menus, popups, or additional application windows may be represented through the Runtime automatic and composite surface model.

Arbitrary foreign or native OS windows are not automatically included.

See [`../compatibility.md`](../compatibility.md) for the current qualified surface scope.

## Graphics-specific cases

Basic QWidget qualification does not automatically cover:

- `QOpenGLWidget`;
- `QQuickWidget` mixed content;
- native child windows;
- custom rendering engines;
- third-party components that bypass normal QWidget rendering assumptions.

These cases require explicit compatibility rows or real-world qualification. Raster Widgets support does not imply them.

## Performance direction

The current Widgets path favors portable correctness and owned frame lifetime.

Future improvements may include:

- reusable frame pools;
- damage-aware capture;
- more efficient graphics readback;
- platform-specific low-copy paths;
- hardware-assisted conversion or encoding.

These changes remain behind the Runtime boundary and must not require ordinary applications to replace `HyRemote::RemoteAccess` or select internal capture classes.

## Current status

In the current reference environment, Widgets through the C++ API and Generic Plugin are peer routes.

QPA Widgets support remains subject to its exact Qt private-ABI compatibility boundary.

See [`../compatibility.md`](../compatibility.md) and [`../known-limitations.md`](../known-limitations.md) for exact product status.
