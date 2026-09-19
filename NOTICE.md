# HyRemote Notices

HyRemote is licensed under the Apache License 2.0. See `LICENSE` for the complete license text.

This notice classifies external dependencies and tooling used to build, test, deploy, or run HyRemote. It is informational and does not replace the license terms of HyRemote or any third-party component.

## Product dependency boundary

### Qt

HyRemote is designed for Qt applications and uses Qt at build time and, for Qt-facing product modes, at runtime. Qt is not relicensed by HyRemote. The adopter is responsible for complying with the license terms of the Qt distribution they use, including any applicable LGPL, GPL, or commercial-license obligations.

HyRemote does not copy the user's Qt SDK into this repository and does not claim that the Apache-2.0 license covers Qt.

The Transparent QPA Proxy additionally depends on exact Qt private QPA interfaces for the qualified build line. Those interfaces remain Qt-owned and version-coupled.

## Development and CI tooling

The following tools are used by repository workflows or test fixtures but are not, by that fact alone, part of the installed HyRemote SDK/product payload:

- CMake;
- Ninja;
- GitHub Actions;
- aqtinstall;
- Python;
- vncdotool;
- Pillow.

Their own license terms apply when installed or redistributed separately.

## Optional or future third-party payloads

A future dependency that is copied, vendored, bundled, or redistributed as part of a HyRemote source or binary package must be reviewed deliberately under the repository dependency policy and must add the applicable license/notice material before release.

Do not infer bundled status merely because a library, tool, protocol implementation, or SDK was evaluated in a spike or CI workflow.

## Distribution rule

The V1 installed SDK includes this notice together with the project `LICENSE`. Release packaging must preserve both files. If a downstream distributor adds other bundled third-party material, that distributor is responsible for adding the corresponding notices without removing the HyRemote notices required by the Apache-2.0 distribution terms.
