# Linux x86_64 Getting Started

Reference target for the current V1 line: Linux x86_64, Qt 6.8.x; current CI/product work uses Qt 6.8.3 with a GCC x86_64 Qt kit. Transparent QPA is qualified only for exact Qt 6.8.3 with the xcb native delegate.

## Build the normal product

Configure against the target Qt installation:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build --parallel
```

A plain configure is intentionally product-only: it builds the standard C++ `HyRemote::RemoteAccess` path but does not build repository tests, examples or spikes. QML and Transparent QPA are opt-in integration packages.

To create an installed SDK, also provide `-DCMAKE_INSTALL_PREFIX=<prefix>` and run `cmake --install build`.

## Maintainer / acceptance test build

Enable repository validation explicitly:

```bash
cmake -S . -B build-test -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64 \
  -DHYREMOTE_BUILD_TESTS=ON \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build-test --parallel
```

For build-tree tests, make the matching Qt libraries discoverable only when the local kit does not already provide suitable runtime lookup:

```bash
export LD_LIBRARY_PATH=/opt/Qt/6.8.3/gcc_64/lib:${LD_LIBRARY_PATH}
ctest --test-dir build-test --output-on-failure
```

That environment override is a **build-tree test concern**, not the product deployment contract. Clean deployed-consumer gates remove SDK/runtime path assistance before launching the application.

Desktop Linux results do not imply Embedded Linux/EGLFS support.

## Run the public examples

With `HYREMOTE_BUILD_EXAMPLES=ON`, the current V1 candidate contains `widgets-basic`, `quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, and `remote-support-showcase` according to the enabled integration packages.

E1/E2 use the same `HyRemote::RemoteAccess` product facade. They default to view-only; remote input is enabled only by an explicit application/test option.

## Display backend

Normal local product acceptance must run through the intended local desktop Qt platform backend. Hosted protocol E2E may use `offscreen`/software rendering to validate the protocol-to-application path; such a run is not evidence of local-visible display/input coexistence.

Transparent QPA V1 uses the exact Qt 6.8.3 `qxcb` delegate path on the Linux reference environment.

## Viewer and security

Connect a standard VNC client to the loopback listener (normally `127.0.0.1:5900`) as described in `docs/viewer-connection.md`.

The current correctness baseline uses unauthenticated RFB SecurityType None. Do not expose it directly to untrusted networks. See `docs/security.md`.

## Support boundary

Final Linux product acceptance must identify the exact Qt/compiler/QPA/viewer configuration and demonstrate the claimed local + remote behavior independently from Windows. Physical E1/E2/E3/E4 evidence is tracked by #109; exact status remains in `docs/compatibility.md` and milestone issues #30/#31/#32.
