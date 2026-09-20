# 03 - Qt Quick + C++ API

This example moves from Widgets to the **Qt Quick UI technology**, while keeping the same HyRemote C++ integration API.

Target/executable:

```text
hyremote-example-quick-cpp
```

The UI is Qt Quick and is described with QML, but HyRemote is attached from C++:

```cpp
HyRemote::RemoteAccess remote(&view);
remote.start();
```

That distinction is intentional:

```text
Qt Quick = UI/rendering technology
QML      = declarative UI language
C++ API  = HyRemote integration used by this example
```

Use `04-quick-qml` next to keep the same Qt Quick/QML UI family while moving the HyRemote integration itself into QML.

## What it teaches

- `QQuickWindow`/`QQuickView` capture through the public Quick adapter;
- view-only default and explicit remote input;
- pointer, wheel, key and text/IME delivery into normal Quick event handling;
- viewer count and reconnect;
- stopped-runtime policy transition without recreating the Quick window.

Run view-only:

```text
hyremote-example-quick-cpp --port 5921
```

Run with explicit remote control:

```text
hyremote-example-quick-cpp --port 5921 --remote-input
```

## Qt coverage

This is controlled HyRemote source. It is intended to remain one logical example across the formal Qt 5.15 and Qt 6.8 lines; #57 owns the actual Qt5 build/QML-macro adaptation and executable evidence rather than duplicating this directory.

## Branding

Branding comes from the canonical root `logo/` asset through `examples/common`.

Refs: #41 #57 #109 #209.
