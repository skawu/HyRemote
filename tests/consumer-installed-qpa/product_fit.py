#!/usr/bin/env python3
"""Installed-SDK QPA deployment product-fit."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
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
            output = process.stdout.read() if process.stdout is not None else ""
            raise RuntimeError(
                f"consumer exited before RFB listener became ready: {process.returncode}\n{output}"
            )
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5) as sock:
                sock.settimeout(1.0)
                return sock.recv(12)
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"deployed QPA listener did not become ready: {last_error}")


def require_native_platform_plugin(app: Path) -> Path:
    prefix = app.parent.parent
    plugin_name = "qwindows.dll" if os.name == "nt" else "libqxcb.so"
    plugin = prefix / "plugins" / "platforms" / plugin_name
    require(plugin.is_file(), f"deployed native QPA delegate is missing: {plugin}")
    return plugin


def verify_linux_dependency_origins(app: Path, native_platform_plugin: Path) -> None:
    if os.name == "nt":
        return

    prefix = app.parent.parent
    qpa_plugins = sorted((prefix / "plugins" / "platforms").glob("*qhyremote*.so*"))
    require(len(qpa_plugins) == 1, f"expected one deployed qhyremote ELF under {prefix}, got {qpa_plugins}")

    artifacts = [app, qpa_plugins[0], native_platform_plugin]
    qml_dir = prefix / "qml" / "HyRemote"
    if qml_dir.is_dir():
        artifacts.extend(sorted(path for path in qml_dir.glob("*.so*") if path.is_file()))

    verifier = Path(__file__).resolve().parents[1] / "release-readiness" / "verify_linux_dependency_origin.py"
    require(verifier.is_file(), f"Linux dependency-origin verifier is missing: {verifier}")

    command = [sys.executable, str(verifier), "--prefix", str(prefix)]
    for artifact in artifacts:
        command.extend(["--artifact", str(artifact)])

    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)
    result = subprocess.run(command, check=False, text=True, capture_output=True, env=env)
    if result.returncode != 0:
        raise RuntimeError(
            "deployed Linux dependency-origin verification failed:\n"
            + result.stdout
            + result.stderr
        )
    print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--port", type=int, default=5992)
    args = parser.parse_args()

    app = args.app.resolve()
    require(app.exists(), f"deployed consumer not found: {app}")
    native_platform_plugin = require_native_platform_plugin(app)
    verify_linux_dependency_origins(app, native_platform_plugin)

    env = os.environ.copy()
    # Product-fit must prove the deployed tree is self-contained with respect to HyRemote/Qt SDK
    # lookup. Plugin/runtime/QML overrides are removed before the application process is created so
    # this same harness can validate both the ordinary E4 executable and the combined QML+QPA
    # deployment without relying on caller-shell cleanup.
    for variable in (
        "QT_PLUGIN_PATH",
        "QT_QPA_PLATFORM_PLUGIN_PATH",
        "QT_QPA_PLATFORM",
        "QML2_IMPORT_PATH",
        "QML_IMPORT_PATH",
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
        # Keep the process working directory inside the deployment tree as well. On Windows this
        # closes the current-directory DLL search path; on Linux it prevents relative application
        # behavior from accidentally reaching the source/build workspace.
        cwd=str(app.parent),
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
            "PASS: deployed consumer -> qhyremote + native QPA delegate + shared RemoteAccess -> "
            "RFB reconnect without SDK/plugin/QML/runtime-path overrides"
        )
        return 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
