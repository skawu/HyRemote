#!/usr/bin/env python3
"""R4 NON-PRODUCT Linux Generic/OpenGL-RHI source→Session→RFB full-pixel smoke.

Starts an ordinary Qt-only QQuickView via the real deployed Generic plugin and
reads a real RFB 3.8 framebuffer using the maintained vncdotool Viewer library.
This is a virtual desktop/Xvfb CI test, NOT physical RealVNC or EGLFS proof.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

from PIL import Image
from vncdotool import api


def require(ok, msg):
    if not ok:
        raise RuntimeError(msg)


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def wait_listener(proc, port, deadline):
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"Qt host quit before RFB listen; exit={proc.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("Qt Generic RFB listener did not become reachable")


def sample(path):
    with Image.open(path) as im:
        rgb = im.convert("RGB")
        return rgb.size, rgb.getpixel((8, 8)), rgb.getpixel((rgb.width - 9, rgb.height - 9))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--arm", choices=["OFF", "ON"], required=True)
    args = parser.parse_args()

    require(args.exe.is_file(), f"installed Qt-only app missing: {args.exe}")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    port = free_port()
    env = os.environ.copy()
    require(env.get("QSG_RHI_BACKEND") == "opengl",
            "This test qualifies only exact Qt6.8.3 OpenGL RHI")
    cmd = [str(args.exe.resolve()), "-plugin", f"hyremote:port={port}", "--test-seconds", "30"]
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="hyremote-r4-generic-") as temp:
        logpath = args.out.parent / f"generic-{args.arm.lower()}-host.log"
        with logpath.open("w", encoding="utf-8") as log:
            proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                                    env=env, text=True, start_new_session=True)
            try:
                wait_listener(proc, port, time.monotonic() + 12)
                # The app must render while the listener is bootstrapping. A
                # bounded startup settlement is allowed, not an A/B sampling
                # workaround or arbitrary retries until color happens to pass.
                time.sleep(0.5)
                png = Path(temp) / "viewer-frame.png"
                with api.connect(f"127.0.0.1::{port}", password=None, timeout=12) as viewer:
                    viewer.captureScreen(str(png))
                shape, top_left, bottom_right = sample(png)
                require(shape == (460, 190),
                        f"unexpected real RFB frame size {shape}; expected 460x190")
                expected = (0x20, 0x27, 0x33)
                require(all(abs(a - b) <= 3 for a,b in zip(top_left, expected)),
                        f"actual RFB pixel mismatch top-left={top_left}, expected={expected}")
                require(all(abs(a - b) <= 3 for a,b in zip(bottom_right, expected)),
                        f"actual RFB pixel mismatch bottom-right={bottom_right}, expected={expected}")
                elapsed = time.monotonic() - started
                log.flush()
                trace = logpath.read_text(encoding="utf-8")
                require("APP_READY" in trace and "PLATFORM_NAME=xcb" in trace,
                        "Generic activation did not preserve the original xcb platform")
                if args.arm == "ON":
                    require("HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=GL_RHI_PBO_SOURCE" in trace,
                            "ON product did not select the real experimental PBO CaptureSource")
                else:
                    require("HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=GL_RHI_PBO_SOURCE" not in trace,
                            "OFF product unexpectedly selected experimental PBO source")
                result = {
                    "schema": "hyremote-r4-generic-rfb-real-viewport/1",
                    "arm": args.arm,
                    "qt": "6.8.3",
                    "platform": "xcb",
                    "graphics_api": "OpenGL_RHI",
                    "pixel_size": list(shape),
                    "top_left_rgb": top_left,
                    "bottom_right_rgb": bottom_right,
                    "source_route": "GlPboCaptureSource" if args.arm == "ON" else "PublicQtGrabToImage",
                    "actual_rfb_handshake_and_full_frame": True,
                    "wall_time_to_first_complete_frame_s": round(elapsed, 3),
                    "real_physical_viewer": False,
                    "full_dynamic_consistency_test": False,
                    "qualified_for_product": False,
                }
                args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
                print("R4_GENERIC_RFB_PIXEL_E2E=PASS_SCOPED_VIRTUAL_VIEWER", json.dumps(result))
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait(timeout=5)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"R4_GENERIC_RFB_PIXEL_E2E=FAIL: {exc}", file=sys.stderr)
        raise
