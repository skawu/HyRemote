#!/usr/bin/env python3
"""Observe actual Windows Viewer *screen* pixels for 1080p A-B-A test app.

A test-only measurement tool, not a synthetic RFB decoder or a GPU timer.
No screenshots/content are stored. Requires an unobscured physical Viewer
viewport rectangle and its actual window-owning PID; refuses to measure the
Host window or an unrelated foreground application as Viewer output.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import math
from pathlib import Path
import statistics
import sys
import time
from datetime import datetime, timezone

COLORS = {0: (0x17, 0x3c, 0xdb), 1: (0xe8, 0x26, 0x3b)}
MARKER = "HYREMOTE_DYNAMIC_STAGE_JSON="


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def percentile(values, p):
    return sorted(values)[max(0, math.ceil(len(values) * p) - 1)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer-pid", required=True, type=int,
                        help="PID that OWNS the displayed remote Viewer window, not Host")
    parser.add_argument("--rect", required=True,
                        help="Unobscured real Viewer canvas in physical desktop pixels: x,y,width,height")
    parser.add_argument("--stage-log", required=True, type=Path,
                        help="Captured fixture C++ stdout including HYREMOTE_DYNAMIC_STAGE_JSON")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--seconds", type=int, default=90,
                        help="10..180, 90s default for >50 full A-B-A changes")
    parser.add_argument("--interval-ms", type=int, default=50,
                        help="20..200, sampling cadence, not time resolution proof")
    parser.add_argument("--rgb-tolerance", type=int, default=22,
                        help="0..50: conservative per-channel Viewer color tolerance")
    args = parser.parse_args()

    require(sys.platform == "win32", "Real Windows visible Viewer is required")
    require(args.viewer_pid > 0 and 10 <= args.seconds <= 180
            and 20 <= args.interval_ms <= 200
            and 0 <= args.rgb_tolerance <= 50,
            "Bad PID, duration, interval or color tolerance")

    try:
        x, y, width, height = [int(v) for v in args.rect.split(",")]
    except ValueError as e:
        raise RuntimeError("Expected --rect x,y,width,height") from e

    u32 = ctypes.WinDLL("user32", use_last_error=True)
    gdi = ctypes.WinDLL("gdi32", use_last_error=True)
    u32.GetSystemMetrics.argtypes = [ctypes.c_int]
    u32.GetSystemMetrics.restype = ctypes.c_int
    screen_w, screen_h = u32.GetSystemMetrics(0), u32.GetSystemMetrics(1)
    require(width >= 320 and height >= 240 and x >= 0 and y >= 0
            and x + width <= screen_w and y + height <= screen_h,
            "Viewer canvas rectangle must fit physical Windows display")
    require(args.stage_log.is_file(), "Qt dynamic A/B/A stage log missing")

    # High/low quadrants, deliberately away from the center label overlay.
    points = [(x + int(width * xf), y + int(height * yf))
              for xf, yf in ((0.20, 0.20), (0.80, 0.20), (0.20, 0.80), (0.80, 0.80))]
    u32.WindowFromPoint.argtypes = [wintypes.POINT]
    u32.WindowFromPoint.restype = wintypes.HWND
    u32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    u32.GetWindowThreadProcessId.restype = wintypes.DWORD
    u32.GetDC.argtypes = [wintypes.HWND]
    u32.GetDC.restype = wintypes.HDC
    u32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
    u32.ReleaseDC.restype = ctypes.c_int
    gdi.GetPixel.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
    gdi.GetPixel.restype = wintypes.COLORREF
    hdc = u32.GetDC(None)
    require(hdc, "GetDC(NULL) failed: no physical display available")

    def read_stage_events():
        contents = args.stage_log.read_text(encoding="utf-8", errors="replace")
        result = []
        for line in contents.splitlines():
            if MARKER not in line:
                continue
            value = line.split(MARKER, 1)[1]
            try:
                event = json.loads(value)
            except json.JSONDecodeError:
                continue  # Incomplete appended output; next sample will read again.
            if (event.get("phase") not in COLORS
                    or not isinstance(event.get("utc_ms"), (int, float))):
                continue
            result.append({"phase": event["phase"], "utc_ms": int(event["utc_ms"])})
        return result

    def observation():
        # Verify EVERY sample point belongs to the supplied Viewer PID.
        # A foreground Host frame matching the test colors is NOT Viewer proof.
        for px, py in points:
            owner = wintypes.DWORD()
            hwnd = u32.WindowFromPoint(wintypes.POINT(px, py))
            require(hwnd and u32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner)),
                    "No window at selected Viewer viewport pixel")
            require(owner.value == args.viewer_pid,
                    f"Viewer viewport pixel ({px},{py}) belongs to PID {owner.value}, "
                    f"not requested Viewer PID {args.viewer_pid}")
        values = []
        for px, py in points:
            c = gdi.GetPixel(hdc, px, py)
            require(c != 0xFFFFFFFF, "GetPixel failed: Viewer not visibly composited")
            rgb = (c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff)
            stage = next(
                (stage for stage, expected in COLORS.items()
                 if all(abs(a - b) <= args.rgb_tolerance for a, b in zip(rgb, expected))),
                -1,
            )
            values.append(stage)
        if len(set(values)) == 1:
            return values[0], False
        return -1, 0 in values and 1 in values

    first = read_stage_events()
    require(first and first[0]["phase"] in COLORS,
            "No readable QML stage events: start/redirect the fixture Host before Observer")

    started = time.monotonic()
    events = first
    seen_event_count = len(events)
    prior_events = len(events)
    transitions = []
    mixed_samples = 0
    unknown_samples = 0
    samples = 0
    a_samples = 0
    b_samples = 0
    consecutive_mixed = 0
    max_consecutive_mixed = 0
    last_matched_event_idx = max(0, seen_event_count - 1)
    # Ignore any stage before observer began: it has no meaningful screen
    # latency baseline. Only count stage changes observed after we started.
    outstanding = []
    try:
        while time.monotonic() - started < args.seconds:
            now_ms = time.time_ns() // 1_000_000
            newest = read_stage_events()
            require(len(newest) >= prior_events, "Qt stage log shrank or rotated during observation")
            for event in newest[prior_events:]:
                outstanding.append({"phase": event["phase"], "utc_ms": event["utc_ms"],
                                    "measured_viewer_ms": None})
            prior_events = len(newest)
            stage, mixed = observation()
            samples += 1
            if mixed:
                mixed_samples += 1
                consecutive_mixed += 1
                max_consecutive_mixed = max(max_consecutive_mixed, consecutive_mixed)
            else:
                consecutive_mixed = 0
            if stage < 0:
                unknown_samples += 1
            elif stage == 0:
                a_samples += 1
            else:
                b_samples += 1
            for event in outstanding:
                if event["measured_viewer_ms"] is not None:
                    continue
                if event["utc_ms"] > now_ms + 5:
                    continue
                if stage == event["phase"] and now_ms >= event["utc_ms"]:
                    event["measured_viewer_ms"] = now_ms
                    break
            if samples % max(1, round(5000 / args.interval_ms)) == 0:
                print(f"PROGRESS viewer_pid={args.viewer_pid} "
                      f"samples={samples} events={len(outstanding)}", flush=True)
            time.sleep(args.interval_ms / 1000.0)
    finally:
        u32.ReleaseDC(None, hdc)

    # A stage missed entirely by the Viewer cannot be assigned the latency of
    # the next same-colored stage. A→B→A changes that were skipped = dropped.
    latencies = []
    missed = 0
    for i, event in enumerate(outstanding):
        measured = event["measured_viewer_ms"]
        if measured is None:
            missed += 1
            continue
        next_t = outstanding[i + 1]["utc_ms"] if i + 1 < len(outstanding) else None
        if next_t is not None and measured >= next_t:
            missed += 1
            continue
        latency = measured - event["utc_ms"]
        if latency < 0 or latency > args.seconds * 1000:
            missed += 1
            continue
        latencies.append(latency)

    data = {
        "schema": "hyremote-windows-real-viewer-pixel-observation/1",
        "source": "real physical Windows desktop GetPixel, not RFB wire",
        "viewer_pid": args.viewer_pid,
        "viewport_rect": [x, y, width, height],
        "sampling_interval_ms": args.interval_ms,
        "duration_seconds": args.seconds,
        "source_change_timestamp": "Qt C++ QDateTime::currentMSecsSinceEpoch / wall UTC ms (not GPU render)",
        "viewer_visible_timestamp": "Python local Windows wall clock at sample",
        "timing_limitations": "wall-clock and finite pixel polling; not per-frame GPU timing",
        "observed_event_count": len(outstanding),
        "matched_events": len(latencies),
        "missed_events": missed,
        "viewer_change_to_visible_p95_ms": percentile(latencies, 0.95) if latencies else None,
        "viewer_change_to_visible_median_ms": statistics.median(latencies) if latencies else None,
        "screen_samples": samples,
        "a_samples": a_samples,
        "b_samples": b_samples,
        "unknown_samples": unknown_samples,
        "mixed_A_and_B_samples": mixed_samples,
        "max_consecutive_mixed_samples": max_consecutive_mixed,
        "color_match_tolerance_per_channel": args.rgb_tolerance,
        "actual_viewer_window_ownership_verified": True,
        "raw_event_latency_ms": latencies,
        "result": "MEASURED_WITH_MISSING_OR_MIXED"
                  if missed or mixed_samples or not latencies else "MEASURED_NO_MISSING_OR_MIXED",
        "sampled_at_utc": datetime.now(timezone.utc).isoformat(),
        "viewer_cpu": "NOT_MEASURED_BY_THIS_TOOL",
        "source_sha": "MUST_BE_RECORDED_FROM_ARTIFACT_MANIFEST_SEPARATELY",
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(data, ensure_ascii=True, indent=2) + "\n",
                        encoding="utf-8")
    require(samples >= 10 and a_samples and b_samples and len(outstanding) >= 5,
            "INSUFFICIENT: expected both A and B stages and >=5 timed transitions")
    print("REAL_VIEWER_PIXELS=" + data["result"], flush=True)
    print("REAL_VIEWER_P95_MS=" + str(data["viewer_change_to_visible_p95_ms"]), flush=True)
    print("REAL_VIEWER_MIXED_SAMPLES=" + str(mixed_samples), flush=True)
    print("REAL_VIEWER_EVIDENCE=" + str(args.out), flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print("WINDOWS_VIEWER_PIXEL_OBSERVATION=BLOCKED_OR_FAILED", file=sys.stderr)
        print("REASON=" + str(error), file=sys.stderr)
        sys.exit(1)
