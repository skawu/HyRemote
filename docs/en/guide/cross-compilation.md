# Cross-compiling to an embedded target

This page explains how to cross-compile HyRemote for an embedded target on a Windows or Linux host, using the
project's build script and a **CMake toolchain file**.

> **Read this first**: a successful cross build proves the *build*, not the behaviour on the target board. This
> project has **not verified** embedded Linux/EGLFS at runtime (`docs/compatibility.md` marks RK3588 / EGLFS + OpenGL
> ES as *Unverified*), so a cross build is **not** a compatibility or support claim and is not release evidence for the
> V1 x86 reference platforms.

## 1. The build entry point: one script, both platforms

`compile.cmd` in the repository root is **both a POSIX shell script and a Windows batch file**:

```text
Windows:  compile.cmd --mode qpa --qt-prefix C:/Qt/6.8.3/mingw_64
Linux:    sh compile.cmd --mode qpa --qt-prefix /opt/Qt/6.8.3/gcc_64
```

The default integration mode is **QPA** (change `HYREMOTE_DEFAULT_MODE` near the top of the script); command-line
arguments override it:

| Argument | Meaning |
| --- | --- |
| `--mode qpa` (default) | Transparent QPA proxy (`-platform hyremote`) |
| `--mode cpp` | Embedded C++ only (`HyRemote::RemoteAccess`) |
| `--mode qml` | Declarative QML (`import HyRemote`) |
| `--mode all` | All three integration modes |
| `--mode minimal` | Minimal build (no examples/tests/QML/QPA) |
| `--toolchain <file>.cmake` | **Select the cross-compilation toolchain file** |
| `--qt-prefix <path>` | Target Qt 6.8.3 installation prefix |
| `--build-dir <dir>` | Build root (default `build/`; the real directory is `<dir>/<mode>`) |
| `--build-type Release\|Debug` | Build type (default Release) |
| `--tests` | Also build tests |
| `--no-examples` | Do not build examples |
| `--clean` | Delete this mode's build directory first |
| `-v` | Verbose output (otherwise logs go to `build.<mode>.log`) |

Both spellings work: `--mode qpa` and `--mode=qpa`.

Cleaning: `clean.cmd` (`--all` removes the whole build tree, `--mode X` only one mode).

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

The cross build needs the same deployment contract as the desktop build - shared runtime, the `qhyremote` platform
plugin and its native delegate dependency (see `docs/deployment.md` and `hyremote_deploy()`). On the target the QPA
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
