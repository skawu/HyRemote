# Viewer Connection and Remote Control

V0.0.1 uses an internal bounded RFB correctness transport behind `HyRemote::RemoteAccess`. The protocol backend is not part of the application-facing API.

## Start the target application

The application must explicitly call `RemoteAccess::start()`. The default product configuration listens on loopback port 5900 and leaves remote input disabled.

A controlled test can set another loopback port and enable remote input before `start()`.

## Connect

With the default configuration, point a standard VNC/RFB client at:

```text
127.0.0.1:5900
```

Viewer syntax varies. The automated product-fit suite uses maintained `vncdotool` as an interoperability client; a GA compatibility entry must record the exact viewer/version used for its claim.

## View-only versus control

Remote viewing and remote input are separate policies. The safe product default is view-only:

```cpp
remote.setRemoteInputEnabled(false);
```

For an explicitly authorized control session, set the property before start:

```cpp
remote.setRemoteInputEnabled(true);
```

The V0.0.1 baseline normalizes pointer position/buttons/wheel and the required keyboard/text subset before Qt target delivery. Unsupported key/IME behavior must be treated as a limitation rather than approximated silently.

## Disconnect and reconnect

A viewer may disconnect and reconnect without recreating the target application. Remote client lifetime is owned by the transport; the Qt target and local application continue independently.

`RemoteAccess::stop()` tears down the session/listener and returns the public facade to the stopped state. A new session can then be started under the documented lifecycle rules.

## Security boundary

The current RFB correctness baseline uses SecurityType None. It is suitable for loopback/trusted test use, not direct untrusted-network exposure. Do not infer encryption/authentication from successful viewer interoperability. See `docs/security-model.md` and `docs/known-limitations.md`.
