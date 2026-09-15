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

    process = subprocess.Popen(
        [str(args.qml.resolve()), "--port", str(port), "--remote-input", "--test-seconds", "10"],
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
        wait_until(
            process,
            lines,
            lambda: any(f"READY {port}" in line for line in lines),
            "qml-basic did not reach Running",
            timeout=8,
        )
        wait_until(
            process,
            lines,
            lambda: line_count(lines, "CLIENT_COUNT 0") >= 1,
            "qml-basic did not expose initial zero-client state",
        )

        with tempfile.TemporaryDirectory(prefix="hyremote-qml-") as temp_dir:
            image_path = Path(temp_dir) / "qml.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(image_path))
                wait_until(
                    process,
                    lines,
                    lambda: line_count(lines, "CLIENT_COUNT 1") >= 1,
                    "QML diagnostic did not mirror first viewer connection",
                )

                image = Image.open(image_path).convert("RGB")
                require(image.width >= 200 and image.height >= 120,
                        f"unexpected qml framebuffer {image.size}")
                require(any(low != high for low, high in ImageStat.Stat(image).extrema),
                        "qml framebuffer is visually uniform")

                client.mouseMove(max(1, int(image.width * 0.25)), max(1, int(image.height * 0.36)))
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.3)

            # The QML facade polls the same RemoteAccess diagnostic. Require the disconnect to be
            # visible before reconnecting so a 1 -> 0 -> 1 lifecycle cannot be coalesced away.
            wait_until(
                process,
                lines,
                lambda: line_count(lines, "CLIENT_COUNT 0") >= 2,
                "QML diagnostic did not mirror first viewer disconnect",
            )

            # A second connection proves viewer reconnect without recreating the QML application.
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
                client.captureScreen(str(Path(temp_dir) / "qml-reconnect.png"))
                wait_until(
                    process,
                    lines,
                    lambda: line_count(lines, "CLIENT_COUNT 1") >= 2,
                    "QML diagnostic did not mirror viewer reconnect",
                )

            wait_until(
                process,
                lines,
                lambda: line_count(lines, "CLIENT_COUNT 0") >= 3,
                "QML diagnostic did not mirror second viewer disconnect",
            )

        result = process.wait(timeout=14)
        thread.join(timeout=2)
        require(result == 0, f"qml-basic exited with {result}: {lines}")
        require(any("APP_POINTER" in line for line in lines), f"pointer did not reach QML: {lines}")
        require(any("APP_KEY" in line for line in lines), f"key did not reach QML: {lines}")
        require(any("APP_TEXT" in line and "a" in line for line in lines),
                f"text commit did not reach QML TextField: {lines}")
        require(line_count(lines, "CLIENT_COUNT 1") >= 2,
                f"connected-client diagnostic missed connect/reconnect: {lines}")
        require(line_count(lines, "CLIENT_COUNT 0") >= 3,
                f"connected-client diagnostic missed disconnect lifecycle: {lines}")
        require(any("STOPPED" in line for line in lines), f"declarative stop not observed: {lines}")

        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0,
                    "listener still accepts after declarative stop")
        finally:
            check.close()

        print(
            "PASS: qml-basic import HyRemote -> viewer -> client-count 0/1/reconnect -> "
            "pointer/key/text -> stop"
        )
        return 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
