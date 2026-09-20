# Project branding assets

`logo/` is the single repository-owned source for HyRemote/Huayan branding used by this project.

Canonical assets:

- `huayan-logo-single.png` — application/window mark used by HyRemote-authored GUI examples;
- `huayan-software-horizontal.png` — horizontal product/company branding asset for documentation/presentation use where appropriate.

Rules:

- self-authored examples reference these assets through their build/resource packaging; they do not copy the PNG files into each example;
- documentation references `logo/` rather than owning a second image tree;
- installed/deployed example execution must not require a source-tree-relative runtime path to these files;
- third-party real-world examples such as qBittorrent and MuseScore retain their upstream branding;
- replacing the canonical artwork is a branding decision; ordinary code/example changes must not fork or redraw it.

Ownership and V1 migration are governed by #41 and #209.
