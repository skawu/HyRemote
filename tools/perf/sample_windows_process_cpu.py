#!/usr/bin/env python3
"""Measure real Windows process CPU as percent of ONE core, not whole-machine %.

Foreground, bounded, one case per invocation. Diagnostic evidence only; does
not infer how many RealVNC Viewers are connected or verify binary source SHA.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import math
import statistics
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def ns_filetime(obj):
    return (obj.dwLowDateTime + (obj.dwHighDateTime << 32)) * 100


def p95(values):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * 0.95) - 1)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True, help="Existing real Host or Viewer PID")
    p.add_argument("--case", required=True, choices=(
        "0v-idle", "1v-static", "1v-dynamic", "2v-static", "post-disconnect",
        "other"), help="Declarative phase label; tool cannot confirm Viewer count")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--duration", type=int, default=60, help="10..600 seconds (default 60)")
    p.add_argument("--interval", type=float, default=1.0, help="0.5..5 seconds")
    p.add_argument("--role", choices=("host", "viewer"), required=True)
    args = p.parse_args()
    require(sys.platform == "win32", "This process probe only runs on Windows")
    require(args.pid > 0 and 10 <= args.duration <= 600
            and 0.5 <= args.interval <= 5.0,
            "Invalid PID, duration or interval")

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    open_process = k32.OpenProcess
    open_process.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    open_process.restype = wintypes.HANDLE
    get_times = k32.GetProcessTimes
    get_times.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.FILETIME),
                          ctypes.POINTER(wintypes.FILETIME), ctypes.POINTER(wintypes.FILETIME),
                          ctypes.POINTER(wintypes.FILETIME)]
    get_times.restype = wintypes.BOOL
    wait = k32.WaitForSingleObject
    wait.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    wait.restype = wintypes.DWORD
    close = k32.CloseHandle
    close.argtypes = [wintypes.HANDLE]
    close.restype = wintypes.BOOL

    handle = open_process(0x1000 | 0x00100000, False, args.pid)  # QUERY_LIMITED | SYNCHRONIZE
    require(handle, f"Cannot open PID {args.pid} for process CPU timing")

    def read_cpu():
        require(wait(handle, 0) == 0x102, "Process ended during sampling")
        created, exited, kernel, user = (wintypes.FILETIME(), wintypes.FILETIME(),
                                          wintypes.FILETIME(), wintypes.FILETIME())
        require(get_times(handle, ctypes.byref(created), ctypes.byref(exited),
                          ctypes.byref(kernel), ctypes.byref(user)),
                "GetProcessTimes failed")
        return ns_filetime(kernel) + ns_filetime(user), ns_filetime(created)

    try:
        started_at = timestamp()
        start_cpu, created_stamp = read_cpu()
        start_ns = time.perf_counter_ns()
        old_ns, old_cpu = start_ns, start_cpu
        n = math.ceil(args.duration / args.interval)
        samples = []
        for i in range(n):
            time.sleep(args.interval)
            now_ns = time.perf_counter_ns()
            now_cpu, current_creation = read_cpu()
            require(current_creation == created_stamp, "Target PID/identity changed")
            delta_ns = now_ns - old_ns
            require(delta_ns > 0 and now_cpu >= old_cpu, "Nonmonotonic process timing")
            cpu = 100.0 * (now_cpu - old_cpu) / delta_ns
            require(math.isfinite(cpu) and cpu >= 0, "Invalid Windows process CPU")
            samples.append({
                "seconds": (now_ns - start_ns) / 1e9,
                "cpu_pct_one_core": cpu,
                "sample_wall_s": delta_ns / 1e9,
            })
            old_ns, old_cpu = now_ns, now_cpu
            if (i + 1) % max(1, math.ceil(5 / args.interval)) == 0:
                print(f"PROGRESS case={args.case} samples={i+1}/{n}", flush=True)
        values = [x["cpu_pct_one_core"] for x in samples]
        data = {
            "schema": "hyremote-windows-process-cpu-sampling/1",
            "process_pid": args.pid,
            "process_creation_filetime_ns": created_stamp,
            "role": args.role,
            "case": args.case,
            "phase_label_is_not_viewer_verification": True,
            "wall_cpu_clock": "QueryPerformanceCounter/Python perf_counter_ns",
            "process_cpu_clock": "GetProcessTimes(kernel+user)",
            "cpu_pct_one_core": "100 percent is one logical core equivalent",
            "started_utc": started_at,
            "duration_measured_s": (old_ns - start_ns) / 1e9,
            "samples": samples,
            "cpu_mean_pct_one_core": statistics.mean(values),
            "cpu_p95_pct_one_core": p95(values),
            "cpu_max_pct_one_core": max(values),
            "source_sha": "NOT_AUTOMATICALLY_VERIFIED",
            "viewer_count": "NOT_AUTOMATICALLY_VERIFIED",
            "actual_render_gpu_cost": "NOT_MEASURED",
        }
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8")
        print(f"PASS case={args.case} role={args.role}"
              f" cpu_mean={data['cpu_mean_pct_one_core']:.2f}% of one core"
              f" cpu_p95={data['cpu_p95_pct_one_core']:.2f}%"
              f" source=UNVERIFIED file={args.out}", flush=True)
    finally:
        close(handle)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as error:
        print(f"CPU_SAMPLE_INVALID={error}", file=sys.stderr)
        sys.exit(1)
