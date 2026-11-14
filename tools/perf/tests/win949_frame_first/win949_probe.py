#!/usr/bin/env python3
"""#949 Windows local verification probe (NON-PRODUCT test instrument).

Drives the frame fixture (`app/frame_fixture.cpp`) through the *product* Generic
plugin, reads real RFB 3.8 pixels with a minimal Viewer client, and reports
source-change -> Viewer framebuffer delivery latency, pixel correctness, request
liveness, and host CPU/GPU cost.

Design notes (why this is evidence and not a guess):

* The client is an independent RFB 3.8 implementation (None authentication, Raw
  encoding). It polls a small rectangle with non-incremental updates so every
  request has exactly one reply and the observation latency is bounded by the
  loopback round trip, not by a screenshot interval.
* A change is timestamped by the fixture (`WIN949_MARK ... t=<epoch_ms>`) at the
  moment it asks Qt to repaint. Detection is timestamped when the pixel arrives.
  The difference is therefore a bounded interval, reported as
  ``latency_upper`` (change -> pixel received) and ``latency_lower``.
* This measures the *Viewer framebuffer*, i.e. what a real VNC client decodes.
  It does not measure the physical screen scan-out of a GUI viewer window, so
  ``viewer_visible_latency`` stays NOT_MEASURED unless a real viewer window is
  captured separately.

Outputs one JSON document per run plus the raw host log and the optional
HYREMOTE_CPU_PROBE_FILE JSONL.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

try:
    import psutil
except ImportError:  # pragma: no cover - psutil is required for host metrics
    psutil = None


# --------------------------------------------------------------------------- #
# Minimal RFB 3.8 client
# --------------------------------------------------------------------------- #

RFB_VERSION = b"RFB 003.008\n"


class RfbError(RuntimeError):
    pass


class RfbClient:
    """Exact RFB 3.8, None auth, Raw encoding, sub-rectangle polling."""

    def __init__(self, host: str, port: int, timeout: float = 10.0, shared: bool = True):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.width = 0
        self.height = 0
        self.name = ""
        self.pixels = {}
        self._handshake(shared)

    # -- transport ---------------------------------------------------------- #
    def _recv_exact(self, count: int) -> bytes:
        chunks = []
        remaining = count
        while remaining > 0:
            chunk = self.sock.recv(remaining)
            if not chunk:
                raise RfbError("connection closed while reading")
            chunks.append(chunk)
            remaining -= len(chunk)
        return b"".join(chunks)

    def _handshake(self, shared: bool) -> None:
        version = self._recv_exact(12)
        if not version.startswith(b"RFB "):
            raise RfbError(f"unexpected RFB greeting {version!r}")
        self.sock.sendall(RFB_VERSION)
        count = self._recv_exact(1)[0]
        if count == 0:
            reason_len = struct.unpack(">I", self._recv_exact(4))[0]
            reason = self._recv_exact(reason_len).decode("utf-8", "replace")
            raise RfbError(f"server refused connection: {reason}")
        types = list(self._recv_exact(count))
        if 1 not in types:
            raise RfbError(f"server does not offer None security (offered {types}); "
                           "this probe only drives the unauthenticated listener")
        self.sock.sendall(bytes([1]))
        result = struct.unpack(">I", self._recv_exact(4))[0]
        if result != 0:
            raise RfbError(f"security handshake failed: {result}")
        self.sock.sendall(bytes([1 if shared else 0]))
        init = self._recv_exact(24)
        self.width, self.height = struct.unpack(">HH", init[0:4])
        name_len = struct.unpack(">I", init[20:24])[0]
        self.name = self._recv_exact(name_len).decode("utf-8", "replace") if name_len else ""
        self._set_pixel_format()
        # Raw only (no lossy or cached path to reason about) plus DesktopSize so a
        # target resize is a normal notification instead of a forced reconnect.
        self._set_encodings([0, -223])

    def _set_pixel_format(self) -> None:
        # 32bpp, depth 24, little endian, true colour, r<<16 g<<8 b (16 bytes).
        body = struct.pack(">BBBBHHHBBBxxx", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0)
        self.sock.sendall(struct.pack(">Bxxx", 0) + body)

    def _set_encodings(self, encodings) -> None:
        payload = struct.pack(">BxH", 2, len(encodings))
        for encoding in encodings:
            payload += struct.pack(">i", encoding)
        self.sock.sendall(payload)

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    # -- protocol ----------------------------------------------------------- #
    def request_update(self, incremental: bool, x: int, y: int, w: int, h: int) -> None:
        self.sock.sendall(struct.pack(">BBHHHH", 3, 1 if incremental else 0, x, y, w, h))

    def send_pointer(self, x: int, y: int, mask: int = 0) -> None:
        self.sock.sendall(struct.pack(">BBHH", 5, mask, x, y))

    def _read_message(self):
        """Return ('update', rects) / ('other', None). rects are (x,y,w,h,bytes)."""
        kind = self._recv_exact(1)[0]
        if kind == 0:
            self._recv_exact(1)
            (count,) = struct.unpack(">H", self._recv_exact(2))
            rects = []
            for _ in range(count):
                x, y, w, h = struct.unpack(">HHHH", self._recv_exact(8))
                (encoding,) = struct.unpack(">i", self._recv_exact(4))
                if encoding == 0:
                    data = self._recv_exact(w * h * 4)
                    rects.append((x, y, w, h, data))
                elif encoding == -223:  # DesktopSize pseudo-encoding
                    self.width, self.height = w, h
                    rects.append((x, y, w, h, None))
                elif encoding == -239:  # Cursor pseudo-encoding
                    self._recv_exact(w * h * 4)
                    self._recv_exact((w + 7) // 8 * h)
                else:
                    raise RfbError(f"unsupported server encoding {encoding}")
            return "update", rects
        if kind == 1:
            self._recv_exact(1)
            (count,) = struct.unpack(">H", self._recv_exact(4)[2:4])
            self._recv_exact(count * 6)
            return "other", None
        if kind == 2:
            return "other", None
        if kind == 3:
            self._recv_exact(3)
            (length,) = struct.unpack(">I", self._recv_exact(4))
            self._recv_exact(length)
            return "other", None
        raise RfbError(f"unknown server message type {kind}")

    def _absorb(self, rects) -> None:
        for x, y, w, h, data in rects:
            if data is None:
                continue
            for row in range(h):
                base = row * w * 4
                for col in range(w):
                    offset = base + col * 4
                    blue = data[offset]
                    green = data[offset + 1]
                    red = data[offset + 2]
                    self.pixels[(x + col, y + row)] = (red, green, blue)

    def poll(self, x: int, y: int, w: int, h: int):
        """One non-incremental sub-rectangle update. Returns (t_send, t_recv, rects)."""
        t_send = time.time()
        self.request_update(False, x, y, w, h)
        while True:
            kind, rects = self._read_message()
            if kind == "update":
                self._absorb(rects)
                return t_send, time.time(), rects

    def full_frame(self):
        t_send, t_recv, rects = self.poll(0, 0, self.width, self.height)
        return t_send, t_recv, rects

    def pixel(self, x: int, y: int):
        return self.pixels.get((x, y))


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #

def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return ordered[index]


def summarize(values):
    if not values:
        return {"n": 0, "p50": None, "p95": None, "p99": None, "max": None, "mean": None}
    return {
        "n": len(values),
        "p50": round(percentile(values, 0.50), 3),
        "p95": round(percentile(values, 0.95), 3),
        "p99": round(percentile(values, 0.99), 3),
        "max": round(max(values), 3),
        "mean": round(sum(values) / len(values), 3),
    }


QUADS = [(0, 0, (215, 36, 57)), (1, 0, (37, 212, 66)),
         (0, 1, (42, 69, 225)), (1, 1, (240, 200, 20))]
# Must match kMarkerPalette in app/frame_fixture.cpp.
MARK_PALETTE = [(0, 0, 0), (255, 255, 255), (255, 0, 0), (0, 255, 0),
                (0, 0, 255), (255, 255, 0), (0, 255, 255), (255, 0, 255)]


def near(actual, expected, tolerance=6):
    if actual is None:
        return False
    return all(abs(a - b) <= tolerance for a, b in zip(actual, expected))


# --------------------------------------------------------------------------- #
# #949 focused retest helpers (Generic composite output is logical 1x, #806)
# --------------------------------------------------------------------------- #

DIAGNOSTIC_FIELDS = frozenset((
    "BUILD_IDENTITY", "QT_VERSION", "OS", "ARCH", "INTEGRATION_ROUTE", "UI_FAMILY",
    "STATE", "LISTENER_CONFIGURED", "LISTENER_EFFECTIVE", "SECURITY_PROFILE",
    "SECURITY_ENABLED", "REMOTE_INPUT", "CONNECTED_CLIENTS", "LAST_ERROR_CODE",
    "LAST_ERROR_MESSAGE", "LAST_ERROR_RECOVERABLE", "DEPLOYMENT_IDENTITY",
))
DIAGNOSTIC_LINE = re.compile(
    r"^(?:[A-Za-z]+:\s*)?(?:WIN949_API_REPORT\s+)?([A-Z_]+)=(.*)$")


def parse_diagnostics(text):
    """Split qInfo/qWarning diagnostic reports from a host log into snapshots.

    A new report starts when BUILD_IDENTITY/QT_VERSION appears after a previous
    report that already had a STATE. Lines that merely mention a field name or
    are comments are ignored. No product code is involved here: this only reads
    the host process log that the probe already captures (stderr folded in).
    """
    reports = []
    current = {}
    fault_lines = []
    for raw in text.splitlines():
        line = raw.strip()
        if any(token in line for token in ("FatalFailure", "Faulted", "Session fault", "Transport fault")):
            fault_lines.append(line)
        match = DIAGNOSTIC_LINE.match(line)
        if not match or match.group(1) not in DIAGNOSTIC_FIELDS:
            continue
        key, value = match.group(1), match.group(2)
        if key in ("BUILD_IDENTITY", "QT_VERSION") and "STATE" in current:
            reports.append(current)
            current = {}
        current[key] = value
    if current:
        reports.append(current)
    return {"reports": reports, "last": reports[-1] if reports else None,
            "fault_lines": fault_lines[:50], "fault_line_count": len(fault_lines)}


def framebuffer_size(width, height, dpr, mode):
    """#806: Generic/Composite emits 1 output pixel per logical canvas unit."""
    if mode == "logical":
        return width, height
    return int(round(width * dpr)), int(round(height * dpr))


