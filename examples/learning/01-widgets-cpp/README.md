# 01 - Widgets + C++

The smallest real Embedded C++ path: a `QMainWindow` that a viewer can see, driven by the public
`HyRemote::RemoteAccess` facade.

This is example 01 of the V0.1 adoption flow (`01` Widgets + C++, `02` Quick + C++, `03` Generic zero-code). It is
written to be read, not to be an acceptance harness - product correctness is covered by the repository's own tests
and release evidence.

## What this teaches

```cpp
#include <HyRemote/RemoteAccess.h>

QMainWindow window;
window.show();

HyRemote::RemoteAccess remote(&window);
remote.setPort(5921);
remote.setRemoteInputEnabled(false);   // view-only unless you opt in
remote.start();                        // -> Running
...
remote.stop();                         // -> Stopped
```

Four things are worth noticing:

1. the runtime is constructed **with the window it shares** - that is the whole capture/target binding;
2. the listener is on `0.0.0.0:5921` by default, so a viewer on another machine on the same LAN can reach it;
3. remote input stays **off** until `--remote-input` is passed;
4. application code never mentions Core, Session, capture, input, transport or target-adapter types.

## Build

Against an installed HyRemote SDK (the normal case for an application):

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="<qt-prefix>;<hyremote-prefix>"
cmake --build build --config Release
```

Inside the repository build, with examples enabled (`build.cmd build --integrations=cpp,qml,generic,qpa --examples ...`),
the same source builds as part of the tree and needs no `find_package` step of its own.

## Run

```sh
./build/hyremote-learning-01-widgets            # uses port 5921
./build/hyremote-learning-01-widgets --port 6001
./build/hyremote-learning-01-widgets --port 6001 --remote-input
./build/hyremote-learning-01-widgets --port 6001 --test-seconds 5
```

| Option | Meaning |
| --- | --- |
| `--port <n>` | listener port the viewer connects to (default `5921`) |
| `--remote-input` | enable remote keyboard/pointer input; without it the session is view-only |
| `--test-seconds <n>` | exit by itself after `n` seconds (used by the smoke test) |

## Connect a viewer

Any RFB 3.8 viewer works, for example:

```sh
vncviewer <host-lan-ip>:5921
```

You should see this example's window. With `--remote-input` you can also drive it; without it the window is
view-only, which is the default.

## Expected result

```text
APP_READY
PLATFORM_NAME=<windows|xcb>
HYREMOTE_START=Running
...
HYREMOTE_STOP=Stopped
```

`PLATFORM_NAME` is the native Qt platform the application is running on. This example keeps the ordinary native
platform integration; it does not replace it.

## V0.1 boundary

- reference matrix: **Windows x86_64** and **Linux x86_64**, **Qt 6.8.3**;
- V0.2 is a **LAN-capable Developer Preview**: the listener is `0.0.0.0:5921` by default, remote input is off by default,
  and nothing in V0.1 is production, GA or Internet-safe;
- there is **no transport encryption and no authentication** in this release: the stream is unencrypted, so the listener is for a trusted LAN only and is not Internet-safe (see `docs/known-limitations.md`).

Quick UI instead of Widgets? See [`../02-quick-cpp`](../02-quick-cpp) - it uses this same C++ facade.
