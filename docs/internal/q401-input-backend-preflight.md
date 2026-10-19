# #401 Input Backend Architecture Preflight

Bounded architecture preflight for the remote-input injection layer, triggered by the real-user
defect #400 and required before #400's production implementation. #400 remains the user-defect
owner and the final acceptance authority; this note only freezes *where* remote input should enter
the target application and what HyRemote may own below that point.

Baseline: `develop = e9d25502ce86fb37ba5409d1f0e2bfbd842cd2bc` (branch
`feature/401-input-backend-preflight`, PR #403; #402 with the earlier `spike/*` branch name was
superseded because the repository Git Flow policy accepts only `feature/<issue>-<topic>` and
`hotfix/<issue>-<topic>` heads). All measurements below were taken on that tree, Qt 6.8.3,
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
| `QComboBox` popup item interaction | `TOPLEVEL_INGRESS` | OBSERVED by a real attempt, not by code inspection: the remote click opens the popup; the fixture then derives the target row's real screen position from the popup's own geometry, maps it into the root target and sends a real press/release through the production path - `currentIndex` stays unchanged, so the item is not selected. Ownership of a bounded transient-surface model is #404 |
| `QLineEdit` focus, item-view single-click selection | `OS_ACTIVATION_FOCUS` / `UNMEASURED` | OBSERVED only as a harness gap: the offscreen scratch scene is not shown and offscreen grants no activation, so focus/selection fidelity is not measurable here (see §7) |
| Quick MouseArea/Button/CheckBox/Slider single click, drag | `NO_DEFECT` | OBSERVED: Qt Quick resolves the item under the point itself; control-local semantics work |
| Quick MouseArea `onDoubleClicked` | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: no `DblClick` event is ever delivered into the window |
| Quick in-window overlay surface | `NO_DEFECT` | OBSERVED and asserted (not printed): the remote click opens the overlay layer and a click inside it reaches its content. This result is regression-bearing - the fixture fails if either step stops holding |
| Quick `Popup` (`QtQuick.Controls`) | `NO_DEFECT` | OBSERVED and asserted in the controls fixture: `onOpened` fires and a click inside the popup reaches its content. That fixture is registered only when the QtQuick.Controls QML module is really available (see §12) |
| Quick focusable text target | `OS_ACTIVATION_FOCUS` / `UNMEASURED` | OBSERVED only as a harness gap (see §7) |
| RFB valid double-click envelope | `QT_SEMANTIC_CLASSIFICATION` | OBSERVED: the parser emits the faithful `move/left-down/left-up/left-down/left-up` facts (two presses, two releases) and routing delivers them; the semantic is lost only at application-level Qt event synthesis |
| RFB outside-interval pair | `NO_DEFECT` | OBSERVED: two singles; the wire carries no interval information at all |
| RFB outside-distance pair | `NO_DEFECT` | OBSERVED: two singles |
| RFB different-receiver pair | `NO_DEFECT` | OBSERVED: each widget receives its own press, and no cross-receiver double click can exist |

Two user-visible halves of #400 are therefore reproduced with different root causes:

```text
"double-click does not work"      -> QT_SEMANTIC_CLASSIFICATION (Qt's own click classification is not
                                     reachable from the public delivery ingress)
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
| Can Qt own click/double-click classification? | Not through this ingress: posting to a receiver or a window delivers exactly what HyRemote constructs, and the layer that does classify (Qt's window-system mouse processing, §5) is not reachable from public API. | OBSERVED (fixtures) / DOCUMENTED_PLATFORM_FACT (§5) |
| Can Qt own the implicit grab? | Widgets: no (HyRemote's `buttonReceivers` reproduce it). Quick: yes. | OBSERVED |
| Can Qt own popup/overlay routing? | Quick: yes for in-window overlays (measured reachable). Widgets: no for the classic top-level popup, because the target model is a single root and no ingress exists for another top-level surface. | OBSERVED |
| Can Qt own focus transitions? | Partly: pointer clicks move focus through the delivered press (Widgets) / Qt Quick's own focus handling. Not measurable offscreen in this harness. | OBSERVED (partial), gap in §7 |
| What must a public-Qt backend own at minimum? | The transport-neutral fact -> Qt event translation, receiver/item resolution where Qt does not do it, per-button held state, implicit-grab bookkeeping for Widgets, and whatever Qt semantic is missing at that ingress (today: double-click; also enter/leave/hover). | OBSERVED |

Why there is no lower public ingress:

```text
OBSERVED (fixtures)   direct public event delivery - QCoreApplication::sendEvent(receiver, QMouseEvent) -
                      delivers exactly the event HyRemote constructs. A valid remote double click
                      therefore arrives as Press, Release, Press, Release and no MouseButtonDblClick
                      is obtained. This ingress sits *above* Qt's normal window-system mouse
                      processing.
