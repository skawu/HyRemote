# #401 Input Backend Architecture Preflight

Bounded architecture preflight for the remote-input injection layer, triggered by the real-user
defect #400 and required before #400's production implementation. #400 remains the user-defect
owner and the final acceptance authority; this note only freezes *where* remote input should enter
the target application and what HyRemote may own below that point.

Baseline: `develop = e9d25502ce86fb37ba5409d1f0e2bfbd842cd2bc` (branch
`spike/401-input-backend-preflight`). All measurements below were taken on that tree, Qt 6.8.3,
Windows x86_64, `QT_QPA_PLATFORM=offscreen`.

Evidence classes used throughout, as required by the #401 execution lock:

```text
OBSERVED                  measured in this repository on this exact tree/Qt build
DOCUMENTED_PLATFORM_FACT  documented behaviour of Qt / the OS, not measured here
HYPOTHESIS                plausible but not verified here; never treated as fact
```

## 1. Frozen upper architecture

```text
Viewer / transport
  -> transport-neutral raw input facts
  -> Shared Runtime bounded state/queue/isolation
  -> Input Backend
  -> target application / Qt
```

`Integration technology != input injection technology`. C++ / QML / Generic / QPA stay peer
product integration technologies; this preflight does not rank them and does not make any of them
depend on Qt private ABI.

## 2. Deterministic reproduction of #400 (OBSERVED)

Three fixtures drive the shipped path (`InputSink::post` -> bounded mailbox -> GUI drain ->
target adapter), not a direct adapter call:

```text
hyremote-widgets-input-preflight-test   Widgets matrix + host-active observation
hyremote-quick-input-preflight-test     Quick matrix
hyremote-rfb-input-preflight-test       production RFB pointer parser over a real socket
```

All three pass; the interesting output is the classification lines they print.

First divergence per scenario, using the #401 vocabulary:

| Scenario | First divergence | Evidence |
| --- | --- | --- |
| Widgets valid remote double click (probe widget) | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: `presses=2 releases=2 dblClicks=0`; Qt receives `Press,Release,Press,Release` and never `MouseButtonDblClick` |
| Widgets outside-interval pair | `NO_DEFECT` | OBSERVED: two singles, which is also the correct platform answer |
| Widgets outside-distance pair | `NO_DEFECT` | OBSERVED: two singles |
| `QPushButton` single click | `NO_DEFECT` | OBSERVED: one `clicked()` |
| `QCheckBox` single click | `NO_DEFECT` | OBSERVED: toggles once |
| `QSlider` press/drag/release | `NO_DEFECT` | OBSERVED: value moves; implicit grab + held state + release routing all work |
| `QListView` double-click activation | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: `activated()` never fires for a valid double click, so view activation / inline edit / open-on-double-click are unreachable remotely |
| `QComboBox` popup item interaction | `TOPLEVEL_INGRESS` | OBSERVED: the click opens the popup, but the popup is its own top-level window and `root->childAt()` cannot reach it: the item list is not clickable |
| `QLineEdit` focus, item-view single-click selection | `OS_ACTIVATION_FOCUS` / `UNMEASURED` | OBSERVED only as a harness gap: the offscreen scratch scene is not shown and offscreen grants no activation, so focus/selection fidelity is not measurable here (see §7) |
| Quick MouseArea/Button/CheckBox/Slider single click, drag | `NO_DEFECT` | OBSERVED: Qt Quick resolves the item under the point itself; control-local semantics work |
| Quick MouseArea `onDoubleClicked` | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: no `DblClick` event is ever delivered into the window |
| Quick `Popup` (in-window overlay) | `NO_DEFECT` | OBSERVED: `onOpened` fires and a click inside the popup reaches its content |
| Quick focusable text target | `OS_ACTIVATION_FOCUS` / `UNMEASURED` | OBSERVED only as a harness gap (see §7) |
| RFB valid double-click envelope | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: the parser emits the faithful `move/left-down/left-up/left-down/left-up` facts (two presses, two releases) and routing delivers them; the semantic is lost only at application-level Qt event synthesis |
| RFB outside-interval pair | `NO_DEFECT` | OBSERVED: two singles; the wire carries no interval information at all |
| RFB outside-distance pair | `NO_DEFECT` | OBSERVED: two singles |
| RFB different-receiver pair | `NO_DEFECT` | OBSERVED: each widget receives its own press, and no cross-receiver double click can exist |

