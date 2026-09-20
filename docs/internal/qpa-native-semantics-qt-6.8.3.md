# QPA Proxy native-semantics qualification — Qt 6.8.3

This document is the exact-version qualification record for HyRemote QPA-02 (#68). It is intentionally limited to **Qt 6.8.3**, Windows x86_64 with the `windows` delegate, and Linux x86_64 with the `xcb` delegate.

The product invariant is unchanged: `QT_QPA_PLATFORM=hyremote` is a native-delegate-preserving proxy/decorator. It is not a replacement-only `qvnc`, `offscreen` or `minimal` platform.

## 1. Upstream facts used by this gate

The qualification is based on the Qt 6.8.3 source contract, not on an unbounded Qt 6 assumption:

- `src/gui/kernel/qplatformintegration.h`: ordinary QPA virtuals and `QPlatformIntegration::call<>`; `call<>` performs `dynamic_cast` against the integration object itself.
- `src/gui/kernel/qplatformkeymapper.{h,cpp}`: keyboard modifier and possible-key behavior is exposed through the public virtual `QPlatformKeyMapper` API; its generic implementation falls back through the current platform integration.
- `src/plugins/platforms/windows/qwindowsintegration.h`: `QWindowsIntegration` implements the Windows native integration mixins in addition to `QPlatformIntegration`.
- `src/plugins/platforms/windows/qwindowskeymapper.h`: the Windows key mapper overrides both `queryKeyboardModifiers()` and `possibleKeyCombinations()`.
- `src/plugins/platforms/xcb/qxcbintegration.h`: `QXcbIntegration` implements the GLX/EGL integration mixins when those Qt features are built.
- `src/plugins/platforms/xcb/qxcbkeyboard.h`: the XCB key mapper overrides both modifier and possible-key queries.
- `src/plugins/platforms/xcb/qxcbnativeinterface.h`: the XCB native-interface object carries `QNativeInterface::QX11Application`.
- `src/gui/painting/qplatformbackingstore.h` and `src/gui/kernel/qwindow.cpp`: backing-store/platform-surface lifetime belongs to Qt/native QPA; `QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed` is delivered before the platform surface is deleted.

## 2. Classification

| Surface | Windows `qwindows` | Linux `qxcb` | HyRemote QPA-02 disposition |
| --- | --- | --- | --- |
| ordinary `QPlatformIntegration` public virtuals | native implementation | native implementation | forward directly to delegate |
| platform-window creation | `QWindowsIntegration` | `QXcbIntegration` | delegate creates/owns real platform window; proxy only observes successful creation |
| backing-store creation | native backing store | native backing store | delegate creates/owns real backing store; proxy records only synchronous creation observations and does not wrap/retain it |
| event dispatcher | Windows GUI dispatcher | XCB dispatcher | return delegate dispatcher unchanged |
| font/clipboard/drag/input/accessibility/services/theme | native objects where configured | native objects where configured | forward accessor/factory unchanged |
| `QPlatformNativeInterface *nativeInterface()` | not used as the primary modern Windows native surface | `QXcbNativeInterface`, including `QX11Application` | forward the native-interface object unchanged |
| Windows application native interface | `QWindowsApplication` mixin on integration | n/a | proxy implements the same exact interface and forwards each method through the real delegate |
| Windows GL native integration | `QWindowsGLIntegration` | n/a | proxy implements/forwards when OpenGL is enabled |
| GLX native integration | n/a | `QGLXIntegration` when `xcb_glx_plugin` is enabled | proxy implements/forwards under the same feature gate |
| EGL native integration | n/a | `QEGLIntegration` when EGL is enabled | proxy implements/forwards under the same feature gate |
| keyboard modifier query | native `QWindowsKeyMapper` | native `QXcbKeyboard` | proxy returns delegate mapper and protected fallback forwards through that mapper |
| possible key combinations | native mapper override | native mapper override | same; do not use the generic empty `QPlatformIntegration::possibleKeys()` result |
| arbitrary platform delegates | not qualified | not qualified | fail closed; only `windows` / `xcb` reference delegate is accepted |
| other Qt versions/private ABIs | not qualified | not qualified | fail at exact Qt 6.8.3 package/compile gate |

## 3. Why forwarding `nativeInterface()` alone is insufficient

Qt 6.8.3 has two different native-interface shapes relevant here.

Some native APIs live on the object returned by `QPlatformIntegration::nativeInterface()`. XCB's `QX11Application` is in that category, so returning the delegate's native-interface object preserves it.

Other native APIs are integration-level mixins. `QPlatformIntegration::call<>` performs a `dynamic_cast` against the **current integration object**. A generic wrapper that only returns `delegate->nativeInterface()` therefore loses Windows application/GL and XCB GLX/EGL mixins. QPA-02 makes the HyRemote proxy implement the exact mixins present on the reference delegate and forwards their calls back through `delegate->call<>`.

No such mixin is added speculatively. The list is pinned to the Qt 6.8.3 reference delegate headers above.

## 4. Keyboard semantics

QPA-01 left `queryKeyboardModifiers()` and `possibleKeys()` on the generic `QPlatformIntegration` fallback because protected methods cannot legally be invoked on an arbitrary base pointer.

For Qt 6.8.3 reference delegates that fallback is unnecessary and would weaken native behavior:

1. HyRemote already forwards `keyMapper()` to the delegate.
2. Both qualified delegates return native key-mapper subclasses.
3. Those subclasses publicly override `queryKeyboardModifiers()` and `possibleKeyCombinations()`.
4. QPA-02 therefore implements the protected proxy hooks by invoking the delegate key mapper's public virtual API and translating `QKeyCombination` back to the legacy combined-int form only where the protected hook requires it.

This preserves native layout/modifier behavior without illegal protected-member access and without implementing another keyboard mapper.

## 5. Interception seam and ownership

QPA-02 intentionally does **not** wrap `QPlatformWindow` or `QPlatformBackingStore`.

The private `InterceptionSeam` records only neutral observations:

- a monotonic HyRemote-internal window token;
- platform-window creation;
- each successful backing-store creation call;
- geometry/visibility changes from the public `QWindow` event stream;
- the platform-window `QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed` boundary;
- final `QWindow` destruction while a platform surface remains active.

The record carries no `QPlatformWindow *`, `QPlatformBackingStore *`, HWND, XCB window id, GLX/EGL object or other backend-native handle. The delegate retains ownership and Qt continues to drive show/hide/resize/input/destruction exactly as before.

Qt 6.8.3 does not expose an ownership-safe public callback for arbitrary `QBackingStore` destruction or flush. QPA-02 therefore **does not synthesize one** and does not keep a backing-store pointer after `createPlatformBackingStore()` returns. A later presentation-mirroring gate must separately qualify any stronger flush/presentation interception instead of silently introducing a forwarding wrapper whose private RHI/backing-store state could diverge from Qt's real object.

This is the narrow prerequisite seam for later QPA composition. It is **not** a framebuffer/presentation-capture implementation and must not be represented as one.

## 6. Deterministic evidence

`hyremote-qpa-native-semantics` is intended to run with the real reference delegate on both hosted reference OSes and checks:

- HyRemote QPA is active;
- native key mapper remains reachable and authoritative;
- reference integration-level native mixins remain dynamically reachable where Qt built them;
- XCB native interface / X11 connection remains reachable on Linux;
- a real platform window is still produced by the native delegate.

`hyremote-qpa-interception-test` is delegate-independent and checks:

- one public `QWindow` maps to one stable neutral token;
- platform-window creation and every backing-store creation are observable without retaining native objects;
- the native platform-surface destruction boundary is emitted once;
- recreation keeps public-window identity without reusing a native pointer;
- callback connections cannot outlive the interception seam/proxy integration.

Hosted execution remains required before this gate can be accepted. A GitHub Actions job that never receives a runner is not pass evidence.

## 7. Explicit non-claims

QPA-02 does not claim:

- remote framebuffer delivery;
- backing-store flush/presentation interception;
- automatic `RemoteAccess` composition (#72/QPA-03 owns it);
- multi-window/dialog/popup composition (#76/QPA-04 owns it);
- Qt Quick RHI/OpenGL rendering coverage;
- physical local-display + remote coexistence;
- compatibility with Qt 5.15, Qt 6.2/6.5, later Qt 6 releases, Wayland delegates, or arbitrary QPA plugins.

Those claims require their own bounded evidence and cannot be inferred from this gate.