def listener_reachable(port):
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=0.5):
            return True
    except OSError:
        return False


def activity_wake_status(host_text, enabled):
    """#952: fail closed unless the running process selected the requested arm."""
    expected = ("HYREMOTE_QUICK_ACTIVITY_WAKE=ON" if enabled
                else "HYREMOTE_QUICK_ACTIVITY_WAKE=OFF")
    opposite = ("HYREMOTE_QUICK_ACTIVITY_WAKE=OFF" if enabled
                else "HYREMOTE_QUICK_ACTIVITY_WAKE=ON")
    if expected in host_text and opposite not in host_text:
        return "SELECTED"
    return "BLOCKED_ROUTE_NOT_SELECTED"


def self_test():
    """Deterministic positive/negative checks for the new pure functions."""
    positive = ("BUILD_IDENTITY=source_sha=abc,build_type=Release\n"
                "QT_VERSION=6.8.3\nSTATE=Running\n"
                "LISTENER_EFFECTIVE=0.0.0.0:5921\n"
                "LAST_ERROR_CODE=none\nLAST_ERROR_MESSAGE=none\n"
                "Warning: BUILD_IDENTITY=source_sha=abc,build_type=Release\n"
                "STATE=Faulted\nLISTENER_EFFECTIVE=none\n"
                "LAST_ERROR_CODE=RuntimeFailure\nFatalFailure: transport stopped\n")
    diag = parse_diagnostics(positive)
    assert len(diag["reports"]) == 2, diag
    assert diag["reports"][0]["LISTENER_EFFECTIVE"] == "0.0.0.0:5921", diag
    assert diag["last"]["STATE"] == "Faulted", diag
    assert diag["fault_line_count"] == 2, diag
    negative = ("WIN949_READY scenario=local dpr=1.5\nSTATEISH=Running\n"
                "# LAST_ERROR_CODE=none commentary\nBUILD_IDENTITYish=abc\n")
    blank = parse_diagnostics(negative)
    assert blank["reports"] == [] and blank["fault_lines"] == [], blank
    assert framebuffer_size(1920, 1080, 1.5, "logical") == (1920, 1080)
    assert framebuffer_size(1920, 1080, 1.5, "physical") == (2880, 1620)
    on_text = "BUILD_IDENTITY=source_sha=abc\nHYREMOTE_QUICK_ACTIVITY_WAKE=ON\n"
    off_text = "HYREMOTE_QUICK_ACTIVITY_WAKE=OFF\n"
    assert activity_wake_status(on_text, True) == "SELECTED"
    assert activity_wake_status(on_text, False) == "BLOCKED_ROUTE_NOT_SELECTED"
    assert activity_wake_status(off_text, False) == "SELECTED"
    assert activity_wake_status("no route line\n", True) == "BLOCKED_ROUTE_NOT_SELECTED"
    print("WIN949_SELF_TEST=PASS diagnostics=positive+negative framebuffer=logical+physical "
          "wake=positive+negative")
    return 0


