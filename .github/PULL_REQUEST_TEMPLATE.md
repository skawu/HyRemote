## What and why

<!-- One paragraph: the problem, the affected layers, and what changes. Link the issue. -->

Issue: #

## Scope

- [ ] Layers touched: `core` / `remoteaccess` / `cmake` / `docs` / `.github`
- [ ] Public API or ABI impact: none, or described above
- [ ] Core boundary respected (no Qt, protocol or platform types in `core/`)
- [ ] Behaviour change is covered by a test that fails without it
- [ ] Draft until the evidence below is complete

## Validation

| What | Command | Result |
|---|---|---|
| Configure | `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<QtRoot>` | |
| Build | `cmake --build build` | |
| Tests | `ctest --test-dir build --output-on-failure` | |
| Commit verified | `git rev-parse HEAD` | |

## Evidence and limitations

<!-- What was actually executed, on which OS and Qt version, and what was NOT executed. Negative
     results, unverified claims and blocked targets belong here too (for example embedded/EGLFS
     validation, or hosted-runner gaps such as #74). -->
