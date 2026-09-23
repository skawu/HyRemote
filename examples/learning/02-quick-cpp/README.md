# 02 - Quick + C++

The same Embedded C++ path as [example 01](../01-widgets-cpp), on a Qt Quick window.

**Quick UI does not require the HyRemote QML frontend.** Quick is a UI family; the `HyRemote` QML module is a
separate integration frontend. A Quick application links Qt plus `HyRemote::RemoteAccess` and never writes
`import HyRemote` - and this example proves it by doing exactly that.

## What this teaches

```text
Main.qml (plain Qt Quick, no HyRemote import)
        |
        v
QQuickView  (a QQuickWindow that loads QML)
        |
        v
C++ bootstrap  ->  HyRemote::RemoteAccess remote(&view);
                   remote.setPort(5921);
                   remote.start();          // -> Running
                   ...
                   remote.stop();           // -> Stopped
```

Compare the integration lines with example 01: they are identical. Only the window and the UI language differ.
That is the whole lesson - the UI family is your choice, the integration frontend is a separate decision, and for
V0.2 ships four peer frontends; this example uses Embedded C++.

## Build

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="<qt-prefix>;<hyremote-prefix>"
cmake --build build --config Release
```

Inside the repository build with examples enabled, this builds as part of the tree.

## Run

```sh
./build/hyremote-learning-02-quick
./build/hyremote-learning-02-quick --port 6001
./build/hyremote-learning-02-quick --port 6001 --remote-input
./build/hyremote-learning-02-quick --port 6001 --test-seconds 5
```

| Option | Meaning |
| --- | --- |
| `--port <n>` | listener port the viewer connects to (default `5921`) |
| `--remote-input` | enable remote keyboard/pointer input; without it the session is view-only |
| `--test-seconds <n>` | exit by itself after `n` seconds (used by the smoke test) |

## Connect a viewer

```sh
vncviewer <host-lan-ip>:6001
```

You should see the Quick window. With `--remote-input` you can also drive it; without it the window is view-only.

## Expected result

```text
APP_READY
PLATFORM_NAME=<windows|xcb>
HYREMOTE_START=Running
...
HYREMOTE_STOP=Stopped
```

## V0.1 boundary

- reference matrix: **Windows x86_64** and **Linux x86_64**, **Qt 6.8.3**;
- V0.2 is a **LAN-capable Developer Preview** - `0.0.0.0:5921` listener, remote input off by default, no production, GA
  or Internet-safe claim;
- the `HyRemote` QML module is a peer frontend, not a preview of the C++ one. If you specifically want the declarative
  API, see the QML examples; this example is the same product reached through the C++ facade.
