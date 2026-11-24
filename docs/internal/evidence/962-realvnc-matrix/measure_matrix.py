"""Run a single fixed-build Windows RealVNC trial with 0, 1 or 2 Viewers."""

import argparse
import bisect
import csv
from collections import Counter
import ctypes as c
import hashlib
import json
import math
import os
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

import psutil

from measure import DesktopSample, qpc, read_lines, user32, viewer_points, w


def summary(samples):
    values = sorted(samples)
    return {"n": len(values), **{
        f"p{p}": values[math.ceil(len(values) * p / 100) - 1] if values else None
        for p in (50, 95, 99)
    }}


def sha256(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def drain(events, destination):
    while True:
        try:
            destination.append(events.get_nowait())
        except queue.Empty:
            return


def gpu_rows(path, pid):
    with path.open(encoding="utf-8-sig", errors="replace", newline="") as file:
        rows = csv.reader(file)
        header = next(rows, [])
        columns = [i for i, name in enumerate(header) if f"(pid_{pid}_" in name.lower()]
        samples = []
        for row in rows:
            values = {}
            for i in columns:
                if i >= len(row) or not row[i].strip():
                    continue
                try:
                    values[header[i].split("(", 1)[1].split(")", 1)[0]] = float(row[i])
                except ValueError:
                    continue
            if values:
                samples.append(values)
    return samples


def capture_trace(path, start_tick, end_tick, frequency):
    def monotonic_ns(tick):
        return tick * 1_000_000_000 // frequency

    all_steps = Counter()
    steady_steps = Counter()
    viewers = set()
    if path.exists():
        with path.open(encoding="utf-8") as file:
            for line in file:
                row = json.loads(line)
                if row.get("type") != "frame_step":
                    continue
                stage = row["step"]
                all_steps[stage] += 1
                if monotonic_ns(start_tick) <= row["at_ns"] <= monotonic_ns(end_tick):
                    steady_steps[stage] += 1
                    if stage == "rfb_socket_queued" and row["viewer_id"]:
                        viewers.add(row["viewer_id"])
    return {"all": all_steps, "steady": steady_steps, "active_viewers": len(viewers),
            "steady_source_completed_hz": (
                steady_steps["source_completed"] * frequency / (end_tick - start_tick)
                if end_tick > start_tick else None)}


def run(args):
    args.out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env.update({
        "PATH": os.pathsep.join([str(args.runtime.parent), str(args.qt_bin), env["PATH"]]),
        "QSG_RHI_BACKEND": "opengl", "QSG_RENDER_LOOP": "threaded",
        "QSG_INFO": "1", "QT_FORCE_STDERR_LOGGING": "1",
        "QT_SCALE_FACTOR": "1", "HYREMOTE_RESEARCH_MATRIX": "1",
        "HYREMOTE_RESEARCH_HOST_MS": str((args.seconds + 1) * 1000),
        "HYREMOTE_CPU_PROBE_FILE": str(args.out / "runtime-cpu.jsonl"),
        "HYREMOTE_FRAME_TRACE_FILE": str(args.out / "runtime-frame-trace.jsonl"),
    })
    env.pop("HYREMOTE_QUICK_ACTIVITY_WAKE", None)
    user32.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
    screen = DesktopSample() if args.viewers else None
    viewers = []
    viewer_logs = []
    gpu = None
    gpu_log = None
    host = None
    events = queue.Queue()
    lines = []
    host_samples = []
    input_posts = []
    samples = [[] for _ in range(args.viewers)]
    slots = [None] * args.viewers
    windows = [None] * args.viewers
    moved = [None] * args.viewers
    diagnostics = [None] * args.viewers
    changes = []
    with (args.out / "host.stderr.log").open("wb") as host_err:
        try:
            host = subprocess.Popen([args.host, str(args.port)], env=env, stdout=subprocess.PIPE,
                                    stderr=host_err, text=True, errors="replace")
            reader = threading.Thread(target=read_lines, args=(host, events), daemon=True)
            reader.start()
            proc = psutil.Process(host.pid)
            frequency = hwnd = None
            deadline = time.monotonic() + 12
            while not (frequency and hwnd):
                line = events.get(timeout=max(0.1, deadline - time.monotonic()))
                lines.append(line)
                if line.startswith("READY "):
                    frequency = int(line.split()[2])
                if line.startswith("HOST_HWND "):
                    hwnd = int(line.split()[1])
            for index in range(args.viewers):
                log = (args.out / f"viewer-{index + 1}.log").open("wb")
                viewer_logs.append(log)
                viewers.append(subprocess.Popen(
                    [args.viewer, "-Shared=true", "-SecurityNotificationTimeout=0",
                     "-WarnUnencrypted=0", "-EnableUdpRfb=false",
                     f"127.0.0.1::{args.port}"],
                    stdout=log, stderr=subprocess.STDOUT,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                ))
            started = time.monotonic()
            next_query = next_sample = started
            next_input = started + 5.35
            while time.monotonic() < started + args.seconds and host.poll() is None:
                now = time.monotonic()
                if gpu is None and now >= started + 4:
                    gpu_log = (args.out / "gpu-counters.csv").open("wb")
                    gpu = subprocess.Popen(
                        ["typeperf", r"\GPU Engine(*)\Utilization Percentage", "-si", "1",
                         "-sc", str(args.seconds - 3)], stdout=gpu_log, stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                    )
                if now >= next_sample:
                    cpu = proc.cpu_times()
                    host_samples.append([qpc(), cpu.user + cpu.system, proc.memory_info().rss])
                    next_sample = now + 0.5
                if now >= next_input:
                    tick = qpc()
                    if user32.PostMessageW(hwnd, 0x0100, 0x87, 1):  # WM_KEYDOWN / VK_F24
                        input_posts.append(tick)
                        user32.PostMessageW(hwnd, 0x0101, 0x87, 0xC0000001)
                    next_input += 1
                if now >= next_query:
                    for i, viewer in enumerate(viewers):
                        info = viewer_points(viewer.pid)
                        diagnostics[i] = [title for _, title, _ in info]
                        windows[i] = None
                        for handle, title, point in info:
                            if not point or f"127.0.0.1::{args.port}" not in title:
                                continue
                            if handle != moved[i]:
                                user32.ShowWindow(handle, 9)
                                if user32.SetWindowPos(handle, w.HWND(-1), 470 + i * 420,
                                                       70, 0, 0, 0x0011):
                                    moved[i] = handle
                            else:
                                windows[i] = point
                            break
                    next_query = now + 0.25
                new_lines = []
                drain(events, new_lines)
                lines.extend(new_lines)
                for line in new_lines:
                    if not line.startswith("CHANGE "):
                        continue
                    _, seq, tick, color = line.split()
                    change = (int(seq), int(tick), int(color[1:], 16))
                    changes.append(change)
                    slots[:] = [change] * args.viewers
                for i, (point, pending) in enumerate(zip(windows, slots)):
                    if point and pending and screen.matches(point, pending[2]):
                        found = qpc()
                        samples[i].append([pending[0], found, (found - pending[1]) * 1000 / frequency])
                        slots[i] = None
                time.sleep(0.003)
            host.wait(timeout=5)
            reader.join(timeout=2)
            drain(events, lines)
            if gpu is not None:
                try:
                    gpu.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        finally:
            for proc in [host, *viewers, gpu]:
                if proc is not None and proc.poll() is None:
                    proc.terminate()
                    proc.wait(timeout=5)
            for log in viewer_logs:
                log.close()
            if gpu_log:
                gpu_log.close()
            if screen:
                screen.close()
            (args.out / "host.stdout.log").write_text("\n".join(lines) + "\n", encoding="utf-8")
    # Drop startup and the final partially observed transition. Raw rows stay available.
    steady = {seq for seq, _, _ in changes[5:-1]}
    swaps = sorted(int(line.split()[1]) for line in lines if line.startswith("FRAME_SWAPPED "))
    input_receipts = [int(line.split()[1]) for line in lines if line.startswith("LOCAL_INPUT ")]
    def duration(ticks):
        return ticks * 1000 / frequency
    local_swap = []
    for seq, tick, _ in changes:
        if seq in steady and (i := bisect.bisect_left(swaps, tick)) < len(swaps):
            local_swap.append(duration(swaps[i] - tick))
    local_input = []
    for j, tick in enumerate(input_posts):
        i = bisect.bisect_left(input_receipts, tick)
        if i < len(input_receipts) and (j + 1 == len(input_posts)
                                        or input_receipts[i] < input_posts[j + 1]):
            local_input.append(duration(input_receipts[i] - tick))
    frame_gaps = [duration(end - start) for start, end in zip(swaps, swaps[1:])]
    resource_rows = [row for row in host_samples if row[0] >= host_samples[0][0] + 5 * frequency]
    cpu = None
    if len(resource_rows) > 1:
        a, b = resource_rows[0], resource_rows[-1]
        cpu = 100 * (b[1] - a[1]) * frequency / (b[0] - a[0])
    gpu_file = args.out / "gpu-counters.csv"
    gpu_samples = gpu_rows(gpu_file, host.pid) if gpu_file.exists() else []
    first_tick, last_tick = (resource_rows[0][0], resource_rows[-1][0]) if resource_rows else (0, 0)
    stages = capture_trace(args.out / "runtime-frame-trace.jsonl",
                           first_tick, last_tick, frequency)
    return {
        "source_sha": args.source_sha, "arm": args.arm,
        "hashes": {p.name: sha256(p) for p in (args.host, args.runtime, args.viewer)},
        "physical_present": "NOT_MEASURED",
        "host_pid": host.pid, "host_exit": host.returncode, "stop_ok": "STOPPED 1" in lines,
        "changes": changes, "viewer_window_titles": diagnostics,
        "viewer_samples": samples, "steady_sequences": sorted(steady),
        "viewer_detect_ms": [summary([v for seq, _, v in rows if seq in steady]) for rows in samples],
        "viewer_missed_sequences": [sorted(steady - {seq for seq, _, _ in rows}) for rows in samples],
        "host_cpu_pct_one_core": cpu, "host_rss_bytes": summary([row[2] for row in resource_rows]),
        "host_resource_rows": host_samples, "local_frame_swaps": len(swaps),
        "local_frame_gap_ms": summary(frame_gaps), "local_marker_to_next_swap_ms": summary(local_swap),
        "local_wm_key_to_qt_ms": summary(local_input), "local_input_posts": input_posts,
        "local_input_receipts": input_receipts, "host_gpu_engine_pct_rows": gpu_samples,
        "host_busiest_gpu_engine_pct": summary([max(row.values()) for row in gpu_samples[1:]]),
        "host_gpu_counter_available": bool(gpu_samples),
        "capture_trace": stages,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("host", "runtime", "qt-bin", "viewer", "out"):
        parser.add_argument("--" + name, required=True, type=lambda p: Path(p).resolve())
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--arm", required=True, choices=("A1", "B", "A2"))
    parser.add_argument("--viewers", required=True, type=int, choices=(0, 1, 2))
    parser.add_argument("--port", type=int, default=15921)
    parser.add_argument("--seconds", type=int, default=35)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.source_sha) or args.seconds < 8:
        parser.error("Supply a 40-digit Git SHA and at least eight seconds.")
    result = run(args)
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: result[k] for k in (
        "host_exit", "stop_ok", "viewer_detect_ms", "host_cpu_pct_one_core",
        "local_marker_to_next_swap_ms", "local_wm_key_to_qt_ms",
    )}))
    trace = result["capture_trace"]
    route = "gpu_readback_submitted" if args.arm == "B" else "quick_grab_submitted"
    unexpected = "quick_grab_submitted" if args.arm == "B" else "gpu_readback_submitted"
    return 0 if (result["stop_ok"] and result["host_exit"] == 0
                 and trace["all"].get(route, 0) > 0 and not trace["all"].get(unexpected)
                 and trace["active_viewers"] == args.viewers
                 and all(row["n"] for row in result["viewer_detect_ms"])) else 4


if __name__ == "__main__":
    raise SystemExit(main())
