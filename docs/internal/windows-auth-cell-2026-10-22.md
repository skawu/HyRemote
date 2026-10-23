# Windows maintained-viewer authentication cell (2026-10-22)

## Scope

This is the optional simple-password authentication cell for #175 on the Windows x86_64
Qt 6.8.3 target. It is not an encrypted-transport claim.

## Baseline

```text
eb4f9e821d979a21f72dd7adba03a6daa3783a47
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
Runtime built with HYREMOTE_WITH_TRANSPORT_SECURITY=ON
OpenSSL Crypto present; RFB stream remains unencrypted
```

The test host used `RemoteSecurityProfile::Authenticated` with a local descriptor:

```text
version=1
credentialId=maintenance
passwordFile=password.txt
```

The password file contained a temporary test-only credential and was removed after the run; no
secret is part of this repository.

## Result

```text
Viewer: TightVNC 2.8.85
Correct password:  connected, title "HyRemote - TightVNC Viewer", TCP established
Wrong password:    rejected, no established TCP connection

Viewer: RealVNC 7.15.1
Correct password:  connected, title "127.0.0.1:5927 (HyRemote) - RealVNC Viewer", TCP established
Wrong password:    rejected, no established TCP connection

Server negotiation: RFB 3.8 security type VncAuth(2)
```

RealVNC presents an unencrypted-connection notice first; after Continue it presents the password
prompt. Both viewers were driven without HyRemote-specific patches.

## Boundaries

- This is the optional-auth cell only; #175 still needs the independent cursor-shape cell and the
  rest of the two-viewer matrix.
- VNC authentication only; the transport is not encrypted.
- No Linux/embedded viewer cell is claimed.
- The test credential is not a product default and is not committed.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-22-auth-cell/
├── summary.txt
├── tightvnc-authenticated.png
└── realvnc-authenticated.png
```

## Verdict

```text
TightVNC optional VNC auth: PASS
RealVNC optional VNC auth: PASS
#175 optional authentication: PASS (two maintained viewers; correct accepts, wrong rejects)
```