OBSERVED (headers)    no stable public Qt API exposes the lower ingress: QWindowSystemInterface is
                      a Qt-private header, absent from this SDK's public include tree
                      (include/QtGui/qwindowsysteminterface.h does not exist; the private header
                      directory is not part of the binary SDK install).
```

The accurate statement is therefore about the *ingress*, not about Qt's capability:

```text
Direct public event delivery (sendEvent) occurs above Qt's normal window-system mouse processing
and therefore does not obtain Qt's click/double-click classification.

No stable public Qt API exposes the lower window-system ingress needed to delegate that
classification.
```

Qt does perform that classification itself, and it does so below the public API (§5): an
application-scoped backend simply cannot reach the layer that does it.

Ingress probe, and why it is not used as the location evidence:

```text
method      a ~60-line throwaway program (non-product, run outside the repository) that links
            Qt6::Widgets/Qt6::Test, shows a recorder widget and issues two rapid QTest::mouseClick()
            calls at the same point, then prints the ordered QEvent types the widget received.
observed    policy: intervalMs=400 distancePx=5
            rapid same-spot pair, 600 ms gap pair, far-apart pair, rapid right-button pair,
            long-first-press pair, and a 250 ms gap after setDoubleClickInterval(120)
            -> every case produced Press/Release pairs with no MouseButtonDblClick.
disposition INSUFFICIENT_FOR_INGRESS_LOCATION - this experiment is NOT used to infer where Qt
            classifies double clicks, and it must not be read as "QWindowSystemInterface does not
            classify". QTest's own contract provides mouseDClick(), and its documentation describes
            testing double clicks by sending press/release pairs with suitable spacing, so this
            particular QTest::mouseClick experiment only shows that that experiment did not produce a
            DblClick. It is retained as a note, not as an architecture fact.
```

The correct attribution for physical input is therefore not "the OS backend produces the double
click": the platform/QPA delivers raw window-system input into Qt, and Qt's
`QGuiApplicationPrivate` mouse-processing path performs its own double-click classification before
the resulting `QMouseEvent` semantics are delivered.

This is exactly why the selected direction in §8 keeps a *finite* classification responsibility in
HyRemote instead of claiming Qt can be given the semantic through a different public call.

## 5. Backend family B: Qt-private / QPA

What would be needed: enter Qt at `QWindowSystemInterface` (or an equivalent private delivery
path) so `QGuiApplicationPrivate::processMouseEvent` and the target's own window/item machinery
run exactly as for a physical device.

```text
Capability                                            Assessment                                Class
-----------------------------------------------------------------------------------------------
mouse ingress exists below the public event API       QWindowSystemInterface / QGuiApplication-
(QWindowSystemInterface / QGuiApplicationPrivate)     Private mouse ingress; Qt-private headers
                                                      and private symbols                     DOCUMENTED_PLATFORM_FACT
Qt itself performs double-click classification in     QGuiApplicationPrivate::processMouseEvent
QGuiApplicationPrivate::processMouseEvent()           classifies timing/distance/button before
                                                      delivering QMouseEvent semantics         DOCUMENTED_PLATFORM_FACT
broader fidelity of a future QPA/private backend      popup behaviour, focus transitions, grab
                                                      and multi-window routing remain
                                                      capability-specific and must be qualified
                                                      separately; nothing here upgrades them    HYPOTHESIS
ABI constraint                                        QPA/private entry points are bound to the
                                                      exact Qt version and build configuration   DOCUMENTED_PLATFORM_FACT
platform constraint                                   the platform plugin is owned by one
                                                      integration instance; a QPA route is
                                                      already the platform plugin, C++/QML/
                                                      Generic routes are normal applications      DOCUMENTED_PLATFORM_FACT
public-Qt (C++/QML/Generic) may depend on it           forbidden by #401 and by the product's
                                                      public-Qt promise                           decision
