# Qt 5.15 LTS adaptation status (2026-10-22)

## Scope

This is the bounded Qt 5.15 compatibility lane for #57/#265. It is not a V0.3.3.0 release claim and does not change the V0.4 support contract or the QPA exact-private-ABI rule.

Reference cell:

```text
Qt 5.15.2 msvc2019_64
Windows 11 x86_64
MSVC 19.51 (v143 toolset)
```

The project keeps one Core, one Shared Runtime and one public C++ surface. Qt5 target names are compatibility aliases over the Qt5 imported targets; there is no second Runtime tree.

## Qualified lane

The following lane is buildable, installable and testable:

```text
Qt5.15 + C++/Widgets Runtime + HyRemoteTool
examples/learning/01-widgets-cpp
examples/learning/03-generic/widgets-app
installed CMake C++ consumer
```

Commands:

```powershell
cmake -S . -B build-qt5 -G Ninja `
  -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64 `
  -DHYREMOTE_BUILD_TESTS=ON -DHYREMOTE_BUILD_EXAMPLES=ON `
  -DHYREMOTE_BUILD_CPP_API=ON -DHYREMOTE_BUILD_QML_API=OFF `
  -DHYREMOTE_BUILD_WIDGETS_ADAPTER=ON -DHYREMOTE_BUILD_QUICK_ADAPTER=OFF

cmake --build build-qt5 -j 4
ctest --test-dir build-qt5 --output-on-failure
```

Evidence:

```text
Qt5 CTest                         62/62 PASS
Qt6 reference CTest               85/85 PASS
installed CMake consumer          PASS
v01 examples smoke                PASS (Widgets examples)
Qt5 deployment                    PASS (windeployqt + qt.conf)
```

The Qt5 deployment path uses `windeployqt`, copies `HyRemoteRemoteAccess.dll` and `Qt5Network.dll`, and writes `bin/qt.conf` so the deployed tree finds `plugins/` below its prefix.

## Experimental Quick lane

The Quick adapter compiles with:

```text
-DHYREMOTE_QT5_ENABLE_QUICK=ON
-DHYREMOTE_BUILD_QUICK_ADAPTER=ON
```

but the Qt5 Quick test suite is not yet qualified:

- DPR-dependent capture geometry expectations differ;
- the item-level pointer-move baseline does not yet pass;
- the inline QML preflight requires a versioned QtQuick import on Qt5;
- `Quick` behavior is therefore **Experimental / build-only**.

Do not use the Qt5 Quick lane as a supported product claim.

## Not yet adapted

```text
QML API (import HyRemote)
Generic Plugin tests
QPA private-ABI plugin
Linux Qt5
ARM64 Qt5
```

QPA remains exact-patch/private-ABI work. A Qt5 public-frontend build never implies a QPA binary works across patches.

## Defaults and guardrails

- Qt6 remains preferred when both Qt6 and Qt5 prefixes are present.
- Qt5 defaults to the Widgets adapter and disables Quick unless `HYREMOTE_QT5_ENABLE_QUICK=ON`.
- The Qt5 deployment helper is a bounded native-desktop path, not the Qt6 deployment-helper contract.
- Qt5 status is `Preview/Experimental` by cell, not a blanket `Supported` claim. Final support status belongs to #57/V0.4.
