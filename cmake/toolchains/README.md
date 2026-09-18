# Toolchain files (embedded cross-compilation)

This directory holds the CMake **toolchain files** used to build HyRemote for an embedded target from a Windows or
Linux host. A toolchain file is the standard CMake mechanism (`-DCMAKE_TOOLCHAIN_FILE=...`) that tells CMake which
compiler to use, where the target sysroot lives and how to search for headers, libraries and packages.

Bundled examples:

| File | Target |
| --- | --- |
| `aarch64-linux-gnu.cmake` | 64-bit ARM embedded Linux (for example RK3588 boards) |
| `arm-linux-gnueabihf.cmake` | 32-bit ARM hard-float embedded Linux |

## Using one

Through the build script (recommended - it keeps the flags in one place):

```text
sh compile.cmd --mode=qpa --toolchain=cmake/toolchains/aarch64-linux-gnu.cmake --qt-prefix=/opt/qt-6.8.3-aarch64
```

or directly with CMake:

```text
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_PREFIX_PATH=/opt/qt-6.8.3-aarch64 \
  -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build
```

The project keeps **one** build directory, `build/`. Remove it (`clean.cmd`) before configuring for a different target
or mode, rather than creating a second tree - two trees silently mix generators, compilers and stale caches.

Point the compiler prefix at a non-default location when your toolchain is not on `PATH` under its default name:

```text
-DHYREMOTE_TOOLCHAIN_PREFIX=/opt/gcc-arm-13/bin/aarch64-none-linux-gnu-
```

## What you must supply

1. **A cross toolchain** that provides `gcc`/`g++`/`ar`/`ranlib`/`strip` for the target.
2. **A Qt 6.8.3 build for the target.** `--qt-prefix` must point at a Qt compiled for the target
   architecture, never at the host Qt: the host Qt ships host binaries and host libraries and cannot be linked
   into target code. Use Qt's own `qt-cmake`/`configure -qt-host-path` flow to produce it.
3. **A target sysroot** if your toolchain does not embed one; the examples default to
   `/usr/<triple>` and can be overridden with `-DCMAKE_SYSROOT=`.

## Adding a new toolchain

Copy one of the examples and adjust three things: `CMAKE_SYSTEM_PROCESSOR`, `HYREMOTE_TOOLCHAIN_TRIPLE` and, if the
target needs it, `CMAKE_SYSROOT`. Keep the four `CMAKE_FIND_ROOT_PATH_MODE_*` settings - they are what stop a host
library from being picked up by accident - and keep `PROGRAM` on `NEVER` so host tools such as `moc`, `rcc`, `ninja`
and `cmake` still run on the host.

## Constraints worth knowing before cross-compiling

- **The Transparent QPA payload uses Qt's private QPA ABI**, so the target Qt must be the same exact version as the
  qualified line (6.8.3). See `docs/compatibility.md`.
- **Cross-compiling proves the build, not the runtime behaviour.** Embedded Linux/EGLFS is an **unverified** platform
  family in this project (`docs/compatibility.md` marks RK3588 / EGLFS + OpenGL ES as *Unverified*), so a successful
  cross build produces a binary to try on hardware - it is not a compatibility or support claim, and runtime evidence
  requires the target board, display and input stack.
- **A cross build is not a release artifact** for the V1 x86 reference platforms; it does not change what V1 claims.

See also: `docs/guide/cross-compilation.md` (Chinese, user-facing) and its English mirror.