```

Double-click classification is confirmed at this lower Qt ingress. That is a fact about where the
classification lives; it is not a general fidelity claim for a future QPA/private backend.

Consequence: the private/QPA family is a **route-specific higher-fidelity capability** - a possible
future capability of the QPA route only - and not a backend for the public-Qt routes. It cannot be
the single answer to #400, because the same defect must be fixed for C++ and QML applications whose
build cannot depend on Qt private ABI.

## 6. Backend family C: Native OS / virtual input

Not selected. The rejection is driven by the product contract (application-scoped, low intrusion,
portable, low privilege) and by platform constraints, not by whether it would restore double click:

| Aspect | Assessment | Class |
| --- | --- | --- |
| fidelity | highest: native input traverses the normal target input stack, so the target application's Qt processing, including Qt's click/double-click classification, applies | DOCUMENTED_PLATFORM_FACT |
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
| fidelity (normal Qt interaction) | medium: Qt owns routing where it is given the window (Quick) or the receiver (Widgets); missing semantics must be restored by HyRemote (OBSERVED) | high: double-click classification is confirmed at that ingress (DOCUMENTED_PLATFORM_FACT); broader fidelity of such a backend stays capability-specific (HYPOTHESIS) | highest: OS produces every semantic (DOCUMENTED_PLATFORM_FACT) |
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
application target / surface selection        which configured target receives remote input
Widgets receiver resolution                   inside the reachable, admitted surfaces, not per type
Widgets per-button receiver / implicit-grab bookkeeping
arrival-time preservation                     the accepted-arrival time of a raw input fact, so a
                                              busy GUI thread cannot change what the user did
minimal click / double-click classification   only the semantic Qt itself would have produced
transport-neutral -> Qt event translation
bounded queue / state / release cleanup       the existing bounded mailbox, per-button held state,
                                              protected releases and shutdown balancing
```

### Qt must own (explicitly not HyRemote's job)

```text
QPushButton / QCheckBox / QSlider / QListView / QComboBox / ... control-specific behaviour
normal QWidget semantics once a correct event reaches the correct receiver
Quick item routing
Quick grab / focus / overlay semantics
host OS activation / focus policy (the #362 class stays outside HyRemote)
```

The principle that keeps this division from drifting, and the reason the two earlier wordings were
reconciled into this one:

```text
HyRemote may know QWidget receiver identity.
HyRemote must NOT know control type semantics.
```

### Explicit non-goals (must not appear in the #400 implementation)

```text
no per-control branches (QPushButton/QComboBox/QListView/Popup special cases) - see the principle above
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
S4  SPLIT TO #404. #400's immediate implementation does NOT grow into broad multi-surface
    composition. #404 owns application-scoped transient top-level surface reach/capture (Widgets
    popup/menu surfaces created by normal controls), with no per-control special cases.
    #400 stays open as the user-defect owner: the original "some visible controls do not react"
    report still has to be mapped to its root cause by real maintained-viewer evidence, and the
    QComboBox fixture failing here is not proof that a given user's failing control is the same
    problem.
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
6  the fixture's double-click evidence is a deterministic in-process result. It does not identify
   which root cause explains a particular user's "some controls do not react"; that mapping stays
   with #400 and needs maintained-viewer evidence.
```

## 12. Quick capability guard for the preflight fixtures

The reproduction fixtures must not make a previously legal configuration invalid, and must not turn
an unavailable capability into a silently skipped test:

```text
test_quick_input_preflight.cpp        QtQuick only (MouseArea, double-click, focus, drag,
                                      in-window overlay layer). Requires nothing beyond
                                      HYREMOTE_REMOTEACCESS_WITH_QUICK.
test_quick_controls_preflight.cpp     Button / CheckBox / Slider / Popup matrix. Requires the
                                      QtQuick.Controls QML module, which the Runtime's Quick
                                      capability contract does not promise, so it is registered only
                                      when that module is actually available.
```

The guard is real (a configure-time capability check), not a runtime skip: when QtQuick.Controls is
unavailable the controls fixture is simply absent from the test graph, and the baseline QtQuick-only
configuration stays valid and buildable. The consequence is recorded in `tests/TEST_CATALOG.md` next
to the entry.

Neither fixture is a product dependency: QtQuick.Controls is not linked into the Runtime, and no
product build path references it.

Verification status of the unavailable-capability configuration, stated exactly:

```text
OBSERVED      configuring with CMAKE_DISABLE_FIND_PACKAGE_Qt6QuickControls2=ON succeeds
              (configure exit 0, "Configuring done"/"Generating done") and the controls fixture is
              absent from that graph, i.e. no broken or unbuildable entry is left behind.
NOT COMPLETED the full "Quick available + QtQuick.Controls unavailable" configuration was not
              reproduced with the canonical capability set in this session: the canonical build
              entry (build.cmd) has no supported way to inject an extra cache variable, and
              re-configuring by hand did not reproduce the same capability set (it also briefly
              disturbed the local build tree, which was restored with the canonical entry).
              The structural argument therefore rests on reading: the baseline fixture lives inside
              the pre-existing HYREMOTE_REMOTEACCESS_WITH_QUICK guard and imports QtQuick only.
FOLLOW-UP     a canonical way to pass extra cache variables (or a CI lane without QtQuick.Controls)
              would let this configuration be measured instead of argued.
```
