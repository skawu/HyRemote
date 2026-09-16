#!/usr/bin/env python3
"""Dual-OS product-fit probe for HyRemote's production internal RFB transport."""

from __future__ import annotations

import argparse
import os
import queue
import socket
import struct
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


def recv_exact(sock: socket.socket, size: int) -> bytes:
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise RuntimeError(f"RFB peer closed while receiving {size} bytes")
        result.extend(chunk)
    return bytes(result)


def connect_raw_rfb(port: int) -> socket.socket:
    """Perform only the RFB 3.8 / SecurityType None handshake needed for precise input messages."""
    sock = socket.create_connection(("127.0.0.1", port), timeout=3)
    sock.settimeout(3)
    require(recv_exact(sock, 12) == b"RFB 003.008\n", "unexpected RFB server version")
    sock.sendall(b"RFB 003.008\n")

    count = recv_exact(sock, 1)[0]
    require(count >= 1, "RFB server offered no security type")
    security_types = recv_exact(sock, count)
    require(1 in security_types, f"SecurityType None was not offered: {security_types!r}")
    sock.sendall(b"\x01")
    require(struct.unpack(">I", recv_exact(sock, 4))[0] == 0, "RFB SecurityResult was not OK")

    sock.sendall(b"\x01")  # ClientInit shared=true
    server_init = recv_exact(sock, 24)
    name_length = struct.unpack(">I", server_init[20:24])[0]
    recv_exact(sock, name_length)
    return sock


def send_raw_key(sock: socket.socket, keysym: int, pressed: bool) -> None:
    sock.sendall(struct.pack(">BBHI", 4, 1 if pressed else 0, 0, keysym))


def send_raw_pointer(sock: socket.socket, mask: int, x: int, y: int) -> None:
    sock.sendall(struct.pack(">BBHH", 5, mask, x, y))


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


def start_harness(executable: Path, port: int, duration_seconds: int = 8):
    env = os.environ.copy()
    env["HYREMOTE_RFB_TEST_SECONDS"] = str(duration_seconds)
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

    deadline = time.monotonic() + min(8, duration_seconds)
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


