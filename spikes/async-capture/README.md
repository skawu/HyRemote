# Async capture spike (issue #16)

Throwaway, non-production code for
[issue #16](https://github.com/skawu/HyRemote/issues/16). The results live in
[`docs/async-capture-spike.md`](../../docs/async-capture-spike.md).

Nothing in this directory is a HyRemote public API. It defines no HyRemote target, contains
no transport code, and is expected to be deleted once the asynchronous capture design is
implemented for real.

The spike evaluates the **public asynchronous** capture path
(`QQuickItem::grabToImage()`) requirement by requirement before any lower-level GL/PBO,
render-thread or RHI mechanism is considered. It does not implement any lower-level path.

## Scenes

| Scene | Content |
|---|---|
| `quick2d` | `QQuickView` with an animated 2D item tree |
| `quick3d` | `QQuickView` with a `View3D` (MSAA) plus 2D QML content in the same root item |
| `customfbo` | `QQuickView` with a `QQuickFramebufferObject` whose renderer encodes its own render counter in its clear colour |
| `quickwidget` | `QQuickWidget` in a 1000x640 `QWidget` hierarchy |
| `openglwidget` | `QOpenGLWidget` in a `QWidget` hierarchy (control: no QML item tree) |

Every QML scene contains two deterministic landmarks:

- a **tick patch**: a 48x48 rectangle whose colour encodes a counter the harness writes
  before each request, so a captured image can be attributed to a request;
- an **overlay sibling**: a QML rectangle parented to `QQuickWindow::contentItem()`, i.e. the
  same parent chain QML overlays, popups and tooltips use, and deliberately *not* a
  descendant of the application's root item.

## Modes

| Mode | What it does |
|---|---|
| `composition` | samples the overlay location in a root-item grab, a `contentItem()` grab and a synchronous whole-window grab |
| `fidelity` | freezes the scene, pins the tick, then compares async `contentItem()`, async root item and synchronous whole-window captures pixel by pixel |
| `pipeline` | drives the asynchronous loop and measures latency, cadence, in-flight behaviour, completion order, tick lag, duplication, memory and GUI-thread impact |
| `sync-baseline` | measures the synchronous public capture for the same scene in the same session |
| `failure` | exercises the documented rejections (hidden window, detached item) and the recovery afterwards |
| `all` | composition + fidelity + pipeline + failure + sync baseline |
| `hidden-sync` | internal child-process mode: synchronous `grabWindow()` on a hidden window |

## Options

| Option | Meaning |
|---|---|
| `--scene <id>[,<id>...]` | scene to run; repeatable; default is every scene |
| `--mode <name>` | see above (default `all`) |
| `--requests <n>` | asynchronous requests in the pipeline mode (default 60) |
| `--sync-captures <n>` | synchronous baseline captures (default 60) |
| `--max-inflight <n>` | concurrent `grabToImage()` requests (default 4) |
| `--paced` | pump between individual requests, so each lands in its own frame |
| `--interval-ms <n>` | event-loop time granted between request slices |
| `--consumer-delay <ms>` | hold each completed frame this long (slow consumer) |
| `--consumer-window <n>` | stop issuing while this many frames are held (0 = unlimited) |
| `--dry-run` | run the pipeline loop without issuing any capture (memory control) |
| `--target <name>` | `contentItem` (default) or `rootItem` |
| `--rhi <name>` | set `QSG_RHI_BACKEND` (default: leave it to Qt) |
| `--json <path>` | write the machine-readable report |
| `--list` / `--help` | list scenes / show usage |

## Build and run

```bash
cmake -S spikes/async-capture -B build/async-spike -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build/async-spike

./build/async-spike/hyremote-async-spike --list
./build/async-spike/hyremote-async-spike --scene quick2d --mode all --requests 60 --max-inflight 4 --json /tmp/quick2d.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --max-inflight 1 --json /tmp/k1.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --dry-run --interval-ms 32 --json /tmp/control.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 60 --consumer-delay 100 --consumer-window 2 --json /tmp/backpressure.json
ctest --test-dir build/async-spike --output-on-failure
```

Requirements: Qt 6.8 or newer with Core, Gui, Widgets, Quick, QuickWidgets, OpenGLWidgets
and OpenGL. `QtQuick3D` is optional; the `quick3d` scene is compiled out when the module is
missing. On Windows the Qt `bin` directory must be on `PATH`.

## Interpreting the numbers

- The `--dry-run --interval-ms <n>` control should be run with `--interval-ms` equal to the
  real run's average latency, so that the loop duration and frame count are comparable.
- The `--paced` control separates "pipelining collapses content onto one frame" from "the
  harness issued requests faster than the render loop could serve them".
- The synchronous baseline is executed in the same session because the Direct3D 11
  `grabWindow()` behaviour is host-dependent (see `docs/capture-spike.md` section 8.2).
- The hidden-window synchronous grab runs in a child process because it can stall.

## Known limitations of the harness

- The custom-FBO scene renders continuously by design, so its frozen state still advances the
  FBO render counter; its fidelity comparisons are therefore tolerance-based and the report
  includes the decoded counter of both images so the difference can be attributed.
- The pipeline decodes only the tick patch and the custom-FBO counter; other content
  differences between frames are covered by the fidelity probe, not by the pipeline.
- The consumer model holds the decoded image; it does not model a real transport's queue.
- The harness releases the `QQuickItemGrabResult` and its pixels as soon as it has decoded
  them. An earlier revision kept a `result -> connection -> lambda -> request -> result`
  reference cycle and measured its own retention instead of the capture path's; the evidence
  set was regenerated after the fix.
