# GUI branding acceptance — 2026-10-30 (#241)

Evidence captured on the local Owner machine against `de88f03` (the commit that landed #730) and the acceptance SHA
`60d8f5a508e5257c4fbbd1fcfd6e76ec4f43cdd6` for PR #736. Screenshots are **real windows** captured with
`CopyFromScreen` after launching each program; nothing here is a mock, a design file or a CI artifact.

Environment: Windows x86_64, interactive desktop 1920×1080, Qt 6.8.3 `msvc2022_64`, MSVC 14.44.35207, Release, Ninja.

## Why the screenshots live on this branch

GitHub comments cannot attach binaries through the API, and the Owner requirement is explicit that visual acceptance
needs real screenshots. They are therefore committed on an **evidence branch that is never merged**, and referenced from
#241 by SHA-pinned raw URLs so the links stay valid.

## Files

| File | What it shows |
| --- | --- |
| `hyremote-learning-01-widgets.png` | Learning 01 (Widgets + C++) window, visible mark in content, icon in the title bar |
| `hyremote-learning-02-quick.png` | Learning 02 (Qt Quick + C++) window |
| `hyremote-learning-03-widgets.png` | Learning 03 Generic Widgets (zero-code host app) |
| `hyremote-learning-03-quick.png` | Learning 03 Generic Quick (zero-code host app) |
| `hyremote-qml-basic.png` | Learning 04 QML, **deployed** tree |
| `hyremote-qpa-proxy-existing-app.png` | Learning 05 QPA console, deployed tree |
| `hyremote-tool.png` | HyRemoteTool client |
| `remote-learning-01.png` | Remote framebuffer of Learning 01 captured by the external viewer (`vncdotool`) — C++ route |
| `remote-qml-basic.png` | Remote framebuffer of Learning 04 — QML route |
| `remote-generic-widgets.png` | Remote framebuffer of Learning 03 widgets launched with `-plugin hyremote` — Generic route |
| `remote-qpa.png` | Remote framebuffer of Learning 05 launched with `-platform hyremote` — QPA route |
| `gui-acceptance-deployed.json` | Per-program machine record: native PE icon count, window icon handle, window title, screenshot path |
| `gui-acceptance-buildtree.json` | Same record for the build-tree copies (Learning 04 exits 65 there, see #241 report) |
