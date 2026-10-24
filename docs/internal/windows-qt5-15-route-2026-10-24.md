# Windows Qt 5.15.2 bounded C++/Widgets route (2026-10-24)

## Scope

This is the first concrete #603/#604 step after V0.3.3.0: prove that the current develop tree
builds and tests on the installed Windows Qt 5.15.2 kit for the **bounded C++/Widgets** route.

It does not claim QML, Generic, QPA, Qt 6.5 or Qt 6.12 support.

## Environment

```text
Windows 11 25H2 x86_64
Qt 5.15.2 msvc2019_64
MSVC 14.44.35207 (VS 2026 Build Tools)
Ninja
```

## Build and tests

```text
build.cmd build --mode=cpp --qt-prefix=C:/Qt/5.15.2/msvc2019_64
                 --tests --no-examples --build-dir=.../qt5-route -j4
```

Result (fresh cache):

```text
CMAKE_PREFIX_PATH = C:/Qt/5.15.2/msvc2019_64
Qt5Core_DIR        = C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5Core
Qt6_DIR            = Qt6_DIR-NOTFOUND
HyRemote build succeeded
CTest: 62/62 PASS
```

Examples/consumer build:

```text
build.cmd build --mode=cpp --qt-prefix=C:/Qt/5.15.2/msvc2019_64
                 --examples --no-tests --build-dir=.../qt5-examples -j4
```

Result:

```text
HyRemote build succeeded
examples/learning/01-widgets-cpp/hyremote-learning-01-widgets.exe linked
HyRemoteTool linked
```

Installed SDK and clean consumer:

```text
cmake --install .../qt5-clean --prefix .../qt5-install2
installed HyRemoteConfig.cmake: find_dependency(Qt5 5.15 ...)
consumer: find_package(HyRemote) + Qt5 Widgets + hyremote_deploy(TARGET ...)
qt5-consumer.exe linked
```

## Interpretation

```text
Windows Qt 5.15.2 bounded C++/Widgets build: PASS
Windows Qt 5.15.2 CTest: 62/62 PASS
```

The #604 discovery regression now has a hermetic Windows Qt5 selectable case:
`HYREMOTE_QT_DISCOVERY_ROOTS` is honored on Windows as well as POSIX, and
`qt-discovery-windows-qt5-selectable` covers a mock `5.15.2/msvc2019_64` kit.
`hyremote-build-authority-selftest` passes with that case.

## Boundaries

- One local Qt 5.15.2 kit and one clean installed-SDK C++/Widgets consumer.
- No QML/Generic/QPA claim.
- No Qt 6.5 or Qt 6.12 claim; those are post-GA under #605.
- No Linux, ARM64/EGLFS or Wayland claim.
