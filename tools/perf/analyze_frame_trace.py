#!/usr/bin/env python3
"""Analyze optional HyRemote host frame-step telemetry (schema=3 JSONL).

Input: HYREMOTE_CPU_PROBE_FILE with HYREMOTE_FRAME_TRACE=1 and
HYREMOTE_ENABLE_CPU_PROBE=ON. Times are monotonic HOST timestamps.
The RFB socket-queued event is NOT a network ACK, Viewer decode, or present.
A QQuickWindow afterAnimating event is a HINT, not content change.
Never match request IDs across overlapping sessions/restarts in one process.
"""
import argparse
import bisect
import collections
import json
import math
from pathlib import Path

PAIRS = (
    ("request_to_source", "capture_request", "source_completed"),
    ("grab_submit_to_source", "quick_grab_submitted", "source_completed"),
    ("gpu_submit_to_ready", "gpu_readback_submitted", "gpu_readback_ready"),
    ("source_to_core_dispatch", "source_completed", "core_frame_queued"),
    ("core_to_rfb_worker", "core_frame_queued", "rfb_observed"),
    ("rfb_compare_to_encode", "rfb_compared", "rfb_encoded"),
    ("rfb_encode_to_socket_queue", "rfb_encoded", "rfb_socket_queued"),
    ("capture_request_to_socket_queue", "capture_request", "rfb_socket_queued"),
)


def percentile(values, p):
    ordered = sorted(values)
    x = (len(ordered) - 1) * p / 100.0
    lo, hi = math.floor(x), math.ceil(x)
    return ordered[lo] * (hi - x) + ordered[hi] * (x - lo) if lo != hi else ordered[lo]


def describe(values):
    return {
        "n": len(values),
        "p50_ms": round(percentile(values, 50), 3) if values else None,
        "p95_ms": round(percentile(values, 95), 3) if values else None,
        "p99_ms": round(percentile(values, 99), 3) if values else None,
    }


def analyze(records):
    requests = collections.defaultdict(lambda: collections.defaultdict(list))
    activity = collections.defaultdict(list)
    totals = collections.Counter()
    overflow = 0
    for entry in records:
        if entry.get("schema") != 3:
            continue  # schema=2 aggregate CPU snapshots coexist in same file
        if entry.get("type") == "frame_trace_overflow":
            overflow += max(0, int(entry.get("dropped", 0)))
            continue
        if entry.get("type") != "frame_step":
            continue
        step = entry.get("step")
        pid = int(entry.get("pid", 0))
        request_id = int(entry.get("request_id", 0))
        at_ns = int(entry.get("at_ns", 0))
        if not step or pid <= 0 or at_ns <= 0:
            continue
        totals[step] += 1
        if step == "qt_activity_hint":
            activity[pid].append(at_ns)
        elif request_id > 0:
            requests[pid, request_id][step].append(entry)

    out = collections.defaultdict(list)
    ambiguous = 0
    impossible = 0
    for (pid, _req), steps in requests.items():
        # One request identity cannot be reused by simultaneous sessions.
        # Flag duplicates rather than silently attributing new frames to old.
        if len(steps["capture_request"]) != 1:
            ambiguous += 1
            continue
        for metric, earlier, later in PAIRS:
            starts = steps[earlier]
            ends = steps[later]
            if not starts or not ends:
                continue
            first = min(int(x["at_ns"]) for x in starts)
            per_viewer = {int(x.get("viewer_id", 0)): int(x["at_ns"])
                          for x in starts} if earlier == "rfb_encoded" else {}
            for event in ends:
                # One frame may be encoded independently for 2+ viewers.
                # Do not combine one viewer's encode with another's write.
                if earlier == "rfb_encoded":
                    viewer = int(event.get("viewer_id", 0))
                    if viewer not in per_viewer:
                        continue
                    earlier_ns = per_viewer[viewer]
                else:
                    earlier_ns = first
                last = int(event["at_ns"])
                if last < earlier_ns:
                    impossible += 1
                    continue
                out[metric].append((last - earlier_ns) / 1_000_000.0)

        hints = sorted(activity[pid])
        initial = int(steps["capture_request"][0]["at_ns"])
        index = bisect.bisect_right(hints, initial) - 1
        if index >= 0 and 0 <= initial - hints[index] <= 250_000_000:
            out["qt_activity_hint_to_request"].append(
                (initial - hints[index]) / 1_000_000.0)

    return {
        "schema": 1,
        "metric_type": "host_monotonic_wall_latency",
        "endpoint": "RFB_socket_queued_not_viewer_present",
        "content_change_to_present": "NOT_MEASURED",
        "qt_activity_is_not_content_damage": True,
        "request_identity_collisions_skipped": ambiguous,
        "negative_or_out_of_order_pairs_skipped": impossible,
        "trace_events_dropped": overflow,
        "steps": dict(sorted(totals.items())),
        "intervals": {name: describe(out[name]) for name, _, _ in PAIRS}
            | {"qt_activity_hint_to_request":
               describe(out["qt_activity_hint_to_request"])},
    }


def self_test():
    def point(step, ns, request=1, frame=8, viewer=0):
        return {"schema": 3, "type": "frame_step", "pid": 42,
                "request_id": request, "frame_id": frame, "viewer_id": viewer,
                "at_ns": ns, "step": step}
    data = [
        {"schema": 2, "stages": {}},
        point("qt_activity_hint", 1_000_000, request=0),
        point("capture_request", 2_000_000),
        point("gpu_readback_submitted", 3_000_000),
        point("gpu_readback_ready", 8_000_000),
        point("source_completed", 10_000_000),
        point("core_frame_queued", 11_000_000),
        point("rfb_observed", 12_000_000),
        point("rfb_compared", 13_000_000),
        point("rfb_encoded", 14_000_000, viewer=1),
        point("rfb_socket_queued", 16_000_000, viewer=1),
        point("rfb_encoded", 14_500_000, viewer=2),
        point("rfb_socket_queued", 17_000_000, viewer=2),
        {"schema": 3, "type": "frame_trace_overflow", "pid": 42, "dropped": 3},
    ]
    result = analyze(data)
    assert result["trace_events_dropped"] == 3
    assert result["intervals"]["gpu_submit_to_ready"]["p50_ms"] == 5.0
    assert result["intervals"]["capture_request_to_socket_queue"]["n"] == 2
    assert result["intervals"]["capture_request_to_socket_queue"]["p50_ms"] == 14.5
    assert result["intervals"]["qt_activity_hint_to_request"]["p50_ms"] == 1.0
    assert result["intervals"]["rfb_encode_to_socket_queue"]["p50_ms"] == 2.25
    assert result["content_change_to_present"] == "NOT_MEASURED"
    print("PASS frame trace analyzer self-test")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, help="HYREMOTE_CPU_PROBE_FILE JSONL")
    parser.add_argument("--output", type=Path, help="optional summary JSON")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.input:
        parser.error("--input is required unless --self-test")
    with args.input.open("r", encoding="utf-8") as stream:
        records = (json.loads(line) for line in stream if line.strip())
        result = analyze(records)
    output = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
