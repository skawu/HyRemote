## What this is

The GUI programs the test cases drive are the surfaces a person actually looks at, so they now carry the project mark as the **window and application icon**. Product ruling: window/application icon only, no in-window logo area, so the composed remote frame content is unchanged and frame-difference evidence stays comparable.

Per example (`widgets-basic`, `quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, `remote-support-showcase`):

- a small `hyremote-branding.qrc` with an alias for `assets/branding/huayan-logo-single.png`;
- `qt_add_resources(<target> hyremote_branding FILES hyremote-branding.qrc)`;
- `QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/hyremote/branding/huayan-logo-single.png")))` immediately after the application object, with `<QIcon>` added at namespace scope.

## Why the .qrc form

Two earlier forms failed and are recorded so they are not retried: inline `FILES <absolute path>` is rejected by Qt (*set QT_RESOURCE_ALIAS*), and the `BASE <dir>` plus relative-file variant failed inside `Qt6CoreMacros` at `target_sources`/`set_property`. A `.qrc` carries its own alias and is the shape `quick-basic` already used for `Main.qml`.

## Boundary check

`docs/repository-layout.md` keeps `assets/` out of the product build graph, enforced by `check_repository_layout.cmake` for **product/integration module** CMake files only. Examples are outside that graph, so this does not weaken the rule. Both gates re-run: `check_repository_layout.cmake` PASS, `check_release_documentation_layout.cmake` PASS.

## Verification

All five examples configure, build and link. After launch the window **class** carries both large and small icons (`GetClassLongPtr(GCLP_HICON)` and `GCLP_HICONSM` non-zero). **`WM_GETICON` returns 0 on the same window**, because Qt publishes the icon on the window class rather than per-window - worth knowing before judging this by `WM_GETICON`.

## Consequence, stated rather than hidden

`examples/**` are the payloads of the #109 physical cells, so this moves the candidate SHA and the Windows evidence recorded on `eab386e` no longer corresponds to the examined binaries. Per the product ruling this is folded into the Windows re-take that the DPR cell already requires, and that re-take will be recorded against this change's SHA.

Refs #109
