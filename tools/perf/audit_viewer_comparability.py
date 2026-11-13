#!/usr/bin/env python3
"""Check the evidence for a matched one-Viewer pair without running a measurement."""

import argparse
import json
import math
from pathlib import Path
import re

from attribute_frame_stages import attribute, load_trace, sha256


ENVIRONMENT = (
    "qt_version", "qt_kit_sha256", "rhi_backend", "render_loop", "gpu_driver",
    "display_mode", "refresh_hz", "display_session", "window_visibility",
    "rfb_encoding", "sampler_sha256", "host_clock", "viewer_clock",
)
SHA = re.compile(r"[0-9a-f]{40}")
DIGEST = re.compile(r"[0-9a-f]{64}")


def digest(value):
    return isinstance(value, str) and DIGEST.fullmatch(value) is not None


def audit_run(result, manifest, trace, trace_digest):
    issues = []
    if not isinstance(result.get("source_sha"), str) or not SHA.fullmatch(result["source_sha"]):
        issues.append("INVALID_SOURCE_SHA")
    if result.get("stop_ok") is not True or result.get("host_exit") != 0:
        issues.append("INVALID_HOST_LIFECYCLE")
    hashes = result.get("hashes") or {}
    if not all(digest(hashes.get(key)) for key in
               ("host.exe", "HyRemoteRemoteAccess.dll", "vncviewer.exe")):
        issues.append("NOT_INSTRUMENTED_BINARY_IDENTITY")
    missing_env = [name for name in ENVIRONMENT if manifest.get(name) in
                   (None, "", "NOT_INSTRUMENTED", "NOT_MEASURED")]
    if missing_env:
        issues.append("NOT_INSTRUMENTED_ENVIRONMENT")
    if not all(digest(manifest.get(name)) for name in ("qt_kit_sha256", "sampler_sha256")):
        issues.append("INVALID_ENVIRONMENT_DIGEST")
    hz = manifest.get("refresh_hz")
    if type(hz) not in (int, float) or not math.isfinite(hz) or hz <= 0:
        issues.append("INVALID_REFRESH_RATE")
    if (manifest.get("host_clock"), manifest.get("viewer_clock")) != (
            "host_monotonic_ns", "viewer_qpc"):
        issues.append("CLOCK_DOMAINS_UNVERIFIED")
    if manifest.get("interval_policy") != "separate_clock_domains":
        issues.append("CROSS_CLOCK_POLICY_UNVERIFIED")
    frequency = manifest.get("viewer_qpc_hz")
    if type(frequency) is not int or frequency <= 0:
        issues.append("NOT_INSTRUMENTED_VIEWER_QPC_FREQUENCY")
    if trace is None:
        issues.append("NOT_INSTRUMENTED_HOST_TRACE")
    elif (not digest(trace_digest) or manifest.get("trace_sha256") != trace_digest
          or trace["host_pids"] != [result.get("host_pid")] or set(trace["schemas"]) != {"3"}
          or trace["trace_events_dropped"] or trace["request_identity_collisions"]):
        issues.append("HOST_TRACE_IDENTITY_OR_INTEGRITY")
    else:
        if any(trace["missing_stage_per_request"].values()):
            issues.append("HOST_REQUEST_STAGE_MISSING")
        if trace["source_without_content_pts"]:
            issues.append("HOST_SOURCE_CONTENT_UNVERIFIED")
        viewers = trace["socket_queued_per_viewer_id"]
        if len(viewers) != 1 or any(int(viewer) <= 0 for viewer in viewers):
            issues.append("HOST_ONE_VIEWER_UNVERIFIED")
        interval = trace["intervals"]["submit_to_source_completed"]
        if not interval["n"] or any(trace["negative_host_intervals"].values()):
            issues.append("HOST_STAGE_INTERVAL_INVALID")
        steps = trace["steps"]
        expected = {"public": "quick_grab_submitted", "gl": "gpu_readback_submitted"}
        route = manifest.get("capture_route")
        if route not in expected or not steps.get(expected[route]) or any(
                steps.get(step) for key, step in expected.items() if key != route):
            issues.append("HOST_ROUTE_UNVERIFIED")

    steady = result.get("steady_sequences")
    raw = result.get("viewer_samples")
    missed = result.get("viewer_missed_sequences")
    counts = result.get("viewer_detect_ms")
    n = None
    if not isinstance(steady, list) or not steady or len(steady) != len(set(steady)):
        issues.append("NOT_INSTRUMENTED_STEADY_SEQUENCE_SET")
    elif not all(isinstance(rows, list) and len(rows) == 1 for rows in (raw, missed, counts)):
        issues.append("NOT_INSTRUMENTED_ONE_VIEWER_SAMPLES")
    else:
        intended = set(steady)
        rows = [row for row in raw[0] if row[0] in intended]
        observed = {row[0] for row in rows}
        n = len(rows)
        if (len(observed) != n or counts[0].get("n") != n
                or set(missed[0]) != intended - observed
                or any(not math.isfinite(row[2]) or row[2] < 0 for row in rows)):
            issues.append("VIEWER_SAMPLE_ACCOUNTING_MISMATCH")
        if intended - observed:
            issues.append("MISSING_STEADY_VIEWER_SAMPLES")
        if type(frequency) is int and frequency > 0:
            markers = {seq: tick for seq, tick, _ in result.get("changes", [])}
            if any(seq not in markers or tick < markers[seq]
                   or not math.isclose(ms, (tick - markers[seq]) * 1000 / frequency,
                                       rel_tol=0, abs_tol=0.001)
                   for seq, tick, ms in rows):
                issues.append("VIEWER_QPC_INTERVAL_MISMATCH")
    return {"issues": issues, "missing_environment": missing_env, "viewer_n": n,
            "host_intervals": trace["intervals"] if trace else "NOT_INSTRUMENTED",
            "missing_host_stages": trace["missing_stage_per_request"] if trace else None,
            "binary_identity_provenance": "DECLARED_IN_RESULT",
            "marker_to_next_swap_ms": result.get("local_marker_to_next_swap_ms", "NOT_INSTRUMENTED")}


