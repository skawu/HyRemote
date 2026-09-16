#!/usr/bin/env python3
"""Standard-viewer product-fit check for examples/qml-basic."""

from __future__ import annotations

import argparse
import os
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


def wait_until(process: subprocess.Popen[str], lines: list[str], predicate, message: str,
               timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            raise RuntimeError(f"{message}; process exited early: {lines}")
        time.sleep(0.05)
    raise RuntimeError(f"{message}: {lines}")


def line_count(lines: list[str], needle: str) -> int:
    return sum(1 for line in lines if needle in line)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qml", required=True, type=Path)
    args = parser.parse_args()

    port = free_port()
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["QT_QUICK_BACKEND"] = "software"

    # Start with the product-safe view-only default, then have the real QML surface execute the
    # documented stop -> configure(remoteInputEnabled=true) -> start lifecycle in the same process.
    process = subprocess.Popen(
        [
            str(args.qml.resolve()),
            "--port", str(port),
            "--policy-transition-ms", "4500",
            "--test-seconds", "14",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=env,
    )
    lines: list[str] = []

    def reader() -> None:
        assert process.stdout is not None
        for raw in process.stdout:
            lines.append(raw.rstrip("\r\n"))

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    try:
        wait_until(process, lines, lambda: line_count(lines, f"READY {port}") >= 1,
                   "qml-basic did not reach initial view-only Running state", timeout=8)
        wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 0") >= 1,
                   "qml-basic did not expose initial zero-client state")

        with tempfile.TemporaryDirectory(prefix="hyremote-qml-") as temp_dir:
            view_only_image = Path(temp_dir) / "qml-view-only.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(view_only_image))
                wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 1") >= 1,
                           "QML diagnostic did not mirror view-only viewer connection")

                image = Image.open(view_only_image).convert("RGB")
                require(image.width >= 200 and image.height >= 120,
                        f"unexpected qml framebuffer {image.size}")
                require(any(low != high for low, high in ImageStat.Stat(image).extrema),
                        "qml framebuffer is visually uniform")

                # Input is deliberately attempted while the QML wrapper is still view-only. The
                # transport may receive these protocol messages, but no APP_* event may reach QML.
                client.mouseMove(max(1, int(image.width * 0.25)), max(1, int(image.height * 0.36)))
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.3)
                require(not any(line.startswith("qml: APP_") or " APP_" in line for line in lines),
                        f"view-only QML path delivered remote input: {lines}")

            wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 0") >= 2,
                       "QML diagnostic did not mirror view-only viewer disconnect")

            # The same application process must stop only the HyRemote runtime, change input policy,
            # restart it, and expose a second Running state. No QML engine/application restart is
            # involved, so this is direct evidence for the declarative lifecycle rather than an
            # inference from the C++ facade tests.
            wait_until(process, lines, lambda: line_count(lines, "POLICY_STOPPED") >= 1,
                       "QML wrapper did not stop for policy transition", timeout=7)
            wait_until(process, lines, lambda: line_count(lines, "POLICY_RESTART_REQUESTED") >= 1,
                       "QML wrapper did not request restart after policy change")
            wait_until(process, lines, lambda: line_count(lines, f"READY {port}") >= 2,
                       "QML wrapper did not return to Running after policy transition")

            control_image = Path(temp_dir) / "qml-control.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(control_image))
                wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 1") >= 2,
                           "QML diagnostic did not mirror control viewer connection")
                require(Image.open(control_image).size == image.size,
                        "QML framebuffer geometry changed unexpectedly across runtime restart")

                client.mouseMove(max(1, int(image.width * 0.25)), max(1, int(image.height * 0.36)))
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")

                wait_until(process, lines, lambda: any("APP_POINTER" in line for line in lines),
                           "pointer did not reach QML after control restart")
                wait_until(process, lines, lambda: any("APP_KEY" in line for line in lines),
                           "key did not reach QML after control restart")
                wait_until(process, lines, lambda: any("APP_TEXT" in line and "a" in line for line in lines),
                           "text commit did not reach QML after control restart")

            wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 0") >= 3,
                       "QML diagnostic did not mirror control viewer disconnect")

            # Reconnect once more in the same control-mode runtime to preserve the original E3
            # reconnect evidence after the policy lifecycle was strengthened.
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(Path(temp_dir) / "qml-control-reconnect.png"))
                wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 1") >= 3,
                           "QML diagnostic did not mirror control viewer reconnect")

            wait_until(process, lines, lambda: line_count(lines, "CLIENT_COUNT 0") >= 4,
                       "QML diagnostic did not mirror final viewer disconnect")

        result = process.wait(timeout=18)
        thread.join(timeout=2)
        require(result == 0, f"qml-basic exited with {result}: {lines}")
        require(line_count(lines, f"READY {port}") >= 2,
                f"QML runtime did not demonstrate stop/configure/start lifecycle: {lines}")
        require(line_count(lines, "CLIENT_COUNT 1") >= 3,
                f"connected-client diagnostic missed view-only/control/reconnect: {lines}")
        require(line_count(lines, "CLIENT_COUNT 0") >= 4,
                f"connected-client diagnostic missed disconnect lifecycle: {lines}")
        require(any("STOPPED" in line for line in lines), f"declarative final stop not observed: {lines}")

        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0,
                    "listener still accepts after declarative final stop")
        finally:
            check.close()

        print(
            "PASS: qml-basic view-only isolation -> QML stop/configure/start -> "
            "control input + client-count reconnect -> final stop"
        )
        return 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
