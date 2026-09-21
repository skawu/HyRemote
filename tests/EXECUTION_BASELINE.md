# HyRemote Hosted Test Execution Baseline

> Phase A execution reconciliation for #274.
>
> Current repository baseline checked: `develop` at `6dc244a8717bdc520544c71d9756499033857c73`.
>
> Hosted execution source: GitHub Actions run `35577711774`, PR #270 merge ref `a545e68d50243c948cc4b4418c60ebdf6adcc4c9`. A repository diff from that merge ref to current `develop` changes CI/release-authority scripts and documentation but **does not change any CTest registration file**, so the discovered CTest name set remains the current Phase-A registration baseline. Release-authority/scope scenarios changed internally under the same registered test names and are tracked at scenario level rather than counted as new CTests.

## 1. Hosted result

| Platform | Configuration | Discovered | Executed | Result | Total test time |
| --- | --- | ---: | ---: | --- | ---: |
| Linux x86_64 | Qt 6.8.3, `cpp,qml,generic,qpa`, Release | 93 | 93 | 100% PASS | 40.50 s |
| Windows x86_64 | Qt 6.8.3, `cpp,qml,generic,qpa`, Release | 92 | 92 | 100% PASS | 72.51 s |

The only platform-set difference is Linux-only `hyremote-qpa-source-payload-relocation`; Windows has no unique test. This matches the explicit `UNIX AND NOT APPLE` guard. No unexplained platform omission was found.

## 2. Executed CTest inventory

Linux is the superset. Windows executes the same names except #81.

### Release/repository gates

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
14. `hyremote-ci-scope-self-test`
15. `hyremote-mainline-audit-self-test`
16. `hyremote-branch-name-gate-self-test`
17. `hyremote-release-readiness-source-qpa-authority`
18. `hyremote-release-profile-develop-all`
19. `hyremote-release-profile-develop-runtime-only`
20. `hyremote-release-profile-retire-v001`
21. `hyremote-release-profile-retire-v002`
22. `hyremote-release-profile-retire-v003`
23. `hyremote-release-profile-v010-cpp-only`
24. `hyremote-release-profile-v020-runtime`
25. `hyremote-release-profile-v030-all`
26. `hyremote-release-profile-v040-all`
27. `hyremote-release-profile-v040-maintenance`
28. `hyremote-release-profile-v100-all`
29. `hyremote-release-profile-v100-cpp-only`
30. `hyremote-release-profile-v100-generic-only`

### Core

31. `hyremote-core-test-frame-lifetime`
32. `hyremote-core-test-damage`
33. `hyremote-core-test-timing`
34. `hyremote-core-test-mailbox`
35. `hyremote-core-test-transport-handoff`
36. `hyremote-core-test-session-lifecycle`
37. `hyremote-core-test-session-defaults`
38. `hyremote-core-test-input-routing`
39. `hyremote-core-test-input-normalization`
40. `hyremote-core-test-callback-lifetime`
41. `hyremote-core-test-callback-exception-boundary`
42. `hyremote-core-test-dependency-boundary`

### Shared Runtime / build authority

43. `hyremote-runtime-automatic-surface-model-test`
44. `hyremote-runtime-automatic-composite-capture-test`
45. `hyremote-runtime-automatic-composite-input-test`
46. `hyremote-build-authority-selftest`

### C++ plus historically misowned Runtime/RFB/adapters

47. `hyremote-remoteaccess-test`
48. `hyremote-security-descriptor-test`
49. `hyremote-remoteaccess-error-ack-test`
50. `hyremote-remoteaccess-target-loss-test`
51. `hyremote-target-component-provider-test`
52. `hyremote-input-mailbox-admission-test`
53. `hyremote-rfb-multi-client-input-test`
54. `hyremote-widgets-capture-test`
55. `hyremote-widgets-input-backpressure-test`
56. `hyremote-widgets-input-routing-test`
57. `hyremote-listener-address-matrix-test`
58. `hyremote-rfb-widget-disconnect-backpressure-test`
59. `hyremote-quick-capture-test`
60. `hyremote-quick-input-routing-test`
61. `hyremote-quick-input-backpressure-test`

