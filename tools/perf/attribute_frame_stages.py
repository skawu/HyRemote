#!/usr/bin/env python3
"""Attribute fixed-SHA Quick capture traces to Host-monotonic stage delays (#967).

The existing ``analyze_frame_trace.py`` already pairs schema=3 stages by
``(pid, requestId)`` and reports the official interval set. This tool reuses
that analyzer (it never re-implements the pairing or percentile logic) and adds
the stage split the #962 matrix could not answer: how much of
``capture_request -> source_completed`` is spent waiting for a Qt render
callback versus executing the readback, and how many admitted Core requests
share the same rendered content.

Inputs: a matrix output directory with ``viewers-<n>-<arm>/`` subdirectories,
each holding ``runtime-frame-trace.jsonl`` and ``result.json``.
Outputs: ``<out>/matrix-stages.json`` and ``<out>/matrix-stages.csv`` plus
SHA-256 for every raw trace. Output paths must be under the repository ``tmp/``
directory or another run-output directory; this script never writes into the
source tree.

Times are HOST monotonic nanoseconds from one process. Viewer desktop-pixel
detection in ``result.json`` is an independent end-to-end observation and is
never subtracted from these timestamps. ``rfb_socket_queued`` is not a present.
"""

import argparse
import collections
import csv
import hashlib
import json
import math
import os
from pathlib import Path

from analyze_frame_trace import analyze
from aggregate_cpu_probe import load as load_cpu_probe, measure as measure_cpu_probe


def percentile(values, p):
    ordered = sorted(values)
    if not ordered:
        return None
    index = max(0, min(len(ordered) - 1, math.ceil(len(ordered) * p / 100) - 1))
    return ordered[index]


def describe(values):
    return {
        "n": len(values),
        "p50_ms": round(percentile(values, 50), 3) if values else None,
        "p95_ms": round(percentile(values, 95), 3) if values else None,
        "p99_ms": round(percentile(values, 99), 3) if values else None,
    }


