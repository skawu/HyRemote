# ADR-0004: QPA delegate scope for EGLFS/Wayland-class targets

Status: **Accepted (product ruling 2026-09-18)**. V1.0.0.0 scope only.

## Context

Transparent QPA is not a replacement platform integration: the `qhyremote` module **decorates a qualified native
platform plugin** so that an ordinary Qt application keeps its normal native behaviour and gains the remote-access
runtime alongside it. The mode can therefore only exist where such a plugin exists.

The V1 qualified delegate set is exactly:

| Target | Native delegate decorated by `qhyremote` |
| --- | --- |
| Windows x86_64 | `qwindows` |
| Linux x86_64 | `qxcb` |

An **EGLFS- or Wayland-class Linux target has no delegate in that set**, so `-platform hyremote` cannot start an
application there. That is *not available*, which is a different statement from the *Unverified* status of the
embedded platform family as a whole: no amount of validation on an EGLFS board would change it, because the plugin
that would have to be decorated does not exist.

Two further facts bound the mode, independently of the platform:

- QPA remote input is **startup policy** - the zero-code mode deliberately exposes no runtime control object;
- the payload is version-coupled to exact Qt 6.8.3 (private QPA ABI), and deployment fails closed otherwise.

This was reported from a downstream appliance integration (RK3588 / EGLFS / Qt 6.8.3 from the BSP), which adopted
**Embedded C++** instead and is not blocked (issue #148).

## Decision

1. **No EGLFS/Wayland-class QPA delegate is in V1.0.0.0.** The V1 qualified delegate set stays `qwindows` + `qxcb`,
   and the guides state the missing-delegate fact rather than leaving it "unverified".
2. A **QEGLFS/Wayland-style delegate is planned**, and it belongs to the **V1.x embedded platform-family expansion**
   (#7, #18), not to V1.0.0.0. It is a roadmap commitment, not a V1 support claim, and no dates are implied.
3. Until it exists, products on an EGLFS/Wayland-class target use **Embedded C++** or **Declarative QML**, which are
   platform-independent application-facing modes and do not depend on a native platform delegate.
4. `docs/compatibility.md` remains the authoritative place where a target/mode pair is called available, and
   `docs/guide/cross-compilation.md` (both languages) carries the cross-compilation consequence.

## Consequences

- A downstream integrator can decide from the documentation alone, before reading `integrations/qpa`.
- The embedded-phase work has a named, already-reachable scope: the delegate must decorate the target's own platform
  plugin and inherit that plugin's startup constraints.
- The zero-code QPA mode keeps its startup-only boundary on every platform; runtime policy control stays with the
  public C++/QML API, so a future delegate does not have to invent a private control surface.
