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


def match_current_visible_transition(events, phase, measured_at_ms):
    """Match only a newly VISIBLE transition while its source phase is active.

    A viewer remaining on A across an unseen B and a later source A must
    never be counted as having displayed that later A. The caller enforces
    a real change from the last known visible color before calling this.
    """
    for index, event in enumerate(events):
        if (event["measured_viewer_ms"] is not None
                or event["phase"] != phase
                or event["utc_ms"] > measured_at_ms):
            continue
        # A late stale B after source has already changed to A is NOT a
        # successful Viewer rendering of B for this transition window.
        if (index + 1 < len(events)
                and measured_at_ms >= events[index + 1]["utc_ms"]):
            continue
        event["measured_viewer_ms"] = measured_at_ms
        return True
    return False


def reconcile_pending_visible_transitions(events, pending):
    """Attribute real screen transitions even when stdout arrives after pixel read.

    Each transition was observed as a genuine visible stage CHANGE, not merely
    the expected color. A stage event can be appended to the Host stdout log
    between the observer's first log read and its 4-point GDI read. Do not
    irrevocably classify this as missed merely because that event was absent
    from the first read. Reconcile when a later log poll sees the event, while
    preserving the strict source-epoch validity check.
    """
    for transition in pending:
        if transition["matched"]:
            continue
        if match_current_visible_transition(
                events, transition["phase"], transition["visible_utc_ms"]):
            transition["matched"] = True


