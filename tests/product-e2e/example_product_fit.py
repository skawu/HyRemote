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

    # Product-fit starts from the safe view-only default. The acceptance-only watchdog is not the
    # normal transition trigger: after the first viewer disconnect the example uses only the public
    # RemoteAccess stop -> configure -> start contract in the same application process.
    command = [
        str(executable),
        "--port", str(port),
        "--policy-transition-ms", "15000",
        "--test-seconds", "24",
    ]

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


def line_count(lines: list[str], needle: str) -> int:
    return sum(1 for line in lines if needle in line)


def wait_for_line_count(process: subprocess.Popen[str], lines: list[str], needle: str,
                        expected: int, context: str, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if line_count(lines, needle) >= expected:
            return
        if process.poll() is not None:
            raise RuntimeError(f"{context}; process exited early: {lines}")
        time.sleep(0.05)
    raise RuntimeError(f"{context}: expected {expected} occurrence(s) of {needle!r}: {lines}")


def wait_for_predicate(process: subprocess.Popen[str], lines: list[str], predicate,
                       context: str, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            raise RuntimeError(f"{context}; process exited early: {lines}")
        time.sleep(0.05)
    raise RuntimeError(f"{context}: {lines}")


def verify_rendered_image(path: Path) -> tuple[int, int]:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    require(width >= 200 and height >= 120, f"unexpected framebuffer size: {image.size}")
    extrema = ImageStat.Stat(image).extrema
    require(any(low != high for low, high in extrema), "captured framebuffer is visually uniform")
    return width, height


def send_acceptance_input(client, width: int, height: int) -> None:
    x = max(1, int(width * 0.25))
    y = max(1, int(height * 0.41))
    client.mouseMove(x, y)

    for button in (1, 2, 3):
        client.mouseDown(button)
        client.mouseUp(button)
    client.mouseDown(4)
    client.mouseUp(4)

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


def verify_example(name: str, executable: Path, quick: bool) -> None:
    require(executable.exists(), f"{name} executable not found: {executable}")
    port = free_port()
    process, reader, lines = start_example(executable, port, quick)

    try:
        require(line_count(lines, "CLIENT_COUNT 0") >= 1,
                f"{name}: initial zero-client status missing: {lines}")

        with tempfile.TemporaryDirectory(prefix=f"hyremote-{name}-lifecycle-") as temp_dir:
            view_only_image = Path(temp_dir) / "view-only.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(view_only_image))
                wait_for_line_count(process, lines, "CLIENT_COUNT 1", 1,
                                    f"{name}: view-only viewer connection not reflected locally")
                width, height = verify_rendered_image(view_only_image)
                send_acceptance_input(client, width, height)
                time.sleep(0.35)

                require(not any(line.startswith("APP_POINTER") for line in lines),
                        f"{name}: pointer reached app in view-only mode: {lines}")
                require(not any(line.startswith("APP_WHEEL") for line in lines),
                        f"{name}: wheel reached app in view-only mode: {lines}")
                require(not any(line.startswith("APP_KEY") for line in lines),
                        f"{name}: key reached app in view-only mode: {lines}")
                require(not any(line.startswith("APP_TEXT") for line in lines),
                        f"{name}: text reached app in view-only mode: {lines}")

            wait_for_line_count(process, lines, "CLIENT_COUNT 0", 2,
                                f"{name}: view-only viewer disconnect not reflected locally")

            # The first disconnect is the deterministic trigger. The 15s timer is only a watchdog.
            wait_for_line_count(process, lines, "POLICY_STOPPED", 1,
                                f"{name}: public RemoteAccess did not stop for policy transition")
            wait_for_line_count(process, lines, "POLICY_INPUT true", 1,
                                f"{name}: remote input was not configured while stopped")
            wait_for_line_count(process, lines, "POLICY_RESTART_REQUESTED", 1,
                                f"{name}: public RemoteAccess restart was not requested")
            wait_for_line_count(process, lines, f"READY {port}", 2,
                                f"{name}: public RemoteAccess did not restart in the same process")

            control_image = Path(temp_dir) / "control.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(control_image))
                wait_for_line_count(process, lines, "CLIENT_COUNT 1", 2,
                                    f"{name}: control viewer connection not reflected locally")
                require(verify_rendered_image(control_image) == (width, height),
                        f"{name}: framebuffer geometry changed across RemoteAccess restart")
                send_acceptance_input(client, width, height)

                wait_for_predicate(
                    process,
                    lines,
                    lambda: len([line for line in lines if line.startswith("APP_POINTER")]) >= 3,
                    f"{name}: left/middle/right remote buttons did not reach Qt after restart",
                )
                wait_for_predicate(process, lines,
                                   lambda: any(line.startswith("APP_WHEEL") for line in lines),
                                   f"{name}: remote wheel did not reach Qt after restart")
                wait_for_predicate(process, lines,
                                   lambda: any(line.startswith("APP_KEY") for line in lines),
                                   f"{name}: remote key did not reach Qt after restart")
                wait_for_predicate(process, lines,
                                   lambda: any(line.startswith("APP_TEXT") for line in lines),
                                   f"{name}: remote text did not reach Qt after restart")

            zero_before_reconnect = line_count(lines, "CLIENT_COUNT 0")
            wait_for_line_count(process, lines, "CLIENT_COUNT 0", zero_before_reconnect + 1,
                                f"{name}: control viewer disconnect not reflected locally")

            reconnect_image = Path(temp_dir) / "control-reconnect.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(reconnect_image))
                wait_for_line_count(process, lines, "CLIENT_COUNT 1", 3,
                                    f"{name}: control viewer reconnect not reflected locally")
                require(verify_rendered_image(reconnect_image) == (width, height),
                        f"{name}: framebuffer geometry changed on reconnect")

            zero_before_final_disconnect = line_count(lines, "CLIENT_COUNT 0")
            wait_for_line_count(process, lines, "CLIENT_COUNT 0", zero_before_final_disconnect + 1,
                                f"{name}: final viewer disconnect not reflected locally")

        result = process.wait(timeout=28)
        reader.join(timeout=2)
        require(result == 0, f"{name} lifecycle run exited with {result}: {lines}")
        require(line_count(lines, f"READY {port}") >= 2,
                f"{name}: same-process stop/configure/start evidence incomplete: {lines}")
        require(line_count(lines, "CLIENT_COUNT 1") >= 3,
                f"{name}: view-only/control/reconnect client diagnostics incomplete: {lines}")
        require("POLICY_STOPPED" in lines and "POLICY_INPUT true" in lines
                and "POLICY_RESTART_REQUESTED" in lines,
                f"{name}: stopped-runtime policy-transition evidence incomplete: {lines}")

        pointer_lines = [line for line in lines if line.startswith("APP_POINTER")]
        require(len(pointer_lines) >= 3,
                f"{name}: left/middle/right button delivery incomplete after restart: {pointer_lines}")
        require(any(line.startswith("APP_WHEEL") for line in lines),
                f"{name}: remote wheel did not reach the Qt application")
        require(any(line.startswith("APP_KEY") and "shift=1" in line for line in lines),
                f"{name}: Shift modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_KEY") and "ctrl=1" in line for line in lines),
                f"{name}: Ctrl modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_KEY") and "alt=1" in line for line in lines),
                f"{name}: Alt modifier did not reach Qt key delivery: {lines}")
        require(any(line.startswith("APP_TEXT") for line in lines),
                f"{name}: remote text commit did not reach the Qt application")
        require("STOPPED" in lines, f"{name}: final public RemoteAccess stop not observed")
        verify_listener_released(name, port)

        print(
            f"PASS: {name} view-only isolation -> public stop/configure/start -> "
            "control buttons/wheel/modifiers/text -> reconnect -> final stop"
        )
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
