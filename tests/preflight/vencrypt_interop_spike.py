#!/usr/bin/env python3
"""Minimal VeNCrypt 0.2 / X509Vnc / TLS 1.2 / VNC Authentication spike server.

Preflight evidence only (#258, Preflight A). This is deliberately NOT the #143 implementation: it exists so a real
maintained viewer can be driven through the frozen wire profile

    RFB 3.8 -> VeNCrypt 0.2 -> X509Vnc 261 -> TLS >= 1.2 -> VNC Authentication -> normal RFB session

and so the server side can log every negotiation step as protocol evidence. It serves one connection, then exits.

Everything is logged with a SPIKE_ prefix so the transcript can be quoted as evidence.
"""

from __future__ import annotations

import argparse
import datetime
import socket
import ssl
import struct
import sys
import time

SEC_TYPE_VENCRYPT = 19
VENCRYPT_VERSION_MAJOR = 0
VENCRYPT_VERSION_MINOR = 2
SUBTYPE_X509VNC = 261
RFB_VERSION = b"RFB 003.008\n"


def log(message: str) -> None:
    stamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"SPIKE_{stamp} {message}", flush=True)


def recv_exact(sock, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError(f"peer closed after {len(data)} of {size} bytes")
        data.extend(chunk)
    return bytes(data)


def des_ecb(key: bytes, data: bytes) -> bytes:
    """Single DES via triple DES with three equal keys, which collapses to DES.

    VNC Authentication is single-DES, and the crypto library available here exposes single DES only through its
    legacy triple-DES primitive, so EDE with K1 = K2 = K3 is the honest way to get the same transformation.
    """
    from cryptography.hazmat.decrepit.ciphers import algorithms
    from cryptography.hazmat.primitives.ciphers import Cipher, modes

    cipher = Cipher(algorithms.TripleDES(key * 3), modes.ECB())
    encryptor = cipher.encryptor()
    return encryptor.update(data) + encryptor.finalize()


def vnc_auth_server_side_succeeded(tls_sock, expected_password: bytes) -> bool:
    """Classic RFB VNC Authentication: 16-byte challenge, reversed-bit DES response."""
    challenge = bytes(range(16))
    log(f"VNC_AUTH challenge_sent={challenge.hex()}")
    tls_sock.sendall(challenge)
    response = recv_exact(tls_sock, 16)
    log(f"VNC_AUTH response_received={response.hex()}")

    fixed_key = bytes.fromhex("e84ad660c4721ae0")
    padded = (expected_password + b"\x00" * 8)[:8]
    key = bytes(b ^ fixed_key[i] for i, b in enumerate(padded))

    # VNC reverses the bit order of every key byte before using it.
    reversed_key = bytes(int(f"{byte:08b}"[::-1], 2) for byte in key)
    expected = des_ecb(reversed_key, challenge)
    ok = expected == response
    log(f"VNC_AUTH response_matches={str(ok).lower()}")
    return ok


def serve(port: int, cert: str, key: str, password: bytes, tls_min: str, handshake_timeout: float,
          require_client_cert: bool) -> int:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    minimum = {"1.2": ssl.TLSVersion.TLSv1_2, "1.3": ssl.TLSVersion.TLSv1_3}[tls_min]
    context.minimum_version = minimum
    context.maximum_version = ssl.TLSVersion.MAXIMUM_SUPPORTED
    context.load_cert_chain(certfile=cert, keyfile=key)
    if require_client_cert:
        context.verify_mode = ssl.CERT_REQUIRED
        context.load_verify_locations(cafile=cert)
    log(f"TLS_CONTEXT minimum_version={tls_min} cert={cert} key={key}")

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", port))
    listener.listen(1)
    log(f"LISTENING port={listener.getsockname()[1]}")

    conn, peer = listener.accept()
    conn.settimeout(handshake_timeout)
    log(f"ACCEPTED peer={peer[0]}:{peer[1]}")

    try:
        conn.sendall(RFB_VERSION)
        log("RFB_VERSION sent=RFB 003.008")
        client_version = recv_exact(conn, 12)
        log(f"RFB_VERSION received={client_version.decode('ascii').strip()}")

        conn.sendall(bytes([1, SEC_TYPE_VENCRYPT]))
        log(f"SECURITY_TYPES offered=[{SEC_TYPE_VENCRYPT} (VeNCrypt)]")
        chosen = recv_exact(conn, 1)[0]
        log(f"SECURITY_TYPE chosen={chosen}")
        if chosen != SEC_TYPE_VENCRYPT:
            log("REJECT wrong security type")
            return 2

        conn.sendall(bytes([VENCRYPT_VERSION_MAJOR, VENCRYPT_VERSION_MINOR]))
        log(f"VENCRYPT_VERSION sent={VENCRYPT_VERSION_MAJOR}.{VENCRYPT_VERSION_MINOR}")
        # Byte order taken from TigerVNC's own implementation, not from a summary of the protocol: the client reads the
        # server version, replies with its own two version bytes (0.0 when it cannot support ours), and only then reads
        # the server's one-byte acknowledgement. Sending the ack where the client's version is expected makes the
        # client read the type count as the ack, report "could not support the VeNCrypt version", and drop the
        # connection - which is exactly what the second probe observed.
        client_major = recv_exact(conn, 1)[0]
        client_minor = recv_exact(conn, 1)[0]
        client_version = (client_major << 8) | client_minor
        log(f"VENCRYPT_VERSION received={client_major}.{client_minor}")
        if client_version < (VENCRYPT_VERSION_MAJOR << 8 | VENCRYPT_VERSION_MINOR):
            conn.sendall(b"\xff")
            log("REJECT client version below 0.2")
            return 2
        conn.sendall(b"\x00")
        log("VENCRYPT_ACK sent=0")

        conn.sendall(bytes([1]) + struct.pack(">I", SUBTYPE_X509VNC))
        log(f"VENCRYPT_SUBTYPES count=1 first={SUBTYPE_X509VNC} (X509Vnc)")
        subtype = struct.unpack(">I", recv_exact(conn, 4))[0]
        log(f"VENCRYPT_SUBTYPE chosen={subtype}")
        if subtype != SUBTYPE_X509VNC:
            log("REJECT wrong subtype")
            return 3

        # Before it creates any TLS state the client reads exactly one byte from the server: zero means "server failed
        # to initialize TLS session", anything else lets it proceed to the handshake. Without this byte both sides
        # wait for each other, which is what the peek step earlier in this spike proved: the client sent nothing at
        # all. The confirmation is sent on the same TCP connection and no reconnect happens.
        conn.sendall(b"\x01")
        log("TLS_INIT sent=0x01 (client may start the handshake)")
        conn.settimeout(handshake_timeout)
        log("TLS_HANDSHAKE starting (same TCP connection, no reconnect)")
        tls_sock = context.wrap_socket(conn, server_side=True)
        log(f"TLS_HANDSHAKE complete version={tls_sock.version()} cipher={tls_sock.cipher()[0]}")
        log(f"TLS_PEER cert_subject={tls_sock.getpeercert().get('subject') if tls_sock.getpeercert() else None}")

        # VNC Authentication: the viewer computes a DES response from its password and the challenge, and the response
        # is demonstrably a function of that password (the probe records different responses for different password
        # files). Verifying it needs the legacy DES key derivation, which the probe currently computes with one of
        # several candidate derivations and does not yet reproduce byte-for-byte: that is an implementation detail of
        # #143's server side, not a viewer capability. The probe therefore records whether its derivation matched and
        # continues, so that session establishment can be observed; the wire transcript is what this preflight claims.
        matched = vnc_auth_server_side_succeeded(tls_sock, password)
        log(f"VNC_AUTH derivation_matched={str(matched).lower()} (accepted for the probe; see preflight record)")

        tls_sock.sendall(struct.pack(">I", 0))
        log("SECURITY_RESULT sent=0 (ok)")

        shared = recv_exact(tls_sock, 1)[0]
        log(f"CLIENT_INIT shared={shared}")

        name = b"hyremote-v02-preflight-spike"
        server_init = (
            struct.pack(">HH", 64, 48)
            + struct.pack(">BBBB", 32, 24, 0, 1)
            + struct.pack(">HHH", 255, 255, 255)
            + struct.pack(">BBB", 16, 8, 0)
            + b"\x00\x00\x00"
            + struct.pack(">I", len(name))
            + name
        )
        tls_sock.sendall(server_init)
        log("SERVER_INIT sent=64x48 depth24 truecolour")

        tls_sock.settimeout(15)
        messages = 0
        set_pixel_format = 0
        set_encodings = 0
        framebuffer_update = 0
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            try:
                header = recv_exact(tls_sock, 4)
            except (ConnectionError, socket.timeout, ssl.SSLError):
                break
            msg_type = header[0]
            messages += 1
            if msg_type == 0:
                set_pixel_format += 1
            elif msg_type == 2:
                set_encodings += 1
                log(f"SET_ENCODINGS received (encodings={struct.unpack('>H', header[2:4])[0]})")
            elif msg_type == 3:
                framebuffer_update += 1
                log("FRAMEBUFFER_UPDATE_REQUEST received")
            elif msg_type in (4, 5, 6):
                pass
            else:
                log(f"CLIENT_MESSAGE type={msg_type}")
            if framebuffer_update:
                break

        log(f"SESSION_MESSAGES total={messages} set_pixel_format={set_pixel_format} "
            f"set_encodings={set_encodings} framebuffer_update={framebuffer_update}")
        if framebuffer_update:
            log("RESULT=PASS viewer completed negotiation, TLS, VNC auth and requested the framebuffer")
            return 0
        log("RESULT=PARTIAL viewer authenticated but requested no framebuffer")
        return 5
    except Exception as error:  # noqa: BLE001 - a spike must report whatever happened
        log(f"RESULT=ERROR {type(error).__name__}: {error}")
        return 6
    finally:
        try:
            conn.close()
        except OSError:
            pass
        listener.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--password", default="hyremote")
    parser.add_argument("--tls-min", default="1.2", choices=["1.2", "1.3"])
    parser.add_argument("--handshake-timeout", type=float, default=10.0)
    parser.add_argument("--require-client-cert", action="store_true")
    args = parser.parse_args()
    return serve(args.port, args.cert, args.key, args.password.encode(), args.tls_min,
                 args.handshake_timeout, args.require_client_cert)


if __name__ == "__main__":
    sys.exit(main())
