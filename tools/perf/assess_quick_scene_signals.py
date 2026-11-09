#!/usr/bin/env python3
"""Analyze passive Qt Quick render-loop signal *provenance* on real Windows.

The C++ fixture reports per-event timestamps in 1s bounded batches; merely
receiving afterAnimating/frameSwapped does NOT prove a user/QML scene change.
No capture is triggered by this tool. Three runs must be separate processes:
static without Viewer, static with Viewer, dynamic with Viewer.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
from pathlib import Path
import statistics
import sys

SIGNAL = "HYREMOTE_QUICK_SCENE_SIGNALS_JSON="
STAGE = "HYREMOTE_DYNAMIC_STAGE_JSON="


def require(value, message):
    if not value:
        raise RuntimeError(message)


def nearest_rank(values, percentage):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * percentage) - 1)]


def read_log(path):
    stages, reports = [], []
    content = path.read_text(encoding="utf-8", errors="replace")
    for row in content.splitlines():
        if SIGNAL in row:
            try:
                reports.append(json.loads(row.split(SIGNAL, 1)[1]))
            except json.JSONDecodeError as exc:
                raise RuntimeError(f"{path.name}: malformed signal event line") from exc
        if STAGE in row:
            try:
                stages.append(json.loads(row.split(STAGE, 1)[1]))
            except json.JSONDecodeError as exc:
                raise RuntimeError(f"{path.name}: malformed stage line") from exc
    require(len(reports) >= 10, f"{path.name}: expected at least 10 one-second signal reports")
    require(stages and all(v.get("phase") in (0, 1) for v in stages),
            f"{path.name}: no valid stage labels")
    require(all(a["utc_ms"] <= b["utc_ms"] for a, b in zip(reports, reports[1:])),
            f"{path.name}: timestamps out of order")
    return {"reports": reports, "stages": stages}


def summarize(record, expected_dynamic):
    stage = record["stages"]
    require(bool(len(stage) >= 8) == expected_dynamic,
            f"Expected {'dynamic' if expected_dynamic else 'static'} QML stage changes")
    require(all(a["phase"] != b["phase"] for a, b in zip(stage, stage[1:])),
            "Fixture stages did not alternate A and B")
    count_anim = [int(x["after_animating_1s"]) for x in record["reports"]]
    count_swap = [int(x["frame_swapped_1s"]) for x in record["reports"]]
    overflows = [
        int(x.get("after_animating_overflow", 0))
        + int(x.get("frame_swapped_overflow", 0))
        for x in record["reports"]
    ]
    require(not any(overflows),
            "Event ring overflow: cannot correlate stage and signal timestamps")
    events = sorted(
        int(value)
        for report in record["reports"]
        for value in report["after_animating_utc_ms"]
    )
    swaps = sorted(
        int(value)
        for report in record["reports"]
        for value in report["frame_swapped_utc_ms"]
    )
    require(sum(count_anim) == len(events)
            and sum(count_swap) == len(swaps),
            "Reported event totals disagree with per-event timestamps")

    latencies, missed = [], 0
    if expected_dynamic:
        for i, item in enumerate(stage):
            if i == 0:
                continue  # initial stage preceded the first animated frame
            start = int(item["utc_ms"])
            end = int(stage[i + 1]["utc_ms"]) if i + 1 < len(stage) else start + 1200
            index = bisect.bisect_left(events, start)
            if index == len(events) or events[index] >= end:
                missed += 1
            else:
                latencies.append(events[index] - start)

    return {
        "total_observed_seconds": len(record["reports"]),
        "stage_events": len(stage),
        "after_animating_per_sec_median": statistics.median(count_anim),
        "after_animating_per_sec_p95": nearest_rank(count_anim, .95),
        "after_animating_total": sum(count_anim),
        "frame_swapped_per_sec_median": statistics.median(count_swap),
        "frame_swapped_per_sec_p95": nearest_rank(count_swap, .95),
        "frame_swapped_total": sum(count_swap),
        "source_change_to_next_after_animating_ms_median":
            statistics.median(latencies) if latencies else None,
        "source_change_to_next_after_animating_ms_p95":
            nearest_rank(latencies, .95) if latencies else None,
        "source_changes_without_main_window_after_animating": missed if expected_dynamic else None,
        "source_changes_with_main_window_after_animating": len(latencies) if expected_dynamic else None,
        "signal_is_not_proven_independent_of_grab": True,
    }


def evaluate(no_viewer, one_viewer, dynamic):
    zero = summarize(no_viewer, False)
    static = summarize(one_viewer, False)
    changing = summarize(dynamic, True)
    return {
        "schema": "hyremote-qtquick-public-signal-provenance/1",
        "result": "INSTRUMENTED_NOT_A_PRODUCT_PASS",
        "static_zero_viewer": zero,
        "static_one_viewer": static,
        "dynamic_one_viewer": changing,
        "question_to_answer": "Do main QQuickWindow afterAnimating signals track actual scene changes rather than HyRemote's grabToImage probes?",
        "warning": (
            "A signal can be produced by Qt scene-graph operations unrelated to a"
            " source change. A high static-1V rate vs static-0V is a red flag for"
            " self-triggered capture loops. A low rate is only a candidate, not proof."
        ),
        "site_handoff": "BLOCKED",
        "independent_b_previous_result": "FAIL_NOT_APPROVED",
    }


def self_test():
    def records(dynamic):
        states = ([{"phase": 0, "utc_ms": 1000}]
                  if not dynamic else
                  [{"phase": i % 2, "utc_ms": 1000 + i * 1200} for i in range(11)])
        stamps = [int(x["utc_ms"]) + 30 for x in states[1:]]
        reports = []
        for i in range(16):
            start = 1000 + i * 1000
            subset = [x for x in stamps if start <= x < start + 1000]
            reports.append({
                "utc_ms": start + 999,
                "after_animating_1s": len(subset),
                "frame_swapped_1s": len(subset),
                "after_animating_utc_ms": subset,
                "frame_swapped_utc_ms": subset,
                "after_animating_overflow": 0,
                "frame_swapped_overflow": 0,
            })
        return {"reports": reports, "stages": states}
    output = evaluate(records(False), records(False), records(True))
    assert output["dynamic_one_viewer"]["source_change_to_next_after_animating_ms_p95"] == 30
    assert output["static_one_viewer"]["after_animating_total"] == 0
    print("PASS public render signal timestamp analyzer: static and A-B-A correlation")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--static-0v", type=Path)
    parser.add_argument("--static-1v", type=Path)
    parser.add_argument("--dynamic-1v", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
        else:
            require(all((args.static_0v, args.static_1v, args.dynamic_1v, args.out)),
                    "--static-0v --static-1v --dynamic-1v --out are all required")
            report = evaluate(
                read_log(args.static_0v), read_log(args.static_1v),
                read_log(args.dynamic_1v),
            )
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("QUICK_SIGNAL_PROVENANCE=" + report["result"])
            print("EVIDENCE=" + str(args.out))
    except (OSError, ValueError, RuntimeError) as exc:
        print("QUICK_SIGNAL_PROVENANCE=BLOCKED_OR_FAILED: " + str(exc), file=sys.stderr)
        sys.exit(1)
