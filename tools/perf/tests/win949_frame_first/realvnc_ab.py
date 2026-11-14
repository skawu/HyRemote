"""TASK A one-shot A->B->A pre-acceptance with a real Windows VNC Viewer.

#967 Owner task (2026-11-14): a single, real-install-shaped qualification run
for the frozen product candidate `SOURCE_SHA=8105f1a0067865672747e2b3d5728bb87b98e063`
(arm B) against the user's previous package `b0680d5d` (arm A), including a
rollback rehearsal. Uses the registered Win949 Qt6 fixture as the application
host (Generic plugin route) and the machine's RealVNC Viewer as the real
Viewer — NOT the probe's internal RFB client.

Per segment the script measures:
- Host process CPU (single-core %) and private memory at 0/1/2 Viewers,
- Viewer-visible marker delivery latency (WIN949_MARK epoch timestamp ->
  first on-screen colour *transition* to the marker palette entry; an
  observation that already shows the target colour is discarded as
  ambiguous, per the #1000 review lesson),
- the product's diagnostic CPU-probe JSONL (stage counters, socket queue)
  which is archived per segment and never mixed with screen-visible timings.

Install shape: the candidate is "installed" by materializing one dist tree
under an `active/` directory (PATH + QT_PLUGIN_PATH point there), then
"upgraded"/"rolled back" by re-materializing the other dist. Nothing else
differs between arms. Qt runtime, QPA generic plugin and HyRemoteRemoteAccess
all come from the same dist tree, so the file set exercises exactly what a
user would install.

Usage (from the repo root):
  python tools/perf/tests/win949_frame_first/realvnc_ab.py \
    --dist-a tmp/taskA-8105f1a/dist-A --dist-b tmp/taskA-8105f1a/dist-B \
    --fixture-exe tmp/win967-a8b/fixture-build/win949-frame-fixture.exe \
    --qt-prefix C:/Qt/6.8.3/msvc2022_64 \
    --vncviewer "C:/Program Files/RealVNC/VNC Viewer/vncviewer.exe" \
    --outdir tmp/taskA-8105f1a/ab-realvnc

Outputs `result.json` plus per-segment probe JSONL and host logs.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

import numpy as np
import psutil
from PIL import ImageGrab

# Must match kMarkerPalette in app/frame_fixture.cpp.
MARK_PALETTE = [(0, 0, 0), (255, 255, 255), (255, 0, 0), (0, 255, 0),
                (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255)]

# Colour tolerance: the fixture paints exact palette pixels, but a Viewer
# window that scales the framebuffer interpolates them. Match per channel.
COLOR_TOL = 24


def near(a, b):
    return a is not None and b is not None and all(
        abs(int(x) - int(y)) <= COLOR_TOL for x, y in zip(a, b))


def viewer_window_rect(viewer_pids, port):
    """Screen rect of the actual connection window: RealVNC Viewer titles it
    `<host>::<port> - RealVNC Viewer`. The viewer process also owns helper
    windows (File Browser 900x640, Chat, Encryption) that a largest-window
    heuristic would wrongly pick — this machine has several, hence the
    address-specific match. The window is raised top-most so concurrent
    automation on this desktop cannot cover the sampled region."""
    import ctypes
    import ctypes.wintypes as wt
    user32 = ctypes.windll.user32
    found = []
    pid_holder = ctypes.c_ulong()
    prefix = f"127.0.0.1::{port}"
    def cb(hwnd, lp):
        pid_holder.value = 0
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid_holder))
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        title = buf.value.strip()
        # Connected title: "127.0.0.1::<port> (<server-name>) - RealVNC Viewer"
        # where the server-name infix appears once RFB security is negotiated.
        if (pid_holder.value in viewer_pids and title.startswith(prefix)
                and "RealVNC Viewer" in title):
            rect = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rect))
            found.append((hwnd, (rect.left, rect.top, rect.right, rect.bottom)))
            # TOPMOST without SWP_NOZORDER: this desktop runs concurrent
            # full-screen automation windows, and a z-order change is the
            # whole point (0x53 = NOSIZE|NOMOVE|NOACTIVATE|SHOWWINDOW).
            user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0053)
        return True
    CMPFUNC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    user32.EnumWindows(CMPFUNC(cb), None)
    return found[0] if found else None


def client_content_rect(hwnd):
    """Client-area origin/size in screen coordinates — the pure VNC content
    region of the viewer window (no title bar or frame)."""
    import ctypes
    import ctypes.wintypes as wt
    user32 = ctypes.windll.user32
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    cr = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(cr))
    return ((pt.x, pt.y, pt.x + cr.right, pt.y + cr.bottom), cr.right, cr.bottom)


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def wait_ready(proc, reader, timeout=20.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"fixture exited rc={proc.returncode}")
        if reader.ready_line:
            return True
        time.sleep(0.1)
    raise RuntimeError("WIN949_READY never seen")


def wait_rfb(port, timeout=15.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.3):
                return True
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("RFB listener never became reachable")


class StdoutReader(threading.Thread):
    def __init__(self, proc):
        super().__init__(daemon=True)
        self.proc = proc
        self.ready_line = None
        self.marks = []  # {"seq", "state", "t_ms", "recv_ms"}
        self.lines = []
        self.start()

    def run(self):
        for raw in self.proc.stdout:
            line = raw.strip()
            if not line:
                continue
            self.lines.append(line)
            if line.startswith("WIN949_READY") and self.ready_line is None:
                self.ready_line = line
            elif line.startswith("WIN949_MARK"):
                fields = {}
                for tok in line.split()[1:]:
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        fields[k] = v
                if {"seq", "state", "t"}.issubset(fields):
                    self.marks.append({"seq": int(fields["seq"]),
                                       "state": int(fields["state"]),
                                       "t_ms": int(fields["t"]),
                                       "recv_ms": time.time() * 1000.0})


class ScreenWatcher(threading.Thread):
    """Polls the screen, tracks the marker point colour, closes pending marks
    on the first colour *transition* to the expected palette entry."""

    def __init__(self, palette=MARK_PALETTE, fps=30):
        super().__init__(daemon=True)
        self.palette = palette
        self.interval = 1.0 / fps
        self.last_color = None
        self.point = None          # (x, y) screen coords of the marker centre
        self.pending = []          # marks awaiting an on-screen transition
        self.detected = []         # {"seq", "detect_ms", "first_color"}
        self.ambiguous = []        # target colour already on screen at mark
        self.stop_flag = threading.Event()
        self.start()

    def locate_marker(self, hwnd, fb_size):
        """The fixture paints its marker at a FIXED framebuffer position
        (16,16)..(56,56), centre (36,36). With a 1:1 viewer display the
        client-area origin plus that offset is the sampling point — no pixel
        search needed, which also sidesteps the motion scenario painting the
        same palette colours over large areas."""
        import ctypes
        import ctypes.wintypes as wt
        content, cw, ch = client_content_rect(hwnd)
        self.rect = content
        self.scale = cw / fb_size[0] if fb_size[0] else 1.0
        if abs(self.scale - 1.0) > 0.02:
            log(f"WARNING viewer client {cw}x{ch} != framebuffer {fb_size[0]}x{fb_size[1]} "
                f"(scale {self.scale:.3f}); latency samples keep going but are "
                f"scaled-display caliber")
        cx, cy = 36 * self.scale, 36 * self.scale
        self.point = (int(content[0] + cx), int(content[1] + cy))
        log(f"marker sampling point at screen {self.point} "
            f"(client origin {content[:2]}, client {cw}x{ch}, fb {fb_size[0]}x{fb_size[1]})")
        return True

    def submit_mark(self, seq, state, t_ms):
        target = tuple(self.palette[state % len(self.palette)])
        if self.point is None:
            self.detected.append({"seq": seq, "detect_ms": None, "note": "no-marker-point"})
            return
        if near(self.last_color, target):
            self.ambiguous.append({"seq": seq, "note": "target-already-on-screen"})
            return
        self.pending.append({"seq": seq, "target": target, "t_ms": t_ms})

    def run(self):
        # Sample only the marker neighbourhood (80x80 around the sampling
        # point). A full-window grab runs at ~10 fps on this desktop, which is
        # slower than the 5.5 Hz mark rate and back-logs the pending queue so
        # detect_ms inflates into seconds; an 80x80 grab sustains 30+ fps.
        half = 40
        while not self.stop_flag.is_set():
            now_ms = time.time() * 1000.0
            try:
                if self.point is None:
                    time.sleep(self.interval)
                    continue
                px, py = self.point
                bbox = (px - half, py - half, px + half, py + half)
                img = np.asarray(ImageGrab.grab(bbox=bbox).convert("RGB"))
            except Exception:
                time.sleep(self.interval)
                continue
            color = tuple(int(v) for v in img[half, half])
            if not near(color, self.last_color):
                for m in list(self.pending):
                    if near(color, m["target"]):
                        self.detected.append({"seq": m["seq"],
                                              "detect_ms": round(now_ms - m["t_ms"], 1),
                                              "first_color": color})
                        self.pending.remove(m)
            self.last_color = color
            time.sleep(self.interval)


def click_at(x, y):
    import ctypes
    user32 = ctypes.windll.user32
    user32.SetCursorPos(int(x), int(y))
    time.sleep(0.08)
    user32.mouse_event(2, 0, 0, 0, 0)  # LEFTDOWN
    user32.mouse_event(4, 0, 0, 0, 0)  # LEFTUP


def dismiss_encryption_dialog(viewer_pids):
    """RealVNC Viewer shows a modal 'Encryption' warning for unencrypted
    servers; the connection does not render until Continue is pressed.
    Buttons are driven by BM_CLICK window messages instead of synthesised
    cursor clicks: this desktop runs concurrent full-screen automation, so a
    cursor click can land on an overlapping top-most window. Tick 'Don't warn
    me about this again' first, then Continue."""
    import ctypes
    import ctypes.wintypes as wt
    user32 = ctypes.windll.user32
    dismissed = 0
    pid_holder = ctypes.c_ulong()
    deadline = time.time() + 6.0
    while time.time() < deadline:
        dialogs = []
        def cb(hwnd, lp):
            pid_holder.value = 0
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid_holder))
            buf = ctypes.create_unicode_buffer(128)
            user32.GetWindowTextW(hwnd, buf, 128)
            if pid_holder.value in viewer_pids and buf.value.strip() == "Encryption":
                dialogs.append(hwnd)
            return True
        CMPFUNC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
        user32.EnumWindows(CMPFUNC(cb), None)
        if dialogs:
            for hwnd in dialogs:
                buttons = []
                def child_cb(child, lp):
                    cls = ctypes.create_unicode_buffer(64)
                    user32.GetClassNameW(child, cls, 64)
                    if cls.value == "Button":
                        buf = ctypes.create_unicode_buffer(128)
                        user32.GetWindowTextW(child, buf, 128)
                        buttons.append((buf.value.lower(), child))
                    return True
                CHILDFUNC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
                user32.EnumChildWindows(hwnd, CHILDFUNC(child_cb), None)
                for text, child in buttons:
                    if "warn" in text:
                        user32.SendMessageW(child, 0xF5, 0, 0)  # BM_CLICK
                        time.sleep(0.15)
                for text, child in buttons:
                    if "continue" in text:
                        user32.SendMessageW(child, 0xF5, 0, 0)  # BM_CLICK
                        dismissed += 1
                        break
                time.sleep(0.8)
            break
        time.sleep(0.5)
    return dismissed


