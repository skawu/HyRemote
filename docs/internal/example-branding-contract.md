# HyRemote V1 Example Branding Contract

Normative product rule: every HyRemote-authored GUI example uses the canonical HyRemote project logo/icon; third-party upstream applications retain their own branding.

The exact canonical asset path is resolved from the final #209 repository asset layout and must be referenced through one project-owned CMake/resource/deployment helper rather than copied into each example.

Requirements:
- application/window icon on supported desktop platforms;
- QML/Quick examples load the same canonical asset through the supported resource/deployment path;
- the production showcase visibly presents the HyRemote logo;
- installed-SDK/deployed examples must resolve branding without the source tree;
- no independent example-local logo copies;
- third-party qBittorrent remains pristine and keeps upstream branding.

Refs #41 #109 #176 #209.