def verify_handshake_slots_expire(executable: Path) -> None:
    """Eight silent sockets must not own every bounded viewer slot forever."""
    port = free_port()
    process, reader, lines = start_harness(executable, port, duration_seconds=7)
    stalled: list[socket.socket] = []
    try:
        for index in range(8):
            sock = socket.create_connection(("127.0.0.1", port), timeout=2)
            sock.settimeout(2)
            banner = sock.recv(12)
            require(banner == b"RFB 003.008\n", f"stalled client {index} was not accepted: {banner!r}")
            # Intentionally never send our protocol version. This pins the client in AwaitVersion
            # until the server-side handshake deadline expires.
            stalled.append(sock)

        # Keep the sockets open beyond the 3 s transport deadline. The worker must abort them and
        # erase their ClientState entries, otherwise a subsequent legitimate viewer is rejected by
        # the max-eight-client guard forever.
        time.sleep(3.6)

        expired = 0
        for sock in stalled:
            try:
                data = sock.recv(1)
                if data == b"":
                    expired += 1
            except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, OSError):
                expired += 1
        require(expired == len(stalled), f"only {expired}/{len(stalled)} incomplete handshakes expired")

        with tempfile.TemporaryDirectory(prefix="hyremote-rfb-handshake-") as temp_dir:
            capture = Path(temp_dir) / "after-timeout.png"
            with api.connect(f"127.0.0.1::{port}", password=None, timeout=3) as client:
                client.captureScreen(str(capture))
            require(Image.open(capture).size == (64, 48), "legitimate viewer could not connect after slot expiry")

        result = process.wait(timeout=10)
        reader.join(timeout=2)
        require(result == 0, f"handshake-timeout harness failed: {lines}")
        require(
            sum(1 for line in lines if "RFB client handshake timed out" in line) >= 8,
            "server did not report every incomplete-handshake expiry",
        )
        require(any("RFB client connected" in line for line in lines), "post-timeout legitimate viewer was not accepted")
        require("STOPPED" in lines, "handshake-timeout transport did not stop cleanly")
    finally:
        for sock in stalled:
            try:
                sock.close()
            except OSError:
                pass
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def verify_abrupt_disconnect_releases_input(executable: Path) -> None:
    """A client disappearing with held state must leave the next viewer/Qt target input clean."""
    port = free_port()
    process, reader, lines = start_harness(executable, port, duration_seconds=7)
    raw: socket.socket | None = None
    try:
        raw = connect_raw_rfb(port)

        # Hold Shift+A. The A release must be synthesized before Shift release so its release still
        # carries modifiers=1, matching a normal key-up sequence.
        send_raw_key(raw, 0xFFE1, True)  # Shift_L
        send_raw_key(raw, ord("a"), True)

        # Explicitly press/release middle once, then leave left+right held. Disconnect cleanup must
        # release only the two still-held buttons; middle must not be released a second time.
        send_raw_pointer(raw, 0x02, 19, 11)
        send_raw_pointer(raw, 0x00, 19, 11)
        send_raw_pointer(raw, 0x05, 23, 17)
        time.sleep(0.15)

        # Abruptly close the socket without any key/button-up messages.
        raw.close()
        raw = None
        time.sleep(0.35)

        # A new normal viewer must start from clean modifier/button state. Its A-down is the final
        # A-down in the log and must carry modifiers=0.
        with api.connect(f"127.0.0.1::{port}", password=None, timeout=3) as client:
            client.keyDown("a")
            client.keyUp("a")
            time.sleep(0.15)

        result = process.wait(timeout=10)
        reader.join(timeout=2)
        require(result == 0, f"disconnect-reset harness failed: {lines}")

        shift_up = [line for line in lines if "INPUT key" in line and "key=16" in line and "pressed=0" in line]
        require(len(shift_up) == 1, f"expected exactly one synthesized Shift release: {shift_up}")
        require("modifiers=0" in shift_up[0], f"Shift release did not clear modifier state: {shift_up[0]}")

        a_down = [line for line in lines if "INPUT key" in line and "key=32" in line and "pressed=1" in line]
        a_up = [line for line in lines if "INPUT key" in line and "key=32" in line and "pressed=0" in line]
        require(len(a_down) >= 2, f"expected held and subsequent A presses: {a_down}")
        require(len(a_up) >= 2, f"expected synthesized and normal A releases: {a_up}")
        require("modifiers=1" in a_down[0], f"held A did not observe Shift: {a_down[0]}")
        require(any("modifiers=1" in line for line in a_up), f"A release was not synthesized before Shift release: {a_up}")
        require("modifiers=0" in a_down[-1], f"next viewer inherited stale modifier state: {a_down[-1]}")

        middle_up = [line for line in lines if "INPUT pointer-button" in line and "button=2" in line and "pressed=0" in line]
        left_up = [line for line in lines if "INPUT pointer-button" in line and "button=1" in line and "pressed=0" in line]
        right_up = [line for line in lines if "INPUT pointer-button" in line and "button=3" in line and "pressed=0" in line]
        require(len(middle_up) == 1, f"already-released middle button was released twice: {middle_up}")
        require(len(left_up) == 1, f"held left button was not released exactly once: {left_up}")
        require(len(right_up) == 1, f"held right button was not released exactly once: {right_up}")
        require("x=23" in left_up[0] and "y=17" in left_up[0], f"left release lost last pointer position: {left_up[0]}")
        require("x=23" in right_up[0] and "y=17" in right_up[0], f"right release lost last pointer position: {right_up[0]}")

        require(sum(1 for line in lines if "RFB client connected" in line) >= 2, "second viewer did not connect")
        require("STOPPED" in lines, "disconnect-reset transport did not stop cleanly")
    finally:
        if raw is not None:
            try:
                raw.close()
            except OSError:
                pass
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


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
        verify_handshake_slots_expire(executable)
        verify_abrupt_disconnect_releases_input(executable)
        verify_standard_client(executable)
        print(
            "PASS: bounded RFB bind + handshake expiry + disconnect input reset + "
            "vncdotool framebuffer/input/reconnect + quiescent stop"
        )
        return 0
    finally:
        api.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
