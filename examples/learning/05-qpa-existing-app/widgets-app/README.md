# 05 - QPA Existing App / Widgets

This fixture is intentionally an **ordinary Qt Widgets application**. Its source and normal link graph do not use the HyRemote C++ or QML application APIs.

Target/executable:

```text
hyremote-example-qpa-existing-widgets
```

The same binary demonstrates two launch modes:

```text
hyremote-example-qpa-existing-widgets -platform windows   # Windows native
hyremote-example-qpa-existing-widgets -platform xcb       # Linux/X11 native
hyremote-example-qpa-existing-widgets -platform hyremote  # HyRemote QPA proxy
```

The exact native delegate depends on the platform. HyRemote QPA must preserve the native local application while adding remote access.

## Application boundary

The target links Qt Widgets only. It deliberately contains:

- no `#include <HyRemote/...>`;
- no `import HyRemote`;
- no link to `HyRemote::RemoteAccess`;
- no private Core/QPA types.

`find_package(HyRemote)` is used only when `HYREMOTE_EXAMPLE_DEPLOY_QPA=ON`, so `hyremote_deploy(TARGET ... QPA)` can package the QPA payload. It does not turn the application into a HyRemote-linked application.

## QPA compatibility

QPA uses Qt private ABI and therefore requires exact build/runtime Qt patch identity. The current primary reference is Qt 6.8.3; #57 owns the Qt 5.15 exact-patch QPA qualification.

## Branding

Because this is a HyRemote-authored teaching fixture, it uses the canonical root project Logo. This does not change its Qt-only application dependency boundary. Third-party real-world projects under `examples/realworld/` keep their own upstream branding instead.

Together with `../quick-app`, this demonstrates that QPA is an integration mechanism across both Qt Widgets and Qt Quick.

Refs: #32 #41 #57 #109 #163 #176 #209.
