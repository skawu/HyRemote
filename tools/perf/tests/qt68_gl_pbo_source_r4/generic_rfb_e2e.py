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
    parser.add_argument("--scale-factor", type=float, default=1.0)
    args = parser.parse_args()
    require(args.scale_factor in (1.0, 1.5),
            "supported deterministic DPR fixture scale factors are 1.0 and 1.5")

    require(args.exe.is_file(), f"installed Qt-only app missing: {args.exe}")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    port = free_port()
    env = os.environ.copy()
    env["QT_SCALE_FACTOR"] = str(args.scale_factor)
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
                colors = set()
                frames_observed = 0
                with api.connect(f"127.0.0.1::{port}", password=None, timeout=12) as viewer:
                    # One real framebuffer frame first, then repeatedly sample
                    # *the same connected RFB client* as ordinary QML Timer
                    # drives new Scene Graph pixels. Do not infer all source
                    # events were seen: this is a bounded delivery smoke.
                    viewer.captureScreen(str(png))
                    for index in range(12):
                        if index:
                            time.sleep(0.3)
                        current = Path(temp) / f"dynamic-{index}.png"
                        viewer.captureScreen(str(current))
                        with Image.open(current) as frame:
                            rgb = frame.convert("RGB")
                            marker_point = (round(245 * args.scale_factor),
                                            round(140 * args.scale_factor))
                            require(marker_point[0] < rgb.width
                                    and marker_point[1] < rgb.height,
                                    f"DPR capture produced undersized RFB image: "
                                    f"scale={args.scale_factor} frame={rgb.size} "
                                    f"expected={(round(460*args.scale_factor),round(190*args.scale_factor))} "
                                    f"marker={marker_point}")
                            marker = rgb.getpixel(marker_point)
                        red = (0xdf, 0x25, 0x34)
                        green = (0x25, 0xd2, 0x42)
                        if all(abs(x-y) <= 8 for x, y in zip(marker, red)):
                            colors.add("red")
                        elif all(abs(x-y) <= 8 for x, y in zip(marker, green)):
                            colors.add("green")
                        else:
                            raise RuntimeError(
                                f"RFB dynamic marker has unexpected pixel {marker} at sample {index}")
                        frames_observed += 1
                shape, top_left, bottom_right = sample(png)
                require(colors == {"red", "green"},
                        f"Real VNC did not display both independent QML source states: {colors}")
                expected_size = (round(460 * args.scale_factor),
                                 round(190 * args.scale_factor))
                require(shape == expected_size,
                        f"incorrect RFB physical framebuffer size at DPR={args.scale_factor}: "
                        f"actual={shape} expected={expected_size}")
                if args.scale_factor != 1.0:
                    # The reported Windows DPR failure also closed the listener
                    # after the original RFB session reset. Exercise a NEW
                    # connection to the same still-alive Host in the same run.
                    with api.connect(f"127.0.0.1::{port}", password=None, timeout=12) as reconnect:
                        reconnect_png = Path(temp) / "reconnected-dpr.png"
                        reconnect.captureScreen(str(reconnect_png))
                        reconnect_size, _, _ = sample(reconnect_png)
                        require(reconnect_size == expected_size,
                                f"DPR reconnect pixel size changed: {reconnect_size}")
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
                require(trace.count("HYREMOTE_R4_QML_SOURCE_PHASE=") >= 2,
                        "Qt QML independent source Timer did not produce two real changes")
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
                    "window_scale_factor": args.scale_factor,
                    "physical_framebuffer_matches_scaled_window": shape == expected_size,
                    "reconnected_after_high_dpr": args.scale_factor != 1.0,
                    "top_left_rgb": top_left,
                    "bottom_right_rgb": bottom_right,
                    "source_route": "GlPboCaptureSource" if args.arm == "ON" else "PublicQtGrabToImage",
                    "actual_rfb_handshake_and_full_frame": True,
                    "dynamic_source_colors_visible_via_real_rfb": sorted(colors),
                    "dynamic_rfb_viewport_observations": frames_observed,
                    "dynamic_qml_timed_source_changes_minimum": 2,
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