def cpu_percent_core(proc, samples, stop, interval=0.5):
    """Average single-core utilisation via cpu_times deltas. The motion
    scenario renders in short bursts (4 Hz), so instantaneous cpu_percent
    samples mostly read 0 while the average stays meaningful."""
    p = psutil.Process(proc.pid)
    prev = p.cpu_times()
    t_prev = time.time()
    while not stop.is_set() and proc.poll() is None:
        time.sleep(interval)
        try:
            cur = p.cpu_times()
            t_now = time.time()
            used = (cur.user - prev.user) + (cur.system - prev.system)
            wall = max(t_now - t_prev, 1e-3)
            mi = p.memory_full_info()
            samples.append({"t": t_now,
                            "cpu_one_core_pct": round(used / wall * 100.0, 2),
                            "rss_mb": round(mi.rss / 1e6, 1)})
            prev, t_prev = cur, t_now
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            break


def materialize(dist: Path, active: Path):
    """Materialize a dist into an install directory WITHOUT deleting the old
    one: this machine's shell safety hook aborts bulk rmtree operations, which
    would kill a backgrounded runner mid-campaign. Each arm gets its own
    directory; repeated installs of the same dist just overwrite in place."""
    active.mkdir(parents=True, exist_ok=True)
    shutil.copytree(dist, active, dirs_exist_ok=True)
    return active