def sha256(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def load_trace(path):
    records = []
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            if line.strip():
                records.append(json.loads(line))
    return records


def attribute(records, *, include_integrity=False):
    """Return the extra stage split for one Host process' schema=3 records."""
    requests = collections.defaultdict(lambda: collections.defaultdict(list))
    steps = collections.Counter()
    overflow = 0
    schemas = collections.Counter()
    pids = set()
    socket_by_viewer = collections.Counter()
    sockets_by_update = collections.defaultdict(list)
    for row in records:
        schemas[row.get("schema")] += 1
        if row.get("schema") != 3:
            continue
        if row.get("type") == "frame_trace_overflow":
            overflow += max(0, int(row.get("dropped", 0)))
            continue
        if row.get("type") != "frame_step":
            continue
        step = row.get("step")
        pid = int(row.get("pid", 0))
        pids.add(pid)
        steps[step] += 1
        if step == "rfb_socket_queued":
            viewer = int(row.get("viewer_id", 0))
            socket_by_viewer[viewer] += 1
            sockets_by_update[int(row.get("update_id", 0))].append(
                (viewer, int(row["at_ns"])))
        requests[(pid, int(row.get("request_id", 0)))][step].append(row)

    request_to_submit = []
    submit_to_source = []
    source_to_core = []
    duplicate_content = collections.Counter()
    duplicate_completion = collections.Counter()
    collisions = 0
    missing = collections.Counter()
    unavailable = 0
    for key, stages in requests.items():
        if include_integrity and key[1] <= 0:
            continue  # activity hints are not request identities
        capture = stages.get("capture_request") or []
        if len(capture) != 1:
            collisions += 1
            continue
        issued = int(capture[0]["at_ns"])
        submit = (stages.get("gpu_readback_submitted")
                  or stages.get("quick_grab_submitted") or [])
        source = stages.get("source_completed") or []
        core = stages.get("core_frame_queued") or []
        if not submit:
            missing["submit"] += 1
        if not source:
            missing["source_completed"] += 1
        if include_integrity and source and not source[0].get("content_pts_ns", 0):
            unavailable += 1
        if submit and source:
            request_to_submit.append((int(submit[0]["at_ns"]) - issued) / 1e6)
            submit_to_source.append(
                (int(source[0]["at_ns"]) - int(submit[0]["at_ns"])) / 1e6)
        if source and core:
            source_to_core.append(
                (int(core[0]["at_ns"]) - int(source[0]["at_ns"])) / 1e6)
        if source:
            content_pts = int(source[0].get("content_pts_ns", 0))
            completion = int(source[0].get("completion_ns", 0))
            if content_pts > 0:
                duplicate_content[content_pts] += 1
            if completion > 0:
                duplicate_completion[completion] += 1

    forks = []
    for events in sockets_by_update.values():
        viewers = {viewer for viewer, _ in events}
        if len(viewers) > 1:
            forks.append((max(at for _, at in events)
                          - min(at for _, at in events)) / 1e6)

    intervals = {
        "capture_request_to_submit": request_to_submit,
        "submit_to_source_completed": submit_to_source,
        "source_completed_to_core_frame_queued": source_to_core,
    }
    result = {
        "host_monotonic_only": True,
        "viewer_present": "NOT_MEASURED",
        "qt_activity_hint_is_noncausal": True,
        "host_pids": sorted(pids),
        "schemas": {str(k): v for k, v in schemas.items()},
        "steps": dict(sorted(steps.items())),
        "trace_events_dropped": overflow,
        "request_identity_collisions": collisions,
        "missing_stage_per_request": dict(missing),
        "duplicate_content_pts_requests": sum(v - 1 for v in duplicate_content.values()),
        "unique_content_pts": len(duplicate_content),
        "duplicate_completion_requests": sum(v - 1 for v in duplicate_completion.values()),
        "unique_completion": len(duplicate_completion),
        "socket_queued_per_viewer_id": {
            str(viewer): count for viewer, count in sorted(socket_by_viewer.items())},
        "viewer_socket_fork_ms": describe(forks),
        "intervals": {name: describe(values) for name, values in intervals.items()},
    }
    if include_integrity:
        result["source_without_content_pts"] = unavailable
        result["negative_host_intervals"] = {
            name: sum(value < 0 for value in values) for name, values in intervals.items()
        }
    return result


def combine(matrix, out):
    rows = []
    runs = {}
    for directory in sorted(matrix.glob("viewers-*-*")):
        trace = directory / "runtime-frame-trace.jsonl"
        result = directory / "result.json"
        if not trace.exists():
            continue
        _, viewers, arm = directory.name.split("-")
        records = load_trace(trace)
        official = analyze(iter(records))
        extra = attribute(records)
        detection = None
        source_sha = None
        if result.exists():
            payload = json.loads(result.read_text(encoding="utf-8"))
            blocks = payload.get("viewer_detect_ms") or []
            detection = blocks[0] if blocks else None
            source_sha = payload.get("source_sha")
        runs[directory.name] = {
            "source_sha": source_sha,
            "analyzer_schema": official["schema"],
            "official": official,
            "extra": extra,
            "viewer_detect_ms": detection,
            "raw_sha256": sha256(trace),
            "raw_bytes": trace.stat().st_size,
        }
        official_intervals = official["intervals"]
        rows.append({
            "viewers": int(viewers), "arm": arm, "directory": directory.name,
            "capture_requests": extra["steps"].get("capture_request", 0),
            "gpu_readback_submitted": extra["steps"].get("gpu_readback_submitted", 0),
            "quick_grab_submitted": extra["steps"].get("quick_grab_submitted", 0),
            "rfb_encoded": extra["steps"].get("rfb_encoded", 0),
            "rfb_socket_queued": extra["steps"].get("rfb_socket_queued", 0),
            "duplicate_content_pts": extra["duplicate_content_pts_requests"],
            "req_to_submit_p50": extra["intervals"]["capture_request_to_submit"]["p50_ms"],
            "req_to_submit_p95": extra["intervals"]["capture_request_to_submit"]["p95_ms"],
            "submit_to_source_p50": extra["intervals"]["submit_to_source_completed"]["p50_ms"],
            "submit_to_source_p95": extra["intervals"]["submit_to_source_completed"]["p95_ms"],
            "req_to_source_p50": official_intervals["request_to_source"]["p50_ms"],
            "req_to_socket_p50": official_intervals["capture_request_to_socket_queue"]["p50_ms"],
            "req_to_socket_p95": official_intervals["capture_request_to_socket_queue"]["p95_ms"],
            "viewer_detect_p50": detection.get("p50") if detection else None,
            "viewer_detect_p95": detection.get("p95") if detection else None,
        })
    payload = {
        "metric_type": "host_monotonic_wall_latency",
        "endpoint": "RFB_socket_queued_not_viewer_present",
        "analyzer": "tools/perf/analyze_frame_trace.py (schema=3, (pid,requestId) pairing)",
        "runs": runs,
    }
    out.mkdir(parents=True, exist_ok=True)
    (out / "matrix-stages.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    with (out / "matrix-stages.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    return rows


def point(step, at_ns, pid=42, request_id=1, **extra):
    return {"schema": 3, "type": "frame_step", "pid": pid, "step": step,
            "at_ns": at_ns, "frame_id": 0, "request_id": request_id,
            "content_pts_ns": 0, "request_ns": 0, "completion_ns": 0,
            "viewer_id": 0, "update_id": 0, "pts_source": 0, **extra}


def self_test():
    records = [
        point("capture_request", 1_000_000, request_id=1),
        point("gpu_readback_submitted", 3_000_000, request_id=1),
        point("source_completed", 4_000_000, request_id=1,
              content_pts_ns=2_500_000, completion_ns=3_800_000, pts_source=1),
        point("core_frame_queued", 4_100_000, request_id=1),
        point("capture_request", 1_500_000, request_id=2),
        point("gpu_readback_submitted", 3_000_000, request_id=2),
        point("source_completed", 4_000_000, request_id=2,
              content_pts_ns=2_500_000, completion_ns=3_800_000, pts_source=1),
        point("core_frame_queued", 4_100_000, request_id=2),
        point("rfb_socket_queued", 5_000_000, viewer_id=1, update_id=7),
        point("rfb_socket_queued", 5_800_000, viewer_id=2, update_id=7),
        point("capture_request", 6_000_000, request_id=3),
        point("capture_request", 6_000_000, request_id=3),
        {"schema": 2, "type": "cpu_snapshot", "pid": 42},
        {"schema": 3, "type": "frame_trace_overflow", "pid": 42, "dropped": 2},
    ]
    result = attribute(records)
    assert result["intervals"]["capture_request_to_submit"]["p50_ms"] == 1.5
    assert result["intervals"]["submit_to_source_completed"]["p50_ms"] == 1.0
    assert result["duplicate_content_pts_requests"] == 1
    assert result["unique_content_pts"] == 1
    assert result["duplicate_completion_requests"] == 1
    assert result["request_identity_collisions"] == 1
    assert result["missing_stage_per_request"].get("submit", 0) == 0
    assert result["trace_events_dropped"] == 2
    assert result["schemas"]["2"] == 1
    assert result["viewer_socket_fork_ms"]["p50_ms"] == 0.8
    assert result["socket_queued_per_viewer_id"] == {"1": 1, "2": 1}
    assert result["viewer_present"] == "NOT_MEASURED"
    empty = attribute([point("capture_request", 1_000_000, request_id=9)])
    assert empty["missing_stage_per_request"]["submit"] == 1
    assert empty["intervals"]["capture_request_to_submit"]["n"] == 0
    assert empty["request_identity_collisions"] == 0
    print("PASS frame stage attribution self-test (positive + negative)")


def public_cpu_breakdown(path, warmup_ms, seconds):
    snapshots = load_cpu_probe(path)
    measurement = measure_cpu_probe(snapshots, warmup_ms, seconds)
    observed = {}
    for stage in ("quick_grab_submit", "quick_grab_ready_wall", "quick_convert"):
        counters = measurement["stages"].get(stage)
        observed[stage] = counters if counters is not None else "NOT_INSTRUMENTED"
    return {
        "host_pid": snapshots[0]["pid"],
        "semantics": {
            "quick_grab_submit": "synchronous grab call scope including submit tracing",
            "quick_grab_ready_wall": "async elapsed from grab start to GUI ready handler observation",
            "quick_convert": "publishImage format-conversion scope",
        },
        "observed_counter_deltas": observed,
        "qt_render_to_image": "NOT_INSTRUMENTED",
        "qt_completion_event_queue_wait": "NOT_INSTRUMENTED",
        "viewer_decode": "NOT_MEASURED",
        "physical_present": "NOT_MEASURED",
        "interval_policy": "overlapping aggregate counters; do not subtract or pair per request",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, help="matrix run directory")
    parser.add_argument("--out", type=Path, help="run-output directory for JSON/CSV")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--public-cpu-probe", type=Path, help="read existing runtime CPU JSONL only")
    parser.add_argument("--warmup-ms", type=int, default=5000)
    parser.add_argument("--seconds", type=int, help="duration of the CPU counter window")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.public_cpu_probe:
        if not args.seconds or args.seconds < 1 or args.warmup_ms < 0 or args.matrix or args.out:
            parser.error("CPU probe mode requires --seconds and no matrix output")
        print(json.dumps(public_cpu_breakdown(args.public_cpu_probe, args.warmup_ms, args.seconds),
                         indent=2, sort_keys=True))
        return
    if not args.matrix or not args.out:
        parser.error("--matrix and --out are required unless --self-test")
    rows = combine(args.matrix, args.out)
    if not rows:
        raise SystemExit("No viewers-<n>-<arm>/runtime-frame-trace.jsonl found")
    print(json.dumps({"runs": len(rows), "out": str(args.out.resolve())},
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
