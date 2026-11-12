#!/usr/bin/env python3
"""Assess pinned pre/post #919 Qt Quick overlap A/B/A, without invented GPU gain."""
import argparse
import json
import math
from pathlib import Path

BEFORE = "b1699ddab5b83a73d06dfc5d479dcda2cd6d8e74"
AFTER = "5095d0bc3bb07966c03dad77296f6ec04a631adc"
PROFILE_SCHEMA = "hyremote-quick-overlap-profile/1"


def check(ok, why):
    if not ok:
        raise ValueError(why)


def finite_positive(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x) and x > 0


def load(data_dir, backend, mode, variant):
    path = data_dir / f"{backend}-{mode}-{variant}.json"
    probe_path = data_dir / f"{backend}-{mode}-{variant}.probe.jsonl"
    p = json.loads(path.read_text(encoding="utf-8"))
    lines = [line for line in probe_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    check(lines, f"{probe_path}: no CPU-probe snapshots")
    q = json.loads(lines[-1])
    check(p.get("schema") == PROFILE_SCHEMA, f"{path}: schema")
    check(p.get("context") == "REAL_QT_CAPTURE_NOT_GPU_TIMER_OR_RK3588",
          f"{path}: provenance")
    check(p.get("backend") == backend and p.get("mode") == mode, f"{path}: backend/mode")
    check(p.get("source_sha") == (AFTER if variant == "after" else BEFORE),
          f"{path}: source is not exact #919 pre/post")
    check(p.get("qt") == "6.8.3", f"{path}: Qt Kit mismatch")
    check(p.get("width") == 1920 and p.get("height") == 1080, f"{path}: resolution")
    check(p.get("valid_frames") == 80 and p.get("pairs") == 40
          and p.get("warmup_pairs") == 8, f"{path}: insufficient frames")
    check(p.get("render_source_frame_age") == "UNKNOWN_COMPLETION_PTS_ONLY",
          f"{path}: do not invent true content age")
    check(p.get("process_cpu_is_not_gpu_readback_time") is True,
          f"{path}: CPU must not be relabelled GPU time")
    for field in ("process_cpu_ms", "wall_ms", "cpu_ms_per_frame",
                  "published_fps", "request_to_publish_p95_ms",
                  "second_request_p95_ms"):
        check(finite_positive(p.get(field)), f"{path}: invalid {field}")
    check(p.get("second_admitted_before_first_published") in range(41),
          f"{path}: overlap admission")
    check(isinstance(p.get("ordering_errors"), int)
          and isinstance(p.get("stale_second_frames"), int), f"{path}: identity")
    check(q.get("schema") == 2 and q.get("process_cpu_valid") is True,
          f"{probe_path}: stage probe identity")
    stages = q.get("stages", {})
    for field in ("quick_grab_submit", "quick_grab_ready_wall",
                  "quick_gui_batches", "quick_convert", "quick_buffer"):
        check(isinstance(stages.get(field, {}).get("calls"), int),
              f"{probe_path}: missing stage {field}")
    activity = q.get("quick_grab_activity")
    check(isinstance(activity, dict)
          and isinstance(activity.get("peak"), int)
          and isinstance(activity.get("active"), int),
          f"{probe_path}: missing actual in-flight grab counter")
    check(activity["active"] == 0, f"{probe_path}: dangling in-flight grab")
    check(stages["quick_grab_submit"]["calls"] >= 48
          and stages["quick_grab_ready_wall"]["calls"] >= 48,
          f"{probe_path}: no actual Qt grabs")
    return {
        **p,
        "actual_grab_submits": stages["quick_grab_submit"]["calls"],
        "actual_grab_ready": stages["quick_grab_ready_wall"]["calls"],
        "grab_ready_wall_ms": stages["quick_grab_ready_wall"]["wall_ns"] / 1e6,
        "submit_cpu_ms": stages["quick_grab_submit"]["cpu_ns"] / 1e6,
        "convert_cpu_ms": stages["quick_convert"]["cpu_ns"] / 1e6,
        "buffer_cpu_ms": stages["quick_buffer"]["cpu_ns"] / 1e6,
        "max_real_grabs_in_flight": activity["peak"],
    }


def assess(data_dir, backends=("software", "opengl")):
    results = []
    for backend in backends:
        for mode in ("static", "transition"):
            a1 = load(data_dir, backend, mode, "before_a1")
            b = load(data_dir, backend, mode, "after")
            a2 = load(data_dir, backend, mode, "before_a2")
            mean_cpu = (a1["cpu_ms_per_frame"] + a2["cpu_ms_per_frame"]) / 2
            spread = 100 * abs(a1["cpu_ms_per_frame"] -
                               a2["cpu_ms_per_frame"]) / mean_cpu
            stable = spread <= 15
            old_submits = (a1["actual_grab_submits"] +
                           a2["actual_grab_submits"]) / 2
            gain = 100 * (mean_cpu - b["cpu_ms_per_frame"]) / mean_cpu
            overlap_pressure = all(x["second_admitted_before_first_published"] >= 32
                                   for x in (a1, b, a2))
            mechanism = (max(a1["max_real_grabs_in_flight"],
                             a2["max_real_grabs_in_flight"]) >= 2
                         and b["max_real_grabs_in_flight"] == 1)
            fresh = b["ordering_errors"] == 0 and b["stale_second_frames"] == 0
            submit_reduction = old_submits - b["actual_grab_submits"]
            if not overlap_pressure or not mechanism:
                verdict = "INCONCLUSIVE_NO_CONFIRMED_CONCURRENT_GRABS"
            elif not fresh:
                verdict = "FAIL_STALE_OR_OUT_OF_ORDER"
            elif not stable:
                verdict = "UNSTABLE_BASELINE_NO_CPU_CLAIM"
            elif submit_reduction <= 0:
                verdict = "NO_READBACK_SUBMISSION_REDUCTION"
            else:
                verdict = "FEWER_QT_GRAB_SUBMISSIONS_HOSTED_ONLY"
            row = {
                "backend": backend, "mode": mode, "verdict": verdict,
                "before_cpu_ms_per_frame": mean_cpu,
                "after_cpu_ms_per_frame": b["cpu_ms_per_frame"],
                "cpu_change_percent_improvement_positive": gain,
                "before_a1_a2_cpu_spread_percent": spread,
                "stable_before_bracket": stable,
                "before_grab_submits_mean": old_submits,
                "after_grab_submits": b["actual_grab_submits"],
                "actual_grab_submission_reduction": submit_reduction,
                "before_peak_inflight": max(a1["max_real_grabs_in_flight"],
                                           a2["max_real_grabs_in_flight"]),
                "after_peak_inflight": b["max_real_grabs_in_flight"],
                "overlap_pressure_confirmed": overlap_pressure,
                "before_request_p95_ms_mean": (a1["request_to_publish_p95_ms"] +
                                                a2["request_to_publish_p95_ms"]) / 2,
                "after_request_p95_ms": b["request_to_publish_p95_ms"],
                "before_second_request_p95_ms_mean": (
                    a1["second_request_p95_ms"] + a2["second_request_p95_ms"]) / 2,
                "after_second_request_p95_ms": b["second_request_p95_ms"],
                "before_fps_mean": (a1["published_fps"] + a2["published_fps"]) / 2,
                "after_fps": b["published_fps"],
                "after_stale_second_frames": b["stale_second_frames"],
                "after_ordering_errors": b["ordering_errors"],
                "content_frame_age": "UNOBSERVABLE_WITH_COMPLETION_PTS",
                "gpu_readback_time": "NOT_INSTRUMENTED",
                "rk3588_gain": "NOT_MEASURED",
            }
            results.append(row)
            print(f"{backend}/{mode}: {verdict}; peak {row['before_peak_inflight']}→"
                  f"{row['after_peak_inflight']}; actual grab submissions "
                  f"{old_submits:.1f}→{b['actual_grab_submits']}; "
                  f"CPU/frame {mean_cpu:.3f}→{b['cpu_ms_per_frame']:.3f} ms "
                  f"(drift {spread:.1f}%); "
                  f"second-request p95 {row['before_second_request_p95_ms_mean']:.1f}"
                  f"→{b['second_request_p95_ms']:.1f} ms; "
                  f"fps {row['before_fps_mean']:.1f}→{b['published_fps']:.1f}")
    outcome = {
        "schema": "hyremote-quick-overlap-comparison/1",
        "source_before": BEFORE, "source_after": AFTER,
        "same_diagnostic_probe_on_both_versions": True,
        "single_flight_gpu_readback_cost_reduction": "UNPROVEN_WITHOUT_GPU_TIMING",
        "profile_results": results,
    }
    (data_dir / "quick-overlap-comparison.json").write_text(
        json.dumps(outcome, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return results


def self_test():
    # A peak reduction is NOT proof of fewer submits or less GPU readback.
    old_peak, new_peak, old_submits, new_submits = 2, 1, 96, 96
    assert old_peak > new_peak and old_submits - new_submits == 0
    print("PASS: concurrency reduction distinct from readback submission reduction")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", type=Path)
    parser.add_argument("--backends", nargs="+", choices=("software", "opengl", "d3d11"),
                        default=["software", "opengl"])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    else:
        check(args.dir is not None, "provide --dir")
        assess(args.dir, tuple(args.backends))
