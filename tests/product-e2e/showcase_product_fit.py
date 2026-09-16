#!/usr/bin/env python3
"""Standard-viewer product-fit for the HyRemote remote support showcase."""

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


def line_count(lines: list[str], needle: str) -> int:
    return sum(1 for line in lines if needle in line)


def wait_for_count(process: subprocess.Popen[str], lines: list[str], value: int,
                   expected_occurrences: int, context: str, timeout: float = 5.0) -> None:
    needle = f"SHOWCASE_CLIENTS {value}"
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if line_count(lines, needle) >= expected_occurrences:
            return
        if process.poll() is not None:
            raise RuntimeError(f"{context}; process exited early: {lines}")
        time.sleep(0.05)
    raise RuntimeError(
        f"{context}: expected {expected_occurrences} occurrence(s) of {needle!r}: {lines}"
    )


def verify_rendered_image(path: Path) -> tuple[int, int]:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    require(width >= 400 and height >= 300, f"unexpected showcase framebuffer size: {image.size}")
    extrema = ImageStat.Stat(image).extrema
    require(any(low != high for low, high in extrema), "showcase framebuffer is visually uniform")
    return width, height


def start_showcase(executable: Path, port: int):
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    process = subprocess.Popen(
        [
            str(executable),
            "--port", str(port),
            "--auto-start",
            "--remote-input",
            "--test-seconds", "10",
        ],
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

    deadline = time.monotonic() + 9
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"showcase exited before remote start: {lines}")
        try:
            line = events.get(timeout=0.1)
        except queue.Empty:
            continue
        if line == f"REMOTE_STARTED {port}":
            wait_for_count(process, lines, 0, 1, "showcase did not expose listening-without-client state")
            return process, thread, lines

    process.kill()
    process.wait(timeout=5)
    raise RuntimeError(f"showcase did not start remote access: {lines}")


def verify_showcase(executable: Path) -> None:
    require(executable.exists(), f"showcase executable not found: {executable}")
    port = free_port()
    process, reader, lines = start_showcase(executable, port)

    try:
        with tempfile.TemporaryDirectory(prefix="hyremote-showcase-") as temp_dir:
            first = Path(temp_dir) / "first.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(first))
                wait_for_count(process, lines, 1, 1,
                               "showcase did not report the first connected viewer")
                width, height = verify_rendered_image(first)

                client.mouseMove(max(1, width // 2), max(1, height // 2))
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.3)

            # Require the disconnected state before reconnect. This proves the local operator UI
            # can distinguish listening from connected instead of treating Running as Connected.
            wait_for_count(process, lines, 0, 2,
                           "showcase did not report first viewer disconnect")

            second = Path(temp_dir) / "second.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(second))
                wait_for_count(process, lines, 1, 2,
                               "showcase did not report viewer reconnect")
                verify_rendered_image(second)

            wait_for_count(process, lines, 0, 3,
                           "showcase did not report second viewer disconnect")

        result = process.wait(timeout=14)
        reader.join(timeout=2)
        require(result == 0, f"showcase exited with {result}: {lines}")
        require(any(line.startswith("SHOWCASE_POINTER") for line in lines),
                "remote pointer did not reach the showcase Qt application")
        require(any(line.startswith("SHOWCASE_KEY") for line in lines),
                "remote keyboard did not reach the showcase Qt application")
        require(line_count(lines, "SHOWCASE_CLIENTS 1") >= 2,
                f"showcase did not expose connect/reconnect status: {lines}")
        require(line_count(lines, "SHOWCASE_CLIENTS 0") >= 3,
                f"showcase did not expose disconnected/listening status: {lines}")

        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0,
                    "showcase listener still accepts after application stop")
        finally:
            check.close()

        print(
            "PASS: remote-support-showcase -> client-count 0/1/reconnect -> "
            "standard viewer -> Qt input -> listener release"
        )
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--showcase", required=True, type=Path)
    args = parser.parse_args()

    try:
        verify_showcase(args.showcase.resolve())
        return 0
    finally:
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
