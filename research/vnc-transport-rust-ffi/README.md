# rustvncserver transport evidence

Issues: #34 (completed FFI feasibility) and #38 (active production-fit child of #27)

Governance mode: `transitional-explicit`.

## Why this directory exists

HyRemote's product API must remain `HyRemote::RemoteAccess` / later `import HyRemote`; the VNC backend is an internal implementation detail. This directory therefore evaluates `rustvncserver` only behind a HyRemote-owned opaque C boundary and never exposes Rust/Tokio/backend types to normal Qt applications or `hyremote-core`.

The first slice (#34 / PR #37) proved that `rustvncserver` v2.2.1 can be consumed through a thin C ABI on Windows x86_64 and Linux x86_64. That result is frozen as **FFI/toolchain feasibility only**.

The current #38 slice asks the harder product question: whether the backend satisfies HyRemote's listener-security, real RFB interoperability, input, reconnect, bounded-handoff, lifecycle and SDK/toolchain requirements well enough to become the common x86 transport.

## Structure

```text
shim/          Rust cdylib; owns Tokio + rustvncserver behind the probe ABI
consumer/      C++17 dynamic-loader smoke; proves C++ remains backend-toolchain neutral
product_fit.py standard RFB client interoperability/input/reconnect/lifecycle probe
```

The ABI in this directory is spike-only. It is not the public HyRemote C/C++ API and is free to change while #38 is open.

## Upstream state used by #38

The crates.io/released baseline remains `rustvncserver` v2.2.1 (Apache-2.0, Rust 1.90). That release's `VncServer::listen(port)` binds wildcard `0.0.0.0` internally, which conflicts with HyRemote's loopback-safe default.

Upstream PR `rustvnc/rustvncserver#29` adds `listen_on(SocketAddr)`. The #38 probe pins the PR commit `da3c35467c424639be56e09fdeb983e93ac609cb` **only to evaluate the proposed explicit-bind shape**. A commit from a third-party fork is not a production dependency decision and must not be presented as one.

The probe routes its default start path to `127.0.0.1`; it never uses the released wildcard listen API.

## Product-fit CI

`.github/workflows/transport-rustvnc-product-fit.yml` runs the same semantics on Windows and Linux:

1. build the Rust C ABI candidate with Rust 1.90;
2. load it from Python through the opaque C ABI;
3. prove an occupied loopback port is reported as start failure within a bounded interval;
4. start on an explicit loopback address;
5. connect with maintained `vncdotool` rather than a HyRemote-written protocol client;
6. verify negotiated framebuffer dimensions and a known RGB pixel;
7. send pointer press/release and keyboard press/release and observe them across the backend boundary;
8. disconnect and reconnect without recreating the VNC server/target;
9. stop and prove the listener no longer accepts connections.

The older `.github/workflows/transport-rustvnc-ffi-spike.yml` remains as the independent C++ dynamic-loading regression.

## Boundedness evidence and remaining blocker

The HyRemote-owned probe event queue is fixed at 256 entries. Pointer motion is freshness-oriented and is coalesced per client; if non-coalescible traffic still fills the queue, the oldest entry is dropped and counted. The normal interoperability test requires zero such drops.

This does **not** yet prove the entire upstream runtime is bounded. Current upstream `VncServer::new()` creates an `mpsc::unbounded_channel()` for `ServerEvent`, and each client creates another unbounded channel for client events. Therefore #38 must not claim B4 production acceptance merely because the outer HyRemote queue is bounded. A production GO requires an upstream-bounded/coalesced event path or equivalent evidence-backed change that does not leave a permanent HyRemote-only fork.

Framebuffer dirty-region accumulation is separately bounded upstream: each client merges dirty regions and collapses the list when region count/pixel thresholds are exceeded. That is useful evidence, but it does not remove the unbounded input/event-channel concern.

## Start-result limitation

PR #29 performs the bind inside the long-running `listen_on()` future. The spike therefore waits for either:

- immediate task completion with a bind/listener error, or
- an actual successful TCP connection to the exact requested loopback address.

This prevents the probe from calling "task scheduled" a successful start. It is still weaker than the preferred production shape of a pre-bound listener or explicit ready result returned after bind and before the accept loop. #38 must record that API gap rather than hiding it.

## Other limits

- The correctness path copies producer RGBA bytes into rustvncserver's framebuffer storage; it is not a zero-copy claim.
- Authentication/encryption/security-policy productization is not accepted merely by an unauthenticated loopback interoperability test.
- Prebuilt SDK users must never need Rust/Cargo; if this backend is selected, its DLL/SO and notices must be built and deployed by HyRemote packaging.
- Source builds may document a Rust requirement only if the burden remains justified and isolated.
- This x86 decision does not replace the separate NeatVNC/GBM-oriented Embedded Linux evidence path.

## Decision rule

#38 closes only as one of:

- **GO** — all production-fit blockers resolved on an acceptable upstream/release path and SDK integration is viable;
- **CONDITIONAL GO** — protocol/product fit is proven, but explicitly named upstream release/API conditions still prevent production selection;
- **NO-GO** — security, boundedness, maintenance/toolchain or packaging burden is disproportionate and cannot be resolved without violating the frozen product architecture.

Whichever result #38 reaches, #27 continues until HyRemote has a production VNC/RFB path on both Windows x86_64 and Linux x86_64 behind the same public product API.
