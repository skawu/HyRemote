#!/usr/bin/env python3
"""#609: summarize opt-in Windows Host CPU probes without elevation.

Input files are JSONL written by the *diagnostic-only* HyRemote runtime.
No raw framebuffer, user input, tokens, addresses or ETW traces are parsed.
The per-stage CPU clocks are thread-local; asynchronous Quick GPU work and
uncategorized Qt/driver threads stay in the explicitly reported remainder.
"""
import argparse
import json
from pathlib import Path


NANOS_PER_SECOND = 1_000_000_000
STAGES = (
    "widget_buffer", "widget_paint", "quick_grab_submit",
    "quick_grab_ready_wall", "quick_convert", "quick_buffer",
    "quick_copy", "generic_compose", "rfb_compare", "rfb_encode", "rfb_write",
)
WALL_ONLY = frozenset({"quick_grab_ready_wall"})


def load(path: Path):
    samples = []
    with path.open(encoding="utf-8-sig") as stream:
        for line in stream:
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("schema") != 1 or not isinstance(row.get("stages"), dict):
                raise ValueError(f"{path}: wrong probe schema")
            if not row.get("process_cpu_valid"):
                raise ValueError(f"{path}: process CPU clock unavailable")
            if any(stage not in row["stages"] for stage in STAGES):
                raise ValueError(f"{path}: incomplete stage accounting")
            samples.append(row)
    if len(samples) < 2:
        raise ValueError(f"{path}: at least two snapshots required")
    if any(b["elapsed_ms"] <= a["elapsed_ms"] or
           b["pid"] != a["pid"] or
           b["process_cpu_ns"] < a["process_cpu_ns"]
           for a, b in zip(samples, samples[1:])):
        raise ValueError(f"{path}: nonmonotone time/CPU or multiple process IDs")
    return samples


def measure(samples, warmup_ms: int, seconds: int):
    selected = [s for s in samples if s["elapsed_ms"] >= warmup_ms]
    if len(selected) < 2:
        raise ValueError("not enough snapshots after warm-up")
    first = selected[0]
    target = first["elapsed_ms"] + seconds * 1000
    last = min(selected[1:], key=lambda s: abs(s["elapsed_ms"] - target))
    window_sec = (last["elapsed_ms"] - first["elapsed_ms"]) / 1000.0
    if window_sec < seconds - 2 or window_sec > seconds + 2:
        raise ValueError(f"wanted {seconds}s, got {window_sec:.1f}s; extend the run")
    process_seconds = (last["process_cpu_ns"] - first["process_cpu_ns"]) / NANOS_PER_SECOND
    stages = {}
    for stage in STAGES:
        a = first["stages"][stage]
        b = last["stages"][stage]
        fields = ("calls", "cpu_ns", "wall_ns", "bytes", "cpu_samples_missing")
        if any(b[k] < a[k] for k in fields):
            raise ValueError(f"{stage}: nonmonotone counters")
        stages[stage] = {k: b[k] - a[k] for k in fields}
    return {"seconds": window_sec, "process_cpu_seconds": process_seconds, "stages": stages}


def print_result(label, result):
    print(f"{label}: {result['seconds']:.1f} s, process CPU "
          f"{result['process_cpu_seconds']:.4f} s "
          f"({100*result['process_cpu_seconds']/result['seconds']:.2f}% of one core)")
    measured = 0.0
    for stage in STAGES:
        s = result["stages"][stage]
        if stage in WALL_ONLY:
            print(f"  {stage:24} calls={s['calls']:5} "
                  f"async_latency_wall_s={s['wall_ns']/NANOS_PER_SECOND:.4f}"
                  "  (NOT CPU)")
            continue
        cpu = s["cpu_ns"] / NANOS_PER_SECOND
        measured += cpu
        print(f"  {stage:24} calls={s['calls']:5} cpu_s={cpu:8.4f} "
              f"wall_s={s['wall_ns']/NANOS_PER_SECOND:8.4f} "
              f"bytes={s['bytes']:10} cpu_clock_misses={s['cpu_samples_missing']}")
    unknown = result["process_cpu_seconds"] - measured
    print(f"  {'UNATTRIBUTED/OVERLAP':24} cpu_s={unknown:8.4f} "
          "(includes Qt render/driver threads, uncaptured stages and probe overhead;"
          " negative means overlapping scopes or measurement error)")
    print("  WARNING: Thread CPU and asynchronous GPU wall latency are NOT interchangeable.")


def compare(b1, b2):
    print("\nB2 − B1 CPU-second differences (matching diagnostic build and workload only):")
    print(f"  process_cpu_seconds = "
          f"{b2['process_cpu_seconds'] - b1['process_cpu_seconds']:+.4f}")
    for stage in STAGES:
        if stage in WALL_ONLY:
            continue
        a = b1["stages"][stage]
        b = b2["stages"][stage]
        print(f"  {stage:24} "
              f"{(b['cpu_ns']-a['cpu_ns'])/NANOS_PER_SECOND:+.4f} "
              f"calls={b['calls']-a['calls']:+d}")
    residual = (
        b2["process_cpu_seconds"]-b1["process_cpu_seconds"] -
        sum((b2["stages"][stage]["cpu_ns"]-b1["stages"][stage]["cpu_ns"])
            / NANOS_PER_SECOND for stage in STAGES if stage not in WALL_ONLY)
    )
    print(f"  {'UNKNOWN_RESIDUAL':24} {residual:+.4f}")
    print("  Treat these as instrumented CPU-clock intervals, not statistical "
          "call-stack attribution or causal proof. Verify three repeated runs.")


def self_test():
    base = {stage: {"calls": 0, "cpu_ns": 0, "wall_ns": 0,
                    "bytes": 0, "cpu_samples_missing": 0} for stage in STAGES}
    a = {"schema": 1, "pid": 1, "elapsed_ms": 10000, "process_cpu_ns": 100000000,
         "process_cpu_valid": True, "stages": base}
    b = {"schema": 1, "pid": 1, "elapsed_ms": 50000, "process_cpu_ns": 2100000000,
         "process_cpu_valid": True,
         "stages": {key: dict(val) for key, val in base.items()}}
    b["stages"]["rfb_compare"]["calls"] = 12
    b["stages"]["rfb_compare"]["cpu_ns"] = 300000000
    b["stages"]["quick_grab_ready_wall"]["wall_ns"] = 1500000000
    got = measure([a, b], 10000, 40)
    assert abs(got["process_cpu_seconds"] - 2) < 1e-9
    assert got["stages"]["rfb_compare"]["calls"] == 12
    assert got["stages"]["quick_grab_ready_wall"]["cpu_ns"] == 0
    try:
        measure([a, b], 10000, 50)
        raise AssertionError("short window accepted")
    except ValueError:
        pass
    print("PASS: #609 aggregation schema/delta/window and async-wall isolation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--b1", type=Path, help="0-Viewer trace JSONL")
    parser.add_argument("--b2", type=Path, help="1-static-Viewer trace JSONL")
    parser.add_argument("--warmup-seconds", type=int, default=10)
    parser.add_argument("--seconds", type=int, default=40)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.b1 or not args.b2:
        parser.error("both --b1 and --b2 are required")
    if args.warmup_seconds < 0 or args.seconds < 5:
        parser.error("warm-up must be nonnegative and sample duration >=5s")
    b1 = measure(load(args.b1), args.warmup_seconds * 1000, args.seconds)
    b2 = measure(load(args.b2), args.warmup_seconds * 1000, args.seconds)
    print_result("B1", b1)
    print_result("B2", b2)
    compare(b1, b2)


if __name__ == "__main__":
    main()
