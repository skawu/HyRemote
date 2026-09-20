# `src/` - the shipping tree

Four directories: **one base and the three access modes a user can integrate through**. The directory you open should
tell you which of those you are looking at.

| Directory | Role | Installed? | What it provides | Qualified against |
| --- | --- | --- | --- | --- |
| `core/` | base | no | internal static library: session, frame, storage, normalized input, capabilities | Qt-free, platform-free - it must not include Qt |
| `embedded/` | access mode 1 | yes | the one shared runtime and its public facade, `HyRemote::RemoteAccess` | Qt 6.6+ public API (no private ABI) |
| `declarative/` | access mode 2 | yes | `import HyRemote`: the Declarative QML module over the same runtime | Qt 6.6+ public API (no private ABI) |
| `transparent/` | access mode 3 | yes | `qhyremote`: the Transparent QPA platform MODULE for an unmodified application | **Qt 6.8.3 exact** (private ABI) |

## Rules the tree encodes

- **One runtime, three ways in.** `embedded`, `declarative` and `transparent` are access modes over the same shared
  runtime in `embedded/`; a payload may depend on the runtime, and the runtime (and `core/`) must never depend on a
  payload. `verification/release-readiness/check_repository_layout.cmake` enforces this, including that neither payload links
  `HyRemote::Core` or compiles a second `RemoteAccess` facade.
- **`core/` is not a link target.** It is composed statically behind the facade and is never installed; applications
  and payloads use `HyRemote::RemoteAccess` only.
- **Names say what is delivered, not how it is built.** A directory is named for the mode it serves, which is why the
  QML payload is `declarative/` rather than `qml/` and the QPA payload is `transparent/` rather than `qpa/` (the
  artifact names `qhyremote` and the `qml/HyRemote` build directory are unchanged).
- **Source directories map to fixed binary directories** in the root `CMakeLists.txt`, so reorganising sources never
  moves an artifact. For example `add_subdirectory(src/embedded remoteaccess)` keeps producing `build/remoteaccess`.
- **A Qt private-ABI dependency may live in exactly one payload directory.** Today that is `transparent/`, qualified
  against Qt 6.8.3; another Qt line is served by a sibling `transparent-<line>/` over the same runtime, never by
  widening the existing one.

Ownership of the layout overall is `docs/internal/repository-layout.md`.
