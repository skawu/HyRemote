# Cross-compiling to an embedded target

This page explains how to cross-compile HyRemote for an embedded target on a Windows or Linux host, using the
project's build script and a **CMake toolchain file**.

> **Read this first**: a successful cross build proves the *build*, not the behaviour on the target board. This
> project has **not verified** embedded Linux/EGLFS at runtime (`docs/reference/compatibility.md` marks RK3588 / EGLFS + OpenGL
> ES as *Unverified*), so a cross build is **not** a compatibility or support claim and is not release evidence for the
>
> **One hard fact that is not "unverified"**: **Transparent QPA has no delegate on an EGLFS/Wayland-class Linux
> target, so `-platform hyremote` cannot start the application there.** That is *not available*, not *not yet
> verified*. The original text continues:
> V1 x86 reference platforms.

## 1. The build entry point: one script, both platforms

`compile.cmd` in the repository root is **both a POSIX shell script and a Windows batch file**:

```text
Windows:  compile.cmd --mode qpa --qt-prefix C:/Qt/6.8.3/mingw_64
Linux:    sh compile.cmd --mode qpa --qt-prefix /opt/Qt/6.8.3/gcc_64
```

The script's default integration mode is **QPA** (`HYREMOTE_DEFAULT_MODE`), but **a cross build for an embedded
target must change it**: Transparent QPA needs a **qualified native delegate** on the target platform, and an embedded
target has none (see section 4); command-line
arguments override it:

| Argument | Meaning |
| --- | --- |
| `--mode qpa` | Transparent QPA proxy (`-platform hyremote`). **Only on targets with a qualified delegate**: the V1 qualified delegates are Windows `qwindows` and Linux x86_64 `qxcb`; an **EGLFS/Wayland-class Linux target has no delegate at all**, and `-platform hyremote` **cannot start the application** there - that is *not available*, not *not yet verified* |
| `--mode cpp` | Embedded C++ only (`HyRemote::RemoteAccess`) |
| `--mode qml` | Declarative QML (`import HyRemote`) |
| `--mode all` | All three integration modes |
| `--mode minimal` | Minimal build (no examples/tests/QML/QPA) |
| `--toolchain <file>.cmake` | **Select the cross-compilation toolchain file** |
| `--qt-prefix <path>` | Target Qt 6.8.3 installation prefix |
| `--build-type Release\|Debug` | Build type (default Release) |
| `--tests` | Also build tests |
| `--no-examples` | Do not build examples |
| `--clean` | Delete the build directory and reconfigure |
| `-v` | Verbose output (otherwise logs go to `build/configure.log` and `build/build.log`) |

Both spellings work: `--mode qpa` and `--mode=qpa`.

### Exactly one build directory

This project keeps **one** build directory, `build/` (git-ignored):

- every artifact lands in `build/`, with no per-mode subdirectory, and the logs live inside it too, so no build file
  appears in the repository root;
