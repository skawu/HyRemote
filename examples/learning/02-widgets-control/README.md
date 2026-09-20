# 02 - Widgets Control

This example is the second step after `01-widgets-basic`. It stays on **Qt Widgets + HyRemote C++ API**, but adds the behaviors needed for real remote control and release acceptance.

Target/executable:

```text
hyremote-example-widgets-control
```

## What it teaches

- explicit view-only versus remote-control policy;
- pointer, wheel, keyboard and committed-text delivery;
- connected-viewer count;
- viewer disconnect/reconnect without restarting the application;
- stopped-runtime policy transition (`stop -> configure -> start`);
- resize/DPR and lifecycle cleanup through the normal Widgets adapter;
- final `stop()` releasing the listener and held input state.

The application still uses only the public facade:

```cpp
HyRemote::RemoteAccess remote(&window);
```

No Core, RFB, transport, capture or private adapter type appears in application code.

## Run

View-only remains the default:

```text
hyremote-example-widgets-control --port 5921
```

Explicit remote control for a trusted test:

```text
hyremote-example-widgets-control --port 5921 --remote-input
```

The additional `--policy-transition-ms` and `--test-seconds` options are bounded acceptance helpers; they do not create a second product API.

## Qt coverage

This is a controlled HyRemote-authored example and therefore participates in the formal Qt 5.15 / Qt 6.8 compatibility work. Version-specific implementation differences belong in HyRemote adapters, not in a duplicated example tree.

## Branding

The application/window icon is supplied from the single repository-owned root `logo/` asset through `examples/common`; no logo file is copied into this example.

Refs: #41 #57 #109 #209.
