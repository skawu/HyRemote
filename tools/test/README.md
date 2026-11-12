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
and `--sha <40-hex-source-SHA>`. Multiple `--file` values are supported;
the plan records those paths as caller-declared. Git diff mode requires two
existing full commit SHAs, binds `--sha` to `--head`, and includes deleted
paths and both sides of renames. Paths are repository-relative and case-sensitive.
The output is stable JSON with a `plan_hash`, selected existing CTest IDs,
reasons, explicit non-qualification, and unclassified paths. An unknown path
returns status `BLOCKED_UNMAPPED` and exit 2; a zero-file request or incomplete
SHA fails rather than generating a false PASS.
The plan also identifies the registry content and Git base used. Rerunning
the planner only regenerates advice and cannot rerun existing evidence.

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


## Cross-cutting risk tags (second #970 slice)

Source-file specificity alone cannot determine whether a change also affects
authentication, the installed SDK ABI, or QML type registration. A planner
caller can explicitly add one or more **registered additive risk tags**:

```sh
python tools/test/plan_tests.py --sha <full-head-SHA> \
  --file src/runtime/src/quick/quick_gl_pbo_capture_source.cpp \
  --gate G1 --issue 970 --risk security-auth
```

Supported tags are defined only in `tests/case_selection_rules.json` under
`risk_rules`: `security-auth`, `public-sdk-abi`, `qml-registration`.
They add the listed existing CTest identities on top of file-scope selections;
unknown tags fail rather than falling through. Risk order and duplicates do not
affect the plan hash. The risk tags are included in `plan_hash` identity.

**This remains advisory, not an enforceable risk classifier.** A human or
trusted caller must identify risks correctly; omitting a risk tag does not
prove that the change is safe. Security-on tests have real CMake/platform
capability guards, and an unavailable test is never a PASS or waiver.
A later #970 gate must derive verified risk tags independently from diff
semantics and product contracts, and cross-check actual CTest discovery and
execution before claiming adequate coverage.

No new CTest, Qt build, environment setup, GPU fixture or CI job is introduced.