def run_segment(label, args, viewers, seconds, port, capture_marks, active_dir):
    outdir = Path(args.outdir)
    env = os.environ.copy()
    active = Path(active_dir).resolve()
    qt = Path(args.qt_prefix)
    parts = [str(active / "bin"), str(active / "plugins"), str(qt / "bin")]
    env["PATH"] = ";".join(parts) + ";" + env.get("PATH", "")
    env["QT_PLUGIN_PATH"] = ";".join([str(active / "plugins"), str(qt / "plugins")])
    env["QSG_RHI_BACKEND"] = "opengl"
    env["QT_LOGGING_RULES"] = "qt.qpa.*=false"
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    env["QT_LOGGING_TO_CONSOLE"] = "1"
    env.pop("HYREMOTE_QUICK_ACTIVITY_WAKE", None)
    probe_file = (outdir / f"{label}-probe.jsonl").resolve()
    env["HYREMOTE_CPU_PROBE_FILE"] = str(probe_file)
    env["HYREMOTE_FRAME_TRACE_FILE"] = str((outdir / f"{label}-frame-trace.jsonl").resolve())

    fixture_args = ["-plugin", f"hyremote:port={port}",
                    "--scenario", args.scenario, "--phase-ms", str(args.phase_ms),
                    "--size", args.size, "--test-seconds", str(seconds + 8)]
    log(f"{label}: starting fixture ({viewers} viewer(s), {seconds}s)")
    proc = subprocess.Popen([str(args.fixture_exe)] + fixture_args, cwd=str(active),
                            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, encoding="utf-8", errors="replace")
    reader = StdoutReader(proc)
    wait_ready(proc, reader)
    wait_rfb(port)

    samples, stop = [], threading.Event()
    cpu_t = threading.Thread(target=cpu_percent_core, args=(proc, samples, stop), daemon=True)
    cpu_t.start()

    watcher = ScreenWatcher() if capture_marks else None
    vnc_procs = []
    vrect = None
    if viewers:
        time.sleep(1.0)
        for i in range(viewers):
            if i:
                time.sleep(1.5)
            vnc_procs.append(subprocess.Popen([args.vncviewer, f"127.0.0.1::{port}"]))
        time.sleep(2.5)
        dismissed = dismiss_encryption_dialog({p.pid for p in vnc_procs})
        if dismissed:
            log(f"{label}: dismissed {dismissed} encryption dialog(s)")
        time.sleep(3.5)
        if watcher is not None:
            match = viewer_window_rect({p.pid for p in vnc_procs}, port)
            fb = tuple(int(x) for x in args.size.lower().split("x"))
            if match is None or not watcher.locate_marker(match[0], fb):
                log(f"{label}: WARNING viewer window/marker not located; "
                    f"latency samples disabled")
        watcher.last_color = None

    seg_marks = []
    deadline = time.time() + seconds
    last_seq = -1
    while time.time() < deadline:
        for m in reader.marks:
            if m["seq"] > last_seq:
                last_seq = m["seq"]
                seg_marks.append(m)
                if watcher is not None and viewers:
                    watcher.submit_mark(m["seq"], m["state"], m["t_ms"])
        time.sleep(0.05)

    stop.set()
    for p in vnc_procs:
        p.terminate()
    try:
        proc.terminate()
        proc.wait(timeout=8)
    except subprocess.TimeoutExpired:
        proc.kill()
    time.sleep(1.0)

    res = {"label": label, "viewers": viewers, "seconds": seconds,
           "cpu_one_core_pct_avg": round(statistics.mean(
               [s["cpu_one_core_pct"] for s in samples]) if samples else 0.0, 2),
           "cpu_one_core_pct_p95": (round(sorted(s["cpu_one_core_pct"] for s in samples)[
               int(len(samples) * 0.95) - 1], 2) if len(samples) > 20 else None),
           "rss_mb_max": round(max((s["rss_mb"] for s in samples), default=0.0), 1),
           "marks_total": len(seg_marks),
           "probe_file": str(probe_file),
           "host_log": reader.lines[-25:]}
    if watcher is not None:
        det = [d["detect_ms"] for d in watcher.detected if d.get("detect_ms") is not None]
        res["viewer_detect_ms"] = {
            "n": len(det),
            "p50": round(statistics.median(det), 1) if det else None,
            "p95": (round(sorted(det)[int(len(det) * 0.95) - 1], 1) if len(det) >= 20 else
                    (max(det) if det else None)),
            "p99": (round(sorted(det)[int(len(det) * 0.99) - 1], 1) if len(det) >= 100 else
                    (max(det) if det else None)),
            "ambiguous": len(watcher.ambiguous),
            "pending_unclosed": len(watcher.pending),
        }
        res["viewer_detect_samples"] = watcher.detected
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dist-a", required=True)
    ap.add_argument("--dist-b", required=True)
    ap.add_argument("--active-dir", required=True)
    ap.add_argument("--fixture-exe", required=True)
    ap.add_argument("--qt-prefix", default="C:/Qt/6.8.3/msvc2022_64")
    ap.add_argument("--vncviewer", required=True)
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--scenario", default="motion")
    # 880x540 keeps the RealVNC Viewer window at a 1:1 pixel display on this
    # desktop (no resampling, so palette colours survive exactly) while still
    # exercising a full-page repaint every tick. The real user page (#567) is
    # larger; local sizing is bounded by the viewer's unscaled display.
    ap.add_argument("--size", default="880x540")
    ap.add_argument("--phase-ms", type=int, default=250)
    args = ap.parse_args()
    args.port = args.port or None
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    # one listener port for the whole run; the fixture picks a free one per
    # start only when --port is omitted, and RealVNC needs the same port
    # across segments of one arm, so allocate once here.
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()

    results = []
    partial = outdir / "partial-results.jsonl"
    plan = [("A-0v", "A", 0, 10), ("A-1v", "A", 1, 20), ("A-2v", "A", 2, 14),
            ("B-0v", "B", 0, 10), ("B-1v", "B", 1, 20), ("B-2v", "B", 2, 14),
            ("A-rollback-1v", "A", 1, 12)]
    dists = {"A": Path(args.dist_a), "B": Path(args.dist_b)}
    active_dirs = {"A": Path(args.active_dir + "-A"), "B": Path(args.active_dir + "-B")}
    current = None
    try:
        for label, arm, viewers, seconds in plan:
            if current != arm:
                log(f"installing dist-{arm} -> {active_dirs[arm]}")
                materialize(dists[arm], active_dirs[arm])
                current = arm
                time.sleep(0.5)
            r = run_segment(label, args, viewers, seconds, port,
                            capture_marks=True, active_dir=str(active_dirs[arm]))
            results.append(r)
            with partial.open("a", encoding="utf-8") as fh:
                fh.write(json.dumps(r) + "\n")
    finally:
        subprocess.run(["taskkill", "/IM", "vncviewer.exe", "/F"],
                       capture_output=True)
        subprocess.run(["taskkill", "/IM", "win949-frame-fixture.exe", "/F"],
                       capture_output=True)

    summary = {"source_sha_b": "8105f1a0067865672747e2b3d5728bb87b98e063",
               "source_sha_a": "b0680d5d",
               "scenario": args.scenario, "size": args.size,
               "phase_ms": args.phase_ms, "vnc": args.vncviewer,
               "segments": results}
    (outdir / "result.json").write_text(json.dumps(summary, indent=2),
                                        encoding="utf-8")
    log("segments:")
    for r in results:
        d = r.get("viewer_detect_ms", {})
        log(f"  {r['label']:>16}: cpu={r['cpu_one_core_pct_avg']}% "
            f"rss={r['rss_mb_max']}MB marks={r['marks_total']} "
            f"detect n={d.get('n')} p50={d.get('p50')} p95={d.get('p95')} "
            f"amb={d.get('ambiguous')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
