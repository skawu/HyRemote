# x86 Windows + Linux VNC/RFB Transport Evaluation

Status: **FFI/toolchain feasibility PASS on Windows x86_64 + Linux x86_64; production fit still open in #27**

Parent: #27  
Completed bounded spike: #34 / PR #37  
Product milestone: **V0.0.1.0 — x86_64 (Windows + Linux) / Embedded C++ API**

This document complements `docs/neatvnc-evaluation.md`. It does not invalidate the accepted NeatVNC findings for Linux/Embedded Linux; it addresses the additional Windows requirement introduced by the frozen x86 product baseline.

## 1. Product requirement

V0.0.1.0 requires a reproducible VNC/RFB server path on **both Windows x86_64 and Linux x86_64** while preserving one HyRemote Core and one application-facing Embedded C++ API.

The transport boundary must satisfy these invariants:

- `hyremote-core` remains transport-neutral;
- backend types do not appear in Core/public API headers;
- `Transport::enqueueFrame()` is bounded and nonblocking from Core's perspective;
- transport owns its runtime, client queues and downstream backpressure;
- pointer/key callbacks are normalized before Qt target delivery;
- construction alone does not open a listener;
- bind address is explicit and the default product policy can be loopback/safe-by-default;
- dependency licensing does not silently change HyRemote's Apache-2.0 distribution model.

Different OS-specific backends are architecturally allowed, but one maintained cross-platform backend is preferred when it does not create a worse toolchain, security or maintenance burden.

## 2. Candidate matrix

| Candidate | License | Windows x86_64 | Linux x86_64 | C/C++ consumption | HyRemote disposition |
| --- | --- | --- | --- | --- | --- |
| NeatVNC v1.0.1 | ISC | not established by upstream/HyRemote evidence | strong fit | native C API | **Keep for Linux/Embedded Linux; not accepted as x86 cross-platform backend** |
| LibVNCServer | GPL-2.0-or-later | yes | yes | native C/CMake | **NO-GO for default linked HyRemote backend** |
| rustvncserver v2.2.1 | Apache-2.0 | **C ABI/toolchain spike PASS** | **C ABI/toolchain spike PASS** | Rust-native behind HyRemote-owned opaque C ABI | **Advance to bounded production-fit evaluation in #27** |
| HyRemote custom RFB | project Apache-2.0 | possible | possible | native | **NO-GO by default; last resort only** |

## 3. NeatVNC

The existing evaluation remains valid:

- stable v1.0.1 candidate;
- ISC license;
- mature frame/damage/input API;
- good separation behind `VncTransport`;
- strong Linux graphics orientation including AML, Pixman and optional GBM/DRM/H.264 paths.

The project is active and v1.0.1 remains the latest published release as of this evaluation.

However, the accepted HyRemote evidence is Linux/Freedesktop oriented. Nothing in #4 proves a native supported Windows server path. Therefore:

> **Do not use successful NeatVNC Linux integration as evidence that the x86 Windows + Linux product milestone is complete.**

NeatVNC remains highly relevant for the later Embedded Linux platform roadmap even if V0.0.1.0 adopts another backend for x86.

Upstream: <https://github.com/any1/neatvnc>

## 4. LibVNCServer

LibVNCServer is technically attractive for x86 portability:

- mature C library;
- CMake build;
- Windows and Unix/Linux support;
- standard VNC server APIs and broad interoperability history.

It is rejected for the normal HyRemote linked-backend path because upstream is GPL-2.0-or-later and its own README explicitly states that a program linking LibVNCServer/LibVNCClient becomes derivative work under GPL.

That conflicts with the frozen product goal that normal downstream use of Apache-2.0 HyRemote must not silently force an incompatible/copyleft application license merely by linking the transport backend.

Decision:

> **NO-GO as the default linked production transport unless HyRemote's licensing policy is explicitly changed.**

Upstream: <https://github.com/LibVNC/libvncserver>

## 5. rustvncserver v2.2.1

### 5.1 Positive fit

Upstream v2.2.1 currently provides:

- Apache-2.0 project license;
- Rust 1.90 minimum toolchain;
- RFB 3.8 server implementation;
- Raw, Hextile, Zlib, Tight, ZRLE and other encodings;
- VNC authentication;
- pointer/key server events;
- Tokio asynchronous networking;
- upstream-declared platform support including Windows x86_64 and Linux x86_64/ARM64;
- Cargo crate output configured as `cdylib` and `rlib`.

The release `v2.2.1` was published 2026-02-09. The repository is young compared with NeatVNC/LibVNCServer, so maturity remains a product risk rather than something inferred from feature count.

Upstream: <https://github.com/rustvnc/rustvncserver>  
Pinned release: <https://github.com/rustvnc/rustvncserver/releases/tag/v2.2.1>

### 5.2 C++ integration gap and #34 result

The crate's public API is Rust-native. Repository code search at evaluation time found no exported `extern "C"` API.

`crate-type = ["cdylib", "rlib"]` means Rust can produce a dynamic library; it does **not** by itself define a stable C ABI that C++ can call.

PR #37 therefore implemented a HyRemote-owned opaque C ABI probe. Rust/Tokio/upstream types remain behind the shim; an independent C++17 executable dynamically loads only the C symbols.

**Result: PASS on both reference operating systems.**

GitHub Actions run `34920869266` validated the same bridge on:

