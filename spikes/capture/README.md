# SPIKE-01 capture harness

Throwaway, non-production code for
[issue #3](https://github.com/skawu/HyRemote/issues/3). The results live in
[`docs/capture-spike.md`](../../docs/capture-spike.md).

Nothing in this directory is a HyRemote public API. It defines no HyRemote target,
contains no transport code, and is expected to be deleted once the capture
architecture decision is implemented for real.

## What it does

For each supported target family it builds an identical animated scene, captures it
through every plausible public Qt API, and records where the cost, the threads and the
buffer ownership actually land:

| Case | Target family | Capture paths |
|---|---|---|
| `widgets` | QWidget / raster | `QWidget::grab()`, `QWidget::render()` into a reused / fresh / borrowed-raw target |
| `quick` | QQuickWindow 2D | `QQuickWindow::grabWindow()`, `QQuickItem::grabToImage()` |
| `quick3d` | Quick3D | `QQuickWindow::grabWindow()`, `QQuickItem::grabToImage()` |
| `openglwidget` | QOpenGLWidget in a hierarchy | `grabFramebuffer()`, `grab()` of the GL widget, `grab()` of the parent |
| `quickwidget` | QQuickWidget in a hierarchy | `grabFramebuffer()`, `grab()` of the parent |
| `customfbo` | custom `QQuickFramebufferObject` | `QQuickWindow::grabWindow()`, `QQuickItem::grabToImage()` |

In addition, every run measures:

- capture cost (first call, average, p50, p95, max) per path, on the GUI thread. For the
  reused-target and fresh-target paths the measured region covers the complete operation
  (buffer allocation or clear plus the render), so the two are comparable;
- process CPU and resident memory trend per frame;
- GUI responsiveness through the delay of a precise 16 ms timer;
- damage through an application-level event filter, bound to one shared target widget.
  Child `QPaintEvent::region()` values are accepted only inside that target's window,
  translated into the target's coordinate system and clipped before they are unioned.
  `QEvent::UpdateRequest` is counted separately and application-wide because Qt Quick
  posts it without geometry;
- deterministic damage-mapping controls in the `widgets` case: one repaints exactly one
  known child widget and expects exactly its rectangle in target coordinates, one
  repaints a widget inside a separate top-level dialog and expects no damage in the
  target;
- producer pixel-storage identity for paths that own a target buffer, sampled with
  `QImage::constBits()` before the first write and after the last write of a capture.
  This is a real backing-store address, unlike `QImage::cacheKey()`, which mixes a serial
  number with a detach counter. Two synthetic controls (a shared write that must
  relocate, a unique write that must not) validate the probe on every capture;
- frame hand-off safety by handing frames to a worker thread that re-reads the pixels
  40 ms later, for both a `QImage` value and a borrowed raw-memory view;
- which thread renders the scene graph, through `QQuickWindow::beforeRendering` /
  `afterRendering`.

## Build

The harness is not part of the default root build. Configure it directly, or enable
`-DHYREMOTE_BUILD_SPIKES=ON` in the root build.

```bash
cmake -S spikes/capture -B build/spike-capture -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build/spike-capture
```

Requirements: Qt 6.8 or newer with Core, Gui, Widgets, Quick, QuickWidgets,
OpenGLWidgets and OpenGL. `QtQuick3D` is optional; the `quick3d` case is compiled out
when the module is missing.

### Windows notes

- The Qt `bin` directory must be on `PATH` when running, otherwise the executable
  fails with `STATUS_DLL_NOT_FOUND`. For the reference run that was the `bin` directory
  of the Qt 6.8.3 kit the harness was built with, i.e. `<QtRoot>\6.8.3\<toolchain>\bin`
  (the same `<CMAKE_PREFIX_PATH>/bin` the build used).
- The harness links `Qt::Gui`, so `WIN32_EXECUTABLE` is explicitly disabled: it is a
  console application and reports through stdout.
- A visible desktop session is required. An occluded window is throttled by the
  compositor, which distorts frame pacing; the samples raise and activate their window
  for that reason.

## Run

```bash
hyremote-capture-spike --list
hyremote-capture-spike --sample widgets --frames 150 --warmup 20 --json /tmp/widgets.json
hyremote-capture-spike --sample quick --rhi opengl --frames 150 --warmup 20 --json /tmp/quick.json
```

| Option | Meaning |
|---|---|
| `--sample <id>[,<id>...]` | capture case to run; repeatable; default is every case |
| `--frames <n>` | measured frames per run (default 120) |
| `--interval-ms <n>` | delay between captures; `0` means as fast as possible (default 16) |
| `--warmup <n>` | discarded warm-up frames (default 15) |
| `--runs <n>` | repeated start/stop runs to check that a path can be re-created |
| `--submit-every <n>` | hand every Nth frame to the ownership probe; `0` disables it |
| `--no-sink` | disable the ownership probe entirely |
| `--rhi <name>` | set `QSG_RHI_BACKEND` for the whole process (default: leave it to Qt) |
| `--disable-path <text>` | skip capture paths whose label contains `<text>`, for example `grabToImage` |
| `--json <path>` | write the machine-readable report |
| `--list` / `--help` | list cases / show usage |

Exit codes: `0` when every executed run captured at least one frame, `2` when a run
captured nothing, `1` for a usage error.

### Environment conflicts

`customfbo` requires `QSG_RHI_BACKEND=opengl` because `QQuickFramebufferObject` is an
OpenGL-specific Quick item. When a selection contains two incompatible requirements for
the same variable, the conflicting case is reported as `skipped` instead of being
executed with a misleading backend. `--rhi` overrides case requirements, which is how
the Direct3D 11 refutation run in the evidence set was produced.

## Tests

```bash
ctest --test-dir build/spike-capture --output-on-failure
```

The smoke tests only assert that each case can produce at least one frame. They are
short by design; the numbers that matter come from the longer documented runs.

## Known limitations of the harness

- The damage stream is a raw child-region union after mapping and clipping. It has no
  coalescing policy, it does not cover separate top-level windows, and it includes
  capture-induced repaints unless the snapshot is taken before capturing (which the
  harness does). The scenes are deliberately damage-heavy, so the reported ratios
  characterise those scenes and not a typical application.
- The storage-identity probe reports whether a capture replaced the pixel storage, not
  the size or the cost of that replacement. For the reused-target path that cost lands in
  the measured clear/render region; it is not isolated further.
- Two of the seven damage/measurement scenes are made deterministic on purpose (a
  non-focusable line edit and a determinate progress bar), because a blinking cursor or
  an indeterminate progress bar repaints asynchronously and would make the damage
  controls non-repeatable.
- The borrowed-buffer probe reads producer memory concurrently with the producer
  writing it. That is deliberate (it is the failure mode being demonstrated) and it is
  benign here because the buffer stays allocated and keeps its size for the whole run.
- Memory slopes over short runs are dominated by warm-up and by graphics driver
  caches; only the 500-frame run is used for a memory conclusion.
- The frame hand-off probe is not a transport. It exists only to answer the ownership
  question.
- Storage-identity numbers depend on `--submit-every`: the reused target only replaces
  its storage on captures that follow a hand-off. Run with `--no-sink` for the negative
  control.
