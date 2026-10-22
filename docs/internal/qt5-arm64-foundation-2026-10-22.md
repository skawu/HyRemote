# Qt 5.15 and ARM64 foundation preparation (2026-10-22)

## Purpose

Under Owner #563, keep Qt 5.15 Windows compatibility preparation where the Windows GA claim requires it; ARM64/EGLFS is **user-triggered/nonblocking portability investigation**, not a V0.3.3, V0.4 or V1 obligation. This document records historical environment gaps, not a release blocker or support claim.

Baseline: `develop@1e56bdc`.

## Ownership and boundaries

| Workstream | Owner | V0.3.3.0 relationship |
| --- | --- | --- |
| Qt 5.15 preflight/adaptation | #57/#260/#265 | Not a V0.3.3.0 child; separate V0.3-family/V0.4 work |
| ARM64/EGLFS runtime | #236/#343 | USER_VALIDATION_PENDING; optional when user/board evidence exists; never V0.3.3/V0.4/V1 blocker (#563) |
| Quick-only loader correctness | #203 | x64 dependency fix already merged; ARM64 runtime remains separate |
| Qt 6.12 LTS | #57/#343 | Preflight only; default DEFER |

No supported-matrix, GA, package or security statement changes in this preparation.

## Current probes

### Qt 5.15

Local Qt installation:

```text
C:/Qt/5.15.2/msvc2019_64
```

The Qt5 C++/Widgets lane now builds, installs and passes its test suite on this cell. See [qt5-adaptation-2026-10-22.md](qt5-adaptation-2026-10-22.md) for commands, evidence and the exact preview/experimental boundary.

Current evidence:

```text
Qt5 CTest                         62/62 PASS
Qt6 reference CTest               85/85 PASS
installed C++ consumer            PASS
Qt5 deployment (windeployqt)      PASS
Qt5 Quick adapter                 COMPILES; tests EXPERIMENTAL
```

Qt5 QML API, Generic Plugin tests, QPA private ABI, Linux and ARM64 remain open. Qt5 stays a bounded legacy line under #57/#265; it is not a V0.3.3.0 child.

Next bounded tasks:

1. Decide whether the Qt5 Quick test failures are in the harness or the adapter, then either qualify or keep the lane build-only.
2. Add the Qt5 Widgets lane to an obtainable CI/reference runner and keep the exact patch/toolchain cell.
3. Continue QML/Generic/QPA/Linux/ARM64 only as separately recorded cells; do not infer them from this Windows Widgets proof.
4. Keep Qt5 out of the V0.3.3.0 admission table and out of the ARM64/pre-V0.4 blocker list.

### ARM64 Windows / Linux

Local Qt installation:

```text
C:/Qt/6.8.3/msvc2022_arm64
```

The Quick-only x64 build already proves the adapter boundary:

```text
HYREMOTE_BUILD_WIDGETS_ADAPTER=OFF
HYREMOTE_BUILD_QUICK_ADAPTER=ON
no Qt6Widgets import
```

The ARM64 compile probe is blocked:

- the installed Visual Studio has no ARM64 MSVC compiler;
- CMake cannot instantiate the expected Visual Studio ARM64 generator on this host;
- no ARM64/EGLFS target board or clean Buildroot rootfs is available.

Therefore:

```text
ARM64 WINDOWS COMPILE=BLOCKED
ARM64/EGLFS RUNTIME=BLOCKED
```

Next bounded tasks:

1. Add the ARM64 MSVC component or a documented cross toolchain, then compile the Quick-only Runtime.
2. Produce `dumpbin`/`readelf` evidence that the ARM64 Quick-only payload has no QtWidgets load dependency.
3. On the RK3588/EGLFS reference, prove clean-rootfs startup, local Quick display/input, remote view/control/reconnect, and loader provenance.
4. Owner #563 supersedes the former #343 ARM64 pre-V0.4 hard gate: user-provided ARM64 evidence is welcome but nonblocking for Feature/RC/GA.

## Non-core control

The following remain excluded from the V0.3.3.0 critical path and require a separate product decision:

- a complete management platform;
- TLS and cloud relay;
- hardware video encoding;
- new transport backends.

They cannot be used to delay or expand the current Feature.

## Current outcome

```text
Qt5 preparation=STARTED, build NOT_RUN
ARM64 Quick-only x64=PAST
ARM64 compile/runtime=BLOCKED (toolchain/device)
V0.3.3.0 required platform=Windows x86_64; Linux/device=user-validation pending/nonblocking
```
