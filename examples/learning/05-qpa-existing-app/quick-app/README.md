# 05 - QPA Existing App / Qt Quick

This fixture is intentionally an **ordinary Qt Quick/QML application**. It demonstrates QPA integration without using either HyRemote application API.

Target/executable:

```text
hyremote-example-qpa-existing-quick
```

Application-source boundary:

- no `#include <HyRemote/...>`;
- no `import HyRemote`;
- no link to `HyRemote::RemoteAccess`;
- no private HyRemote or QPA types.

Run it first with the native Qt platform plugin, then deploy the QPA payload and run the same application with:

```text
hyremote-example-qpa-existing-quick -platform hyremote
```

The expected product behavior is additive: the native local Qt Quick window/input remains usable while HyRemote supplies remote display/control through the QPA platform integration.

## Why this exists next to the Widgets fixture

QPA is not a Widgets-only feature. The pair under `05-qpa-existing-app/` deliberately proves the same transparent integration model against both Qt UI technology families:

```text
Qt Widgets       + QPA
Qt Quick / QML   + QPA
```

## QPA compatibility

The QPA binary must match the exact Qt patch used by the application because it relies on Qt private ABI. The current primary V1 reference is Qt 6.8.3; #57 owns the exact Qt 5.15 qualification.

## Branding

This HyRemote-authored fixture uses the canonical root Logo through `examples/common`. Real third-party applications under `examples/realworld/` keep upstream branding unchanged.

Refs: #32 #41 #57 #109 #163 #176 #209.
