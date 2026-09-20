# 07 - Production Showcase

This is the final product-level HyRemote-authored demonstration for V1. It intentionally uses **Qt Quick/QML UI + HyRemote QML API** so it is visibly different from the small teaching examples while still sharing the same `HyRemoteRemoteAccess` runtime.

## What it demonstrates

- root-project branding from `logo/`;
- `import HyRemote` and `RemoteAccess { ... }`;
- explicit start/stop lifecycle;
- loopback endpoint and connected-viewer status;
- view-only safe default and opt-in remote input;
- V1 security-profile selection and descriptor path;
- runtime error visibility;
- a normal local Qt Quick application remaining usable alongside remote access.

It does **not** invent Session UI before #170 lands. Session lists/termination are added only after the public Session API is real.

## Relationship to 04

`04-quick-qml` is the small teaching example. This showcase deliberately adds product presentation and operational controls, but it does not create another transport, session stack or QML-specific runtime.

```text
04: learn the QML API
07: see the same QML API in a production-style application
```

## Acceptance helpers

The existing product-fit contract is preserved:

- `--port <n>`
- `--remote-input`
- `--auto-start`
- `--test-seconds <n>`

Acceptance mode emits the existing machine-readable markers `REMOTE_STARTED`, `SHOWCASE_CLIENTS`, `SHOWCASE_POINTER` and `SHOWCASE_KEY` so the current V1 GA harness can continue validating framebuffer/reconnect/input behavior while the UI implementation is now Qt Quick/QML.

Refs: #41 #109 #170 #209.
