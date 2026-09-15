#!/usr/bin/env python3
"""Dual-OS product-fit probe for HyRemote's production internal RFB transport."""

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

from PIL import Image
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


def verify_occupied_port_failure(executable: Path) -> None:
    occupied = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    occupied.bind(("127.0.0.1", 0))
    occupied.listen(1)
    port = int(occupied.getsockname()[1])
    try:
        started = time.monotonic()
        result = subprocess.run(
            [str(executable), str(port)],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
        elapsed = time.monotonic() - started
        require(result.returncode == 2, f"occupied port returned {result.returncode}: {result.stdout} {result.stderr}")
        require("START_FAILED" in result.stdout, "occupied port was not reported as start failure")
        require(elapsed < 5, f"occupied-port failure was not bounded ({elapsed:.3f}s)")
    finally:
        occupied.close()


def start_harness(executable: Path, port: int):
    env = os.environ.copy()
    env["HYREMOTE_RFB_TEST_SECONDS"] = "8"
    process = subprocess.Popen(
        [str(executable), str(port)],
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
            raise RuntimeError(f"RFB harness exited before READY: {lines}")
        try:
            line = events.get(timeout=0.1)
        except queue.Empty:
            continue
        if line == f"READY {port}":
            return process, thread, lines
    process.kill()
    process.wait(timeout=5)
    raise RuntimeError(f"RFB harness did not become ready: {lines}")


def verify_standard_client(executable: Path) -> None:
    port = free_port()
    process, reader, lines = start_harness(executable, port)
    try:
        with tempfile.TemporaryDirectory(prefix="hyremote-rfb-product-") as temp_dir:
            first = Path(temp_dir) / "first.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(first))
                image = Image.open(first).convert("RGB")
                require(image.size == (64, 48), f"unexpected framebuffer size {image.size}")
                pixel = image.getpixel((32, 24))
                require(
                    all(abs(actual - expected) <= 1 for actual, expected in zip(pixel, (0x33, 0x66, 0x99))),
                    f"known framebuffer pixel mismatch: {pixel}",
                )
                client.mouseMove(17, 13)
                client.mouseDown(1)
                client.mouseUp(1)
                # RFB buttons 4/5 are wheel up/down. Exercise one wheel transition through the
                # maintained client rather than a HyRemote protocol test double.
                client.mouseDown(4)
                client.mouseUp(4)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.2)

            second = Path(temp_dir) / "second.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(second))
            require(Image.open(second).size == (64, 48), "reconnect capture failed")

        # Let the harness perform its own Transport::stop() so listener release/quiescence are part
        # of the evidence rather than hidden by process termination.
        result = process.wait(timeout=12)
        reader.join(timeout=2)
        require(result == 0, f"RFB harness failed: {lines}")
        require(any(line.startswith("INPUT pointer-move x=17") for line in lines), "pointer move missing")
        require(any("INPUT pointer-button" in line and "pressed=1" in line for line in lines), "button down missing")
        require(any("INPUT pointer-button" in line and "pressed=0" in line for line in lines), "button up missing")
        require(any("INPUT pointer-scroll" in line and "scrollY=1" in line for line in lines), "wheel input missing")
        require(any("INPUT key" in line and "pressed=1" in line for line in lines), "key down missing")
        require(any("INPUT key" in line and "pressed=0" in line for line in lines), "key up missing")
        require(any("INPUT text" in line and "text=a" in line for line in lines), "committed text missing")
        require(sum(1 for line in lines if "RFB client connected" in line) >= 2, "reconnect event missing")
        require("STOPPED" in lines, "transport did not stop cleanly")

        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0, "listener still accepts after stop")
        finally:
            check.close()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=Path)
    args = parser.parse_args()
    executable = args.server.resolve()
    require(executable.exists(), f"test server not found: {executable}")

    try:
        verify_occupied_port_failure(executable)
        verify_standard_client(executable)
        print("PASS: bounded RFB bind + vncdotool framebuffer/input/reconnect + quiescent stop")
        return 0
    finally:
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
