# QPA existing application - Qt Quick fixture

This fixture is intentionally a normal Qt Quick/QML application.

It contains **no HyRemote application API**:

- no `#include <HyRemote/...>`;
- no `import HyRemote`;
- no link to `HyRemote::RemoteAccess`.

Run it first with the native platform plugin. Then deploy the QPA payload and launch the same binary with:

```text
-platform hyremote
```

The expected result is that the native local Qt Quick window/input remains functional while HyRemote supplies remote display/control through the QPA platform integration.

Together with `../widgets-app`, this proves that QPA is cross-cutting across both supported Qt UI technology families: Widgets and Qt Quick.

The window/application icon uses the project-owned root Logo through the shared Qt resource file. That branding does not change the fact that the application source is Qt-only.