### QML / Generic / QPA / deploy

62. `hyremote-qml-module-test`
63. `hyremote-qml-deploy-helper-non-qml`
64. `hyremote-qml-deploy-helper-qml`
65. `hyremote-generic-plugin-smoke`
66. `hyremote-qpa-deploy-helper-ordinary`
67. `hyremote-qpa-deploy-helper-qml-only`
68. `hyremote-qpa-deploy-helper-qml-composed`
69. `hyremote-qpa-deploy-helper-installed-payload`
70. `hyremote-qpa-deploy-helper-installed-payload-qml-only`
71. `hyremote-qpa-deploy-helper-installed-payload-qml`
72. `hyremote-qpa-deploy-helper-reject-missing-qml`
73. `hyremote-qpa-deploy-helper-reject-stale-qml-metadata`
74. `hyremote-qpa-deploy-helper-reject-missing-qml-root`
75. `hyremote-qpa-deploy-helper-reject-missing-qml-module-dir`
76. `hyremote-qpa-deploy-helper-reject-stale-qpa-metadata`
77. `hyremote-qpa-deploy-helper-reject-qt-mismatch`
78. `hyremote-qpa-deploy-helper-reject-missing-package`
79. `hyremote-qpa-deploy-helper-single-config-generator`
80. `hyremote-qpa-deploy-helper-multi-config-generator`
81. `hyremote-qpa-source-payload-relocation` — Linux only
82. `hyremote-qpa-proxy-smoke`
83. `hyremote-qpa-native-semantics`
84. `hyremote-qpa-remote-config-test`
85. `hyremote-qpa-auto-remoteaccess-smoke`
86. `hyremote-qpa-remote-failure-native-survival-smoke`
87. `hyremote-qpa-multi-surface-connection-smoke`
88. `hyremote-qpa-widget-popup-connection-smoke`
89. `hyremote-qpa-widget-opengl-capture-smoke`
90. `hyremote-cpp-installed-consumers`
91. `hyremote-qpa-quick-multi-window-connection-smoke`
92. `hyremote-generic-installed-consumers`
93. `hyremote-v01-example-smoke`

## 3. Meaningful assets absent from hosted CTest execution

| Asset | Final Phase-A disposition | Consequence |
| --- | --- | --- |
| `src/integrations/cpp/tests/rfb_product_fit.py` | KEEP contract, MOVE to semantic T5 RFB owner; TG-009 | maintained-viewer framebuffer/input/reconnect/timeout code is not release evidence until an explicit authority executes it; reported to #229 |
| `tests/product-e2e/qml_product_fit.py` | KEEP T5 preview | QML viewer/lifecycle logic remains dormant until a non-fast/manual/candidate owner executes it |
| `tests/product-e2e/showcase_product_fit.py` | KEEP T5 showcase | showcase is still maintained and documents product-fit intent; its harness remains dormant until a non-fast owner executes it |
| `tests/product-e2e/example_product_fit.py` | KEEP T5 app-level contract | real-viewer/control assertions remain useful but historical internal labels must retarget to canonical 01/02 or a canonical fixture; removed teaching paths stay removed |

## 4. Claimed scenarios not separately registered

Both adapter capture sources state a second forced-HiDPI execution is intended using `QT_SCALE_FACTOR=1.5` and `HYREMOTE_EXPECT_DPR=1.5`. The hosted inventory contains only `hyremote-widgets-capture-test` and `hyremote-quick-capture-test`; there is no DPR-specific second CTest or forced-DPR environment. TG-001/TG-002 are therefore confirmed gaps.

## 5. Scope

This baseline proves the **registered/executed name inventory** and expected platform difference. Necessity, ownership, overlap and sufficiency are decided by `TEST_CATALOG.md`, `TEST_SCENARIOS.md`, `TEST_MATRIX.md` and `COVERAGE_GAPS.md`.

A test file is not evidence merely because it exists. A defined authority must execute it and fail closed when the required execution does not occur.