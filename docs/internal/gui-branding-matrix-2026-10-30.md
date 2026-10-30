# GUI branding matrix — #241 (2026-10-30)

Every HyRemote-owned **runnable GUI** program must show the canonical mark **and** carry both icon layers:

- **window icon** — `QGuiApplication::setWindowIcon()` / `setWindowIcon()` from `logo/hyremote-branding.qrc`;
- **Windows executable icon** — `logo/hyremote-app-icon.rc` compiled into the PE file, which is the only thing Explorer,
  the taskbar and Alt-Tab read for the *file*.

`setWindowIcon()` alone never satisfies the second layer, and the second layer never substitutes for the visible mark.

## Canonical assets (one source, no copies)

| Asset | Role |
| --- | --- |
| `logo/huayan-logo-single.png` | the one canonical mark (source of every other form) |
| `logo/huayan-software-horizontal.png` | the one canonical horizontal lock-up |
| `logo/hyremote-branding.qrc` | Qt resource prefix `:/hyremote/branding/` used by the window icon and the visible marks |
| `logo/hyremote-app.ico` | **generated** multi-size Windows icon (16/24/32/48/64/128/256), never hand-edited |
| `logo/hyremote-app-icon.rc` | Windows resource script: `IDI_ICON1 ICON "hyremote-app.ico"` |

Regeneration (documented in the `.rc` itself):

```text
python -c "from PIL import Image; im=Image.open('logo/huayan-logo-single.png').convert('RGBA');
           s=max(im.size); c=Image.new('RGBA',(s,s),(0,0,0,0)); c.paste(im,((s-im.width)//2,(s-im.height)//2));
           c.save('logo/hyremote-app.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])"
```

## Matrix

| TARGET | CATEGORY | VISIBLE_LOGO | WINDOW_ICON | WINDOWS_EXE_ICON | RESOURCE_VALIDATED | EXCEPTION |
| --- | --- | --- | --- | --- | --- | --- |
| `hyremote-learning-01-widgets` | example, Widgets | yes (header mark) | yes (`hyremote_branding.cpp`) | **yes** (`.rc`, WIN32 only) | repository-layout gate + example smoke | — |
| `hyremote-learning-02-quick` | example, Quick | yes (QML mark) | yes | **yes** | same | — |
| `hyremote-learning-03-widgets` | example, Widgets zero-code | yes | yes | **yes** | same | Qt-only branding; no HyRemote API/branding injected into a *customer* app |
| `hyremote-learning-03-quick` | example, Quick zero-code | yes | yes | **yes** | same | same |
| `hyremote-qml-basic` | example, QML | yes (QML mark) | yes (`main.cpp`) | **yes** | same | — |
| `hyremote-qpa-proxy-existing-app` | example, QPA | yes | yes | **yes** | same | keeps the native platform identity; the proxy is a plugin, not a brand layer |
| `hyremote-remote-support-showcase` (`hyremote-tool.exe`) | product tool | yes (header mark) | yes, asserted by `--verify-ui` | **yes** | `hyremote-tool-client-ui` + layout gate | — |
| `hyremote-installed-qpa-consumer` | installed/source consumer GUI | yes (same E4 source) | yes | **yes** | installed QPA product fit | built from the exact E4 source; no second toy app |
| `qpa_widget_opengl_capture_smoke` and other internal GUI fixtures | internal test GUI | no | one fixture only | no | n/a | pixel-accuracy fixtures must not gain decorative pixels; they are not shippable programs |
| headless/unit/transport tests | library tests | n/a | n/a | n/a | n/a | must not link QtGui/Widgets only to draw a mark |

## Verification actually available in CI

- `hyremote-tool-client-ui` keeps its existing brand assertion (unchanged).
- `hyremote-release-readiness-repository-layout` now fails if any listed GUI CMakeLists drops either the logo `.qrc` or
  the `.rc`, or if the `.rc`/`.ico` disappear. One check for all targets — deliberately not one CTest per program.
- Compilation of the `.rc` itself is proven by the Windows lanes building those targets (RC is part of the PE link).
- A **file-icon** claim still needs one human-visible check in Explorer on a Windows host; that evidence is recorded
  with the release/acceptance note rather than inferred from a green build.

## Non-goals (unchanged)

Image/logo *watermark* (#593), third-party application branding, embedded runtimes, and any second brand system.
