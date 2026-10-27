# Windows Qt LTS route matrix (2026-10-24)

This is the honest per-cell status for the first GA Qt families. It does not claim a Cartesian
product from one passing cell.

## Qt 6.8.3 reference

| Route | Status | Evidence / boundary |
| --- | --- | --- |
| C++ / Widgets | QUALIFIED | V0.3.3.0 Windows release and CTest |
| C++ / Quick | QUALIFIED | Product matrix and examples |
| QML | QUALIFIED | Installed QML consumer and QML tests |
| Generic | QUALIFIED | Generic package payload and tests |
| QPA | QUALIFIED (exact 6.8.3) | `qwindows` exact private ABI; no other Qt patch |
| RFB Raw / TRLE / viewer | QUALIFIED | V0.3.3.0 Windows evidence |

## Qt 5.15.2

| Route | Status | Evidence / boundary |
| --- | --- | --- |
| C++ / Widgets | QUALIFIED (bounded) | Windows Qt5 CI 62/62, installed CMake consumer, examples/Tool link |
| C++ / Widgets via qmake | QUALIFIED (bounded) | Installed `hyremote.pri`; `tests/consumer-qmake` built and ran against the Qt5 SDK |
| C++ / Quick | EXPERIMENTAL / NOT_QUALIFIED | Explicit `HYREMOTE_QT5_ENABLE_QUICK=ON` builds, but Quick capture/input tests fail; normal-user experimental entry is hidden, internal test capability only |
| QML | UNSUPPORTED | Owner D2 (2026-10-27): Qt5 QML is not a qualified route |
| Generic | UNSUPPORTED | Owner D2 (2026-10-27): Generic requires Qt6 >= 6.8 |
| QPA | REJECTED | Owner D2 (2026-10-27): QPA is exact Qt 6.8.3 private ABI |
| HyRemoteTool | LIMITED | Built on Qt5; no full Tool UX qualification |

## Qt 6.5 / 6.12

```text
DETECTED_BUT_NOT_QUALIFIED / POST-GA
```

They are not selected as GA-supported, not a V0.4 RC/V1 blocker, and owned by #605 after V1 GA.

## Non-Windows

```text
USER_VALIDATION_PENDING / NONBLOCKING
```

Linux desktop, Wayland, ARM64 and EGLFS are user-validation targets under #563, not V0.3.3/V0.4/V1
Windows release gates.

## Next gaps

- Qt5 Quick/QML/Generic/QPA have no qualification claim; either implement a bounded route or keep
  them explicitly unsupported.
- Qt6.8 and Qt5 installed consumers now have source fixtures (`tests/consumer-installed-cpp`,
  `tests/consumer-qmake`) and local evidence; the #603 evidence index still needs the exact candidate
  SHA and physical Viewer record.
- This matrix is a status record, not a new release authority. Candidate-specific evidence is indexed
in [V0.3.4.0 Windows Qt cell evidence](v0.3.4.0-qt-cell-evidence-2026-10-26.md).

Owner D1/D7 (2026-10-27): RealVNC 7.15.1 is the required maintained Viewer for V0.3.4.0. The native
context-menu remote visual remains `NOT_CONFIRMED` and moves to V0.4.0.0 qualification; automated
native-popup capture PASS and right-click input reaching the host remain V0.3.4 evidence. It is not
rewritten as PASS and does not grant a blanket D05 exemption.
