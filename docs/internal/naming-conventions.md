# Naming Conventions

This document codifies the naming conventions HyRemote **already** uses. It exists so that the conventions are
written down for contributors instead of having to be inferred from the tree. It introduces no rename: see
"What V1 does not do" at the end, and [`v1-api-stability.md`](../v1-api-stability.md) for the frozen public surface.

Placement rules (which directory a new thing belongs in) live in
[`repository-layout.md`](repository-layout.md), together with the extension points for future work.

## Product identity and the four spellings

`HyRemote` is the product identity and the public namespace. Around it there are four deliberate spellings, and
each is correct **only** in its own context:

| Spelling | Applies to |
| --- | --- |
| `HyRemote` | Product name, `HyRemote::` C++ namespace alias, installed header directory `HyRemote/`, QML module and URI `HyRemote`, artifact `HyRemoteRemoteAccess` |
| `hyremote` | C++ namespace `hyremote`, internal source file names, and lower-case identifiers that never leave the library |
| `HYREMOTE_` | CMake options and preprocessor macros |
| `hyremote-` | CMake targets, library and test target names |
| `qhyremote` | The Transparent QPA platform module, per Qt's own platform-plugin naming convention |

A new globally exposed C-style symbol or macro carries the `HYREMOTE_` prefix. Public C++ types inside
`HyRemote::` keep normal PascalCase names and are **not** additionally prefixed (no `HYRemoteAccess`-style
duplication). Internal types stay normally scoped inside their own namespace or class.

## C++

- Namespace: `hyremote` for the implementation, `HyRemote::` as the public alias namespace.
- Types, classes and structs: `PascalCase` (`RemoteAccess`, `RemoteFrame`, `SessionStats`).
- Enumerations: `enum class` with `PascalCase` enumerators.
- Functions and methods: `camelCase`.
- Private data members: `m_` followed by camelCase, with the implementation behind a `struct Impl` pimpl in the
  files that use one; member-name style therefore rarely appears in public headers.
- Public headers: the file name may follow the public type it declares, as in
  `src/embedded/include/HyRemote/RemoteAccess.h`. This is the one place where a `.h`/PascalCase file name is
  the convention.
- New internal source and test files: `snake_case`.

### The two public-header forms are deliberate

Two forms coexist and both are intentional, not drift:

- `src/core/include/hyremote/core/*.hpp` - lower-case directory and extension, mirroring the internal namespace
  of an internal STATIC target that is not an installed application SDK;
- `src/embedded/include/HyRemote/RemoteAccess.h` - PascalCase directory and `.h`, matching the installed
  public header directory name and the public type.

Keep new headers in whichever of the two forms their module already uses.

## CMake

- Options and preprocessor macros: `HYREMOTE_` (`HYREMOTE_BUILD_TESTS`, `HYREMOTE_WITH_QPA_PROXY`).
- Targets, libraries and test targets: `hyremote-` (`hyremote-core`, `hyremote-remoteaccess`,
  `hyremote-qpa-native-semantics`).
- Exported package and alias targets: `HyRemote::<Component>`.
- The platform module keeps Qt's convention: `qhyremote`.

## QML

- Module and URI: `HyRemote`, imported as `import HyRemote`.
- The module directory must equal the URI, which is why the payload lives at `src/declarative`
  rather than in a flat directory; this is required by QML, not a style choice.
- The element is `QML_NAMED_ELEMENT(RemoteAccess)`: it carries no prefix, because QML imports form one global
  namespace. This is the single real collision risk in the naming surface and it is **accepted for 1.x**;
  changing it would be a breaking public-API change.

## What V1 does not do

- No blanket rename to a unified spelling, and no rename of existing V1 files for style.
- No rename of `src/embedded`, its target, `HyRemote::RemoteAccess`, its `EXPORT_NAME` or the installed
  package contract; the layout, build and package expectations are frozen (see `repository-layout.md` and
  `v1-api-stability.md`).
- No prefix or alias for the QML element, and no V1.1 pre-commitment about one.
- Identifiers, log tokens and CMake target names are never changed as part of a language or comment change.
