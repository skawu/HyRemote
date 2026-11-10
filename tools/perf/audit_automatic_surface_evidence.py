#!/usr/bin/env python3
"""Fail-closed offline audit of Automatic Surface A1/ON/A2 evidence.

Does not retroactively upgrade invalid OFF data. It distinguishes source-log/
Viewer observation gaps from the CPU stability gate without rerunning tests.
No network calls, no raw screenshots or personal window content are read.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import statistics
import sys


def require(ok, why):
    if not ok:
        raise RuntimeError(why)


def load(path):
    obj = json.loads(path.read_text(encoding="utf-8-sig"))
    require(isinstance(obj, dict), f"{path}: JSON must be an object")
    return obj


def valid_number(n):
    return isinstance(n, (int, float)) and not isinstance(n, bool) and math.isfinite(n)


def cpu(row, label):
    require(row.get("schema") == "hyremote-windows-process-cpu-sampling/1",
            f"{label}: wrong CPU schema")
    require(row.get("role") == "host" and row.get("case") == "1v-static",
            f"{label}: not a static one-Viewer Host CPU series")
    samples = row.get("samples")
    require(isinstance(samples, list) and len(samples) >= 45,
            f"{label}: insufficient full minute samples")
    require(valid_number(row.get("duration_measured_s"))
            and row["duration_measured_s"] >= 59.5,
            f"{label}: CPU process died / incomplete 60s window")
    values = [v.get("cpu_pct_one_core") for v in samples]
    require(all(valid_number(v) and v >= 0 for v in values),
            f"{label}: invalid CPU sample")
    mean = statistics.mean(values)
    require(valid_number(row.get("cpu_mean_pct_one_core"))
            and abs(mean - row["cpu_mean_pct_one_core"]) < .0001,
            f"{label}: reported mean does not match raw samples")
    return {
        "mean_one_core_pct": mean,
        "median_one_core_pct": statistics.median(values),
        "max_one_core_pct": max(values),
        "per_sample_stddev_one_core_pct": statistics.pstdev(values),
        "duration_s": row["duration_measured_s"],
        "process_identity_verified_for_window": bool(
            row.get("process_creation_filetime_ns")),
    }


def viewer(row, label):
    require(row.get("schema") == "hyremote-windows-real-viewer-pixel-observation/1",
            f"{label}: wrong physical Viewer observation schema")
    require(row.get("actual_viewer_window_ownership_verified") is True,
            f"{label}: real Viewer PID/window attribution failed")
    count = row.get("observed_event_count")
    missed = row.get("missed_events")
    matched = row.get("matched_events")
    require(all(isinstance(z, int) and z >= 0
                for z in (count, missed, matched))
            and count == missed + matched,
            f"{label}: source stage accounting inconsistent")
    require(row.get("duration_seconds") >= 89,
            f"{label}: incomplete 90s Viewer observation")
    return {
        "source_changes": count,
        "matched": matched,
        "missed": missed,
        "miss_fraction": missed / count if count else None,
        "visible_p95_ms": row.get("viewer_change_to_visible_p95_ms"),
        "measured_sample_start_p95_ms": row.get("actual_sample_start_interval_p95_ms"),
        "four_pixel_read_p95_ms": row.get("sequential_four_pixel_read_p95_ms"),
        "event_timeline_available": "source_event_timeline" in row,
        "color_transition_timeline_available": "visible_transition_timeline" in row,
    }


def audit(a1_static, a2_static, a1_viewer=None, on_viewer=None):
    a = cpu(a1_static, "OFF-A1-static")
    b = cpu(a2_static, "OFF-A2-static")
    drift = abs(a["mean_one_core_pct"] - b["mean_one_core_pct"])
    drift_pct = 100 * drift / ((a["mean_one_core_pct"] + b["mean_one_core_pct"]) / 2)
    result = {
        "schema": "hyremote-automatic-surface-attribution-audit/1",
        "off_static_a1": a,
        "off_static_a2": b,
        "absolute_cpu_delta_one_core_pp": drift,
        "off_static_drift_pct": drift_pct,
        "original_static_acceptance_threshold_pct": 15,
        "off_static_stability": "PASS" if drift_pct <= 15 else "FAIL",
        "off_a1_real_viewer": viewer(a1_viewer, "OFF-A1-Viewer")
            if a1_viewer is not None else None,
        "on_real_viewer": viewer(on_viewer, "ON-Viewer")
            if on_viewer is not None else None,
    }
    reasons = []
    if drift_pct > 15:
        reasons.append("OFF_STATIC_DRIFT_GT_15_PERCENT")
    if result["off_a1_real_viewer"]:
        off = result["off_a1_real_viewer"]
        if off["missed"] > 0:
            reasons.append("OFF_BASELINE_HAS_UNACCOUNTED_SOURCE_PHASES")
        if not off["event_timeline_available"]:
            reasons.append("OFF_EVENT_TIMELINE_NOT_RETAINED_BY_OLD_OBSERVER")
    if result["on_real_viewer"]:
        on = result["on_real_viewer"]
        if on["missed"] > 0:
            reasons.append("ON_HAS_UNACCOUNTED_SOURCE_PHASES")
    if a1_viewer is None or on_viewer is None:
        reasons.append("MISSING_COMPLETE_REAL_VIEWER_PAIR")
    result["reasons"] = reasons
    result["verdict"] = "INCONCLUSIVE" if reasons else "PAIR_READY_FOR_FULL_ABA_REVIEW"
    result["not_an_end_to_end_capture_or_network_attribution"] = True
    result["no_thresholds_changed"] = True
    return result


def example_cpu(mean):
    return {
        "schema": "hyremote-windows-process-cpu-sampling/1",
        "role": "host", "case": "1v-static", "duration_measured_s": 60.,
        "cpu_mean_pct_one_core": mean,
        "samples": [{"cpu_pct_one_core": mean} for _ in range(60)],
        "process_creation_filetime_ns": 123,
    }


def example_viewer(missed=0, timeline=True):
    data = {
        "schema": "hyremote-windows-real-viewer-pixel-observation/1",
        "actual_viewer_window_ownership_verified": True,
        "observed_event_count": 75, "matched_events": 75-missed,
        "missed_events": missed, "duration_seconds": 90,
        "viewer_change_to_visible_p95_ms": 420,
        "actual_sample_start_interval_p95_ms": 131,
        "sequential_four_pixel_read_p95_ms": 69,
    }
    if timeline:
        data["source_event_timeline"] = []
        data["visible_transition_timeline"] = []
    return data


def self_test():
    old = audit(example_cpu(6.74), example_cpu(5.34),
                example_viewer(26, timeline=False), example_viewer(0))
    require(old["verdict"] == "INCONCLUSIVE"
            and old["off_static_drift_pct"] > 15
            and "OFF_BASELINE_HAS_UNACCOUNTED_SOURCE_PHASES" in old["reasons"]
            and "OFF_EVENT_TIMELINE_NOT_RETAINED_BY_OLD_OBSERVER" in old["reasons"],
            "Historical invalid baseline was falsely accepted")
    stable = audit(example_cpu(6.51), example_cpu(6.92),
                   example_viewer(), example_viewer())
    require(stable["verdict"] == "PAIR_READY_FOR_FULL_ABA_REVIEW",
            "Consistent clean pair unexpectedly failed")
    invalid = example_cpu(7)
    invalid["duration_measured_s"] = 30.
    try:
        audit(invalid, example_cpu(7), example_viewer(), example_viewer())
    except RuntimeError:
        pass
    else:
        raise RuntimeError("Truncated 30s CPU was accepted as full 60s")
    print("PASS automatic evidence audit: drift, OFF stage loss, untraceable historic JSON, valid pair, truncated CPU")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--off-a1-static", type=Path)
    p.add_argument("--off-a2-static", type=Path)
    p.add_argument("--off-a1-viewer", type=Path)
    p.add_argument("--on-viewer", type=Path)
    p.add_argument("--out", type=Path)
    args = p.parse_args()
    if args.self_test:
        self_test()
        return
    require(all((args.off_a1_static, args.off_a2_static, args.out)),
            "Need --off-a1-static --off-a2-static --out")
    result = audit(
        load(args.off_a1_static), load(args.off_a2_static),
        load(args.off_a1_viewer) if args.off_a1_viewer else None,
        load(args.on_viewer) if args.on_viewer else None,
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")
    print("AUTOMATIC_EVIDENCE_AUDIT=" + result["verdict"])
    print("REASONS=" + ",".join(result["reasons"]))
    print("EVIDENCE=" + str(args.out))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, TypeError, KeyError) as exc:
        print("AUTOMATIC_EVIDENCE_AUDIT=BLOCKED: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
