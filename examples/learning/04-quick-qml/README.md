# 04 - Qt Quick + QML API

This example uses the same **Qt Quick/QML UI technology family** as `03-quick-cpp`, but changes the HyRemote integration entry point from C++ to the declarative QML API.

Target/executable:

```text
hyremote-example-quick-qml
```

Minimal integration:

```qml
import HyRemote

RemoteAccess {
    target: window
    enabled: true
}
```

The QML type is a thin wrapper over the same `HyRemoteRemoteAccess` runtime used by the C++ examples. It does not own a second transport, capture path, input stack or session implementation.

## What it teaches

- `import HyRemote`;
- declarative target and lifecycle binding;
- the same loopback/view-only defaults as the C++ API;
- runtime state, connected-viewer count and error visibility in QML;
- remote pointer/key/text delivery through the same Quick adapter;
- disconnect/reconnect and declarative stop.

This example exists to compare **HyRemote integration styles**, not to imply that “Quick” and “QML” are separate UI technologies.

## Qt coverage

The source remains the single controlled declarative example for the supported LTS lines. #57 owns any Qt 5.15-specific CMake/QML registration adaptation and evidence; do not create a parallel Qt5 example tree.

## Branding

Branding comes from the canonical root `logo/` asset through `examples/common`.

Refs: #41 #57 #109 #209.
