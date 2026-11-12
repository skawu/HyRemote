# Read-only affected-case test plan (first #970 implementation slice)

This is **not an executable test gate**. It proposes existing CTest identities from the
checked-in `tests/case_selection_rules.json` for a set of changed paths. It does not
build, run tests, inspect Qt capabilities, claim product qualification, override
`tests/EXECUTION_MANUAL.md`, or replace `.github/scripts/resolve-ci-scope.py`.

Use a full SHA and a stable, known diff:

```sh
python tools/test/plan_tests.py --self-test
python tools/test/plan_tests.py --base <base-SHA> --head <head-SHA> --sha <head-SHA> --issue 967 --gate G1
```

For a pure file-list plan, pass `--file src/runtime/src/quick/quick_gl_pbo_capture_source.cpp`
and `--sha <40-hex-source-SHA>`. Multiple `--file` values are supported.
The output is stable JSON with a `plan_hash`, selected existing CTest IDs,
reasons, explicit non-qualification, and unclassified paths. An unknown path
returns status `BLOCKED_UNMAPPED` and exit 2; a zero-file request or incomplete
SHA fails rather than generating a false PASS.

**Important limitations**:

* Rules are conservative provisional scope hints, not claim-completeness profiles;
  the authoritative ER/CCS-1 model remains under #393.
* Case names must exist and be actually selected/executed under their Qt/platform
  capability guards before they can be used as acceptance evidence.
* Some unrelated cases can be suggested because scope mapping is coarse.
  A further #970 TS-1 audit will narrow them and check registered CTests,
  true need, test time, Qt applicability and G0/G1/G2/G6 selections.
* #972 will later consume the same plan identity in CI cost analysis; this
  initial PR deliberately changes **no** GitHub Actions workflow, CI required
  context or CTest identity.
* Local Codebuddy remains responsible for running *approved* tests on its
  existing machine; read-only suggestions are not license to start more tests.
