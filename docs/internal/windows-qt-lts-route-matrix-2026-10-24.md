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
| C++ / Widgets | QUALIFIED (bounded) | Fresh Qt5 cache, 62/62 CTest, examples/Tool link, installed SDK consumer |
| C++ / Widgets via qmake | QUALIFIED (bounded) | Installed `hyremote.pri`; `tests/consumer-qmake` built and ran against the Qt5 SDK |
| C++ / Quick | NOT_RUN | No Qt5 Quick claim |
| QML | UNSUPPORTED in current selector | Qt5 QML is not a qualified route |
| Generic | UNSUPPORTED in current selector | Generic requires Qt6 >= 6.8 |
| QPA | NOT_APPLICABLE | QPA is exact Qt 6.8.3 private ABI |
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
- This matrix is a status record, not a new release authority.
