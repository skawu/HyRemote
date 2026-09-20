# 01 - Widgets Basic

This is the smallest HyRemote C++ integration example.

It teaches exactly four ideas:

1. create a normal Qt Widgets window;
2. construct `HyRemote::RemoteAccess` with that top-level window;
3. call `start()` explicitly;
4. call `stop()` during application shutdown.

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

No Core, Session, capture, transport or RFB implementation type appears in application code.

## Safe defaults

The example intentionally keeps every product default:

- construction is inert;
- listener address is loopback;
- port is the product default;
- remote input is disabled;
- the default security profile is the explicit V1 compatibility profile defined by the product contract.

Use `02-widgets-control` next for remote input policy, reconnect, resize/DPR and lifecycle cleanup.

## Qt coverage

The source is written against Qt public Widgets APIs and is intended to remain single-source for the supported Qt 5.15 and Qt 6.8 lanes. Version-specific implementation differences belong inside HyRemote, not in this example.

## Branding

The application/window icon comes from the repository-owned `logo/huayan-logo-single.png` through `examples/common/hyremote-branding.qrc`. The image is not copied into this directory.
