#!/usr/bin/env python3
"""Real Generic Qt6 OpenGL Source -> Runtime -> RFB Raw CPU pixel smoke.

This is a headless Xvfb/Mesa protocol test, NOT a physical RealVNC Viewer.
Only an ordinary Qt-only QQuickView app is launched. No third-party modules,
no browser and no CI packages. It proves a fresh dynamic RFB pixel path.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time


def read_exact(sock: socket.socket, count: int) -> bytes:
    got = bytearray()
    while len(got) < count:
        new = sock.recv(min(count - len(got), 1 << 17))
        if not new:
            raise RuntimeError(f"RFB transport closed with {count-len(got)} bytes remaining")
        got.extend(new)
    return bytes(got)


def rfb_handshake(sock: socket.socket) -> tuple[int, int]:
    version = read_exact(sock, 12)
    if version != b"RFB 003.008\n":
        raise RuntimeError(f"Unexpected RFB protocol {version!r}")
    sock.sendall(version)
    n = read_exact(sock, 1)[0]
    if n == 0 or n > 16:
        raise RuntimeError("Bad RFB security count")
    choices = read_exact(sock, n)
    if 1 not in choices:
        raise RuntimeError(f"RFB NoAuthentication unavailable: {choices!r}")
    sock.sendall(b"\x01")
    if read_exact(sock, 4) != b"\0\0\0\0":
        raise RuntimeError("RFB security result nonzero")
    sock.sendall(b"\x01")
    init = read_exact(sock, 24)
    width, height = struct.unpack(">HH", init[:4])
    name_size = struct.unpack(">I", init[20:24])[0]
    if not (0 < width <= 3840 and 0 < height <= 2160 and name_size <= 2048):
        raise RuntimeError("Invalid RFB ServerInit dimensions/name")
    read_exact(sock, name_size)
    # Only Raw so no decoder ambiguity or inferred tight/trle rectangle.
    sock.sendall(struct.pack(">BBHi", 2, 0, 1, 0))
    return width, height


def next_frame(sock: socket.socket, width: int, height: int):
    # The four interior probes each live in a different QML quadrant. Unlike
    # uniform-center tests, this detects reversed GL scanlines and mixtures
    # of different source epochs during RFB composition.
    points = [
        (width // 4, height // 4),
        (3 * width // 4, height // 4),
        (width // 4, 3 * height // 4),
        (3 * width // 4, 3 * height // 4),
    ]
    samples = [None] * len(points)
    sock.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, width, height))
    header = read_exact(sock, 4)
    if header[0] != 0:
        raise RuntimeError(f"Unexpected RFB message {header[0]}")
    count = struct.unpack(">H", header[2:4])[0]
    if not (0 <= count <= 512):
        raise RuntimeError(f"Unbounded RFB rectangle count {count}")
    for _ in range(count):
        x, y, w, h, encoding = struct.unpack(">HHHHi", read_exact(sock, 12))
        if encoding != 0:
            raise RuntimeError(f"Non-Raw RFB rectangle despite Raw-only request: {encoding}")
        if not (0 < w <= width and 0 < h <= height and x+w <= width and y+h <= height):
            raise RuntimeError("Invalid RFB pixel rectangle")
        raw = read_exact(sock, w * h * 4)
        for i, (px, py) in enumerate(points):
            if x <= px < x+w and y <= py < y+h:
                off = ((py-y)*w + (px-x))*4
                samples[i] = tuple(raw[off:off+3])
    if not all(sample is not None for sample in samples):
        raise RuntimeError(f"Raw full-frame request lacked all four quadrants: {samples}")
    return tuple(samples)


def within(rgb: tuple[int, int, int], expected: tuple[int, int, int]) -> bool:
    return all(abs(a - b) <= 20 for a, b in zip(rgb, expected))


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--app", type=Path, required=True)
    p.add_argument("--port", type=int, default=6102)
    p.add_argument("--mode", choices=["ON", "OFF"], default="ON")
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    logfile = args.out / f"generic-quick-{args.mode.lower()}.log"
    if not args.app.is_file():
        raise RuntimeError("Installed Qt-only Quick test app missing")

    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "xcb"
    env["QSG_RHI_BACKEND"] = "opengl"
    env["QSG_RENDER_LOOP"] = "threaded"
    env["LIBGL_ALWAYS_SOFTWARE"] = "1"
    palette = [(226, 38, 52), (39, 207, 64), (44, 68, 232),
               (238, 196, 30)]
    results = []
    pid = None

    with logfile.open("wb") as stdout:
        host = subprocess.Popen(
            [str(args.app), "-plugin", f"hyremote:port={args.port}"],
            stdout=stdout, stderr=subprocess.STDOUT, env=env)
        pid = host.pid
        try:
            conn = None
            for _ in range(100):
                if host.poll() is not None:
                    raise RuntimeError(f"QtQuick Generic Host exited early code={host.returncode}")
                try:
                    conn = socket.create_connection(("127.0.0.1", args.port), timeout=0.5)
                    conn.settimeout(10)
                    break
                except (OSError, ConnectionError):
                    time.sleep(0.1)
            if conn is None:
                raise RuntimeError("Real Generic RFB listener not reachable")
            with conn:
                width, height = rfb_handshake(conn)
                if (width, height) != (640, 360):
                    raise RuntimeError(f"Unexpected Generic Quick surface geometry {width}x{height}")
                for _ in range(18):
                    results.append(next_frame(conn, width, height))
                    time.sleep(0.12)
        finally:
            host.terminate()
            try:
                host.wait(timeout=6)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait(timeout=3)

    contents = logfile.read_text(encoding="utf-8", errors="replace")
    observed_phases = []
    for image in results:
        found = None
        for phase in range(3):
            if all(within(image[q], palette[(phase + q) % 4])
                   for q in range(4)):
                found = phase
                break
        observed_phases.append(found)
    valid_phases = [phase for phase in observed_phases if phase is not None]
    distinct = sorted(set(valid_phases))
    tap_count = contents.count("HYREMOTE_TEST_QT6_GL_TAP_READBACK=OK")
    fallback_count = contents.count("HYREMOTE_TEST_QT6_GL_TAP_FALLBACK=NO_NATURAL_FRAME")
    outcome = {
        "schema": "hyremote-qt6-gl-generic-rfb-smoke/1",
        "mode": args.mode,
        "qt": "6.8.3",
        "qpa": "xcb",
        "rhi": "opengl",
        "render_loop": "threaded",
        "real_rfb_handshake": True,
        "rfb_full_frame_requests": 18,
        "rfb_raw_pixel_centers": len(results),
        "complete_four_quadrant_source_frames": len(valid_phases),
        "source_epoch_mixed_or_wrong_quadrant_frames": len(results) - len(valid_phases),
        "different_valid_scene_phases": len(distinct),
        "quadrants_per_frame": 4,
        "test_only_render_tap_success": tap_count,
        "test_only_natural_frame_fallback": fallback_count,
        "real_viewer_p95_qualified": False,
        "site_handoff": "BLOCKED",
        "product_pass": False,
        "app_process_exited_under_controller": True,
    }
    (args.out / f"rfb-smoke-{args.mode.lower()}.json").write_text(
        json.dumps(outcome, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    print("GENERIC_QT6_GL_RFB_SMOKE_JSON="+json.dumps(outcome, sort_keys=True))
    if not contents.count("APP_READY"):
        raise RuntimeError("Qt-only Host failed to print APP_READY")
    if not contents.count("PLATFORM_NAME=xcb"):
        raise RuntimeError("Generic plugin silently changed the native Qt QPA")
    if len(results) < 6 or len(distinct) < 2 or len(valid_phases) != len(results):
        raise RuntimeError(
            "Real RFB delivery had stale/mixed/wrong-orientation four-quadrant frames: "
            f"phases={observed_phases}; samples={results[:3]}")
    if args.mode == "ON" and tap_count < 1:
        raise RuntimeError("Experimental Qt6 OpenGL frame tap never executed on any real Generic RFB request")
    if args.mode == "OFF" and (tap_count or fallback_count):
        raise RuntimeError("OFF build unexpectedly executed experimental source code")
    print("GENERIC_QT6_GL_RFB_SMOKE=PASS_NON_PRODUCT_PIXELS")

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("GENERIC_QT6_GL_RFB_SMOKE=FAIL " + str(exc), file=sys.stderr)
        raise
