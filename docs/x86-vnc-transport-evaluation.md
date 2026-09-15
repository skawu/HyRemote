# x86 Windows + Linux VNC/RFB Transport Evaluation

Status: **in progress — rustvncserver FFI feasibility spike selected**

Parent: #27  
Bounded spike: #34  
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
| rustvncserver v2.2.1 | Apache-2.0 | upstream claims/supports x86_64 Windows | upstream claims/supports x86_64/ARM64 Linux | Rust-native; no exported C ABI found | **CONDITIONAL GO for #34 FFI feasibility only** |
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

The release `v2.2.1` was published 2026-02-09. The repository is young compared with NeatVNC/LibVNCServer, so maturity must be treated as a risk rather than inferred from feature count.

Upstream: <https://github.com/rustvnc/rustvncserver>  
Pinned release: <https://github.com/rustvnc/rustvncserver/releases/tag/v2.2.1>

### 5.2 C++ integration gap

The crate's public API is Rust-native. Repository code search at evaluation time found no exported `extern "C"` API.

`crate-type = ["cdylib", "rlib"]` means Rust can produce a dynamic library; it does **not** by itself define a stable C ABI that C++ can call.

Therefore a HyRemote-owned thin C ABI shim is required if this backend is adopted.

#34 validates that boundary by keeping all Rust/Tokio/upstream objects behind an opaque handle and calling it from an independent C++17 process on Windows and Linux.

### 5.3 Security blocker — bind address

Upstream v2.2.1 implements:

```rust
TcpListener::bind(format!("0.0.0.0:{port}"))
```

inside `VncServer::listen(port)`.

That does not satisfy HyRemote's frozen safe-listener requirement. HyRemote needs an explicit bind address and must be able to make loopback (or another explicitly safe policy) the product default.

A successful #34 build/FFI result therefore remains only a feasibility result.

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

Tokio can be owned entirely by a transport/shim runtime; this is compatible with HyRemote's rule that networking must not execute on the Qt GUI/render thread.

The probe uses an owned multi-thread Tokio runtime, an opaque server handle, an async listener task and explicit client disconnect cleanup. Production still needs to prove:

- listener start failure propagation rather than merely scheduling a task;
- deterministic listener cancellation;
- reconnect/client cleanup;
- bounded frame handoff and per-client overload behavior;
- viewer interoperability;
- input-event completeness;
- no unbounded queues between Core and network encoders.

## 6. Why HyRemote does not implement RFB now

RFB is simple at the baseline protocol level, but a production server rapidly expands into:

- negotiation/version compatibility;
- pixel formats;
- multiple encodings;
- authentication/security types;
- client update semantics;
- reconnect/error handling;
- cursor/desktop-size extensions;
- compression state;
- interoperability quirks;
- per-client backpressure and resource bounds.

HyRemote's value is Qt application integration, not owning another protocol stack. A custom implementation is only reconsidered if bounded evidence shows that maintained permissive backends cannot satisfy the Windows + Linux product contract.

## 7. Current decision

### Accepted now

1. **NeatVNC remains the preferred Linux/Embedded Linux-oriented backend candidate.**
2. **LibVNCServer is rejected as the default linked backend on licensing grounds.**
3. **rustvncserver v2.2.1 receives Conditional GO only for the #34 cross-platform C ABI/toolchain spike.**
4. **No custom RFB implementation is authorized.**

### Not accepted yet

- rustvncserver as the production V0.0.1.0 backend;
- a mandatory Rust toolchain in the final HyRemote consumer build;
- any zero-copy/performance claim;
- safe listener behavior;
- Windows/Linux viewer interoperability;
- downstream backpressure correctness.

## 8. Next decision after #34

If #34 passes on both GitHub-hosted x86 operating systems:

- keep rustvncserver in #27 for a second bounded production-fit increment covering safe bind, listener-start result, bounded frame handoff and real viewer interoperability;
- prefer upstream contribution for the missing bind API;
- decide whether the final x86 product uses rustvncserver on both OSes or uses an OS-specific split while preserving one HyRemote `Transport`/public API contract.

If #34 fails on either OS or the C ABI/toolchain burden is disproportionate, retain the evidence and evaluate the next permissive candidate/split-backend option rather than weakening V0.0.1.0 acceptance.