- **switching integration mode or rebuilding means cleaning first**: `clean.cmd` removes the whole `build/`;
- if `build/` already holds a configuration for a different mode, the script **refuses to mix** and prints a command
  instead of silently reusing a possibly stale cache. The command it prints (`compile.cmd --clean --mode <new-mode>`)
  is currently *rejected*, because the mode guard runs **before** the clean (recorded on #141). **The usable sequence
  is clean first, then configure in the new mode**:

```text
Windows:  clean.cmd  &&  compile.cmd --mode cpp --qt-prefix C:/path/to/target-qt
Linux:    sh clean.cmd && sh compile.cmd --mode cpp --qt-prefix /opt/qt-6.8.3-aarch64
```

Cleaning entry point: `clean.cmd` (removes `build/`).

## 2. Selecting a cross-compilation toolchain

```text
sh compile.cmd --mode qpa \
  --toolchain cmake/toolchains/aarch64-linux-gnu.cmake \
  --qt-prefix /opt/qt-6.8.3-aarch64
```

Two example toolchain files ship with the repository (see `cmake/toolchains/README.md`):

| File | Target |
| --- | --- |
| `cmake/toolchains/aarch64-linux-gnu.cmake` | 64-bit ARM embedded Linux (for example RK3588 boards) |
| `cmake/toolchains/arm-linux-gnueabihf.cmake` | 32-bit ARM hard-float embedded Linux |

To add your own target, copy one and change three things: `CMAKE_SYSTEM_PROCESSOR`, the cross prefix and (if needed)
`CMAKE_SYSROOT`. **Keep** the four `CMAKE_FIND_ROOT_PATH_MODE_*` settings - they are what prevents host headers and
libraries from being picked up - and leave `PROGRAM` on `NEVER` so host tools (`moc`, `rcc`, `ninja`, `cmake`) still
run on the host.

## 3. What you must provide

1. **A cross toolchain** with the target `gcc`/`g++`/`ar`/`ranlib`/`strip`. If it is not on `PATH` under its default
   name, pass `-DHYREMOTE_TOOLCHAIN_PREFIX=/opt/gcc-arm-13/bin/aarch64-none-linux-gnu-`.
2. **Qt 6.8.3 built for the target.** `--qt-prefix` must point at a Qt compiled for the target architecture, never at
   the host Qt: a host Qt ships host binaries and host libraries and cannot be linked into target code. Produce it
   with Qt's `qt-cmake` / `configure -qt-host-path` flow.
3. **A target sysroot** if your toolchain does not embed one; the examples default to `/usr/<triple>` and accept
   `-DCMAKE_SYSROOT=`.

## 4. Deploying and running

**Decide the integration mode first, because on this target it decides whether the application can start at all:**

| Target | Usable integration modes | Note |
| --- | --- | --- |
| Windows x86_64 | Embedded C++ / QML / **Transparent QPA** (delegate `qwindows`) | V1 reference platform |
| Linux x86_64 | Embedded C++ / QML / **Transparent QPA** (delegate `qxcb`) | V1 reference platform |
| Embedded Linux / EGLFS class | **Embedded C++ / QML** | **no QPA delegate**: `-platform hyremote` cannot start the application |
| Embedded Linux / Wayland class | **Embedded C++ / QML** | same; there is no delegate |

Transparent QPA works by decorating an **existing qualified native platform plugin**, so it is only meaningful where
such a plugin exists - and on an embedded target it does not:

- **do not put `-platform hyremote` in an embedded start script**; cross-build such targets with `--mode cpp` or
  `--mode qml`;
- QPA remote input is **startup policy** (the zero-code mode deliberately has no runtime control object), so products
  needing runtime `start()` / `stop()` or policy changes should use **Embedded C++** (`HyRemote::RemoteAccess`) or
  **Declarative QML**.

**Deployment contract (for targets that do have a delegate)**: the application stays Qt-only and the plugin lives in
the **application's own plugin directory** (`<app>/plugins/platforms/`), placed by
`hyremote_deploy(TARGET MyApp QPA)`, with **no** `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH` or `LD_LIBRARY_PATH`
required. The payload that helper adds is:

1. `qhyremote` - the Qt platform MODULE selected by the chosen SDK/source build;
2. the shared `HyRemoteRemoteAccess` runtime that module uses internally;
3. the Qt / native-platform dependencies resolved by Qt deployment tooling, i.e. **the decorated native delegate
   itself**.

**There is no delegate in that list other than the target Qt's own**, which is exactly why the target Qt has to ship a
usable platform plugin. A missing QPA payload or a mismatched Qt version makes `hyremote_deploy(... QPA)` **fail
closed** (the private QPA ABI is qualified at Qt 6.8.3). See `docs/deployment.md`.

## 5. Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| `Could not find a package configuration file provided by Qt6` | `--qt-prefix` points at a host Qt, or the target Qt is not installed; use the target Qt prefix |
| Link errors against host-architecture libraries | The toolchain file's `CMAKE_FIND_ROOT_PATH_MODE_*` settings were changed; restore `PROGRAM NEVER` and `ONLY` for the rest |
| The `qhyremote` plugin fails to load | Target Qt does not match the qualified version (the QPA payload uses Qt's private QPA ABI, qualified at **6.8.3** - see `docs/reference/compatibility.md`) |
| Builds, but nothing appears on the board | If you used `--mode qpa`: **that target has no QPA delegate**, so `-platform hyremote` cannot start the application - use `--mode cpp` / `--mode qml`. If you did use C++/QML: that is runtime behaviour; the embedded platform family is **unverified** and needs real display/input evidence from the target |
