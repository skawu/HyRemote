# Deployment

HyRemote owns deployment of its product runtime instead of requiring application developers to discover internal backend files manually.

## Installed package helper

The installed/source CMake package exposes:

```cmake
hyremote_deploy(TARGET MyApp)
```

The current helper delegates Qt runtime deployment to Qt's supported CMake deployment API when available and provides the stable HyRemote hook for product runtime payload. The current bounded RFB correctness transport is compiled into the `RemoteAccess` product library rather than shipped as a separately selected application backend.

Applications should not copy or select RFB implementation files by name.

## Runtime dependencies

Deploy the application with the Qt modules required by the application and by the built HyRemote SDK. An SDK containing the Widgets and/or Quick adapters records those Qt package dependencies in its generated CMake package configuration.

The QML module and QPA Proxy belong to later V0.0.2/V0.0.3 product modes and are not part of the V0.0.1 Embedded C++ completion claim until their own gates pass.

## Network configuration

Deployment does not change the security defaults:

- creating `RemoteAccess` does not open a listener;
- explicit `start()` is required;
- loopback is the normal safe bind default;
- remote input is disabled by default;
- the current SecurityType None correctness baseline is not a public-Internet security solution.

If an application intentionally changes the bind address, its operator/deployment documentation must describe the resulting trust boundary. See `docs/security-model.md`.

## Validation

Installed-prefix and source-consumer fixtures validate the CMake/product target boundary. Final release packaging must additionally be exercised from a clean Windows and Linux environment under #30/#33 before a release artifact is described as GA-ready.