Two user-visible halves of #400 are therefore reproduced with different root causes:

```text
"double-click does not work"      -> QT_SEMANTIC_CLASSIFICATION (platform synthesis bypassed)
"some controls do not react"      -> TOPLEVEL_INGRESS (separate top-level popup surface unreachable)
```

Host ACTIVE / INACTIVE: the ACTIVE case is observed (in-window delivery works on a shown, active
host window, and the adapter does not depend on activation state). The INACTIVE case is **not
reproducible** in this harness: offscreen has no competing top-level window that can steal
activation. It is recorded as `OS_ACTIVATION_FOCUS` with the architectural consequence stated
explicitly rather than measured: an application-level event delivery backend cannot activate the
host window, which is the #362 class of limitation.

## 3. Current responsibility map (OBSERVED, with the code that owns it)

| Responsibility | Today | Where |
| --- | --- | --- |
| top-level selection | HyRemote Runtime: the configured root widget / window | `src/runtime/src/widgets/widget_target.cpp`, `src/runtime/src/quick/quick_target.cpp` (target component) |
| child / item hit test (Widgets) | HyRemote, manually: `root->childAt(rootPoint)` | `widget_target.cpp:503` |
| child / item hit test (Quick) | Qt Quick itself (adapter sends to the window) | `quick_target.cpp:654` |
| button held state | HyRemote, per button bitmask | `widget_target.cpp:555-560`, `quick_target.cpp:638-643` |
| implicit grab (Widgets) | HyRemote, per-button receiver bookkeeping | `widget_target.cpp:484`, `507-518`, `528-531`, `556`, `573-574` |
| implicit grab (Quick) | Qt Quick itself | item delivery via the window |
| click classification | no one: every button transition becomes a plain press/release | `widget_target.cpp:557/560`, `quick_target.cpp:640/643` |
| double-click | no one (`MouseButtonDblClick` is never produced anywhere) | OBSERVED repo-wide: zero `DblClick`/`doubleClick` references |
| focus | partly Qt (Widgets `QApplication::focusWidget()`, Quick active focus) | `widget_target.cpp:349` (`keyboardReceiver`) |
| popup / overlay | Widgets: nobody (separate top-level, `childAt()` cannot reach it); Quick: Qt Quick overlay inside the same window | reproduction table §2 |
| enter/leave/hover | nobody: only `MouseMove` is synthesized, no `QEvent::Enter/Leave/HoverMove` | `widget_target.cpp:550`, `quick_target.cpp:634` |
| wheel | HyRemote, manual `QWheelEvent` | `widget_target.cpp:536-548`, `quick_target.cpp:619-630` |
| key / text | HyRemote, manual `QKeyEvent`/`QInputMethodEvent` with its own held-key bookkeeping | `widget_target.cpp:577-622`, `quick_target.cpp:657-...` |

The capabilities HyRemote currently hand-builds that Qt/OS would otherwise own:

```text
OBSERVED hand-built and low-risk to keep:  bounded queue/admission, held state, implicit grab,
                                           release routing, coordinate mapping, wheel/key/text
                                           translation (transport-neutral facts -> Qt events)
OBSERVED hand-built and demonstrably incomplete: click/double-click classification,
                                           enter/leave/hover, popup/overlay reach,
                                           item-view activation
```

The incomplete set is exactly the set that grows control-by-control if each missing semantic is
patched independently, which is the risk #401 exists to stop.

## 4. Backend family A: Public-Qt application-scoped

Ingress levels that exist today (OBSERVED in this tree):

```text
A1  QCoreApplication::sendEvent(<receiver>, QMouseEvent/...)    <- used by both adapters today
A2  QCoreApplication::sendEvent(<QQuickWindow>, QMouseEvent)    <- used by the Quick adapter today
A3  QTest::mouseClick / qt_handleMouseEvent                     <- test-only, links Qt6::Test
```

