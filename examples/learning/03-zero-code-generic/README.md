# 03 - Generic Plugin, zero application code

Two **ordinary Qt applications** that reach HyRemote without a single line of HyRemote code.

```text
widgets-app/   an ordinary Qt Widgets application
quick-app/     an ordinary Qt Quick application
```

Neither application:

- includes a HyRemote header;
- calls a HyRemote API;
- links a HyRemote library.

The only HyRemote-related content is in each `CMakeLists.txt`, and it is deployment packaging, not application code.

## What this teaches

```text
same Qt-only binary
    |
    +-- ./hyremote-learning-03-widgets                       ordinary launch, no HyRemote
    +-- ./hyremote-learning-03-widgets -plugin hyremote      same binary, HyRemote reachable
```

The zero-code route is a Qt generic plugin, activated at launch. The application's own platform integration is
**preserved**: under Generic activation the native Qt platform stays `windows` on Windows and `xcb` on Linux. It
never becomes a HyRemote platform, and the Transparent QPA proxy is not involved.

## Build

```sh
cmake -S widgets-app -B build-widgets -DCMAKE_PREFIX_PATH="<qt-prefix>;<hyremote-prefix>"
cmake --build build-widgets --config Release
```

The application target links only its own Qt libraries. `hyremote_deploy(TARGET ... GENERIC)` in the same
`CMakeLists.txt` is what deploys the Generic payload, the shared runtime and the Qt runtime closure next to the
application; if the SDK was built without Generic support, the application still builds and runs as a plain Qt
application.

Inside the repository build with examples enabled, both applications build as part of the tree.

## Run

Ordinary launch - the application behaves like any other Qt application:

```sh
./build-widgets/hyremote-learning-03-widgets
```

Generic activation - the same binary, now remotely viewable:

```sh
./build-widgets/hyremote-learning-03-widgets -plugin hyremote
./build-widgets/hyremote-learning-03-widgets -plugin hyremote:port=6001
```

`--test-seconds <n>` makes the application exit by itself, which is what the smoke test uses.

## Deployed tree

After `hyremote_deploy(... GENERIC)` the deployed application directory contains:

- the application executable;
- the Generic payload under the Qt `generic` plugin directory (`plugins/generic/`) - the exact artifact name comes
  from the installed package metadata, so scripts and documentation must not hard-code it;
- the **native** Qt platform plugin under `plugins/platforms/`;
- the one shared `RemoteAccess` runtime;
- the Qt runtime closure the application actually needs.

There must be **no** HyRemote platform plugin in that tree. Generic preserves the native platform integration;
Transparent QPA is the frontend that replaces it, and the two cannot be combined on one application.

## Connect a viewer

```sh
vncviewer 127.0.0.1:6001
```

## Expected result

Ordinary and Generic launches both print:

```text
APP_READY
PLATFORM_NAME=<windows|xcb>
```

The platform name is the whole point: it must be the same in both launches.

## V0.1 boundary

- reference matrix: **Windows x86_64** and **Linux x86_64**, **Qt 6.8.3**;
- V0.1 is a **loopback-only Developer Preview** - the listener is loopback, remote input is off unless enabled, and
  nothing here is a production, GA or Internet-safe claim;
- see [`docs/guide/deployment.md`](../../../docs/guide/deployment.md) for the deployment details and
  [`docs/getting-started/generic.md`](../../../docs/getting-started/generic.md) for the integration guide.

Using HyRemote from application code instead? See [example 01](../01-widgets-cpp) and
[example 02](../02-quick-cpp) - that is the Embedded C++ frontend, the other V0.1 primary surface.