def compare(left, right, mode):
    results = [run[0] for run in (left, right)]
    manifests = [run[1] for run in (left, right)]
    runs = [audit_run(*run) for run in (left, right)]
    issues = []
    if results[0].get("source_sha") != results[1].get("source_sha"):
        issues.append("SOURCE_SHA_MISMATCH")
    if results[0].get("steady_sequences") != results[1].get("steady_sequences"):
        issues.append("STEADY_SEQUENCE_SET_MISMATCH")
    env_drift = [key for key in ENVIRONMENT if manifests[0].get(key) != manifests[1].get(key)]
    if env_drift:
        issues.append("ENVIRONMENT_MISMATCH")
    hashes = [result.get("hashes") or {} for result in results]
    if any(hashes[0].get(key) != hashes[1].get(key) for key in ("host.exe", "vncviewer.exe")):
        issues.append("HOST_OR_VIEWER_BINARY_MISMATCH")
    routes = [manifest.get("capture_route") for manifest in manifests]
    same_runtime = hashes[0].get("HyRemoteRemoteAccess.dll") == hashes[1].get("HyRemoteRemoteAccess.dll")
    if mode == "aa" and (not same_runtime or routes != ["public", "public"]):
        issues.append("AA_RUNTIME_OR_ROUTE_MISMATCH")
    if mode == "ab" and (same_runtime or set(routes) != {"public", "gl"}):
        issues.append("AB_RUNTIME_OR_ROUTE_MISMATCH")
    return {
        "status": "BLOCKED" if issues or any(r["issues"] for r in runs) else "MATCHED_METADATA",
        "mode": mode, "issues": issues, "environment_drift": env_drift, "runs": runs,
        "physical_present": "NOT_MEASURED", "qualification": "NOT_ACCEPTANCE",
    }


def read_run(directory):
    def read(name):
        return json.loads((directory / name).read_text(encoding="utf-8"))

    result = read("result.json")
    manifest_path = directory / "comparison-environment.json"
    trace = directory / "runtime-frame-trace.jsonl"
    return (result, read(manifest_path.name) if manifest_path.is_file() else {},
            attribute(load_trace(trace), include_integrity=True) if trace.is_file() else None,
            sha256(trace) if trace.is_file() else None)


