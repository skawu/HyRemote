# rustvncserver C ABI feasibility spike

Issue: #34 (bounded child of #27)

Governance mode: `transitional-explicit`.

## Purpose

This spike answers one narrow question for the `V0.0.1.0` x86 transport decision:

> Can HyRemote's C++/Qt product consume `rustvncserver` v2.2.1 through a thin backend-owned C ABI on both Windows x86_64 and Linux x86_64?

It does **not** select `rustvncserver` as the production transport by itself.

## Structure

```text
shim/       Rust cdylib; owns Tokio + rustvncserver
consumer/   C++17 program; dynamically loads only the exported C ABI
```

The C++ smoke consumer deliberately does not include Rust/upstream headers and does not link to a Rust-native API. It resolves these opaque C functions at runtime:

- ABI version;
- create/destroy;
- full RGBA update;
- start/running/stop lifecycle;
- event polling.

The event struct is a probe-only representation of connect/disconnect/key/pointer information. It is **not** the final HyRemote `InputEvent` ABI.

## Upstream pin

- project: `rustvnc/rustvncserver`
- release: `v2.2.1`
- license: Apache-2.0
- upstream MSRV: Rust 1.90
- upstream library shape: Rust `cdylib` / `rlib`; no repository `extern "C"` API was found when this spike was created

The shim therefore exists specifically to test whether a HyRemote-owned C boundary is practical without leaking Rust types into `hyremote-core`.

## CI acceptance

`.github/workflows/transport-rustvnc-ffi-spike.yml` runs the same probe on:

- `windows-latest`
- `ubuntu-latest`

Each job:

1. selects Rust 1.90.0;
2. builds the Rust shim in Release;
3. builds the independent C++17 loader;
4. loads the generated shared library;
5. checks ABI version and required symbols;
6. creates a small framebuffer;
7. copies one RGBA frame through the upstream framebuffer API;
8. schedules a listener on ephemeral port `0`;
9. confirms the listener task stays alive;
10. stops and destroys cleanly.

## Important limits

### Not a security acceptance

Upstream `VncServer::listen(port)` currently binds `0.0.0.0:{port}` internally. The smoke uses port `0` only to exercise lifecycle and does **not** approve this behavior for production.

HyRemote's security model requires explicit/safe listener binding. Production GO therefore still requires an upstream bind-address/pre-bound-listener API, an accepted temporary upstreamable patch, or rejection of the candidate.

### Not a zero-copy acceptance

Upstream v2.2.1 keeps framebuffer bytes in `Arc<RwLock<Vec<u8>>>`; `update_from_slice()` compares/copies producer bytes into that storage. This spike intentionally uses that CPU copy path for correctness only.

### Not viewer interoperability

No standard VNC viewer is connected by this CI smoke. Viewer negotiation, authentication, reconnect, input fidelity and downstream backpressure remain production acceptance work under #27.

### Not an embedded decision

This spike exists for the x86 Windows + Linux milestone. NeatVNC remains independently relevant to later Embedded Linux work because its Linux graphics/GBM integration characteristics are different.

## Result interpretation

- both jobs green: **FFI/toolchain feasibility passes**, but #27 remains open for security, viewer, backpressure and production adapter acceptance;
- one OS fails: do not generalize the working OS to the other; use the failure as transport-selection evidence;
- build passes but safe bind cannot be resolved without a permanent fork: reject `rustvncserver` for the default production path despite the successful bridge.
