# Qt Widgets capture and input baseline

Status: production integration slice for #6 / V0.0.1.0.

## Product boundary

Normal applications attach a `QWidget` target through `HyRemote::RemoteAccess`; they do not create
or configure a `CaptureSource` or `InputSink` directly. The built-in adapter is selected internally
when the attached `QObject` is a `QWidget`.

When remote input is enabled, the same built-in adapter supplies the normalized QWidget input sink.
The application-facing API remains `HyRemote::RemoteAccess`; Qt Widgets is an implementation
dependency of the installed HyRemote package when this adapter is present.

## Capture path

```text
Core scheduler thread
    -> WidgetCaptureSource::requestFrame()
    -> queued invocation on the Qt application / GUI thread
    -> QWidget::render()
    -> CpuFrameStorage-owned RGBA8888 premultiplied pixels
    -> RemoteFrame { Completion PTS, FullFrame damage }
    -> Core bounded mailbox
```

Properties of the baseline:

- QWidget is touched only on the Qt GUI thread;
- each published frame owns its pixel storage for the complete `RemoteFrame` lifetime;
- no borrowed/reused `QImage` pointer is published;
- DPR is applied to the captured pixel dimensions exactly once;
- capture request time remains diagnostic; completion time supplies the baseline content PTS;
- damage is conservatively `FullFrame` until production damage tracking is justified;
- separate top-level dialogs, menus and popups are not implicitly composited into one QWidget
  target; that compatibility behavior remains explicit in #6 acceptance evidence;
- `stop()` suppresses queued-but-not-published frames and waits for any already-entered Core
  callback to drain.

## Remote input path

```text
transport-specific normalization
    -> hyremote::InputEvent
    -> Core InputSink::post()
    -> queued invocation on the Qt application / GUI thread
    -> QWidget / focused descendant
```

The V0.0.1.0 baseline supports pointer move, left/middle/right button transitions, logical wheel
steps, logical key press/release, modifier state, and committed UTF-8 text. Pointer coordinates are
mapped from the exact captured frame viewport to current QWidget logical coordinates through the
Core mapping contract; DPR is not applied twice.

`InputSink::post()` only queues GUI work and never waits for QWidget event processing. Queued input
is dropped if the sink is destroyed before delivery. Text is injected as committed input-method
text; full IME pre-edit/composition parity remains outside the minimum contract.

This is the correctness baseline, not the final performance ceiling. Reuse pools, damage-aware
rendering, GPU paths and external storage can replace the capture implementation later without
changing `HyRemote::RemoteAccess`.
