# Qt 5.15 and ARM64 foundation preparation (2026-10-22)

## Purpose

Start the Qt 5.15 and ARM64/EGLFS foundation work in parallel with V0.3.3.0 without adding either to the V0.3.3.0 product commitment. This document records preparation and blockers only; it is not a support claim.

Baseline: `develop@1e56bdc`.

## Ownership and boundaries

| Workstream | Owner | V0.3.3.0 relationship |
| --- | --- | --- |
| Qt 5.15 preflight/adaptation | #57/#260/#265 | Not a V0.3.3.0 child; separate V0.3-family/V0.4 work |
| ARM64/EGLFS runtime | #236/#343 | Not a V0.3.3.0 child; mandatory before V0.4 |
| Quick-only loader correctness | #203 | x64 dependency fix already merged; ARM64 runtime remains separate |
| Qt 6.12 LTS | #57/#343 | Preflight only; default DEFER |

No supported-matrix, GA, package or security statement changes in this preparation.

## Current probes

### Qt 5.15

Local Qt installation:

```text
C:/Qt/5.15.2/msvc2019_64
```

Current project configuration requires Qt 6.8 or exact Qt 6.8.3 for QPA. A direct Qt 5.15 build is therefore a separate adaptation workstream (#265), not a configuration-only operation. The first CMake probe did not run because Ninja was not on `PATH`; no Qt5 source/build result is claimed here.

Next bounded tasks:

1. Add a Qt5-only configure probe to CI or a local toolchain shell, with an explicit Qt5 prefix.
2. Inventory Qt6-only APIs in Runtime/Widgets/Quick/QML and QPA exact-private code.
3. Keep Qt5 adaptation behind #260 GO and #265; do not make it a V0.3.3.0 blocker.
4. Record the exact Qt5 patch/toolchain/compiler cell before any support statement.

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
4. Keep #343's ARM64 baseline as a pre-V0.4 requirement, not a V0.3.3.0 freeze condition.

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
V0.3.3.0 scope=unchanged x86_64 Windows/Linux
```
