#!/usr/bin/env python3
"""Assess one real Qt Quick PUBLIC-grab profile, never an inferred native gain.

Only validates provenance, framebuffer, A1/motion/A2 payload, baseline stability,
and sample semantics. Do not compare CPU values across different graphics APIs,
machines, builds or Qt Kits as though they were a controlled native A/B trial.
"""
import argparse
import json
import math
from pathlib import Path

SCHEMA = "hyremote-quick-public-profile/1"
NAMES = ("public_static_a1", "public_a_split_b_a", "public_static_a2")
STABILITY_PERCENT = 15.0


def require(value, message):
    if not value:
        raise ValueError(message)


def numeric(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def evaluate(root, expected=None):
    require(isinstance(root, dict), "profile must be a JSON object")
    require(root.get("schema") == SCHEMA, "profile schema mismatch")
    require(root.get("context") == "HOSTED_SYNTHETIC_NOT_DEVICE_OR_NATIVE_GAIN",
            "synthetic-only provenance is mandatory")
    require(root.get("route") == "public_grabToImage", "wrong capture route")
    require(root.get("native_performance_qualification") == "NOT_MEASURED_DO_NOT_PROMOTE",
            "private-native gain must not be inferred")
    require(root.get("whole_process_cpu_not_gpu_time") is True,
            "process CPU time is not a GPU timing")
    backend = root.get("renderer")
    require(backend in ("software", "opengl", "d3d11", "d3d12", "vulkan", "metal",
                        "unknown", "other"), "unrecognised graphics API")
    if expected:
        require(backend == expected, "different Qt scenegraph backend than requested")
    require(root.get("actual_1080p") is True, "actual physical framebuffer is not 1080p")
    require(root.get("requested_width") == 1920 and root.get("requested_height") == 1080,
            "different requested capture resolution")
    count = root.get("measured_per_case")
    require(isinstance(count, int) and count > 0, "invalid measured frame count")
    require(isinstance(root.get("warmup_per_case"), int)
            and root["warmup_per_case"] >= 1, "missing warmups")
    sha = root.get("source_sha")
    require(isinstance(sha, str) and len(sha) == 40
            and all(c in "0123456789abcdef" for c in sha.lower()),
            "bench cannot be compared without exact checked-out source")
    require(isinstance(root.get("qt"), str) and root["qt"], "missing Qt version")
    cases = root.get("cases")
    require(isinstance(cases, list) and len(cases) == 3, "expected three bracketed cases")
    require(tuple(s.get("name") for s in cases if isinstance(s, dict)) == NAMES,
            "noncanonical A1 / A-S-B-A / A2 ordering")
    for sample in cases:
        require(sample["valid_frames"] == count, "incomplete real-frame capture")
        require(sample["observed_width"] == 1920 and sample["observed_height"] == 1080,
                "physical 1080p pixel requirement not met")
        for field in ("process_cpu_ms", "wall_ms", "request_p50_ms", "request_p95_ms"):
            require(numeric(sample.get(field)) and sample[field] > 0,
                    f"missing or invalid {field}")
        require(sample["request_p95_ms"] >= sample["request_p50_ms"],
                "latency percentiles are inconsistent")
    a1, _, a2 = cases
    mean_cpu = (a1["process_cpu_ms"] + a2["process_cpu_ms"]) / 2
    spread = 100 * abs(a1["process_cpu_ms"] - a2["process_cpu_ms"]) / mean_cpu
    require(numeric(root.get("public_a1_a2_cpu_spread_percent"))
            and abs(root["public_a1_a2_cpu_spread_percent"] - spread) < 1e-5,
            "reported CPU drift differs from raw A1 and A2 samples")
    require(root.get("stable_reference_max_spread_percent") == STABILITY_PERCENT,
            "unknown baseline stability threshold")
    stable = spread <= STABILITY_PERCENT
    require(root.get("public_reference_stability")
            == ("STABLE" if stable else "UNSTABLE_BASELINE"),
            "incorrect stability classification")
    return {
        "backend": backend,
        "sha": sha,
        "stable": stable,
        "spread": spread,
        "samples": cases,
        "qt": root["qt"],
        "mode": root.get("probe_mode", "unknown"),
    }


def self_test():
    cases = [{"name": name, "valid_frames": 24, "observed_width": 1920,
              "observed_height": 1080, "process_cpu_ms": cpu, "wall_ms": 800.0,
              "request_p50_ms": 20.0, "request_p95_ms": 25.0}
             for name, cpu in zip(NAMES, (100.0, 120.0, 110.0))]
    root = {
        "schema": SCHEMA,
        "context": "HOSTED_SYNTHETIC_NOT_DEVICE_OR_NATIVE_GAIN",
        "route": "public_grabToImage",
        "native_performance_qualification": "NOT_MEASURED_DO_NOT_PROMOTE",
        "whole_process_cpu_not_gpu_time": True,
        "renderer": "opengl", "actual_1080p": True,
        "requested_width": 1920, "requested_height": 1080,
        "measured_per_case": 24, "warmup_per_case": 8,
        "source_sha": "a" * 40, "qt": "6.8.3", "cases": cases,
        "stable_reference_max_spread_percent": 15.0,
        "public_a1_a2_cpu_spread_percent": 100.0 * 10 / 105,
        "public_reference_stability": "STABLE",
    }
    assert evaluate(root, "opengl")["stable"]
    # Dynamic workload CPU must not be mistaken for a second static baseline.
    root["cases"][1]["process_cpu_ms"] = 750.0
    assert evaluate(root)["stable"]
    root["cases"][2]["process_cpu_ms"] = 200.0
    root["public_a1_a2_cpu_spread_percent"] = 100 * 100 / 150
    root["public_reference_stability"] = "UNSTABLE_BASELINE"
    assert not evaluate(root)["stable"]
    for key, invalid in [
        ("actual_1080p", False),
        ("source_sha", "unstamped"),
        ("route", "native"),
        ("native_performance_qualification", "GAIN_CONFIRMED"),
    ]:
        original = root[key]
        root[key] = invalid
        try:
            evaluate(root)
            raise AssertionError(f"unqualified {key} was accepted")
        except ValueError:
            pass
        finally:
            root[key] = original
    root["cases"][1]["valid_frames"] = 23
    try:
        evaluate(root)
        raise AssertionError("missing frame was accepted")
    except ValueError:
        pass
    print("PASS: public Qt scenegraph profile schema, drift, physical pixels, "
          "identity, no-native-promotion and incomplete-frame guards")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profiles", nargs="*", type=Path)
    parser.add_argument("--expected-backend", choices=("opengl", "software"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(bool(args.profiles), "provide one actual profile JSON")
    for path in args.profiles:
        profile = evaluate(json.loads(path.read_text(encoding="utf-8")),
                           args.expected_backend)
        print(f"PUBLIC_PROFILE={path.name} SOURCE_SHA={profile['sha']} "
              f"QT={profile['qt']} API={profile['backend']} "
              f"PROBE_MODE={profile['mode']}")
        for case in profile["samples"]:
            print(f"  CASE={case['name']} FRAMES={case['valid_frames']} "
                  f"PROCESS_CPU_MS={case['process_cpu_ms']:.3f} "
                  f"WALL_MS={case['wall_ms']:.3f} "
                  f"REQUEST_P95_MS={case['request_p95_ms']:.3f}")
        label = "STABLE" if profile["stable"] else "UNSTABLE_BASELINE"
        print(f"PUBLIC_BASELINE_STABILITY={label} "
              f"A1_A2_CPU_SPREAD_PERCENT={profile['spread']:.2f}")
        print("NATIVE_READBACK_GAIN=NOT_MEASURED; "
              "TARGET_RK3588_CPU=NOT_MEASURED; RELEASE_DECISION=HOLD")


if __name__ == "__main__":
    main()