def self_test():
    import copy

    result = {"source_sha": "a" * 40, "host_pid": 11, "host_exit": 0, "stop_ok": True,
              "hashes": dict.fromkeys(("host.exe", "HyRemoteRemoteAccess.dll", "vncviewer.exe"), "b" * 64),
              "steady_sequences": [6, 7], "viewer_samples": [[[6, 30, 1], [7, 60, 2]]],
              "changes": [[6, 20, 0], [7, 40, 0]],
              "viewer_detect_ms": [{"n": 2}], "viewer_missed_sequences": [[]]}
    manifest = dict.fromkeys(ENVIRONMENT, "observed")
    manifest.update(host_clock="host_monotonic_ns", viewer_clock="viewer_qpc",
                    interval_policy="separate_clock_domains", capture_route="public",
                    viewer_qpc_hz=10_000,
                    qt_kit_sha256="e" * 64, sampler_sha256="f" * 64, refresh_hz=60,
                    trace_sha256="c" * 64)
    records = [
        {"schema": 3, "type": "frame_step", "pid": 11, "request_id": request,
         "step": step, "at_ns": timestamp,
         "content_pts_ns": timestamp if step == "source_completed" else 0}
        for request in (1, 2, 3)
        for step, timestamp in (("capture_request", 1_000_000),
                                ("quick_grab_submitted", 2_000_000),
                                ("source_completed", 3_000_000))
    ]
    records.append({"schema": 3, "type": "frame_step", "pid": 11,
                    "request_id": 0, "step": "qt_activity_hint", "at_ns": 500_000})
    socket = {"schema": 3, "type": "frame_step", "pid": 11, "request_id": 1,
              "step": "rfb_socket_queued", "at_ns": 3_500_000, "update_id": 1}
    records.append(dict(socket, viewer_id=1))
    trace = attribute(records, include_integrity=True)
    run = result, manifest, trace, "c" * 64
    if compare(run, run, "aa")["status"] != "MATCHED_METADATA":
        raise AssertionError("valid pair rejected")
    for rows in (records[:-1], records + [dict(socket, viewer_id=2)]):
        other = (result, manifest, attribute(rows, include_integrity=True), "c" * 64)
        if "HOST_ONE_VIEWER_UNVERIFIED" not in compare(run, other, "aa")["runs"][1]["issues"]:
            raise AssertionError("Host must record exactly one positive Viewer identity")
    for extra in (
        [{"step": "capture_request", "at_ns": 4_000_000}],
        [{"step": "capture_request", "at_ns": 4_000_000},
         {"step": "quick_grab_submitted", "at_ns": 5_000_000}],
        [{"step": "capture_request", "at_ns": 4_000_000},
         {"step": "quick_grab_submitted", "at_ns": 5_000_000},
         {"step": "source_completed", "at_ns": 6_000_000, "content_pts_ns": 0}],
    ):
        incomplete = attribute(records + [
            dict(row, schema=3, type="frame_step", pid=11, request_id=4) for row in extra
        ], include_integrity=True)
        bad = (result, manifest, incomplete, "c" * 64)
        if compare(run, bad, "aa")["status"] != "BLOCKED":
            raise AssertionError("incomplete or contentless request accepted")
    records[2]["at_ns"] = 1_000_000
    bad_trace = attribute(records, include_integrity=True)
    if (bad_trace["intervals"]["submit_to_source_completed"]["p50_ms"] != 1
            or compare(run, (result, manifest, bad_trace, "c" * 64), "aa")["status"] != "BLOCKED"):
        raise AssertionError("negative Host sample hidden by a positive median")
    for target, key, value in ((0, "source_sha", "d" * 40), (0, "viewer_samples", None),
                               (0, "viewer_samples", [[[6, 30, 1]]]),
                               (1, "viewer_clock", "host_monotonic_ns"),
                               (1, "viewer_qpc_hz", 1_000_000),
                               (1, "render_loop", None), (2, "host_pids", [12])):
        changed = copy.deepcopy(run)
        changed[target][key] = value
        if compare(run, changed, "aa")["status"] != "BLOCKED":
            raise AssertionError(f"invalid pair accepted: {key}")
    print("VIEWER_COMPARABILITY_SELF_TEST=PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", type=Path, nargs="*")
    parser.add_argument("--mode", choices=("aa", "ab"), default="aa")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test and not args.runs:
        self_test()
        return 0
    if len(args.runs) != 2 or args.self_test:
        parser.error("provide exactly two run directories or --self-test")
    try:
        report = compare(*(read_run(path) for path in args.runs), args.mode)
    except (OSError, ValueError, KeyError, IndexError, TypeError) as exc:
        report = {"status": "BLOCKED", "error": str(exc), "qualification": "NOT_ACCEPTANCE"}
    print(json.dumps(report, indent=2, sort_keys=True))
    return 2 if report["status"] == "BLOCKED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
