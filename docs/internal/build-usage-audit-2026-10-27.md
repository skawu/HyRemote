# Build usage audit (2026-10-27)

## Canonical path

The repository build authority is:

```text
build.cmd + build.yml + cmake/HyRemoteBuild.cmake
```

The normal developer/Owner commands are:

```text
build.cmd build
build.cmd test
build.cmd install
build.cmd rebuild
build.cmd clean
```

`build.yml` owns the build directory, integrations, examples/tests defaults, toolchain and Qt
selection. A machine-specific Qt Kit belongs in a local profile, not in the checked-in
`build.yml`:

```text
copy build.yml build/local-qt683.yml
# set qt.prefix to the exact Kit
build.cmd test --config=build/local-qt683.yml
```

The local profile may live inside the ignored `build/` tree. Do not pass `--build-dir`,
`--qt-prefix`, `--mode` or `--no-config` in routine acceptance.

## Diagnostic overrides

The following forms are real, but they are **focused diagnostics**, not canonical acceptance:

```text
build.cmd build --no-config --mode=cpp --qt-prefix=... --build-dir=...
```

They deliberately bypass `build.yml`, may select fewer integrations/examples/tests, and therefore
produce a different test count and a different product surface. In this audit:

```text
canonical profile test (all integrations/examples): 87/87 PASS
--no-config --mode=cpp --no-examples diagnostic:     69/69 PASS
```

The two counts are not interchangeable evidence.

## Current state

The checked-in `build.yml` leaves `qt.prefix` empty on purpose. On a machine with more than one
usable Kit the canonical command must either use a local profile or show the interactive numbered
Kit choice; it must not silently choose the newest/first Kit. The current Windows environment has
Qt5.15.2 and Qt6.8.3, so an unattended `--no-prompt` run without a profile is expected to fail
closed.

## Findings

- The checked-in default enables cpp/qml/generic/qpa and examples; canonical `test` covers the
  installed consumers as well.
- The prior local evidence recorded 69/69 or 71/71 because it used diagnostic flags; those numbers
  must not be presented as the canonical all-integration acceptance count.
- The local single `build/` directory is correct. The local profile is inside it and untracked.
- The canonical `build.cmd test --config=build/local-qt683.yml` run passed 87/87 at `c6b586d`.
