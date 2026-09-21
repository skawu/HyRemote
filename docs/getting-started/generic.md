# Generic Plugin - zero-code integration for an ordinary Qt application

The Generic Plugin is the fourth peer frontend and, together with Embedded C++, one of the two **primary** V0.1
surfaces. It is a public Qt *generic* plugin: your application stays an ordinary Qt application, links no HyRemote
target, and acquires remote access at run time through Qt's own plugin mechanism.

## What you get and what you do not

| | |
| --- | --- |
| Public Qt plugin | yes - the integration is a Qt generic plugin loaded through `-plugin hyremote` |
| HyRemote application API / link dependency | none - you do not link `HyRemote::RemoteAccess` and you do not call HyRemote code |
| Native Qt platform | preserved - the plugin never replaces or installs a Qt platform plugin, so your application keeps running on the platform it already used |
| Qt private APIs | none - the plugin uses public Qt APIs only |

This is the difference between the two Qt-facing frontends: the Generic Plugin leaves your application's platform
integration alone, while the Transparent QPA Proxy replaces it with `-platform hyremote`.

## Consume the installed SDK

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)   # or Quick, for a Qt Quick application
find_package(HyRemote CONFIG REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)   # note: no HyRemote target

install(TARGETS MyApp RUNTIME DESTINATION bin)

# The Generic payload, the shared runtime and the runtime closure your application needs.
hyremote_deploy(TARGET MyApp GENERIC)
```

`GENERIC` is a mode of the same `hyremote_deploy()` family every other payload uses - there is no second deploy script
to learn and no separate tool to install. `hyremote_deploy(TARGET ... GENERIC QPA)` is refused: the Generic Plugin
preserves the native platform integration and QPA replaces it, so the two cannot both be deployed for one application.

Deploy from the build tree (`cmake --install`) or from the installed SDK. The payload identity always comes from the
target artifact when you deploy a source build, and only from the installed package metadata when you deploy an
installed SDK; it is never guessed from a toolchain prefix/suffix and never taken from a build-tree search.

## Activate it

```text
MyApp -plugin hyremote
MyApp -plugin hyremote:port=5921
```

The plugin is discovered where Qt looks for generic plugins, so the argument alone is enough once the payload is
deployed. `port` is optional; when omitted the runtime uses its default port.

## What a deployment contains

`hyremote_deploy(TARGET MyApp GENERIC)` produces a tree that runs without the HyRemote SDK and without the Qt SDK on
its search path:

- the deployed application executable;
- the Generic Plugin payload, below Qt's generic plugin directory (`plugins/generic/`);
- the **native** Qt platform plugin, below `plugins/platforms/`, so the application keeps its normal platform;
- the one shared `RemoteAccess` runtime artifact;
- the Qt runtime closure the deployed application needs.

Exact file names are platform specific and are decided by the installed package metadata and the deploy helper - do
not hard-code them in build scripts or documentation. Read `HyRemote_GENERIC_AVAILABLE` and
`HyRemote_GENERIC_PLUGIN_FILE` from the package if you need the installed payload location; the deployed location is
whatever `QT_DEPLOY_PLUGINS_DIR/generic` resolves to for your application.

No HyRemote platform plugin is ever installed. If you find `plugins/platforms/*hyremote*` in a Generic deployment,
that is a defect: the integration would no longer be preserving your native platform.

## Security boundary (V0.1)

V0.1 is a **loopback-only Developer Preview**:

- the listener is bound to loopback and is not reachable from other machines;
- `Insecure` is loopback, unauthenticated and unencrypted;
- `Authenticated` is RFB VNC Authentication and is unencrypted;
- `AuthenticatedEncrypted` is **unavailable in V0.1** and fails closed before a listener exists - there is no
  downgrade to a weaker profile;
- the VeNCrypt/TLS work is V0.2 under #143, not V0.1 and not V1.0-only.

Do not describe a V0.1 deployment as production ready, authenticated, encrypted or GA. See
[`../known-limitations.md`](../known-limitations.md) and [`../security-model.md`](../security-model.md).
