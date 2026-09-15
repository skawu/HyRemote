#!/usr/bin/env python3
"""Cross-platform product-fit probe for HyRemote's rustvncserver candidate.

This intentionally exercises the backend only through the HyRemote-owned opaque C ABI and a
standard RFB client implementation (vncdotool). It is spike evidence, not the production API.
"""

from __future__ import annotations

import argparse
import ctypes
import socket
import tempfile
import time
from pathlib import Path

from PIL import Image
from vncdotool import api


EVENT_CLIENT_CONNECTED = 1
EVENT_CLIENT_DISCONNECTED = 2
EVENT_KEY = 3
EVENT_POINTER = 4


class ProbeEvent(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_uint32),
        ("client_id", ctypes.c_uint64),
        ("x", ctypes.c_uint32),
        ("y", ctypes.c_uint32),
        ("button_mask", ctypes.c_uint32),
        ("key", ctypes.c_uint32),
        ("pressed", ctypes.c_uint32),
    ]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def reserve_free_port() -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])
    finally:
        sock.close()


def load_probe(path: Path):
    lib = ctypes.CDLL(str(path.resolve()))
    lib.hyremote_vnc_probe_abi_version.restype = ctypes.c_uint32

    lib.hyremote_vnc_probe_create.argtypes = [ctypes.c_uint16, ctypes.c_uint16]
    lib.hyremote_vnc_probe_create.restype = ctypes.c_void_p

    lib.hyremote_vnc_probe_update_rgba.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.hyremote_vnc_probe_update_rgba.restype = ctypes.c_int

    lib.hyremote_vnc_probe_start_ipv4.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint16,
    ]
    lib.hyremote_vnc_probe_start_ipv4.restype = ctypes.c_int

    lib.hyremote_vnc_probe_running.argtypes = [ctypes.c_void_p]
    lib.hyremote_vnc_probe_running.restype = ctypes.c_int

    lib.hyremote_vnc_probe_poll_event.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProbeEvent)]
    lib.hyremote_vnc_probe_poll_event.restype = ctypes.c_int

    lib.hyremote_vnc_probe_dropped_events.argtypes = [ctypes.c_void_p]
    lib.hyremote_vnc_probe_dropped_events.restype = ctypes.c_uint64

    lib.hyremote_vnc_probe_stop.argtypes = [ctypes.c_void_p]
    lib.hyremote_vnc_probe_stop.restype = ctypes.c_int

    lib.hyremote_vnc_probe_destroy.argtypes = [ctypes.c_void_p]
    lib.hyremote_vnc_probe_destroy.restype = None
    return lib


def poll_events(lib, handle, timeout: float = 2.0) -> list[ProbeEvent]:
    events: list[ProbeEvent] = []
    deadline = time.monotonic() + timeout
    quiet_since = None
    while time.monotonic() < deadline:
        event = ProbeEvent()
        result = lib.hyremote_vnc_probe_poll_event(handle, ctypes.byref(event))
        require(result >= 0, f"poll_event failed with {result}")
        if result == 1:
            copy = ProbeEvent()
            ctypes.memmove(ctypes.byref(copy), ctypes.byref(event), ctypes.sizeof(event))
            events.append(copy)
            quiet_since = None
            continue
        if events:
            if quiet_since is None:
                quiet_since = time.monotonic()
            elif time.monotonic() - quiet_since >= 0.15:
                break
        time.sleep(0.01)
    return events


def create_filled_probe(lib, width: int, height: int):
    handle = lib.hyremote_vnc_probe_create(width, height)
    require(bool(handle), "probe create failed")
    rgba = bytes((0x33, 0x66, 0x99, 0xFF)) * (width * height)
    array_type = ctypes.c_uint8 * len(rgba)
    array = array_type.from_buffer_copy(rgba)
    result = lib.hyremote_vnc_probe_update_rgba(handle, array, len(rgba))
    require(result == 0, f"framebuffer update failed with {result}")
    return handle


