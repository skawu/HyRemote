# Linux x86_64 Getting Started

Reference target for the current V1 line: Linux x86_64, Qt 6.8.x; current CI/product work uses Qt 6.8.3 with a GCC x86_64 Qt kit.

## Build HyRemote

Configure against the target Qt installation:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build --parallel
```

For runtime tests, make the matching Qt libraries discoverable when the environment does not already provide an rpath/runtime setup:

```bash
export LD_LIBRARY_PATH=/opt/Qt/6.8.3/gcc_64/lib:${LD_LIBRARY_PATH}
ctest --test-dir build --output-on-failure
```

Desktop Linux results do not imply Embedded Linux/EGLFS support.

## Run the public examples

Once #60 / PR #61 lands, enable `HYREMOTE_BUILD_EXAMPLES=ON` and run `widgets-basic` or `quick-basic`. Both applications use the same `HyRemote::RemoteAccess` product facade.

The examples default to view-only. Remote input is enabled only by an explicit application/test option.

## Display backend

Normal local product acceptance must run through the intended local desktop Qt platform backend. Hosted protocol E2E may use `offscreen`/software rendering to validate the protocol-to-application path; such a run is not evidence of local-visible display/input coexistence.

## Viewer and security

Connect a standard VNC client to the loopback listener (normally `127.0.0.1:5900`) as described in `docs/viewer-connection.md`.

The current correctness baseline uses unauthenticated RFB SecurityType None. Do not expose it directly to untrusted networks. See `docs/security-model.md`.

## Support boundary

Final Linux V0.0.1 acceptance must identify the exact Qt/compiler/QPA/viewer configuration and demonstrate the claimed local + remote behavior independently from Windows. See `docs/compatibility.md` and #30.