| OS | Toolchain evidence | Result |
| --- | --- | --- |
| Windows Server 2025 x86_64 | Rust 1.90.0, Cargo 1.90.0, MSVC 19.51, CMake 4.4.3 | **PASS** |
| Ubuntu 24.04.5 x86_64 | Rust 1.90.0, Cargo 1.90.0, GCC 13.3.0, CMake 3.31.6 | **PASS** |

Both jobs built `rustvncserver` v2.2.1 + the `cdylib`, built the independent C++ consumer, loaded the shared library, resolved the exported ABI, created a framebuffer, copied an RGBA frame, exercised listener start/running/stop, and destroyed cleanly. Both smoke programs ended with:

```text
PASS: C++ loaded Rust VNC probe ABI, updated RGBA frame, and exercised lifecycle
```

Accepted conclusion:

> A thin HyRemote-owned C ABI over rustvncserver is technically feasible on both x86 reference operating systems. This removes FFI/toolchain feasibility as a blocker, but it is **not** production transport acceptance.

### 5.3 Security blocker — bind address

Upstream v2.2.1 implements:

```rust
TcpListener::bind(format!("0.0.0.0:{port}"))
```

inside `VncServer::listen(port)`.

That does not satisfy HyRemote's frozen safe-listener requirement. HyRemote needs an explicit bind address and must be able to make loopback (or another explicitly safe policy) the product default.

Production GO requires one of:

1. upstream adds a bind-address or pre-bound-listener API;
2. HyRemote carries a small, explicitly temporary, upstreamable patch with deletion criteria;
3. the candidate is rejected if satisfying the contract would require a permanent fork.

### 5.4 Frame ownership/copy semantics

Upstream v2.2.1 `Framebuffer` stores pixels in:

```text
Arc<RwLock<Vec<u8>>>
```

and its `update_from_slice()` path compares/copies incoming producer bytes into that internal storage.

This is acceptable as a **CPU full-frame correctness baseline** for V0.0.1.0 if bounded and later measured, but it is not HyRemote zero-copy. The upstream README's general “zero-copy” wording must not be converted into a HyRemote performance claim without measured end-to-end evidence.

### 5.5 Runtime/lifecycle fit

The successful probe demonstrates that Tokio can be owned entirely by the transport/shim runtime without becoming a `hyremote-core` dependency. Production still needs to prove:

- listener bind/start failure is reported deterministically before `Transport::start()` succeeds;
- explicit bind address / loopback default;
- deterministic listener cancellation and reconnect cleanup;
- bounded frame handoff and per-client overload behavior;
- real standard-viewer interoperability on Windows and Linux;
- pointer/key event fidelity through the normalized HyRemote input model;
- no unbounded queues between Core and network encoders.

## 6. Why HyRemote does not implement RFB now

RFB is simple at the baseline protocol level, but a production server rapidly expands into negotiation/version compatibility, pixel formats, encodings, authentication/security types, client-update semantics, reconnect/error handling, cursor/desktop-size extensions, compression state, interoperability quirks and per-client resource bounds.

HyRemote's value is Qt application integration, not owning another protocol stack. A custom implementation is only reconsidered if bounded evidence shows that maintained permissive backends cannot satisfy the Windows + Linux product contract.

## 7. Current decision

> **Supersession note (2026-09-15).** The decision record below is the state at evaluation time. Issue
> #53 subsequently authorized a **bounded custom RFB 3.8 baseline** for the x86 product path, and that
> baseline is what the product uses today (`HYREMOTE_WITH_VNC`, bounded queues/input/handshake). Item 5
> under "Accepted now" is therefore historical. Items 1-4 are unchanged, and the production-frozen
> backend decision remains open under #27.

### Accepted now

1. **NeatVNC remains the preferred Linux/Embedded Linux-oriented backend candidate.**
2. **LibVNCServer remains rejected as the default linked backend on licensing grounds.**
3. **rustvncserver v2.2.1 C++/Rust FFI and x86 toolchain feasibility are accepted on Windows + Linux.**
4. **rustvncserver advances to a bounded production-fit increment under #27; it is not yet the production-frozen backend.**
5. **No custom RFB implementation is authorized.** *(Superseded 2026-09-15 by issue #53: a bounded custom
   RFB 3.8 correctness baseline was authorized for the x86 product path - see the note above and
   [`dependency-policy.md`](dependency-policy.md). The general default remains "do not own an RFB stack".)*

### Still not accepted

- rustvncserver as the production V0.0.1.0 backend;
- a mandatory Rust toolchain in every HyRemote consumer configuration;
- any zero-copy/performance claim;
- safe listener behavior;
- Windows/Linux real-viewer interoperability;
- normalized input fidelity;
- downstream/per-client backpressure correctness.

## 8. Next bounded decision

The next #27 increment must validate production fit rather than repeat build feasibility:

1. explicit/safe bind address with loopback-capable default;
2. synchronous/deterministic listener-start success or failure reporting;
3. bounded frame handoff consistent with `Transport::enqueueFrame()`;
4. standard VNC viewer connect/view on Windows and Linux;
5. pointer/key events captured and mapped toward #29 normalized input;
6. disconnect/reconnect and deterministic stop;
7. explicit decision on whether a Rust backend remains optional/internal or becomes the default x86 transport implementation.

Only after that evidence may #27 freeze the production x86 backend strategy.