# --------------------------------------------------------------------------- #
# host application under test
# --------------------------------------------------------------------------- #

class HostApp:
    def __init__(self, exe, args, env, cwd, log_path):
        self.log_path = Path(log_path)
        self.log_file = self.log_path.open("w", encoding="utf-8")
        self.proc = subprocess.Popen(
            [str(exe)] + list(args), cwd=str(cwd), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        self.lines = []
        self.marks = []
        self.pointers = []
        self.ready = threading.Event()
        self.lock = threading.Lock()
        self.thread = threading.Thread(target=self._pump, name="host-log", daemon=True)
        self.thread.start()

    def _pump(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            self.log_file.write(line + "\n")
            self.log_file.flush()
            with self.lock:
                self.lines.append(line)
            if line.startswith("WIN949_READY"):
                self.ready.set()
            elif line.startswith("WIN949_MARK"):
                fields = dict(part.split("=", 1) for part in line.split()[1:] if "=" in part)
                with self.lock:
                    self.marks.append({"seq": int(fields["seq"]), "state": int(fields["state"]),
                                       "t": int(fields["t"]) / 1000.0})
            elif line.startswith("WIN949_POINTER"):
                fields = dict(part.split("=", 1) for part in line.split()[1:] if "=" in part)
                with self.lock:
                    self.pointers.append({"x": int(fields["x"]), "y": int(fields["y"]),
                                          "t": int(fields["t"]) / 1000.0})

    def text(self):
        with self.lock:
            return "\n".join(self.lines)

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=5)
        self.thread.join(timeout=5)
        try:
            self.log_file.close()
        except OSError:
            pass


class CpuSampler(threading.Thread):
    """Samples the host process wall-time and per-thread CPU at ~1 Hz."""

    def __init__(self, pid):
        super().__init__(daemon=True, name="cpu-sampler")
        self.pid = pid
        self.stop_event = threading.Event()
        self.samples = []
        self.thread_samples = []
        self._process = psutil.Process(pid) if psutil else None

    def run(self):
        if self._process is None:
            return
        try:
            self._process.cpu_percent(None)
        except psutil.Error:
            return
        while not self.stop_event.wait(1.0):
            try:
                total = self._process.cpu_percent(None)
                threads = {}
                for thread in self._process.threads():
                    threads[thread.id] = thread.user_time + thread.system_time
                first = {}
                time.sleep(0.25)
                for thread in self._process.threads():
                    base = threads.get(thread.id)
                    if base is None:
                        continue
                    delta = (thread.user_time + thread.system_time) - base
                    first[thread.id] = delta / 0.25
                self.samples.append(total)
                self.thread_samples.append(first)
            except psutil.Error:
                break

    def stop(self):
        self.stop_event.set()
        self.join(timeout=3)


def stage_snapshot(path: Path):
    """Last complete snapshot line of the HYREMOTE_CPU_PROBE_FILE JSONL."""
    if not path or not path.exists():
        return None
    try:
        lines = [line for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]
    except OSError:
        return None
    for line in reversed(lines):
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            continue
    return None


def stage_delta(first, second):
    if not first or not second:
        return None
    delta = {}
    stages_a = first.get("stages", {})
    stages_b = second.get("stages", {})
    for name, value in stages_b.items():
        before = stages_a.get(name, {})
        entry = {}
        for key in ("calls", "cpu_ns", "wall_ns", "bytes"):
            entry[key] = int(value.get(key, 0)) - int(before.get(key, 0))
        delta[name] = entry
    for name, value in second.get("quick_grab_backend_counts", {}).items():
        delta[f"backend:{name}"] = {"calls": int(value) - int(
            first.get("quick_grab_backend_counts", {}).get(name, 0))}
    return delta


# --------------------------------------------------------------------------- #
# scenario execution
# --------------------------------------------------------------------------- #

def build_env(args, probe_file, frame_trace_file=None):
    root = Path(args.root).resolve()
    env = os.environ.copy()
    parts = [str(root / "build" / "runtime"), str(root / "build" / "plugins"),
             str(root / "build" / "install" / "bin"),
             str(Path(args.qt_prefix) / "bin")]
    env["PATH"] = ";".join(parts) + ";" + env.get("PATH", "")
    env["QT_PLUGIN_PATH"] = ";".join([str(root / "build" / "plugins"),
                                      str(Path(args.qt_prefix) / "plugins")])
    env["QSG_RHI_BACKEND"] = args.backend
    env["QT_LOGGING_RULES"] = "qt.qpa.*=false"
    # On Windows Qt routes qInfo/qWarning to the debugger unless explicitly
    # forced to the owning console. The product's STATE / LAST_ERROR_* /
    # LISTENER_EFFECTIVE report is emitted with qInfo (automatic access
    # controller), so these two switches are what make it observable in the
    # host log. They change log routing only, never product behaviour.
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    env["QT_LOGGING_TO_CONSOLE"] = "1"
    if args.scale_factor and args.scale_factor != "1":
        env["QT_SCALE_FACTOR"] = args.scale_factor
        env["QT_ENABLE_HIGHDPI_SCALING"] = "1"
    # #952: one binary, three arms. Compilation only grants the experimental
    # capability; this runtime env var is what actually selects A1/B/A2.
    env["HYREMOTE_QUICK_ACTIVITY_WAKE"] = "1" if getattr(args, "activity_wake", 0) else "0"
    if probe_file:
        env["HYREMOTE_CPU_PROBE_FILE"] = str(probe_file)
    else:
        env.pop("HYREMOTE_CPU_PROBE_FILE", None)
    # schema=3 frame steps must never be mixed into the legacy schema=2 file.
    if frame_trace_file:
        env["HYREMOTE_FRAME_TRACE_FILE"] = str(frame_trace_file)
    else:
        env.pop("HYREMOTE_FRAME_TRACE_FILE", None)
    return env


def wait_listener(app, port, timeout=15.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if app.proc.poll() is not None:
            raise RfbError(f"host quit before RFB listen; exit={app.proc.returncode}\n{app.text()[-2000:]}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.3):
                return True
        except OSError:
            time.sleep(0.1)
    raise RfbError("RFB listener never became reachable")


def measure(args):
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    label = args.label or f"{args.arm}-{args.backend}-{args.scenario}-{args.viewers}v"
    probe_file = outdir / f"{label}-probe.jsonl"
    if probe_file.exists():
        probe_file.unlink()
    # #952: schema=3 frame steps go to a SEPARATE absolute JSONL path; the
    # legacy schema=2 aggregate CPU file must stay clean.
    frame_trace_file = outdir / f"{label}-frame-trace.jsonl" if args.frame_trace else None
    if frame_trace_file and frame_trace_file.exists():
        frame_trace_file.unlink()
    log_path = outdir / f"{label}-host.log"

    width, height = [int(x) for x in args.size.lower().split("x")]
    dpr = float(args.scale_factor or "1")
    fb_w, fb_h = framebuffer_size(width, height, dpr, args.framebuffer_mode)
    mark_scale = 1.0 if args.framebuffer_mode == "logical" else dpr
    # 'reconnect' is a probe-side timeline only; the Qt fixture keeps painting
    # exactly the same local-marker scene it used for the earlier 56 runs.
    fixture_scenario = "local" if args.scenario == "reconnect" else args.scenario

    # Both probe files must be absolute: the fixture process cwd is --root,
    # not the probe's cwd. Relative paths would silently write into the tree.
    env = build_env(args, probe_file.resolve() if args.cpu_probe else None,
                    frame_trace_file.resolve() if frame_trace_file else None)
    port = args.port or free_port()
    if args.route == "plugin":
        plugin_option = f"hyremote:port={port}"
        if args.remote_input:
            plugin_option += ";input=1"
        fixture_args = ["-plugin", plugin_option,
                        "--scenario", fixture_scenario, "--phase-ms", str(args.phase_ms),
                        "--size", args.size, "--test-seconds", str(int(args.seconds) + 8)]
    else:
        # Qt5.15 route: the Generic plugin is Qt6-only, so the fixture owns a
        # legal HyRemote::RemoteAccess listener instead of a -plugin argument.
        fixture_args = ["--port", str(port),
                        "--scenario", fixture_scenario, "--phase-ms", str(args.phase_ms),
                        "--size", args.size, "--test-seconds", str(int(args.seconds) + 8)]
        if args.remote_input:
            fixture_args += ["--remote-input"]
    timeline = {}
    if args.scenario == "lifecycle":
        timeline = {"--hide-at": "5000", "--show-at": "8000",
                    "--resize-at": "12000", "--resize-to": args.resize_to}
        for key, value in timeline.items():
            fixture_args += [key, value]

    app = HostApp(Path(args.exe), fixture_args, env, Path(args.root), log_path)
    result = {
        "schema": "hyremote-win949-probe/1",
        "label": label,
        "source_sha": args.source_sha,
        "arm": args.arm,
        "route": args.route,
        "backend_requested": args.backend,
        "scenario": args.scenario,
        "viewers": args.viewers,
        "logical_size": [width, height],
        "scale_factor": dpr,
        "framebuffer_mode": args.framebuffer_mode,
        "fixture_scenario": fixture_scenario,
        "framebuffer_expected": [fb_w, fb_h],
        "capture_route": None,
        "viewer_visible_latency": "NOT_MEASURED",
        "activity_wake_expected": bool(getattr(args, "activity_wake", 0)),
        "cpu_probe_path": str(probe_file.resolve()) if args.cpu_probe else None,
        "frame_trace_path": str(frame_trace_file.resolve()) if frame_trace_file else None,
        "errors": [],
    }
    stage_start = None
    sampler = CpuSampler(app.proc.pid)
    sampler.start()
    viewer = []
    try:
        app.ready.wait(timeout=20)
        if not app.ready.is_set():
            raise RfbError("fixture never reported WIN949_READY")
        wait_listener(app, port)

        for index in range(args.viewers):
            client = RfbClient("127.0.0.1", port, timeout=max(10.0, args.seconds))
            viewer.append(client)
            deadline = time.time() + 10
            while time.time() < deadline and (client.width, client.height) != (fb_w, fb_h):
                client.full_frame()
                if (client.width, client.height) == (fb_w, fb_h):
                    break
                time.sleep(0.1)
                if client.name is None:
                    break
        if viewer:
            result["framebuffer_observed"] = [viewer[0].width, viewer[0].height]

        # --- quadrant pixel truth ---------------------------------------- #
        def quadrant_check(client):
            checks = []
            for gx, gy, expected in QUADS:
                px = int((gx + 0.5) * client.width / 2) - 1
                py = int((gy + 0.5) * client.height / 2) - 1
                client.poll(px, py, 2, 2)
                actual = client.pixel(px, py)
                checks.append({"quadrant": [gx, gy], "at": [px, py],
                               "expected": list(expected), "actual": list(actual) if actual else None,
                               "ok": near(actual, expected)})
            return checks

        if viewer:
            client = viewer[0]
            client.full_frame()
            result["pixel_checks"] = quadrant_check(client)
            result["pixel_truth"] = all(check["ok"] for check in result["pixel_checks"])
            result["first_connect"] = {
                "framebuffer": [client.width, client.height],
                "pixel_checks": result["pixel_checks"],
                "pixel_truth": result["pixel_truth"],
            }
        else:
            result["pixel_truth"] = None

        # --- marker region geometry -------------------------------------- #
        stage_start = None
        if args.cpu_probe:
            stage_deadline = time.time() + 4
            while stage_start is None and time.time() < stage_deadline:
                stage_start = stage_snapshot(probe_file)
                if stage_start is None:
                    time.sleep(0.25)
        mark_x = int(round(16 * mark_scale))
        mark_y = int(round(16 * mark_scale))
        mark_w = int(round(40 * mark_scale))
        mark_h = int(round(40 * mark_scale))

        latencies_upper = []
        latencies_lower = []
        rtts = []
        samples = []
        detected = 0
        missed = 0
        mismatched = 0

        seen_seq = set()
        start = time.time()
        settle_until = start + args.settle_seconds
        deadline = start + args.seconds
        target = viewer[0] if viewer else None
        reconnect_done = False
        reconnect_wall = None
        unplanned_reconnects = 0
        warmup = []
        iteration = 0
        transport_events = []
        resize_events = []

        def reconnect_viewers(count):
            for client in viewer:
                client.close()
            fresh = []
            for _ in range(count):
                client = RfbClient("127.0.0.1", port, timeout=max(10.0, args.seconds))
                deadline_connect = time.time() + 8
                while time.time() < deadline_connect:
                    client.full_frame()
                    if client.width and client.height:
                        break
                fresh.append(client)
            return fresh

        while time.time() < deadline:
            if target is not None:
                iteration += 1
                try:
                    # A second Viewer is a real consumer as well: it must issue its
                    # own update requests, otherwise it silently receives nothing.
                    if len(viewer) > 1 and iteration % 2 == 0:
                        viewer[1].poll(mark_x, mark_y, mark_w, mark_h)
                    t_send, t_recv, _ = target.poll(mark_x, mark_y, mark_w, mark_h)
                except (RfbError, OSError) as exc:
                    transport_events.append({"t": time.time(), "error": f"{type(exc).__name__}: {exc}",
                                             "framebuffer": [target.width, target.height]})
                    log_line(f"WIN949_TRANSPORT_EVENT {type(exc).__name__}: {exc}")
                    unplanned_reconnects += 1
                    viewer = reconnect_viewers(max(1, len(viewer)))
                    target = viewer[0]
                    continue
                if (target.width, target.height) != (fb_w, fb_h):
                    resize_events.append({"t": time.time(),
                                          "framebuffer": [target.width, target.height]})
                    fb_w, fb_h = target.width, target.height
                rtts.append((t_recv - t_send) * 1000.0)
                time.sleep(args.poll_interval)
                actual = target.pixel(mark_x + mark_w // 2, mark_y + mark_h // 2)
                with app.lock:
                    marks = list(app.marks)
                for entry in marks:
                    if entry["seq"] in seen_seq:
                        continue
                    expected = MARK_PALETTE[entry["state"] % len(MARK_PALETTE)]
                    if not near(actual, expected):
                        continue
                    seen_seq.add(entry["seq"])
                    upper = (t_recv - entry["t"]) * 1000.0
                    lower = (t_send - entry["t"]) * 1000.0
                    sample = {"seq": entry["seq"], "upper_ms": round(upper, 3),
                              "lower_ms": round(lower, 3)}
                    if entry["t"] < settle_until:
                        warmup.append(sample)
                        samples.append(dict(sample, warmup=True))
                    else:
                        latencies_upper.append(upper)
                        latencies_lower.append(lower)
                        detected += 1
                        samples.append(sample)
                    break
            else:
                # 0-Viewer baseline: nothing to poll, only host cost is sampled.
                time.sleep(0.05)

            if args.scenario == "lifecycle" and not reconnect_done and time.time() - start > 20:
                reconnect_done = True
                for client in viewer:
                    client.close()
                viewer = []
                time.sleep(1.5)
                viewer = reconnect_viewers(1)
                target = viewer[0]
                result["reconnect"] = {"framebuffer": [target.width, target.height],
                                       "pixel_checks": quadrant_check(target)}

            # #949 focused DPR=1.5 retest: close the first RFB client, wait, then
            # connect a second one to the SAME host/listener. No hide/show/resize,
            # so the logical 1920x1080 ServerInit must stay unchanged.
            if args.scenario == "reconnect" and not reconnect_done \
                    and time.time() - start > args.reconnect_at:
                reconnect_done = True
                reconnect_wall = time.time()
                with app.lock:
                    marks_before = len(app.marks)
                first_fb = [target.width, target.height] if target else None
                for client in viewer:
                    client.close()
                viewer = []
                time.sleep(1.0)
                viewer = reconnect_viewers(1)
                target = viewer[0]
                target.full_frame()
                reconnect_checks = quadrant_check(target)
                result["reconnect"] = {
                    "at_since_start_s": round(reconnect_wall - start, 1),
                    "first_framebuffer": first_fb,
                    "framebuffer": [target.width, target.height],
                    "pixel_checks": reconnect_checks,
                    "pixel_truth": all(check["ok"] for check in reconnect_checks),
                    "marks_before": marks_before,
                    "second_client_handshake": bool(target.width and target.height),
                }

            if args.scenario == "input" and target is not None:
                sent = []
                positions = [(int(fb_w * 0.30), int(fb_h * 0.60)),
                             (int(fb_w * 0.60), int(fb_h * 0.35)),
                             (int(fb_w * 0.45), int(fb_h * 0.70))] * 3
                for px, py in positions:
                    target.send_pointer(px, py)
                    sent.append({"x": px, "y": py, "t": time.time()})
                    time.sleep(0.06)
                time.sleep(0.4)
                with app.lock:
                    received = list(app.pointers)
                echo = []
                used = set()
                for item in sent:
                    match = None
                    for index, event in enumerate(received):
                        if index in used:
                            continue
                        if (event["x"] == item["x"] and event["y"] == item["y"]
                                and 0 <= event["t"] - item["t"] <= 3.0):
                            used.add(index)
                            match = event
                            break
                    echo.append({"sent": [item["x"], item["y"]],
                                 "delivered": match is not None,
                                 "delivery_ms": round((match["t"] - item["t"]) * 1000.0, 1) if match else None})
                result["input_echo_unique_events"] = len(used)
                result["input_echo"] = {
                    "samples": echo,
                    "host_pointer_events": len(received),
                    "all_delivered": all(item["delivered"] for item in echo),
                    "note": "coordinate-level echo; the on-screen pointer indicator is not a "
                            "reliable pixel probe while a physical mouse is over the window",
                }
                break

        with app.lock:
            marks = list(app.marks)
        missing = [entry["seq"] for entry in marks if entry["seq"] not in seen_seq]
        missed = len(missing)

        result["unplanned_reconnects"] = unplanned_reconnects
        result["listener_after_test"] = listener_reachable(port)
        result["host_marks"] = {
            "total": len(marks),
            "first_t": marks[0]["t"] if marks else None,
            "last_t": marks[-1]["t"] if marks else None,
            "last_to_end_s": round(time.time() - marks[-1]["t"], 2) if marks else None,
            "after_planned_reconnect": (
                sum(1 for item in marks if item["t"] >= reconnect_wall) if reconnect_wall else None),
        }

        result.update({
            "latency_upper": summarize(latencies_upper),
            "latency_lower": summarize(latencies_lower),
            "latency_warmup": summarize([item["upper_ms"] for item in warmup]),
            "poll_rtt": summarize(rtts),
            "marks_host": len(marks),
            "marks_detected": detected,
            "marks_warmup": len(warmup),
            "marks_missed": missed,
            "settle_seconds": args.settle_seconds,
            "transport_events": transport_events,
            "resize_events": resize_events,
            "remote_input_enabled": bool(args.remote_input),
            "samples": samples[:600],
        })
    except Exception as exc:  # noqa: BLE001 - evidence tool: record, do not mask
        result["errors"].append(f"{type(exc).__name__}: {exc}")
    finally:
        for client in viewer:
            client.close()
        sampler.stop()
        app.stop()

    host_text = app.text()
    # #952 hard gate: the same binary must prove which scheduling arm actually
    # ran. A missing/contradictory log line is a blocked route, never a result.
    wake_expected = bool(getattr(args, "activity_wake", 0))
    wake = activity_wake_status(host_text, wake_expected)
    result["runtime_route_validation"] = wake
    result["activity_wake_log"] = {
        "expected": ("HYREMOTE_QUICK_ACTIVITY_WAKE=ON" if wake_expected
                     else "HYREMOTE_QUICK_ACTIVITY_WAKE=OFF"),
        "on_seen": "HYREMOTE_QUICK_ACTIVITY_WAKE=ON" in host_text,
        "off_seen": "HYREMOTE_QUICK_ACTIVITY_WAKE=OFF" in host_text,
    }
    if wake != "SELECTED":
        result["errors"].append("BLOCKED_ROUTE_NOT_SELECTED: " + str(result["activity_wake_log"]))
    identity = re.search(r"BUILD_IDENTITY=([^\s]+)", host_text)
    result["build_identity"] = identity.group(1) if identity else "NOT_MEASURED"
    result["host_log_path"] = str(log_path.resolve())
    diagnostics = parse_diagnostics(host_text)
    result["session_and_listener"] = diagnostics
    result["session_and_listener"]["required_fields_from_last_report"] = {
        key: (diagnostics["last"] or {}).get(key)
        for key in ("STATE", "LAST_ERROR_CODE", "LAST_ERROR_MESSAGE", "LISTENER_EFFECTIVE")
    }
    if args.scenario == "reconnect":
        states = [report.get("STATE") for report in diagnostics["reports"] if report.get("STATE")]
        error_codes = [report["LAST_ERROR_CODE"] for report in diagnostics["reports"]
                       if report.get("LAST_ERROR_CODE") not in (None, "none")]
        focus = {
            "framebuffer_logical_1x": result.get("framebuffer_observed") == [width, height],
            "first_pixel_truth": bool((result.get("first_connect") or {}).get("pixel_truth")),
            "reconnect_framebuffer": (result.get("reconnect") or {}).get("framebuffer") == [width, height],
            "reconnect_pixel_truth": bool((result.get("reconnect") or {}).get("pixel_truth")),
            "no_transport_events": not result.get("transport_events"),
            "no_unplanned_reconnects": result.get("unplanned_reconnects") == 0,
            "marks_after_reconnect": ((result.get("host_marks") or {}).get("after_planned_reconnect") or 0) > 0,
            "listener_alive_at_end": bool(result.get("listener_after_test")),
            "state_reported_running": "Running" in states,
            "no_faulted_state": "Faulted" not in states,
            "no_error_code": not error_codes,
            "no_probe_errors": not result.get("errors"),
        }
        result["focus_checks"] = focus
        result["focus_result"] = "PASS" if all(focus.values()) else "FAIL"

    if "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=GL_RHI_PBO_SOURCE" in host_text:
        result["capture_route"] = "GL_RHI_PBO_SOURCE"
    elif "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=D3D11_STAGING_SOURCE" in host_text:
        result["capture_route"] = "D3D11_STAGING_SOURCE"
    elif "HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=PUBLIC_QT_FALLBACK" in host_text:
        result["capture_route"] = "PUBLIC_QT_FALLBACK"
    else:
        result["capture_route"] = "UNREPORTED_PUBLIC_QT" if args.arm.upper().startswith("A") else "UNREPORTED"

    # 8 marker phases at 250 ms means any lag >= ~2 s aliases onto an older
    # phase: the observation is still real staleness, but the exact value is not
    # resolvable. Report the confident subset separately instead of letting a
    # handful of aliased samples define P95/P99.
    latent = [item["upper_ms"] for item in result.get("samples", []) if not item.get("warmup")]
    if latent:
        confident = [value for value in latent if value < 1990.0]
        result["latency_upper_confident"] = summarize(confident)
        result["aliased_samples"] = len(latent) - len(confident)

    if sampler.samples:
        result["host_process_cpu_percent"] = summarize(sampler.samples)
    if sampler.thread_samples:
        peak = {}
        for sample in sampler.thread_samples:
            for thread_id, value in sample.items():
                if value > peak.get(thread_id, 0.0):
                    peak[thread_id] = value
        result["host_thread_cpu_peak_percent"] = {str(key): round(value * 100, 1) for key, value in peak.items()}

    snapshot = stage_snapshot(probe_file) if args.cpu_probe else None
    if snapshot and stage_start:
        result["stage_delta"] = stage_delta(stage_start, snapshot)
    if snapshot:
        result["probe_last_snapshot"] = {"elapsed_ms": snapshot.get("elapsed_ms"),
                                         "process_cpu_ns": snapshot.get("process_cpu_ns")}
        stages = snapshot.get("stages", {})
        result["stage_totals"] = {name: {"calls": value.get("calls"),
                                         "cpu_ms": round(value.get("cpu_ns", 0) / 1e6, 3),
                                         "wall_ms": round(value.get("wall_ns", 0) / 1e6, 3)}
                                  for name, value in stages.items()}
        result["quick_grab_backend_counts"] = snapshot.get("quick_grab_backend_counts")

    # The product only prints HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE from the
    # experimental branches; with both flags OFF (A1/A2) nothing is printed at
    # all, and some builds suppress the runtime diagnostic block entirely. The
    # stage counters are then the authoritative route evidence: a native source
    # increments its own stage names, the public Qt route only increments the
    # grab/convert/buffer names.
    if result["capture_route"].startswith("UNREPORTED"):
        stages = result.get("stage_totals") or {}

        def calls(name):
            return int((stages.get(name) or {}).get("calls") or 0)

        native_d3d11 = calls("quick_d3d11_staging_reuse") + calls("quick_d3d11_staging_allocation")
        native_gl = calls("quick_gl_gpu_map_copy") + calls("quick_gl_pbo_reuse") + calls("quick_gl_render_callback")
        if native_d3d11:
            result["capture_route"] = "D3D11_STAGING_SOURCE_STAGE_INFERRED"
        elif native_gl:
            result["capture_route"] = "GL_RHI_PBO_SOURCE_STAGE_INFERRED"
        elif calls("quick_grab_submit"):
            result["capture_route"] = "PUBLIC_QT_FALLBACK_STAGE_INFERRED"
        result["capture_route_stage_evidence"] = {
            "d3d11_staging_calls": native_d3d11,
            "gl_native_calls": native_gl,
            "public_grab_calls": calls("quick_grab_submit"),
        }

    result["log_path"] = str(log_path)
    result["probe_path"] = str(probe_file) if args.cpu_probe else None
    out = outdir / f"{label}.json"
    out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({key: result.get(key) for key in
                      ("label", "capture_route", "pixel_truth", "framebuffer_observed",
                       "latency_upper", "marks_detected", "marks_missed", "errors")},
                     ensure_ascii=False))
    return result


PROGRESS_PATH = None


def log_line(text):
    """Progress line to stdout and, when requested, to the long-run arm log."""
    print(text, flush=True)
    if PROGRESS_PATH:
        try:
            with open(PROGRESS_PATH, "a", encoding="utf-8") as handle:
                handle.write(text + "\n")
        except OSError:
            pass


ARM_RUNS = [
    # (#949) 1920x1080 fixed comparisons: quasi-static / local change / continuous
    # motion, plus 0- and 2-Viewer cases and the lifecycle/input scenarios.
    ("static", 1, 60.0),
    ("local", 1, 60.0),
    ("motion", 1, 60.0),
    ("local", 2, 60.0),
    ("local", 0, 30.0),
    ("lifecycle", 1, 35.0),
    ("input", 1, 25.0),
]


def run_arm(args):
    """Run the whole #949 scenario set for one build arm, printing progress per run."""
    import copy

    for backend in args.backend.split(","):
        for scenario, viewers, seconds in ARM_RUNS:
            run_args = copy.copy(args)
            run_args.backend = backend
            run_args.scenario = scenario
            run_args.viewers = viewers
            run_args.seconds = seconds
            run_args.remote_input = scenario == "input"
            run_args.label = f"{args.arm}-{backend}-{scenario}-{viewers}v"
            log_line(f"WIN949_RUN_START label={run_args.label} seconds={seconds}")
            try:
                measure(run_args)
            except Exception as exc:  # noqa: BLE001 - continue the matrix, record the failure
                log_line(f"WIN949_RUN_ERROR label={run_args.label} {type(exc).__name__}: {exc}")
            log_line(f"WIN949_RUN_DONE label={run_args.label}")
    log_line("WIN949_ARM_DONE")
    return 0


def build_fixture(args):
    """Configure and build the non-product Qt-only fixture with the requested Kit.

    MSVC is entered through vcvars64.bat because a bare cl.exe has no INCLUDE/LIB.
    This runs here (not in a shell snippet) so the Qt Kit and the compiler family
    are stated explicitly and reproducibly.
    """
    capture = subprocess.run(f'call "{args.vcvars}" >nul && set', shell=True, cwd=str(args.root),
                             capture_output=True, text=True)
    if capture.returncode != 0:
        raise SystemExit(f"vcvars failed: {capture.stdout}{capture.stderr}")
    env = os.environ.copy()
    for line in capture.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    source = Path(args.root) / "tools" / "perf" / "tests" / "win949_frame_first" / "app"
    build = Path(args.fixture_build)
    configure = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={args.qt_prefix}"]
    print("configure:", " ".join(configure))
    subprocess.run(configure, env=env, check=True)
    compile_cmd = ["cmake", "--build", str(build)]
    print("build:", " ".join(compile_cmd))
    subprocess.run(compile_cmd, env=env, check=True)
    exe = build / "win949-frame-fixture.exe"
    if not exe.is_file():
        raise SystemExit(f"fixture executable missing: {exe}")
    print(f"WIN949_FIXTURE_READY={exe}")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mode", default="measure",
                        choices=["measure", "build-fixture", "arm", "self-test"])
    parser.add_argument("--fixture-build", default=None)
    parser.add_argument("--vcvars", default=None)
    parser.add_argument("--root", required=True, help="repository root that owns build/")
    parser.add_argument("--exe", default=None, help="frame fixture executable")
    parser.add_argument("--qt-prefix", required=True, help="Qt kit prefix used at run time")
    parser.add_argument("--outdir", default=None)
    parser.add_argument("--label", default=None)
    parser.add_argument("--arm", default=None, help="A1 | B | A2")
    parser.add_argument("--backend", default="opengl",
                        help="opengl | d3d11 | vulkan | software, or a comma separated list in --mode arm")
    parser.add_argument("--scenario", default="local",
                        choices=["static", "local", "motion", "lifecycle", "input", "reconnect"])
    parser.add_argument("--viewers", type=int, default=1)
    parser.add_argument("--seconds", type=float, default=60.0)
    parser.add_argument("--settle-seconds", type=float, default=5.0,
                        help="exclude this warm-up window from latency statistics")
    parser.add_argument("--size", default="1920x1080")
    parser.add_argument("--scale-factor", default="1")
    parser.add_argument("--framebuffer-mode", default="physical", choices=["physical", "logical"],
                        help="logical = #806 Generic Composite 1 output pixel per logical unit; "
                             "physical = direct Quick DPR-scaled pixels")
    parser.add_argument("--reconnect-at", type=float, default=20.0,
                        help="seconds after start when the first RFB client is closed and a second one connects")
    parser.add_argument("--phase-ms", type=int, default=250)
    parser.add_argument("--poll-interval", type=float, default=0.025,
                        help="seconds between Viewer update requests (~40 Hz, a realistic viewer cadence)")
    parser.add_argument("--resize-to", default="1280x720")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--remote-input", action="store_true",
                        help="activate the Generic 'input=1' contract for the input scenario")
    parser.add_argument("--cpu-probe", action="store_true")
    parser.add_argument("--activity-wake", type=int, choices=[0, 1], default=0,
                        help="#952: 0=A1/A2 original scheduling, 1=B activity-driven scheduling")
    parser.add_argument("--frame-trace", action="store_true",
                        help="#952: write schema3 HYREMOTE_FRAME_TRACE_FILE to a separate absolute JSONL")
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--route", default="plugin", choices=["plugin", "cpp-api"],
                        help="plugin = Qt6 Generic -plugin route; cpp-api = fixture-owned "
                             "HyRemote::RemoteAccess listener (required for Qt5.15)")
    parser.add_argument("--progress-log", default=None)
    args = parser.parse_args(argv)
    global PROGRESS_PATH
    PROGRESS_PATH = args.progress_log
    if args.mode == "self-test":
        return self_test()
    if args.mode == "build-fixture":
        if not args.fixture_build or not args.vcvars:
            parser.error("--fixture-build and --vcvars are required for build-fixture")
        return build_fixture(args)
    if not args.exe or not args.outdir or not args.arm:
        parser.error("--exe, --outdir and --arm are required for measure")
    if args.mode == "arm":
        return run_arm(args)
    measure(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
