# HyRemote V1 Example Branding Contract

Normative product rule: every HyRemote-authored GUI example uses the canonical HyRemote project logo/icon; third-party upstream applications retain their own branding.

## Canonical asset provenance

The previously implemented and verified project-logo work in PR #160 used:

```text
assets/branding/huayan-logo-single.png
```

That PR was closed during V1 scope convergence and was not merged, which is why the current `develop` tree does not contain `assets/` today. The V1 #209/example migration must **restore this existing project-logo asset provenance** rather than inventing a new example-specific logo.

Unless the #209 final asset-layout migration deliberately renames the shared branding asset once, `assets/branding/huayan-logo-single.png` is the canonical source to restore. If a final path rename is needed, it must be a single repository-wide move with no duplicate legacy copy left behind.

## Consumption contract

Examples must consume the logo through one project-owned resource/deployment mechanism rather than copying the image into each example.

PR #160 already established a working Qt resource shape:

```text
hyremote-branding.qrc
  -> alias :/hyremote/branding/huayan-logo-single.png
```

and application setup equivalent to:

```cpp
QGuiApplication::setWindowIcon(
    QIcon(QStringLiteral(":/hyremote/branding/huayan-logo-single.png")));
```

The migration may centralize this further, but must preserve the one-source principle.

## Requirements

- application/window icon on supported desktop platforms;
- Widgets, Quick and QML self-authored examples use the same canonical logo;
- the production showcase visibly presents the HyRemote logo in addition to application/window icon branding;
- installed-SDK/deployed examples must resolve branding without the original source tree;
- no independent example-local logo copies;
- CI/package checks detect missing/stale canonical branding references;
- third-party qBittorrent remains pristine and keeps upstream branding.

## Historical evidence

PR #160 verified that all five then-current self-authored GUI examples configured, built and linked with the shared `.qrc` branding mechanism and that Windows application/window class icons were populated. Reuse that evidence/design input; do not retry the already-rejected absolute-resource and broken `BASE` variants documented there.

Refs #41 #109 #160 #176 #209.
