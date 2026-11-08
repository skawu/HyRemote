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
from tempfile import TemporaryDirectory


NANOS_PER_SECOND = 1_000_000_000
STAGES = (
    "widget_buffer", "widget_paint", "quick_accepted_requests",
    "quick_gui_batches", "quick_grab_submit", "quick_grab_ready_wall",
    "quick_convert", "quick_buffer", "quick_copy", "generic_compose",
    "watermark_compose", "watermark_reuse", "rfb_compare", "rfb_encode", "rfb_write",
)
NEW_STAGES = frozenset({"quick_accepted_requests", "quick_gui_batches",
                        "watermark_compose", "watermark_reuse"})
COUNT_ONLY = frozenset({"quick_accepted_requests", "quick_gui_batches", "watermark_reuse"})
BACKENDS = ("unknown", "software", "opengl", "d3d11", "d3d12", "vulkan", "metal", "other")
WALL_ONLY = frozenset({"quick_grab_ready_wall"})


def load(path: Path):
    samples = []
    with path.open(encoding="utf-8-sig") as stream:
        for line in stream:
            if not line.strip():
                continue
            row = json.loads(line)
            schema = row.get("schema")
            if schema not in (1, 2) or not isinstance(row.get("stages"), dict):
                raise ValueError(f"{path}: wrong probe schema")
            if not row.get("process_cpu_valid"):
                raise ValueError(f"{path}: process CPU clock unavailable")
            mandatory = STAGES if schema == 2 else tuple(
                stage for stage in STAGES if stage not in NEW_STAGES)
            if any(stage not in row["stages"] for stage in mandatory):
                raise ValueError(f"{path}: incomplete stage accounting")
            if schema == 2:
                counts = row.get("quick_grab_backend_counts")
                if not isinstance(counts, dict) or any(
                        not isinstance(counts.get(backend), int)
                        or counts[backend] < 0 for backend in BACKENDS):
                    raise ValueError(f"{path}: missing/invalid Qt backend counts")
            samples.append(row)
    if len(samples) < 2:
        raise ValueError(f"{path}: at least two snapshots required")
    fields = ("calls", "cpu_ns", "wall_ns", "bytes", "cpu_samples_missing")
    for a, b in zip(samples, samples[1:]):
        if (b["elapsed_ms"] <= a["elapsed_ms"] or
                b["pid"] != a["pid"] or
                b["schema"] != a["schema"] or
                b["process_cpu_ns"] < a["process_cpu_ns"]):
            raise ValueError(f"{path}: nonmonotone time/CPU or multiple process IDs")
        for stage in (STAGES if b["schema"] == 2 else
                      tuple(stage for stage in STAGES if stage not in NEW_STAGES)):
            before = a["stages"][stage]
            after = b["stages"][stage]
            if (any(k not in before or k not in after for k in fields) or
                    any(not isinstance(before[k], int) or not isinstance(after[k], int)
                        or before[k] < 0 or after[k] < before[k] for k in fields)):
                raise ValueError(f"{path}: invalid/nonmonotone {stage} stage counters")
        if b["schema"] == 2 and any(
                b["quick_grab_backend_counts"][backend]
                < a["quick_grab_backend_counts"][backend] for backend in BACKENDS):
            raise ValueError(f"{path}: nonmonotone Qt backend counters")
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
        if stage not in first["stages"]:
            continue  # Old probes never measured this; do not invent zero cost.
        a = first["stages"][stage]
        b = last["stages"][stage]
        fields = ("calls", "cpu_ns", "wall_ns", "bytes", "cpu_samples_missing")
        if any(b[k] < a[k] for k in fields):
            raise ValueError(f"{stage}: nonmonotone counters")
        stages[stage] = {k: b[k] - a[k] for k in fields}
    backend_counts = None
    if first["schema"] == 2:
        backend_counts = {
            backend: last["quick_grab_backend_counts"][backend]
                     - first["quick_grab_backend_counts"][backend]
            for backend in BACKENDS
        }
    return {"seconds": window_sec, "process_cpu_seconds": process_seconds,
            "schema": first["schema"], "stages": stages,
            "backends": backend_counts}