Answers to the #401 questions for the public-Qt family:

| Question | Answer | Class |
| --- | --- | --- |
| Can Qt own child/item routing? | Widgets: only below the receiver you pick — `childAt()` is HyRemote's job, then Qt routes no further. Quick: yes, fully (Qt Quick resolves the item and dispatches, including hover/grab/focus). | OBSERVED |
| Can Qt own click/double-click classification? | No public Qt ingress performs it. Posting events to a receiver or a window delivers exactly what HyRemote constructs; no Qt layer promotes a press to `MouseButtonDblClick`. | OBSERVED |
| Can Qt own the implicit grab? | Widgets: no (HyRemote's `buttonReceivers` reproduce it). Quick: yes. | OBSERVED |
| Can Qt own popup/overlay routing? | Quick: yes for in-window overlays (measured reachable). Widgets: no for the classic top-level popup, because the target model is a single root and no ingress exists for another top-level surface. | OBSERVED |
| Can Qt own focus transitions? | Partly: pointer clicks move focus through the delivered press (Widgets) / Qt Quick's own focus handling. Not measurable offscreen in this harness. | OBSERVED (partial), gap in §7 |
| What must a public-Qt backend own at minimum? | The transport-neutral fact -> Qt event translation, receiver/item resolution where Qt does not do it, per-button held state, implicit-grab bookkeeping for Widgets, and whatever Qt semantic is missing at that ingress (today: double-click; also enter/leave/hover). | OBSERVED |

Why there is no lower public ingress (OBSERVED): the platform-level entry points are not public
API. `QWindowSystemInterface` is a Qt-private header — it is absent from this SDK's public include
tree (`include/QtGui/qwindowsysteminterface.h` does not exist; the private header directory is not
part of the binary SDK install).

That single fact is what makes the deferral impossible through public Qt: for a physically
attached mouse the DblClick semantic is produced *below* the public event-delivery API, so an
application-scoped backend cannot inherit it by choosing a different public call.

Ingress probe (non-product, run outside the repository; recorded here so the fact is auditable):

```text
method     a ~60-line throwaway program that links Qt6::Widgets/Qt6::Test, shows a recorder widget
           and issues two rapid QTest::mouseClick() calls (raw window-system input through
           QWindowSystemInterface, i.e. the same entry a real device uses) at the same point, then
           prints the ordered QEvent types the widget received. Not committed on purpose: Qt6::Test
           is a test-only module and adding it as a build dependency of a repository target would be
           a dependency-policy change, not a preflight artifact.
observed   policy: intervalMs=400 distancePx=5
           rapid same-spot pair            -> Press(Left), Release(Left), Press(Left), Release(Left)   [D=0]
           600 ms gap pair                 -> Press, Release, Press, Release                        [D=0]
           far-apart pair                  -> Press, Release, Press, Release                        [D=0]
           rapid right-button pair         -> Press(Right), Release(Right), Press(Right), Release(Right) [D=0]
           long first press + quick second -> Press, Release, Press, Release                        [D=0]
           after setDoubleClickInterval(120), 250 ms gap -> Press, Release, Press, Release          [D=0]
conclusion OBSERVED: on this Qt build, Qt itself never promotes a press to MouseButtonDblClick
           without platform participation. The double-click a physically attached mouse produces is
           therefore created by the platform layer (Windows window class with CS_DBLCLKS, or the
           X11 backend), i.e. below the public event-delivery API that an application-scoped
           backend can reach.
```

This is exactly why the selected direction in §8 keeps a *finite* classification responsibility in
HyRemote instead of claiming Qt can be given the semantic through a different public call.

## 5. Backend family B: Qt-private / QPA

What would be needed: enter Qt at `QWindowSystemInterface` (or an equivalent private delivery
path) so `QGuiApplicationPrivate::processMouseEvent` and the target's own window/item machinery
run exactly as for a physical device.

```text
Capability                                            Assessment                                Class
-----------------------------------------------------------------------------------------------
lower ingress exists                                  yes, Qt-private headers + private symbols HYPOTHESIS
Qt would then own platform/window input semantics     plausible: same path as platform events   HYPOTHESIS
ABI constraint                                        QPA/private entry points are bound to the
                                                      exact Qt version and build configuration   DOCUMENTED_PLATFORM_FACT
platform constraint                                   the platform plugin is owned by one
                                                      integration instance; a QPA route is
                                                      already the platform plugin, C++/QML/
                                                      Generic routes are normal applications      DOCUMENTED_PLATFORM_FACT
public-Qt (C++/QML/Generic) may depend on it           forbidden by #401 and by the product's
                                                      public-Qt promise                           decision
```

Consequence: the private/QPA family is a *possible* future capability of the QPA route only, not a
backend for the public-Qt routes. It cannot be the single answer to #400, because the same defect
must be fixed for C++ and QML applications whose build cannot depend on Qt private ABI.

## 6. Backend family C: Native OS / virtual input

Not selected. The rejection is driven by the product contract (application-scoped, low intrusion,
portable, low privilege) and by platform constraints, not by whether it would restore double click:

| Aspect | Assessment | Class |
| --- | --- | --- |
| fidelity | highest: the OS produces click/double-click/activation for the target window | DOCUMENTED_PLATFORM_FACT |
| application isolation | violated: injection targets the desktop input stream, not our application | DOCUMENTED_PLATFORM_FACT |
| whole-desktop side effects | present: input can reach other applications and windows | DOCUMENTED_PLATFORM_FACT |
| privileges | Windows `SendInput` needs no elevation but is desktop-wide; Linux `uinput` needs device access (root/udev); XTest needs the X server | DOCUMENTED_PLATFORM_FACT |
| inactive-window behaviour | improves (OS-level activation), but by acting outside our application | HYPOTHESIS |
| Wayland | clients cannot inject into other clients; the sanctioned path is portal-brokered (libei/EIS) with session consent | HYPOTHESIS |
| multi-instance isolation | two HyRemote instances would fight over the same desktop input stream | HYPOTHESIS |
| deployment / embedded | needs platform agents or device nodes; EGLFS/embedded has no such desktop input layer | HYPOTHESIS |
| security surface | largest: a remote viewer could drive the whole desktop, not one application | DOCUMENTED_PLATFORM_FACT |
| deployability, testability | worst: needs a real desktop session, cannot be covered by the offscreen fixtures | HYPOTHESIS |

## 7. Decision table

Each row is tagged; `HYPOTHESIS` rows are explicitly not architecture facts.

| Criterion | A. Public-Qt application-scoped | B. Qt-private / QPA | C. Native OS / virtual input |
| --- | --- | --- | --- |
| fidelity (normal Qt interaction) | medium: Qt owns routing where it is given the window (Quick) or the receiver (Widgets); missing semantics must be restored by HyRemote (OBSERVED) | high: same machinery as platform input (HYPOTHESIS) | highest: OS produces every semantic (DOCUMENTED_PLATFORM_FACT) |
| application isolation | full: every event is delivered inside our process to our target (OBSERVED) | full (HYPOTHESIS) | violated (DOCUMENTED_PLATFORM_FACT) |
| public/private ABI cost | none (OBSERVED) | exact-Qt-version private ABI (DOCUMENTED_PLATFORM_FACT) | none in-process, but platform agents/APIs (HYPOTHESIS) |
| Windows/Linux portability | one implementation, portable (OBSERVED) | per-Qt-version, per-platform plugin (HYPOTHESIS) | per-platform implementations (DOCUMENTED_PLATFORM_FACT) |
| Wayland feasibility | works (no desktop coupling) (OBSERVED: no platform coupling in the adapters) | requires being the platform plugin (HYPOTHESIS) | portal/libei only; XTest/uinput unusable for other clients (HYPOTHESIS) |
| permissions | none beyond the application (OBSERVED) | none (HYPOTHESIS) | device access / session portals (DOCUMENTED_PLATFORM_FACT) |
| inactive-window behaviour | cannot activate the host window by itself (OBSERVED consequence: app-level delivery only) | could inherit platform activation semantics if implemented inside the plugin (HYPOTHESIS) | solves activation, but desktop-wide (DOCUMENTED_PLATFORM_FACT) |
| multi-viewer isolation | already proven: bounded mailbox + per-button held state + protected releases + disconnect cleanup (OBSERVED: existing tests pass) | would need reimplementation of the same bounded state (HYPOTHESIS) | two viewers drive the same desktop input stream (HYPOTHESIS) |
| multi-instance isolation | per-process, no interference (OBSERVED) | per-process (HYPOTHESIS) | instances interfere (HYPOTHESIS) |
| Widgets behaviour | routing, grab, drag, wheel, key/text work; click classification, hover, popup reach missing (OBSERVED) | plausible best-in-class (HYPOTHESIS) | works, but not application-scoped (DOCUMENTED_PLATFORM_FACT) |
| Quick behaviour | item routing, drag, popup overlay work; double-click missing (OBSERVED) | plausible (HYPOTHESIS) | works, but not application-scoped (DOCUMENTED_PLATFORM_FACT) |
| popup / multi-window | Quick overlay reachable; Widgets top-level popup unreachable in the single-root model (OBSERVED) | plausible (HYPOTHESIS) | works (DOCUMENTED_PLATFORM_FACT) |
| Qt5/Qt6 implications | Qt5 has the same public ingress shape; no version-specific code added (HYPOTHESIS for Qt5 detail) | version-bound twice (Qt5 and Qt6 plugins) (DOCUMENTED_PLATFORM_FACT) | unaffected by Qt version (DOCUMENTED_PLATFORM_FACT) |
| EGLFS/embedded implications | works: nothing depends on a desktop input layer (OBSERVED: adapters are Qt-only) | QPA is the embedded story already; still private (HYPOTHESIS) | not available on EGLFS-style deployments (HYPOTHESIS) |
| security surface | unchanged: one bounded mailbox per target, no new capability (OBSERVED) | small in-process increase; private ABI risk (HYPOTHESIS) | unacceptable increase (DOCUMENTED_PLATFORM_FACT) |
| deployability | unchanged (OBSERVED) | Qt-version-locked builds (DOCUMENTED_PLATFORM_FACT) | platform agents/privileges (HYPOTHESIS) |
| testability | already covered by deterministic offscreen fixtures including the new #401 ones (OBSERVED) | needs an exact-Qt build and a real platform plugin (HYPOTHESIS) | needs a real desktop session; not CI-testable (HYPOTHESIS) |

## 8. Selected direction

**Selected: A. Public-Qt application-scoped backend, extended with a small finite compatible
semantics layer owned by HyRemote — and no control-type-specific special cases.**

Why:

1. It is the only family that simultaneously preserves the product contract that #400 must not
   break: application scoping, low intrusion, public-Qt compatibility for C++/QML/Generic,
   Windows/Linux portability, no privileges, multi-viewer/multi-instance isolation, and it is the
   only one already covered by deterministic in-repo fixtures (OBSERVED).
2. The measured gap is small and *finite*: exactly one missing Qt semantic on both adapters
   (`MouseButtonDblClick`), plus one surface-reach gap (Widgets top-level popup) that is a target
   model question rather than a per-control question (OBSERVED).
3. Families B and C would restore the same semantics only by giving up something the product
   contract will not give up (public-Qt compatibility / application isolation).

### HyRemote must own (finite, frozen)

```text
timestamp preservation               the accepted-arrival time of a raw input fact, so a busy GUI
                                     thread cannot change what the user did
minimal pointer sequence classification   only the semantic Qt itself would have produced
                                     (press / double-click), never control-specific behaviour
bounded state                        the existing bounded mailbox + per-button held state +
                                     protected releases stay the only state
receiver identity                    a pair can only combine for the same receiver
```

### Qt / OS must own (explicitly not HyRemote's job)

```text
control routing and control semantics (QPushButton, QCheckBox, QSlider, item views, combo box)
item routing, hover, implicit grab and focus inside Qt Quick
popup/overlay behaviour of the surfaces the target model can reach
activation/focus policy of the host window (the #362 class stays outside HyRemote)
all normal Qt event semantics once an event with the right type reaches the right receiver
```

### Explicit non-goals (must not appear in the #400 implementation)

```text
no per-control branches (QPushButton/QComboBox/QListView/Popup special cases)
no RealVNC-specific logic and no RFB protocol extension
no Qt private ABI (QWindowSystemInterface/QPA) dependency in C++/QML/Generic paths
no native OS injection backend
no #362 activation workaround
no public API/ABI redesign; no change to normalised InputEvent's public contract
```

## 9. Rejected alternatives

Public-Qt alternatives rejected:

```text
A3 QTest-driven injection as a product backend      test-only API, links Qt6::Test, would not
                                                    change the missing semantic anyway (OBSERVED)
"A4" no lower public ingress than today's           the platform entry point is not public API
     sendEvent to receiver/window                   (OBSERVED); therefore expecting Qt to own
                                                    double-click without any HyRemote layer is not
                                                    a possible design, only an aspiration
per-control reconstruction                          forbidden by #401 and rejected here: it grows
                                                    without bound and duplicates Qt's own behaviour
```

Qt-private / QPA alternatives rejected:

```text
using QWindowSystemInterface in the Widgets/Quick adapters   forbidden: binds public-Qt routes to
                                                             Qt private ABI (decision, #401 lock)
making the QPA route the input backend for all four routes   violates the peer-route model
"fix it only for QPA"                                        leaves C++/QML users with the defect
```

Native OS alternatives rejected:

```text
Win32 SendInput / PostMessage            desktop-wide injection and/or bypasses normal semantics
XTest on X11                             desktop-wide, needs the X server, no Wayland future
uinput device injection                  requires elevated privileges / device access
libei / EIS portal session               needs a user-consented portal session per session; adds a
                                         platform daemon dependency and an OS-level grant for
                                         something the product only needs inside its own process
```

## 10. #400 implementation slices after this preflight

```text
S1  arrival-time envelope (Widgets + Quick): mailbox entry carries the accepted-arrival time
    (internal only; the public normalised InputEvent contract is unchanged)
S2  shared minimal press-sequence classification helper in runtime detail (Widgets + Quick),
    using Qt's own policy values (QStyleHints interval/distance) and receiver identity
S3  deliver MouseButtonDblClick in place of the second qualifying press (Widgets + Quick),
    keeping every existing contract: bounded mailbox, protected releases, coalescing, held state,
    implicit grab, drag, disconnect/shutdown cleanup, multi-viewer isolation, DPR mapping
S4  Product decision needed (not necessarily code): what the target model does about Widgets
    top-level popups. Options: document the limitation, or extend the target model to a bounded
    multi-surface route. #400 must not special-case QComboBox.
S5  Enter/leave/hover: keep out of #400 unless the amended scope says otherwise; it is a separate
    fidelity slice with its own tests.
```

```text
T1  Widgets double-click semantics + control matrix (one DblClick, no duplicate activation)
T2  Quick double-click semantics (MouseArea onDoubleClicked, cross-item negative)
T3  production-RFB sequence tests (valid envelope / outside interval / outside distance /
    different receiver)
T4  regression: existing widgets/quick routing, backpressure, capture(+DPR), mailbox admission,
    RFB multi-client and wire robustness keep passing unchanged
```

## 11. Relation to #362 and known limitations of this preflight

```text
#362 (inactive host window)   separate class: OS activation/focus. An application-scoped backend
                              cannot activate another application's window, so this stays a
                              documented requirement (#362), never a HyRemote injection workaround.
```

Known limitations of this note:

```text
1  focus and item-view single-click selection were not measurable in this offscreen harness;
   they are recorded as UNMEASURED gaps, not as product defects, and need a real desktop run.
2  HOST_INACTIVE is not reproducible offscreen; its row is architectural analysis, not measurement.
3  the Qt-private and native-OS rows marked HYPOTHESIS are not verified in this repository. They
   are rejected on product-contract and documented-platform grounds, not on measured telemetry.
4  the Widgets popup finding is measured for the single-root target model; the composite/automatic
   Runtime route is a separate target model and was not re-measured here.
5  no physical RealVNC viewer run was part of this preflight; the reproduction is deterministic
   in-process (RFB transport + adapter), which is the layer #401 asked to freeze.
```