def self_test_transition_oracle():
    # A Viewer stuck on blue A may not claim a second A event after skipping B.
    sequence = [
        {"phase": 1, "utc_ms": 1200, "measured_viewer_ms": None},
        {"phase": 0, "utc_ms": 2400, "measured_viewer_ms": None},
    ]
    visible = 0
    for measured_at_ms, phase in ((1300, 0), (2500, 0)):
        if phase != visible:
            match_current_visible_transition(sequence, phase, measured_at_ms)
            visible = phase
    require(all(e["measured_viewer_ms"] is None for e in sequence),
            "False-positive: stale A counted as an actual Viewer transition")

    # Normal A->B->A as observed by screen pixels is two distinct matches.
    real = [
        {"phase": 1, "utc_ms": 1200, "measured_viewer_ms": None},
        {"phase": 0, "utc_ms": 2400, "measured_viewer_ms": None},
    ]
    require(match_current_visible_transition(real, 1, 1320),
            "B transition should match the B source event")
    require(match_current_visible_transition(real, 0, 2540),
            "A transition should match the A source event")
    require([e["measured_viewer_ms"] for e in real] == [1320, 2540],
            "Correct A-B-A latency stamps were lost")

    # A late B response after the next A source event started is a stale
    # frame observation, not an accepted B transition.
    stale = [
        {"phase": 1, "utc_ms": 1200, "measured_viewer_ms": None},
        {"phase": 0, "utc_ms": 2400, "measured_viewer_ms": None},
    ]
    require(not match_current_visible_transition(stale, 1, 2500),
            "False-positive: stale B counted after newer A source event")
    # Host writes a source transition during the 4 sequential GetPixel
    # reads, AFTER the first stage-log read. The visible color transition is
    # real even though no matching event was in the in-memory list yet.
    # The next log poll must backfill it without declaring missed.
    late_log = []
    observed_transitions = [{
        "phase": 1, "visible_utc_ms": 1370, "matched": False,
    }]
    reconcile_pending_visible_transitions(late_log, observed_transitions)
    require(not observed_transitions[0]["matched"],
            "Unseen source event falsely matched before log delivery")
    late_log.append({
        "phase": 1, "utc_ms": 1300, "measured_viewer_ms": None,
    })
    reconcile_pending_visible_transitions(late_log, observed_transitions)
    require(observed_transitions[0]["matched"]
            and late_log[0]["measured_viewer_ms"] == 1370,
            "Host stage appended during GetPixel was incorrectly lost")

    # A transition to B observed after source returned to A is too late
    # to be attributed; a delayed log must never override the epoch guard.
    expired_log = [
        {"phase": 1, "utc_ms": 1200, "measured_viewer_ms": None},
        {"phase": 0, "utc_ms": 2400, "measured_viewer_ms": None},
    ]
    late_b = [{"phase": 1, "visible_utc_ms": 2500, "matched": False}]
    reconcile_pending_visible_transitions(expired_log, late_b)
    require(not late_b[0]["matched"]
            and expired_log[0]["measured_viewer_ms"] is None,
            "Late stale B was falsely certified after newer A source")

    # The originally visible A cannot match a later A event when B was
    # never actually displayed between them.
    skipped = [
        {"phase": 1, "utc_ms": 1200, "measured_viewer_ms": None},
        {"phase": 0, "utc_ms": 2400, "measured_viewer_ms": None},
    ]
    no_changes = []
    reconcile_pending_visible_transitions(skipped, no_changes)
    require(all(e["measured_viewer_ms"] is None for e in skipped),
            "Skipped B was falsely matched through an already-visible A")

    print("PASS viewer transition oracle: skipped-B, A-B-A, stale-B, log-read/viewport-read race")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="Run deterministic transition false-positive tests; no Viewer needed")
    parser.add_argument("--viewer-pid", type=int,
                        help="PID that OWNS the displayed remote Viewer window, not Host")
    parser.add_argument("--rect",
                        help="Unobscured real Viewer canvas in physical desktop pixels: x,y,width,height")
    parser.add_argument("--stage-log", type=Path,
                        help="Captured fixture C++ stdout including HYREMOTE_DYNAMIC_STAGE_JSON")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--seconds", type=int, default=90,
                        help="10..180, 90s default for >50 full A-B-A changes")
    parser.add_argument("--interval-ms", type=int, default=50,
                        help="20..200, sampling cadence, not time resolution proof")
    parser.add_argument("--rgb-tolerance", type=int, default=22,
                        help="0..50: conservative per-channel Viewer color tolerance")
    args = parser.parse_args()
    if args.self_test:
        self_test_transition_oracle()
        return
    require(args.viewer_pid is not None and args.rect is not None
            and args.stage_log is not None and args.out is not None,
            "Real Viewer observation requires --viewer-pid, --rect, --stage-log, --out")
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
        # Four DWM/GDI pixels are read sequentially, not atomically.
        # Two colors here alone do NOT prove one simultaneously torn frame.
        pixel_read_start_ns = time.perf_counter_ns()
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
        pixel_read_ms = (time.perf_counter_ns() - pixel_read_start_ns) / 1_000_000.0
        if len(set(values)) == 1:
            return values[0], False, pixel_read_ms
        return -1, 0 in values and 1 in values, pixel_read_ms

    first = read_stage_events()
    require(first and first[0]["phase"] in COLORS,
            "No readable C++ source-stage events: start/redirect the fixture Host before Observer")

    started = time.monotonic()
    prior_events = len(first)
    last_visible_stage = None
    sample_intervals_ms = []
    pixel_read_spans_ms = []
    last_sample_start_ns = None
    mixed_samples = 0
    unknown_samples = 0
    samples = 0
    a_samples = 0
    b_samples = 0
    consecutive_mixed = 0
    max_consecutive_mixed = 0
    # Ignore any stage before observer began: it has no meaningful screen
    # latency baseline. Only count stage changes observed after we started.
    outstanding = []
    # This is a timeline of actual *visible* color changes, not what the
    # application was supposed to draw. It is retained solely as colors/times
    # to diagnose large missing-stage counts without screenshots or pixels.
    visible_transitions = []
    try:
        while time.monotonic() - started < args.seconds:
            sample_start_ns = time.perf_counter_ns()
            if last_sample_start_ns is not None:
                sample_intervals_ms.append(
                    (sample_start_ns - last_sample_start_ns) / 1_000_000.0)
            last_sample_start_ns = sample_start_ns
            newest = read_stage_events()
            require(len(newest) >= prior_events, "Qt stage log shrank or rotated during observation")
            for event in newest[prior_events:]:
                outstanding.append({"phase": event["phase"], "utc_ms": event["utc_ms"],
                                    "measured_viewer_ms": None})
            prior_events = len(newest)
            stage, mixed, pixel_read_ms = observation()
            pixel_read_spans_ms.append(pixel_read_ms)
            # Time after observing the actual screen pixels; never stamp
            # before the expensive four-point DWM/GDI read.
            now_ms = time.time_ns() // 1_000_000
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
            # Read the Host stage log *again* AFTER GetPixel. It may have
            # gained a source event between the first read and the 4-point
            # sequential screen read. Earlier versions irrevocably marked a
            # real color transition without a source event and could then
            # miscount that transition as missing forever.
            end_events = read_stage_events()
            require(len(end_events) >= prior_events,
                    "Qt stage log shrank during post-viewport reconciliation")
            for event in end_events[prior_events:]:
                outstanding.append({"phase": event["phase"], "utc_ms": event["utc_ms"],
                                    "measured_viewer_ms": None})
            prior_events = len(end_events)
            # A source event counts only when actual screen pixels CHANGE.
            if stage in COLORS:
                if last_visible_stage is None:
                    last_visible_stage = stage  # initial baseline
                elif stage != last_visible_stage:
                    visible_transitions.append({
                        "phase": stage, "visible_utc_ms": now_ms,
                        "matched": False,
                    })
                    last_visible_stage = stage
            reconcile_pending_visible_transitions(outstanding, visible_transitions)
            if samples % max(1, round(5000 / args.interval_ms)) == 0:
                print(f"PROGRESS viewer_pid={args.viewer_pid} "
                      f"samples={samples} events={len(outstanding)}", flush=True)
            time.sleep(args.interval_ms / 1000.0)
    finally:
        u32.ReleaseDC(None, hdc)

    # Flush final Host stdout before scoring; no second-screen read is
    # invented. Filter any new source events after the final viewport read.
    final_events = read_stage_events()
    if visible_transitions:
        latest_view_utc_ms = visible_transitions[-1]["visible_utc_ms"]
    else:
        latest_view_utc_ms = time.time_ns() // 1_000_000
    require(len(final_events) >= prior_events,
            "Qt stage log shrank after final visible observation")
    for event in final_events[prior_events:]:
        if event["utc_ms"] <= latest_view_utc_ms:
            outstanding.append({"phase": event["phase"], "utc_ms": event["utc_ms"],
                                "measured_viewer_ms": None})
    reconcile_pending_visible_transitions(outstanding, visible_transitions)

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
        "actual_sample_start_interval_p50_ms":
            statistics.median(sample_intervals_ms) if sample_intervals_ms else None,
        "actual_sample_start_interval_p95_ms":
            percentile(sample_intervals_ms, 0.95) if sample_intervals_ms else None,
        "actual_sample_start_interval_max_ms":
            max(sample_intervals_ms) if sample_intervals_ms else None,
        "actual_sample_start_interval_count": len(sample_intervals_ms),
        "sequential_four_pixel_read_p50_ms":
            statistics.median(pixel_read_spans_ms) if pixel_read_spans_ms else None,
        "sequential_four_pixel_read_p95_ms":
            percentile(pixel_read_spans_ms, 0.95) if pixel_read_spans_ms else None,
        "sequential_four_pixel_read_max_ms":
            max(pixel_read_spans_ms) if pixel_read_spans_ms else None,
        "duration_seconds": args.seconds,
        "source_change_timestamp": "Qt C++ QDateTime::currentMSecsSinceEpoch / wall UTC ms (not GPU render)",
        "viewer_visible_timestamp": "Python local Windows wall clock at sample",
        "timing_limitations": "wall-clock, measured actual polling cadence, and sequential pixel reads; mixed corners could straddle a normal repaint and do not prove a simultaneously torn GPU frame",
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
        "source_event_timeline": [
            {
                "phase": event["phase"],
                "source_utc_ms": event["utc_ms"],
                "viewer_visible_utc_ms": event["measured_viewer_ms"],
                "matched_visible_transition": event["measured_viewer_ms"] is not None,
            }
            for event in outstanding
        ],
        "visible_transition_timeline": visible_transitions,
        "visible_transition_count": len(visible_transitions),
        "visible_transition_unmatched_source_count": sum(
            1 for t in visible_transitions if not t["matched"]),
        "diagnostic_scope": "Only synthetic full-color scene change/Viewer screen samples; "
                            "no RFB packets or actual Qt capture phase timestamps",
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
    print("REAL_VIEWER_ACTUAL_SAMPLE_INTERVAL_P95_MS=" +
          str(data["actual_sample_start_interval_p95_ms"]), flush=True)
    print("REAL_VIEWER_SEQUENTIAL_PIXEL_READ_P95_MS=" +
          str(data["sequential_four_pixel_read_p95_ms"]), flush=True)
    print("REAL_VIEWER_EVIDENCE=" + str(args.out), flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print("WINDOWS_VIEWER_PIXEL_OBSERVATION=BLOCKED_OR_FAILED", file=sys.stderr)
        print("REASON=" + str(error), file=sys.stderr)
        sys.exit(1)
