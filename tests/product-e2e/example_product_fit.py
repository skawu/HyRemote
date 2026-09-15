#!/usr/bin/env python3
"""Dual-OS standard-viewer acceptance for the public HyRemote C++ examples."""

from __future__ import annotations

import argparse
import os
import queue
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path

from PIL import Image, ImageStat
from vncdotool import api


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def free_port() -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])
    finally:
        sock.close()


def start_example(executable: Path, port: int, quick: bool):
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    if quick:
        env["QT_QUICK_BACKEND"] = "software"

    process = subprocess.Popen(
        [str(executable), "--port", str(port), "--remote-input", "--test-seconds", "7"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=env,
    )
    lines: list[str] = []
    events: queue.Queue[str] = queue.Queue()

    def reader() -> None:
        assert process.stdout is not None
        for raw in process.stdout:
            line = raw.rstrip("\r\n")
            lines.append(line)
            events.put(line)

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"example exited before READY: {lines}")
        try:
            line = events.get(timeout=0.1)
        except queue.Empty:
            continue
        if line == f"READY {port}":
            return process, thread, lines

    process.kill()
    process.wait(timeout=5)
    raise RuntimeError(f"example did not become ready: {lines}")


def verify_rendered_image(path: Path) -> tuple[int, int]:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    require(width >= 200 and height >= 120, f"unexpected framebuffer size: {image.size}")
    extrema = ImageStat.Stat(image).extrema
    require(any(low != high for low, high in extrema), "captured framebuffer is visually uniform")
    return width, height


def verify_example(name: str, executable: Path, quick: bool) -> None:
    require(executable.exists(), f"{name} executable not found: {executable}")
    port = free_port()
    process, reader, lines = start_example(executable, port, quick)

    try:
        with tempfile.TemporaryDirectory(prefix=f"hyremote-{name}-") as temp_dir:
            first = Path(temp_dir) / "first.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(first))
                width, height = verify_rendered_image(first)

                # Both examples intentionally place the click target at the same proportional
                # location. Using framebuffer-relative coordinates also covers non-1.0 DPR mapping.
                x = max(1, int(width * 0.25))
                y = max(1, int(height * 0.41))
                client.mouseMove(x, y)
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.25)

            second = Path(temp_dir) / "second.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(second))
                verify_rendered_image(second)

        result = process.wait(timeout=12)
        reader.join(timeout=2)
        require(result == 0, f"{name} exited with {result}: {lines}")
        require(any(line.startswith("APP_POINTER") for line in lines),
                f"{name}: remote pointer did not reach the Qt application")
        require(any(line.startswith("APP_KEY") for line in lines),
                f"{name}: remote keyboard did not reach the Qt application")
        require("STOPPED" in lines, f"{name}: public RemoteAccess did not stop cleanly")

        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0,
                    f"{name}: listener still accepts after RemoteAccess::stop()")
        finally:
            check.close()

        print(f"PASS: {name} public facade -> standard viewer -> Qt input -> reconnect -> stop")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--widgets", required=True, type=Path)
    parser.add_argument("--quick", required=True, type=Path)
    args = parser.parse_args()

    try:
        verify_example("widgets-basic", args.widgets.resolve(), quick=False)
        verify_example("quick-basic", args.quick.resolve(), quick=True)
        return 0
    finally:
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
