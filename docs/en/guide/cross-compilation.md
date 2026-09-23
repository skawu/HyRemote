# Cross-compiling to an embedded target

This page explains how to cross-compile HyRemote for an embedded target on a Windows or Linux host, using the
project's build script and a **CMake toolchain file**.

> **Read this first**: a successful cross build proves the *build*, not the behaviour on the target board. This
> project has **not verified** embedded Linux/EGLFS at runtime (`docs/compatibility.md` marks RK3588 / EGLFS + OpenGL
> ES as *Unverified*), so a cross build is **not** a compatibility or support claim and is not release evidence for the
> V1 x86 reference platforms.

## 1. The build entry point: one script, both platforms

`build.cmd` in the repository root is **both a POSIX shell script and a Windows batch file**, and it is the
repository's **single build and install authority**:

```text
Windows:  build.cmd build --integrations=qpa --qt-prefix C:/Qt/6.8.3/mingw_64
Linux:    sh build.cmd build --integrations=qpa --qt-prefix /opt/Qt/6.8.3/gcc_64
```

Configuration precedence is **built-in defaults < `build.yml` < the command line**: `build.yml` holds the
repository's developer profile and command-line arguments override it:

| Argument | Meaning |
| --- | --- |
| `--mode=qpa` | Transparent QPA proxy (`-platform hyremote`) |
| `--mode=cpp` | Embedded C++ only (`HyRemote::RemoteAccess`) |
| `--mode=qml` | Declarative QML (`import HyRemote`) |
| `--mode=generic` | Zero-code `QGenericPlugin` frontend (`-plugin hyremote`) |
| `--mode=all` | Every integration frontend |
| `--mode=runtime` | The shared runtime alone, with no frontend |
| `--mode=minimal` | Minimal build (no examples, tests, QML or QPA) |
| `--integrations=cpp,qml,generic,qpa` | Select frontends exactly; never prepends C++ |
| `--toolchain=FILE.cmake` | **Select the cross-compilation toolchain file** |
| `--qt-prefix=PATH` | Target Qt 6.8.3 installation prefix |
| `--build-type=Release\|Debug` | Build type (default Release) |
| `--build-dir=DIR` | Build tree (default `build`) |
| `--tests` / `--run-tests` | Build tests / build and run them |
| `--no-examples` | Do not build examples |
| `--security` | Transport-security capability (VNC Authentication) |
| `--clean` | Delete the build tree and reconfigure |
| `--cmake=KEY=VALUE`, `--env=KEY=VALUE` | Extra configure cache entries / environment, repeatable |
| `-v` | Verbose output (otherwise logs go to `build/configure.log` and `build/build.log`) |

Both spellings work: `--mode qpa` and `--mode=qpa`.

### Exactly one build directory

This project keeps **one** build directory, `build/` (git-ignored):

- every artifact lands in `build/`, with no per-mode subdirectory, and the logs live inside it too, so no build file
  appears in the repository root;
- **switching integration mode or rebuilding means cleaning first**: `build.cmd clean` removes the whole `build/` (and with it the install root inside it);
- if `build/` already holds a configuration for a different mode, the script **refuses to mix** and prints the exact
  command (`build.cmd rebuild --integrations=<new-mode>`) instead of silently reusing a possibly stale cache.

Cleaning entry point: `build.cmd clean` (removes `build/`).

## 2. Selecting a cross-compilation toolchain

```text
sh build.cmd build --integrations=qpa \
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

The cross build needs the same deployment contract as the desktop build - shared runtime, the `qhyremote` platform
plugin and its native delegate dependency (see `docs/guide/deployment.md` and `hyremote_deploy()`). On the target the QPA
launch is identical to the desktop:

```text
MyApp -platform hyremote
```

## 5. Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| `Could not find a package configuration file provided by Qt6` | `--qt-prefix` points at a host Qt, or the target Qt is not installed; use the target Qt prefix |
| Link errors against host-architecture libraries | The toolchain file's `CMAKE_FIND_ROOT_PATH_MODE_*` settings were changed; restore `PROGRAM NEVER` and `ONLY` for the rest |
| The `qhyremote` plugin fails to load | Target Qt does not match the qualified version (the QPA payload uses Qt's private QPA ABI, qualified at **6.8.3** - see `docs/compatibility.md`) |
| Builds, but nothing appears on the board | That is runtime behaviour; the embedded platform family is **unverified** and needs real display/input evidence from the target |