def print_result(label, result):
    print(f"{label}: {result['seconds']:.1f} s, process CPU "
          f"{result['process_cpu_seconds']:.4f} s "
          f"({100*result['process_cpu_seconds']/result['seconds']:.2f}% of one core)")
    measured = 0.0
    for stage in STAGES:
        if stage not in result["stages"]:
            print(f"  {stage:24} NOT_INSTRUMENTED (older schema)")
            continue
        s = result["stages"][stage]
        if stage in COUNT_ONLY:
            print(f"  {stage:24} count={s['calls']:5} (not CPU time)")
            continue
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
    if result["backends"] is not None:
        print("  public Qt Quick grab submissions by reported scenegraph API: "
              + ", ".join(f"{api}={count}" for api, count
                          in result["backends"].items()))
        print("  These are public Qt grab submissions, NOT native GPU readback hits.")
    unknown = result["process_cpu_seconds"] - measured
    print(f"  {'UNATTRIBUTED/OVERLAP':24} cpu_s={unknown:8.4f} "
          "(includes Qt render/driver threads, uncaptured stages and probe overhead;"
          " negative means overlapping scopes or measurement error)")
    print("  WARNING: Thread CPU and asynchronous GPU wall latency are NOT interchangeable.")


def normalized_cpu_delta(b1, b2, stage, nominal_seconds):
    """Return CPU seconds scaled to a common nominal sampling window."""
    def rate(result):
        if stage is None:
            cpu = result["process_cpu_seconds"]
        else:
            cpu = result["stages"][stage]["cpu_ns"] / NANOS_PER_SECOND
        return cpu / result["seconds"]
    return (rate(b2) - rate(b1)) * nominal_seconds


def compare(b1, b2, nominal_seconds):
    print(f"\nB2 − B1 CPU deltas NORMALIZED to {nominal_seconds}s "
          "(matched build/workload; actual durations may differ):")
    process_delta = normalized_cpu_delta(b1, b2, None, nominal_seconds)
    print(f"  process_cpu_seconds = {process_delta:+.4f} "
          f"({100*process_delta/nominal_seconds:+.3f} percentage points of one core)")
    stage_sum = 0.0
    for stage in STAGES:
        if stage in WALL_ONLY or stage in COUNT_ONLY:
            continue
        if stage not in b1["stages"] or stage not in b2["stages"]:
            print(f"  {stage:24} NOT_COMPARABLE: older schema")
            continue
        delta = normalized_cpu_delta(b1, b2, stage, nominal_seconds)
        stage_sum += delta
        a = b1["stages"][stage]
        b = b2["stages"][stage]
        calls_per_sec = b["calls"]/b2["seconds"] - a["calls"]/b1["seconds"]
        print(f"  {stage:24} {delta:+.4f} "
              f"calls_per_second_delta={calls_per_sec:+.3f}")
    residual = process_delta - stage_sum
    print(f"  {'UNKNOWN_RESIDUAL':24} {residual:+.4f} "
          "(normalized process difference minus measured stage difference)")
    print("  These are instrumented CPU-clock intervals, not sampling call-stack "
          "attribution or causal proof. Report each actual window and repeat.")


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

    # A trace may have a large intermediate counter then reset to a small
    # ending value. Endpoints alone must never conceal that corruption.
    middle = {"schema": 1, "pid": 1, "elapsed_ms": 30000,
              "process_cpu_ns": 1000000000, "process_cpu_valid": True,
              "stages": {key: dict(value) for key, value in b["stages"].items()}}
    middle["stages"]["rfb_compare"]["cpu_ns"] = 9000000000
    with TemporaryDirectory() as directory:
        path = Path(directory) / "regressed.jsonl"
        path.write_text("\n".join(json.dumps(row) for row in (a, middle, b)) + "\n",
                        encoding="utf-8")
        try:
            load(path)
            raise AssertionError("intermediate stage-counter reset accepted")
        except ValueError as e:
            assert "nonmonotone rfb_compare" in str(e)

    # Original comparison was invalid for unequal but accepted windows: both
    # runs use exactly 10% of a core for 38s/42s, so normalized CPU delta is 0.
    cpu_b1 = {"seconds": 38.0, "process_cpu_seconds": 3.8,
              "stages": {key: {"cpu_ns": 0} for key in STAGES}}
    cpu_b2 = {"seconds": 42.0, "process_cpu_seconds": 4.2,
              "stages": {key: {"cpu_ns": 0} for key in STAGES}}
    assert abs(normalized_cpu_delta(cpu_b1, cpu_b2, None, 40)) < 1e-9
    print("PASS: #609 schema, intermediate monotonicity, normalized windows, "
          "async-wall isolation")


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
    if b1["schema"] != b2["schema"]:
        raise ValueError("cannot compare different probe schemas; use one build")
    print_result("B1", b1)
    print_result("B2", b2)
    compare(b1, b2, args.seconds)


if __name__ == "__main__":
    main()
