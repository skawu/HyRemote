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


def start_example(executable: Path, port: int, quick: bool, remote_input: bool, seconds: int):
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    if quick:
        env["QT_QUICK_BACKEND"] = "software"

    command = [str(executable), "--port", str(port), "--test-seconds", str(seconds)]
    if remote_input:
        command.append("--remote-input")

    process = subprocess.Popen(
        command,
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


def send_acceptance_input(client, width: int, height: int) -> None:
    # Both examples intentionally place the click target at the same proportional location. Using
    # framebuffer-relative coordinates also covers non-1.0 DPR mapping.
    x = max(1, int(width * 0.25))
    y = max(1, int(height * 0.41))
    client.mouseMove(x, y)

    # RFB buttons 1/2/3 are left/middle/right. Buttons 4/5 are wheel transitions; exercise wheel-up
    # here because the lower transport product-fit already checks its normalized scroll value.
    for button in (1, 2, 3):
        client.mouseDown(button)
        client.mouseUp(button)
    client.mouseDown(4)
    client.mouseUp(4)

    # Plain key/text is checked independently from modifier delivery. vncdotool's documented
    # compound-key syntax sends explicit modifier press/release sequences around the base key.
    client.keyDown("a")
    client.keyUp("a")
    client.keyPress("shift-a")
    client.keyPress("ctrl-a")
    client.keyPress("alt-a")


def verify_listener_released(name: str, port: int) -> None:
    check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        check.settimeout(0.25)
        require(check.connect_ex(("127.0.0.1", port)) != 0,
                f"{name}: listener still accepts after RemoteAccess::stop()")
    finally:
        check.close()


def verify_view_only(name: str, executable: Path, quick: bool) -> None:
    port = free_port()
    process, reader, lines = start_example(executable, port, quick, remote_input=False, seconds=4)
    try:
        with tempfile.TemporaryDirectory(prefix=f"hyremote-{name}-view-only-") as temp_dir:
            image_path = Path(temp_dir) / "view-only.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(image_path))
                width, height = verify_rendered_image(image_path)
                send_acceptance_input(client, width, height)
                time.sleep(0.35)

        # View-only is not merely a UI label: the transport may accept a viewer connection, but no
        # normalized remote input may reach the Qt application while the policy is disabled.
        require(not any(line.startswith("APP_POINTER") for line in lines),
                f"{name}: pointer reached application in default view-only mode: {lines}")
        require(not any(line.startswith("APP_WHEEL") for line in lines),
                f"{name}: wheel reached application in default view-only mode: {lines}")
        require(not any(line.startswith("APP_KEY") for line in lines),
                f"{name}: key reached application in default view-only mode: {lines}")
        require(not any(line.startswith("APP_TEXT") for line in lines),
                f"{name}: text reached application in default view-only mode: {lines}")

        result = process.wait(timeout=9)
        reader.join(timeout=2)
        require(result == 0, f"{name} view-only run exited with {result}: {lines}")
        require("STOPPED" in lines, f"{name}: view-only run did not stop cleanly")
        verify_listener_released(name, port)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def verify_control(name: str, executable: Path, quick: bool) -> None:
    port = free_port()
    process, reader, lines = start_example(executable, port, quick, remote_input=True, seconds=7)

    try:
        with tempfile.TemporaryDirectory(prefix=f"hyremote-{name}-control-") as temp_dir:
            first = Path(temp_dir) / "first.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(first))
                width, height = verify_rendered_image(first)
                send_acceptance_input(client, width, height)
                time.sleep(0.35)

            second = Path(temp_dir) / "second.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(second))
                verify_rendered_image(second)

        result = process.wait(timeout=12)
        reader.join(timeout=2)
        require(result == 0, f"{name} control run exited with {result}: {lines}")

        pointer_lines = [line for line in lines if line.startswith("APP_POINTER")]
        require(len(pointer_lines) >= 3,
                f"{name}: left/middle/right button delivery incomplete: {pointer_lines}")
        require(any(line.startswith("APP_WHEEL") for line in lines),
                f"{name}: remote wheel did not reach the Qt application")
        require(any(line.startswith("APP_KEY") for line in lines),
                f"{name}: remote keyboard did not reach the Qt application")
        require(any(line.startswith("APP_KEY") and "shift=1" in line for line in lines),
                f"{name}: Shift modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_KEY") and "ctrl=1" in line for line in lines),
                f"{name}: Ctrl modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_KEY") and "alt=1" in line for line in lines),
                f"{name}: Alt modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_TEXT") for line in lines),
                f"{name}: remote text commit did not reach the Qt application")
        require("STOPPED" in lines, f"{name}: public RemoteAccess did not stop cleanly")
        verify_listener_released(name, port)

        print(
            f"PASS: {name} view-only isolation + public facade -> viewer -> "
            "Qt buttons/wheel/modifiers/text -> reconnect -> stop"
        )
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def verify_example(name: str, executable: Path, quick: bool) -> None:
    require(executable.exists(), f"{name} executable not found: {executable}")
    verify_view_only(name, executable, quick)
    verify_control(name, executable, quick)


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
