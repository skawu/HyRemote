# Generic Plugin integration

`src/integrations/generic` owns HyRemote's public-Qt zero-code integration frontend.

The payload is a `QGenericPlugin` loaded through Qt's normal generic-plugin mechanism, for example:

```text
-plugin hyremote:port=5921
```

or the equivalent `QT_QPA_GENERIC_PLUGINS` setting.

Its architectural contract is intentionally narrow:

- keep the application's native Qt platform integration unchanged;
- parse only Generic-frontend startup configuration;
- create the shared `HyRemote::Runtime::Automatic::AccessController`;
- own no capture, input, transport, surface-composition or Core implementation;
- use Qt public APIs only; Qt private/QPA APIs are forbidden here.

Supported specification keys in the initial #219 implementation are `address`, `port`, `input`, `security` and `security-config`, separated by `;` when multiple values are present. Defaults remain loopback + view-only.

The Generic frontend is post-V1 development work. Existing formal release profiles reject it until a release milestone is explicitly assigned.
