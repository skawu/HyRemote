## What and why

<!-- One paragraph: the problem, the affected layers, and what changes. Link the issue. -->

Issue: #

## Scope

- [ ] Layers touched: `src/core` / `src/remoteaccess` / `integrations` / `cmake` / `docs` / `.github`
- [ ] Public API or ABI impact: none, or described above
- [ ] Core boundary respected (no Qt, protocol or platform types in `src/core`)
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

<!-- State what actually executed, on which OS/Qt, and what did NOT execute. Queued/no-step hosted jobs,
     physical/native evidence gaps, and other limitations belong here explicitly. -->
