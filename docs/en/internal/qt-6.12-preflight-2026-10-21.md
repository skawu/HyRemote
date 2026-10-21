# Qt 6.12 LTS compatibility preflight (2026-10-21)

## Status

```text
BASELINE_HEAD=c18d284
RESULT=NOT_RUN / DEFER
AUTHORITY=#343/#57 (supported matrix unchanged)
```

Qt 6.12 LTS was released on 2026-09-30. This preflight only decides whether later compatibility work is justified. It does **not** add Qt 6.12 to the current Supported statement or change
`.github/release/release-trains.json`.

## Current environment

```text
Installed Qt: 5.15.2, 6.8.3 (msvc2022_64 / msvc2022_arm64)
Qt 6.12:      not installed
```

Qt 6.8.3 remains the reference version. The QPA proxy explicitly depends on the 6.8.3 private QPA ABI, so public-API compatibility cannot be inferred across patches.

## Preflight matrix

| Route | Result | Basis/gap |
| --- | --- | --- |
| C++ API + Widgets/Quick | **NOT_RUN** | Needs an isolated Qt 6.12 prefix; not installed |
| QML API | **NOT_RUN** | Needs Qt 6.12 QML/Quick target; not installed |
| Generic Plugin | **NOT_RUN** | Needs Qt 6.12 public Qt target; not installed |
| QPA proxy | **BLOCKED** | `find_package(Qt6 6.8.3 EXACT)` plus private Gui ABI; no 6.12 inference |
| Qt 5.15 | **Unchanged** | Existing #343/#57 baseline; not changed by this preflight |

## Executable preflight after installing Qt 6.12

Use an isolated prefix; do not commit a support claim:

```text
build.cmd clean
build.cmd build --tests --examples --qt-prefix=<qt-6.12-prefix>
ctest --output-on-failure
build.cmd build --mode=generic --tests --qt-prefix=<qt-6.12-prefix>
build.cmd build --mode=qml --tests --qt-prefix=<qt-6.12-prefix>
```

QPA must be validated separately with the exact 6.8.3 prefix; 6.12 results cannot be extrapolated to QPA.

## Owner decision

Default: **DEFER**. Only after the public-route preflight passes and support cost is known should the Owner choose GO / DEFER / NO-GO. Any GO requires a separate #343/#57 and machine-authority update.
