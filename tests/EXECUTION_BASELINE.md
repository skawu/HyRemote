# HyRemote Hosted Test Execution Baseline

> Final #274 execution reconciliation. Historical counts are retained as before-refactor evidence; current counts are taken from the final Phase-D exact-head hosted gate.

## Current reference configuration

Reference configuration: top-level product build, Qt 6.8.3, `cpp,qml,generic,qpa`, VNC enabled. Transport security is conditional and adds two RFB auth tests when available.

Final Phase-D PR #358 exact head `0eacc6127349aa3a5687271fcac937467c1b3f96`, CI run `35819190493`:

| Platform | Security | Discovered | Executed by affected PR lane | Result | Executed test time |
| --- | --- | ---: | ---: | --- | ---: |
| Linux | enabled | 108 | 73 | 73/73 PASS | 34.38s |
| Windows | enabled | 107 | 72 | 72/72 PASS | hosted job PASS |

The affected PR lane deliberately excludes release-readiness, installed-consumer, deploy-helper and candidate-product-fit classes; therefore discovered count is not execution count. Zero execution remains invalid evidence.

Security-off equivalent registration is Linux **106** / Windows **105** because these two identities are absent:

- `hyremote-vnc-auth-test`;
- `hyremote-rfb-vnc-auth-handshake-test`.

Linux has exactly one additional platform-specific identity versus Windows:

- `hyremote-qpa-source-payload-relocation`.

The #274 final authority reconciliation adds no CTest registration, so these counts remain unchanged.

## Before/after inventory

| Milestone | Linux | Windows | Meaning |
| --- | ---: | ---: | --- |
| Phase-A/#300 baseline | 95 | 94 | all-frontends, security off; historical starting inventory |
| Phase C completed | 105 | 104 | final semantic-label tree under the then-triggered security-enabled reference configuration |
| Phase D completed | 108 | 107 | + Widgets forced-DPR, Quick forced-DPR, RFB wire robustness |

Do not use 95/94 as current authority. It is retained only to explain the structural delta from the audit baseline.

## Explained identity changes after Phase A

Cross-platform identities added after the 95/94 baseline:

- `hyremote-runtime-listener-binding-test` — deterministic Runtime listener policy;
- `hyremote-rfb-listener-reachability-test` — B4 Runtime/RFB real-socket reachability split;
- `hyremote-runtime-notifications-test` — Runtime typed notifications;
- `hyremote-qml-notifications-test` — QML notification observability;
- `hyremote-generic-config-test` — Generic launch/config mapping;
- `hyremote-release-readiness-security-runtime-deploy` — durable deployment/security closure;
- `hyremote-release-readiness-build-install-contract` — build/install contract;
- `hyremote-release-authority-v02-user-first` — V0.2 user-first release authority;
- `hyremote-widgets-capture-forced-dpr-test` — TG-001;
- `hyremote-quick-capture-forced-dpr-test` — TG-002;
- `hyremote-rfb-wire-robustness-test` — TG-013/TG-014.

Security-enabled configurations additionally register the two VNC-auth identities listed above. B1/B2/B3 moved ownership without renaming existing CTests. B4 deliberately added exactly one identity because the old mixed listener executable was split into two semantic owners while preserving the existing C++ facade identity.

## Final Phase-D new-test evidence

Both final exact-head artifacts show all three Phase-D additions actually started and passed on Linux and Windows:

- `hyremote-widgets-capture-forced-dpr-test`;
- `hyremote-quick-capture-forced-dpr-test`;
- `hyremote-rfb-wire-robustness-test`.

No retry, skip or `WILL_FAIL` was used to manufacture green status.

## Reduced-capability evidence retained from the workstream

Important capability-guard evidence is not replaceable by the all-frontends inventory:

- #327 Local A/B/C proved Widgets/Quick adapter registrations are independent of the C++ frontend and that `hyremote-rfb-widget-disconnect-backpressure-test` has registration count zero when VNC is off;
- qpa-only run `35676277901` proved `hyremote-v01-example-smoke` is absent when C++ API is off while the remaining suite executes nonzero/pass;
- QPA popup bounded synchronization passed fresh on both platforms without retry.

## Semantic-label evidence

Phase-C PR #357 preserved CTest identities/guards and added type/owner/cost labels. Hosted `Label Time Summary` demonstrated nonzero classes across Core, Runtime/RFB, frontend, repository/contract, consumer and release/qualification families while retaining `candidate-evidence`.

Labels are metadata, not capability gates. Ordinary PR selection still must execute a nonzero suite and explain exclusions.

## Assets outside normal product CTest inventory

| Asset | Authority | Treatment |
| --- | --- | --- |
| `tests/preflight/hyremote-tls-transition-preflight` | standalone preflight CMake project + `tls-preflight.yml` | PRE evidence; not counted in product CTest inventory |
| VeNCrypt probe scripts | bounded preflight/manual authority | feasibility evidence only |
| `tests/product-e2e/example_product_fit.py` | no active execution owner | deferred P2 TG-010 |
| `tests/product-e2e/qml_product_fit.py` | no active execution owner | deferred P2 TG-011 |
| `tests/product-e2e/showcase_product_fit.py` | no active execution owner | deferred P2 TG-011 |

## Final reconciliation rule

`TEST_CATALOG.md` is now configure-time checked against every registered CTest identity. A future registration/rename that does not update the catalog fails configuration, preventing this baseline/catalog drift from recurring silently.
