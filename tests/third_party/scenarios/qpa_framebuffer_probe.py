#!/usr/bin/env python3
"""Generic pristine-third-party Transparent-QPA RFB smoke probe.

This intentionally uses only RFB 3.8 / SecurityType None and requests a tiny
raw framebuffer rectangle. It proves that the deployed third-party executable
starts through qhyremote, accepts a real viewer protocol handshake, produces
framebuffer bytes, and accepts a second viewer connection. Physical local-input
coexistence remains the separate #109 gate.
"""

from __future__ import annotations

import argparse
import os
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError(f"RFB peer closed while waiting for {size} bytes ({len(data)} received)")
        data.extend(chunk)
    return bytes(data)


def wait_for_port(port: int, process: subprocess.Popen[str]) -> None:
    deadline = time.monotonic() + 15.0
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            output = process.stdout.read() if process.stdout is not None else ""
            raise RuntimeError(f"application exited before RFB listener: {process.returncode}\n{output}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.4):
                return
        except OSError as error:
            last_error = error
            time.sleep(0.1)
    raise RuntimeError(f"RFB listener did not become ready: {last_error}")


def rfb_session(port: int, request_framebuffer: bool) -> tuple[int, int, str]:
    with socket.create_connection(("127.0.0.1", port), timeout=3.0) as sock:
        sock.settimeout(5.0)
        banner = recv_exact(sock, 12)
        require(banner.startswith(b"RFB 003.008"), f"unexpected RFB banner: {banner!r}")
        sock.sendall(b"RFB 003.008\n")

        count = recv_exact(sock, 1)[0]
        require(count > 0, "server offered no RFB security types")
        security_types = recv_exact(sock, count)
        require(1 in security_types, f"SecurityType None missing: {security_types!r}")
        sock.sendall(b"\x01")
        status = struct.unpack(">I", recv_exact(sock, 4))[0]
        require(status == 0, f"RFB SecurityResult failed: {status}")

        sock.sendall(b"\x01")  # shared ClientInit
        server_init = recv_exact(sock, 24)
        width, height = struct.unpack(">HH", server_init[:4])
        bits_per_pixel = server_init[4]
        name_length = struct.unpack(">I", server_init[20:24])[0]
        name = recv_exact(sock, name_length).decode("utf-8", errors="replace")
        require(width > 0 and height > 0, f"invalid framebuffer size {width}x{height}")
        require(bits_per_pixel in (8, 16, 32), f"unexpected bpp {bits_per_pixel}")

        if request_framebuffer:
            # No SetEncodings means Raw is the required baseline encoding. Ask
            # for a single pixel so the probe is deterministic and bounded.
            sock.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, 1, 1))
            msg_type = recv_exact(sock, 1)[0]
            require(msg_type == 0, f"expected FramebufferUpdate, got server message {msg_type}")
            header = recv_exact(sock, 3)
            rect_count = struct.unpack(">H", header[1:3])[0]
            require(rect_count >= 1, "FramebufferUpdate contained no rectangles")
            rect = recv_exact(sock, 12)
            _x, _y, rw, rh, encoding = struct.unpack(">HHHHi", rect)
            require(encoding == 0, f"expected bounded Raw rectangle, got encoding {encoding}")
            require(rw >= 1 and rh >= 1, f"invalid Raw rectangle {rw}x{rh}")
            payload_size = rw * rh * (bits_per_pixel // 8)
            payload = recv_exact(sock, payload_size)
            require(len(payload) == payload_size, "short Raw framebuffer payload")

        return width, height, name


def locate_platform_plugins(prefix: Path) -> tuple[Path, Path]:
    qhyremote = list(prefix.rglob("qhyremote.dll")) + list(prefix.rglob("libqhyremote.so"))
    native_name = "qwindows.dll" if os.name == "nt" else "libqxcb.so"
    native = list(prefix.rglob(native_name))
    require(len(qhyremote) == 1, f"expected one deployed qhyremote under {prefix}, got {qhyremote}")
    require(len(native) == 1, f"expected one deployed native QPA delegate under {prefix}, got {native}")
    return qhyremote[0], native[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--port", type=int, default=5994)
    parser.add_argument(
        "--app-arg",
        action="append",
        default=[],
        help="Extra application argument inserted before the Qt -platform option; repeatable.",
    )
    args = parser.parse_args()

    app = args.app.resolve()
    prefix = args.prefix.resolve()
    require(app.is_file(), f"third-party executable not found: {app}")
    qhyremote, native = locate_platform_plugins(prefix)

    env = os.environ.copy()
    for key in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_PLATFORM", "LD_LIBRARY_PATH"):
        env.pop(key, None)

    command = [str(app), *args.app_arg, "-platform", f"hyremote:hyremote-port={args.port}"]
    process = subprocess.Popen(
        command,
        cwd=str(app.parent),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        wait_for_port(args.port, process)
        width, height, name = rfb_session(args.port, request_framebuffer=True)
        width2, height2, _ = rfb_session(args.port, request_framebuffer=False)
        require((width2, height2) == (width, height), "framebuffer geometry changed across reconnect unexpectedly")
        print(
            f"PASS: pristine third-party QPA -> {qhyremote.name} + {native.name} -> "
            f"RFB {width}x{height} raw framebuffer + reconnect; desktop={name!r}"
        )
        return 0
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        if process.returncode not in (0, None):
            output = process.stdout.read() if process.stdout is not None else ""
            if output:
                print(output, file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
