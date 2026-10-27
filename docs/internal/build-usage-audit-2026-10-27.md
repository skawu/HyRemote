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
- The wrapper now reuses the `qt=` entry in `.hyremote-build-identity` before discovery, so a
  successfully selected Kit is not prompted again on every build. Explicit CLI/profile selection
  still wins; a deliberate switch causes the existing identity guard to recreate the tree.
- The internal wrapper handoff now uses `HYREMOTE_QT_SELECTION`, not the public `--qt-prefix`
  option. The user still types only a number, and logs no longer suggest a user-supplied path.
- The wrapper now checks for a real console with `mode con` before `set /p`; redirected stdin fails
  closed instead of hanging or consuming a scripted value.
- `--no-config` now prints a diagnostic-mode warning that build.yml, integrations, examples and
  tests may be missing; it cannot be mistaken for canonical all-integration acceptance.
- The Windows candidate/sentinel files are still written under
  `${HYREMOTE_SOURCE_DIR}/build`, while `build.directory` is configurable. A custom build directory
  does not own those transient files. This remains a P1 handoff-path gap.
