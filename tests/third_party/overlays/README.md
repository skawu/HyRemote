# Throwaway integration overlays

No file in an upstream submodule may be edited for HyRemote integration.

Embedded C++ and Declarative QML application tests may place minimal, reviewable patches or generated snippets here. Test tooling first materializes an upstream ref into a disposable build-area source tree and applies the overlay there. QPA tests do not use overlays: they must exercise pristine upstream source through the normal Qt-only application + deployed `qhyremote` path.

An overlay must use only documented HyRemote public surfaces and must not introduce an application-specific adapter/runtime.
