# Remote input architecture

Status: **accepted direction for #400 input refactor**.

This note supersedes the input-ingress choice made by #401. The product constraints from #401 remain: HyRemote is application-scoped, low-intrusion, cross-platform Qt remote access; C++, QML, Generic and QPA are peer integration frontends. What changes is only the layer where pointer input enters Qt.

## Decision

```text
Viewer / protocol
    -> transport-neutral raw InputEvent
    -> Runtime admission / lifecycle / isolation
    -> Qt window-system ingress
    -> Qt input processing
    -> QWidgetWindow / QQuickWindow
    -> Widgets / Qt Quick
```

Pointer input enters Qt through `QWindowSystemInterface`. HyRemote does not synthesize application-level `QMouseEvent`/`QWheelEvent` semantics for controls.

The Qt private dependency is intentionally isolated to `src/runtime/src/detail/qt_window_system_input.*`. Frontends and target adapters do not include QPA/private headers.

## HyRemote owns

- protocol -> transport-neutral raw input facts;
- source framebuffer coordinates and mapping into the selected remote surface;
- bounded input admission and explicit backpressure;
- protected release admission and disconnect/shutdown balancing;
- multi-viewer contribution/arbitration in the transport/runtime layer;
- target/session/application isolation;
- the thin Qt-version-specific window-system ingress shim;
- surface discovery/selection as a separate Runtime concern.

## Qt owns

After a raw pointer fact crosses the window-system boundary, Qt owns:

- child/item hit testing;
- mouse grab and release routing;
- single/double-click classification;
- movement/distance click policy;
- wheel propagation/routing;
- enter/leave/hover;
- context-menu synthesis and platform trigger convention;
- Widgets and Qt Quick pointer delivery semantics;
- control-specific behaviour.

HyRemote must not add control-specific compatibility rules for `QPushButton`, `QComboBox`, `QListView`, `QMenu`, `MouseArea`, `PointerHandler`, or similar controls.

## Pointer queue rule

Pointer history is semantic input to Qt. Runtime therefore must not coalesce or evict accepted pointer moves and later reconstruct their meaning with a HyRemote state machine.

The normal lane stays bounded. When full, admission fails explicitly and applies backpressure. Protected releases keep their reserved lane so accepted held state can always be balanced.

## Timestamp rule

Runtime records a monotonic timestamp when the raw remote fact is accepted, before GUI-thread queue delay, and passes it to `QWindowSystemInterface`. The timestamp is a transport/runtime fact, not a HyRemote click-classification input. Remote input deliberately uses one consistent monotonic domain; it is not coupled to an uninstalled Qt private global timer.

## Deliberate non-goals of this change

This input-ingress change does not solve unrelated ownership layers:

- #404 transient top-level capture/composition and surface selection;
- #362 OS active-window activation;
- RFB client framebuffer-generation / resize coordinate synchronization;
- desktop-wide native input injection;
- touch/tablet protocol expansion.

Those concerns remain separate so the pointer ingress stays small.

## Private API maintenance

`QWindowSystemInterface` is QPA/private API. The maintenance rule is simple: compile the shim against the same Qt private development package used by the application, keep private includes confined to one translation unit, and qualify supported Qt versions with behavior tests. Do not mirror Qt private implementation details in HyRemote.

## Architecture invariant

If a future input bug appears, first ask whether Qt already owns that semantic below the window-system boundary. If yes, fix the ingress/surface/raw-fact correctness; do not add another HyRemote semantic emulator.
