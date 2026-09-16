#!/usr/bin/env python3
"""Installed-SDK Transparent QPA deployment product-fit."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import time
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def wait_for_rfb(port: int, process: subprocess.Popen[str]) -> bytes:
    deadline = time.monotonic() + 8.0
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"consumer exited before RFB listener became ready: {process.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5) as sock:
                sock.settimeout(1.0)
                return sock.recv(12)
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"deployed QPA listener did not become ready: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--port", type=int, default=5992)
    args = parser.parse_args()

    app = args.app.resolve()
    require(app.exists(), f"deployed consumer not found: {app}")

    env = os.environ.copy()
    # Product-fit must prove the deployed tree is self-contained with respect to HyRemote/Qt SDK
    # lookup. Plugin/runtime overrides are removed before the application process is created.
    for variable in (
        "QT_PLUGIN_PATH",
        "QT_QPA_PLATFORM_PLUGIN_PATH",
        "QT_QPA_PLATFORM",
        "LD_LIBRARY_PATH",
    ):
        env.pop(variable, None)

    if os.name == "nt":
        # Do not let an aqt/Visual Studio build-step PATH accidentally supply Qt or HyRemote DLLs.
        # Windows still searches the executable directory and normal system directories.
        system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
        env["PATH"] = os.pathsep.join(
            [str(app.parent), str(system_root / "System32"), str(system_root)]
        )

    process = subprocess.Popen(
        [
            str(app),
            "--test-seconds", "6",
            "-platform", f"hyremote:hyremote-port={args.port}",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    )

    try:
        first_banner = wait_for_rfb(args.port, process)
        require(first_banner.startswith(b"RFB 003.008"), f"unexpected first RFB banner: {first_banner!r}")

        second_banner = wait_for_rfb(args.port, process)
        require(second_banner.startswith(b"RFB 003.008"), f"unexpected reconnect banner: {second_banner!r}")

        result = process.wait(timeout=10)
        output = process.stdout.read() if process.stdout is not None else ""
        require(result == 0, f"deployed QPA consumer exited with {result}: {output}")
        print(
            "PASS: installed Qt-only consumer -> deployed qhyremote + shared RemoteAccess -> "
            "RFB reconnect without SDK/plugin/runtime-path overrides"
        )
        return 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
