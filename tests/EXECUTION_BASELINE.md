# HyRemote Hosted Test Execution Baseline

> Phase A execution reconciliation for #274.
>
> Repository baseline: `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.

## What the current numbers mean

Two different facts must not be conflated:

1. **Current all-frontends registration inventory** — PR #300 exact-head CI run `35675915625` configured the current tree and reported:
   - Linux Qt 6.8.3: **95 discovered**, 62 selected/executed by the PR fast lane, 62/62 PASS;
   - Windows Qt 6.8.3: **94 discovered**, 61 selected/executed, 61/61 PASS.
2. **Latest one-run execution of the previous complete inventory** — PR #280 run `35591928798` executed Linux 94/94 and Windows 93/93 PASS. After that, #281 added one cross-platform CTest, `hyremote-v01-rfb-product-fit`, deliberately labeled `candidate-evidence` and excluded from ordinary PR fast lanes.

Therefore **95/94 is the current registration authority**. Do not claim a single normal PR run executed all 95/94 tests; candidate/full authority owns the extra maintained-viewer test.

The only platform-name difference in the default all-frontends/security-off set is Linux-only `hyremote-qpa-source-payload-relocation`.

Security-enabled configurations additionally register:
- `hyremote-vnc-auth-test`;
- `hyremote-rfb-vnc-auth-handshake-test`.

## Current default all-frontends CTest inventory — Linux superset (95)

### Release / repository / package authority

1. `hyremote-release-readiness-deployment-relocation`
2. `hyremote-release-readiness-runtime-contract`
3. `hyremote-release-readiness-consumer-simplicity`
4. `hyremote-release-readiness-metadata`
5. `hyremote-release-readiness-repository-layout`
6. `hyremote-release-readiness-release-documentation-layout`
7. `hyremote-release-readiness-release-authority-policy`
8. `hyremote-release-readiness-licensing-boundary`
9. `hyremote-release-readiness-ci-environment-baseline`
10. `hyremote-release-readiness-documentation-paths`
11. `hyremote-release-readiness-deploy-helper-contract`
12. `hyremote-release-readiness-package-acquisition-isolation`
13. `hyremote-release-scope-self-test`
14. `hyremote-acquisition-audit-self-test`
15. `hyremote-ci-scope-self-test`
16. `hyremote-mainline-audit-self-test`
17. `hyremote-branch-name-gate-self-test`
18. `hyremote-release-readiness-source-qpa-authority`
19. `hyremote-release-profile-develop-all`
20. `hyremote-release-profile-develop-runtime-only`
21. `hyremote-release-profile-retire-v001`
22. `hyremote-release-profile-retire-v002`
23. `hyremote-release-profile-retire-v003`
24. `hyremote-release-profile-v010-cpp-only`
25. `hyremote-release-profile-v020-runtime`
26. `hyremote-release-profile-v030-all`
27. `hyremote-release-profile-v040-all`
28. `hyremote-release-profile-v040-maintenance`
29. `hyremote-release-profile-v100-all`
30. `hyremote-release-profile-v100-cpp-only`
31. `hyremote-release-profile-v100-generic-only`

### Core

32. `hyremote-core-test-frame-lifetime`
33. `hyremote-core-test-damage`
34. `hyremote-core-test-timing`
35. `hyremote-core-test-mailbox`
36. `hyremote-core-test-transport-handoff`
37. `hyremote-core-test-session-lifecycle`
38. `hyremote-core-test-session-defaults`
39. `hyremote-core-test-input-routing`
40. `hyremote-core-test-input-normalization`
41. `hyremote-core-test-callback-lifetime`
42. `hyremote-core-test-callback-exception-boundary`
43. `hyremote-core-test-dependency-boundary`

### Shared Runtime / build authority

44. `hyremote-runtime-automatic-surface-model-test`
45. `hyremote-runtime-automatic-composite-capture-test`
46. `hyremote-runtime-automatic-composite-input-test`
47. `hyremote-build-authority-selftest`

### C++ registration area, including historically misowned Runtime/RFB/adapters

48. `hyremote-remoteaccess-test`
49. `hyremote-security-descriptor-test`
50. `hyremote-remoteaccess-error-ack-test`
51. `hyremote-remoteaccess-target-loss-test`
52. `hyremote-target-component-provider-test`
53. `hyremote-input-mailbox-admission-test`
54. `hyremote-rfb-multi-client-input-test`
55. `hyremote-v01-rfb-product-fit` — candidate-evidence, added by #281
56. `hyremote-widgets-capture-test`
57. `hyremote-widgets-input-backpressure-test`
58. `hyremote-widgets-input-routing-test`
59. `hyremote-listener-address-matrix-test`
60. `hyremote-rfb-widget-disconnect-backpressure-test`
61. `hyremote-quick-capture-test`
62. `hyremote-quick-input-routing-test`
63. `hyremote-quick-input-backpressure-test`

### QML / Generic / QPA / deploy / adoption

64. `hyremote-qml-module-test`
65. `hyremote-qml-deploy-helper-non-qml`
66. `hyremote-qml-deploy-helper-qml`
67. `hyremote-generic-plugin-smoke`
68. `hyremote-qpa-deploy-helper-ordinary`
69. `hyremote-qpa-deploy-helper-qml-only`
70. `hyremote-qpa-deploy-helper-qml-composed`
71. `hyremote-qpa-deploy-helper-installed-payload`
72. `hyremote-qpa-deploy-helper-installed-payload-qml-only`
73. `hyremote-qpa-deploy-helper-installed-payload-qml`
74. `hyremote-qpa-deploy-helper-reject-missing-qml`
75. `hyremote-qpa-deploy-helper-reject-stale-qml-metadata`
76. `hyremote-qpa-deploy-helper-reject-missing-qml-root`
77. `hyremote-qpa-deploy-helper-reject-missing-qml-module-dir`
78. `hyremote-qpa-deploy-helper-reject-stale-qpa-metadata`
79. `hyremote-qpa-deploy-helper-reject-qt-mismatch`
80. `hyremote-qpa-deploy-helper-reject-missing-package`
81. `hyremote-qpa-deploy-helper-single-config-generator`
82. `hyremote-qpa-deploy-helper-multi-config-generator`
83. `hyremote-qpa-source-payload-relocation` — Linux only
84. `hyremote-qpa-proxy-smoke`
85. `hyremote-qpa-native-semantics`
86. `hyremote-qpa-remote-config-test`
87. `hyremote-qpa-auto-remoteaccess-smoke`
88. `hyremote-qpa-remote-failure-native-survival-smoke`
89. `hyremote-qpa-multi-surface-connection-smoke`
90. `hyremote-qpa-widget-popup-connection-smoke`
91. `hyremote-qpa-widget-opengl-capture-smoke`
92. `hyremote-cpp-installed-consumers`
93. `hyremote-qpa-quick-multi-window-connection-smoke`
94. `hyremote-generic-installed-consumers`
95. `hyremote-v01-example-smoke`

Windows has the same default names except #83, hence 94.

## Reduced-capability evidence that changed Phase A conclusions

PR #296 fresh exact-head run `35676277901` selected **QPA-only** and is important because all-frontends inventory cannot expose capability-guard defects:

| Platform | CPP | QPA | Discovered | Executed | Result |
| --- | --- | --- | ---: | ---: | --- |
| Linux | OFF | ON | 72 | 59 | 59/59 PASS |
| Windows | OFF | ON | 71 | 58 | 58/58 PASS |

Both uploaded `test.log` artifacts prove:
- `hyremote-qpa-widget-popup-connection-smoke` passes with the #296 bounded-condition fix;
- `hyremote-v01-example-smoke` is absent when the C++ API is OFF after #300;
- the suite remains nonzero.

This closes TG-019 and TG-021 without a local-only rerun.

## Meaningful assets outside the normal product CTest inventory

| Asset | Execution authority | Phase-A treatment |
| --- | --- | --- |
| `tests/preflight/hyremote-tls-transition-preflight` | standalone `tests/preflight` CMake project + `.github/workflows/tls-preflight.yml` on Win/Linux | KEEP PRE evidence; deliberately outside normal product build/test graph |
| `tests/preflight/run_vencrypt_interop_probe.py` / `vencrypt_interop_spike.py` | #258 bounded preflight workflow/manual evidence | KEEP PRE; feasibility evidence, not product acceptance |
| `tests/product-e2e/example_product_fit.py` | no current hosted authority found | KEEP asset, TG-010 |
| `tests/product-e2e/qml_product_fit.py` | no current hosted authority found | KEEP asset, TG-011 |
| `tests/product-e2e/showcase_product_fit.py` | no current hosted authority found | KEEP asset, TG-011 |

`rfb_product_fit.py` is **no longer outside CTest authority**: #281 registered it as `hyremote-v01-rfb-product-fit` while preserving its maintained-viewer/mainline role.

## Reconciled findings

- TG-001/TG-002 remain: only one normal Widgets/Quick capture CTest is registered; intended forced-DPR second executions are absent.
- TG-009 is closed: candidate product-fit is now registered/fail-closed.
- TG-012 is closed: acquisition self-test is in the normal inventory.
- TG-019 is closed: popup synchronization is bounded and fresh qpa-only Win/Linux passed first attempt.
- TG-020 remains: all-frontends VNC-enabled inventory cannot prove the Widgets-only registration guard is correct; source inspection proves it is not.
- TG-021 is closed: qpa-only hosted evidence proves the adoption smoke no longer registers without C++ API.

## Scope rule

This file distinguishes **registration**, **selection**, and **execution**. A discovered count is not execution evidence; a source file is not evidence; and a green job with zero selected tests is non-evidence. `TEST_CATALOG.md` decides necessity/ownership, while `TEST_MATRIX.md` records required capability guards and execution tiers.
