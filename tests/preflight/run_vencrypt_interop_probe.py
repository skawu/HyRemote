#!/usr/bin/env python3
"""Run the real-viewer interoperability probe for the frozen V0.2 secure profile.

Wire profile under test (issue #258, Preflight A):

    RFB 3.8 -> VeNCrypt 0.2 -> X509Vnc 261 -> TLS >= 1.2 -> VNC Authentication -> normal RFB session

The probe starts the minimal VeNCrypt spike server, then drives a real maintained viewer (TigerVNC vncviewer) at it
with the security type forced, and prints both transcripts. Evidence comes from the server-side negotiation log and
the viewer's own exit status.
"""

from __future__ import annotations

import argparse
import datetime
import ipaddress
import os
import pathlib
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import vencrypt_spike_server as spike  # noqa: E402


def free_port() -> int:
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.bind(("127.0.0.1", 0))
    port = int(probe.getsockname()[1])
    probe.close()
    return port


def make_self_signed(cert_path: pathlib.Path, key_path: pathlib.Path) -> None:
    import datetime as dt

    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID

    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "hyremote-preflight-spike")])
    now = dt.datetime.now(dt.timezone.utc)
    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(subject)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - dt.timedelta(minutes=5))
        .not_valid_after(now + dt.timedelta(days=2))
        .add_extension(x509.SubjectAlternativeName([x509.DNSName("localhost"),
                                                    x509.IPAddress(ipaddress.ip_address("127.0.0.1"))]), critical=False)
        .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
        .sign(key, hashes.SHA256())
    )
    key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                                            serialization.PrivateFormat.TraditionalOpenSSL,
                                            serialization.NoEncryption()))
    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    print(f"SPIKE_ORCH self_signed cert={cert_path.name} key={key_path.name} cn=hyremote-preflight-spike")


def make_vnc_passwd(password: str, path: pathlib.Path) -> None:
    """TigerVNC's password file: the 8-byte password XOR the fixed VNC key, no bit reversal."""
    fixed_key = bytes.fromhex("e84ad660c4721ae0")
    padded = (password.encode() + b"\x00" * 8)[:8]
    path.write_bytes(bytes(b ^ fixed_key[i] for i, b in enumerate(padded)))
    print(f"SPIKE_ORCH password_file={path.name} password={password!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--viewer", required=True, help="path to the real viewer executable")
    parser.add_argument("--workdir", required=True)
    parser.add_argument("--password", default="hyremote")
    parser.add_argument("--tls-min", default="1.2")
    parser.add_argument("--viewer-seconds", type=int, default=25)
    args = parser.parse_args()

    work = pathlib.Path(args.workdir)
    work.mkdir(parents=True, exist_ok=True)
    cert = work / "spike-cert.pem"
    key = work / "spike-key.pem"
    passwd = work / "spike-passwd"
    make_self_signed(cert, key)
    make_vnc_passwd(args.password, passwd)

    port = free_port()
    result: dict[str, int] = {}

    def server() -> None:
        result["rc"] = spike.serve(port, str(cert), str(key), args.password.encode(), args.tls_min, 15.0, False)

    thread = threading.Thread(target=server, daemon=True)
    thread.start()
    time.sleep(1.5)

    # TigerVNC stores accepted server certificates per host:port; seeding it lets the viewer proceed unattended, which
    # is the same trust decision a user makes through its prompt.
    home = pathlib.Path(os.environ.get("USERPROFILE", str(pathlib.Path.home())))
    known = home / ".vnc"
    known.mkdir(parents=True, exist_ok=True)
    for name in (f"localhost:{port}.pem", f"127.0.0.1:{port}.pem"):
        (known / name).write_bytes(cert.read_bytes())
    # TigerVNC verifies the server certificate against X509CA, which defaults to <vnc config dir>/x509_ca.pem. Seeding
    # it is the documented way a user trusts a CA, and it keeps the probe unattended.
    (known / "x509_ca.pem").write_bytes(cert.read_bytes())
    print(f"SPIKE_ORCH seeded_certs={known} names=localhost:{port}.pem,127.0.0.1:{port}.pem,x509_ca.pem")

    command = [
        args.viewer,
        f"localhost:{port}",
        f"-SecurityTypes=X509Vnc",
        f"-PasswordFile={passwd}",
        "-Log=*:stderr:30",
    ]
    print("SPIKE_ORCH viewer_command=" + " ".join(command))
    started = time.monotonic()
    # A viewer that succeeds keeps the session open, so it is expected not to exit on its own. Running past the window
    # means the session came up; the server transcript is what decides the result.
    timed_out = False
    try:
        viewer = subprocess.run(command, capture_output=True, text=True, timeout=args.viewer_seconds)
    except subprocess.TimeoutExpired as expired:
        timed_out = True
        viewer = expired
    elapsed = time.monotonic() - started
    stdout = viewer.stdout or b""
    stderr = viewer.stderr or b""
    if isinstance(stdout, bytes):
        stdout = stdout.decode("utf-8", "replace")
    if isinstance(stderr, bytes):
        stderr = stderr.decode("utf-8", "replace")
    print(f"SPIKE_ORCH viewer_exit={'timeout(kept-open)' if timed_out else viewer.returncode} elapsed={elapsed:.1f}s")
    for line in stdout.splitlines()[-20:]:
        print(f"SPIKE_VIEWER_OUT {line}")
    for line in stderr.splitlines()[-30:]:
        print(f"SPIKE_VIEWER_ERR {line}")

    thread.join(timeout=20)
    print(f"SPIKE_ORCH server_exit={result.get('rc', 'still-running')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
