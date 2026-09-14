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
  same parent chain a same-scene QML overlay / `Popup.Item` uses, and deliberately *not* a
  descendant of the application's root item. It is a plain item, not a Qt Quick Controls
  `Popup`/`ToolTip` in a specific popup mode; `Popup.Window` and `Popup.Native` are not
  exercised here.

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
| `--consumer-service-ms <n>` | serial consumer service time per frame (`1000/n` = sustained fps; 0 = no consumer modelled) |
| `--completed-queue-capacity <n>` | bound on the completed queue depth (0 = unbounded). The frame being serviced is owned in addition, so peak ownership is `capacity + 1` frames |
| `--backpressure <name>` | `none`, `drop-oldest` (latest-frame-wins) or `producer-throttle` (admission control) |
| `--no-drain` | stop as soon as the last request completes instead of letting the consumer drain the queue |
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

# single serial 10 fps consumer, unbounded completed queue
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 60 --max-inflight 4 \
    --consumer-service-ms 100 --json /tmp/consumer-unbounded.json
# bounded queue of 2 with latest-frame-wins (drops, capture keeps its rate)
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --max-inflight 4 \
    --consumer-service-ms 100 --completed-queue-capacity 2 --backpressure drop-oldest --json /tmp/consumer-dropoldest.json
# same bound, but the producer is throttled instead of dropping frames
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 60 --max-inflight 4 \
    --consumer-service-ms 100 --completed-queue-capacity 2 --backpressure producer-throttle --json /tmp/consumer-throttle.json

ctest --test-dir build/async-spike --output-on-failure
```

## Consumer model

`--consumer-service-ms` plus `--completed-queue-capacity` implement **one serial consumer**:
completed frames enter a single queue, the consumer takes one frame at a time and is busy with
it for the configured service time, so two frames are never serviced in parallel and the
sustained rate is `1000 / serviceMs` frames per second. `--backpressure` decides what happens
when the queue is at capacity:

| Strategy | Behaviour | Applies to |
|---|---|---|
| `none` | the queue is unbounded; the backlog grows and is drained after the load | measuring the unbounded case |
| `drop-oldest` | the oldest queued frame is discarded so the newest survives; the producer is never blocked | transport drop policy (recommended) |
| `producer-throttle` | admission control stops issuing while `queued + in flight` reaches the capacity | producer-side alternative, couples capture to the consumer |

The reported bounds are `maxQueueDepthObserved <= completedQueueCapacity` and
`maxOwnedFramesObserved <= completedQueueCapacity + 1` (the frame being serviced).

Requirements: Qt 6.8 or newer with Core, Gui, Widgets, Quick, QuickWidgets, OpenGLWidgets
and OpenGL. `QtQuick3D` is optional; the `quick3d` scene is compiled out when the module is
missing. On Windows the Qt `bin` directory must be on `PATH`.

## Interpreting the numbers

- The `--dry-run --interval-ms <n>` control should be run with `--interval-ms` equal to the
  real run's average latency, so that the loop duration and frame count are comparable.
- The `--paced` control separates "pipelining collapses content onto one frame" from "the
  harness issued requests faster than the render loop could serve them".
- In a consumer run, `producerPerSecond` and `consumerPerSecond` are measured over the load
  window (until the last request completed), while `delivered`/`dropped`/`dropRatio` cover the
  whole run including the drain. `delivered + dropped == completed` holds in every
  configuration.
- The synchronous baseline is executed in the same session because the Direct3D 11
  `grabWindow()` behaviour is host-dependent (see `docs/capture-spike.md` section 8.2).
- The hidden-window synchronous grab runs in a child process because it can stall.

## Known limitations of the harness

- The custom-FBO scene renders continuously by design, so its frozen state still advances the
  FBO render counter; its fidelity comparisons are therefore tolerance-based and the report
  includes the decoded counter of both images so the difference can be attributed.
- The pipeline decodes only the tick patch and the custom-FBO counter; other content
  differences between frames are covered by the fidelity probe, not by the pipeline.
- The consumer model holds the decoded image; it does not model a real transport (no
  encoding, no variable service time, no transport-side buffering). It models *one serial
  consumer with a fixed service time* and a queue with a fixed capacity in frames.
- An earlier revision of the consumer probe released each frame independently
  (`ready + 100 ms`), which let several frames be consumed in parallel and allowed the
  retained-frame count to overshoot the configured window by up to K. That model and its
  numbers are withdrawn; the current probe serializes the service and bounds the queue.
- The visibility probes call `hide()`; `minimized` is not tested.
- The harness releases the `QQuickItemGrabResult` and its pixels as soon as it has decoded
  them. An earlier revision kept a `result -> connection -> lambda -> request -> result`
  reference cycle and measured its own retention instead of the capture path's; the evidence
  set was regenerated after the fix.