def verify_occupied_port_failure(lib) -> None:
    occupied = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    occupied.bind(("127.0.0.1", 0))
    occupied.listen(1)
    port = int(occupied.getsockname()[1])
    handle = create_filled_probe(lib, 8, 8)
    try:
        started = time.monotonic()
        result = lib.hyremote_vnc_probe_start_ipv4(handle, 127, 0, 0, 1, port)
        elapsed = time.monotonic() - started
        require(result != 0, "occupied port was incorrectly reported as a successful start")
        require(elapsed < 2.5, f"occupied-port failure was not bounded ({elapsed:.3f}s)")
        require(lib.hyremote_vnc_probe_running(handle) == 0, "failed listener still reports running")
    finally:
        lib.hyremote_vnc_probe_destroy(handle)
        occupied.close()


def verify_standard_client_round_trip(lib) -> None:
    width, height = 64, 48
    handle = create_filled_probe(lib, width, height)
    port = reserve_free_port()
    try:
        result = lib.hyremote_vnc_probe_start_ipv4(handle, 127, 0, 0, 1, port)
        require(result == 0, f"explicit loopback start failed with {result}")
        require(lib.hyremote_vnc_probe_running(handle) == 1, "listener did not remain running")

        with tempfile.TemporaryDirectory(prefix="hyremote-vnc-fit-") as temp_dir:
            first_capture = Path(temp_dir) / "first.png"
            with api.connect(f"127.0.0.1::{port}", password=None) as client:
                client.timeout = 5
                client.captureScreen(str(first_capture))
                image = Image.open(first_capture).convert("RGB")
                require(image.size == (width, height), f"unexpected framebuffer size {image.size}")
                pixel = image.getpixel((width // 2, height // 2))
                require(
                    all(abs(actual - expected) <= 1 for actual, expected in zip(pixel, (0x33, 0x66, 0x99))),
                    f"known framebuffer pixel mismatch: {pixel}",
                )

                client.mouseMove(17, 13)
                client.mouseDown(1)
                client.mouseUp(1)
                client.keyDown("a")
                client.keyUp("a")
                time.sleep(0.1)

            first_events = poll_events(lib, handle)
            require(any(e.kind == EVENT_CLIENT_CONNECTED for e in first_events), "no client-connected event")
            require(
                any(e.kind == EVENT_POINTER and e.x == 17 and e.y == 13 for e in first_events),
                "pointer input did not cross the backend boundary",
            )
            require(
                any(e.kind == EVENT_KEY and e.key == ord("a") and e.pressed == 1 for e in first_events),
                "key-down input did not cross the backend boundary",
            )
            require(
                any(e.kind == EVENT_KEY and e.key == ord("a") and e.pressed == 0 for e in first_events),
                "key-up input did not cross the backend boundary",
            )

            # Reconnect to the same listener without recreating the server/target.
            second_capture = Path(temp_dir) / "second.png"
            with api.connect(f"127.0.0.1::{port}", password=None) as client:
                client.timeout = 5
                client.captureScreen(str(second_capture))
            require(Image.open(second_capture).size == (width, height), "reconnect capture failed")
            second_events = poll_events(lib, handle)
            require(
                any(e.kind == EVENT_CLIENT_CONNECTED for e in second_events),
                "reconnect did not produce a second client-connected event",
            )

        require(
            lib.hyremote_vnc_probe_dropped_events(handle) == 0,
            "normal interoperability path overflowed the HyRemote-owned bounded event queue",
        )

        require(lib.hyremote_vnc_probe_stop(handle) == 0, "stop failed")
        require(lib.hyremote_vnc_probe_running(handle) == 0, "listener still reports running after stop")

        # Listener ownership must actually be released, not just hidden in local state.
        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            check.settimeout(0.25)
            require(check.connect_ex(("127.0.0.1", port)) != 0, "listener still accepts after stop")
        finally:
            check.close()
    finally:
        lib.hyremote_vnc_probe_destroy(handle)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    args = parser.parse_args()

    lib = load_probe(args.library)
    require(lib.hyremote_vnc_probe_abi_version() == 2, "unexpected product-fit probe ABI version")
    verify_occupied_port_failure(lib)
    verify_standard_client_round_trip(lib)
    print(
        "PASS: loopback bind + occupied-port failure + vncdotool framebuffer/input/reconnect + deterministic stop"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
