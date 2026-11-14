#!/usr/bin/env python3
"""#949 non-product result summarizer.

Turns the raw `win949_probe.py` JSON documents in one directory into the A/B/A2
comparison table used in the local verification report. Reading only what the
probe already recorded, it applies three explicit sample rules so no arm is
scored with a different yardstick:

* ``valid``   : 0 ms <= upper < 1990 ms
* ``aliased`` : upper >= 1990 ms - the 8-phase / 250 ms marker palette cannot
  resolve a lag of two seconds or more, so the value is real staleness but not
  an exact latency.
* ``invalid`` : upper < 0 ms - the mark match picked a phase newer than the
  observation, which is physically impossible and therefore a probe artefact.

Nothing here changes a product decision; it only reports measurements.

Usage:
    python tools/perf/tests/win949_frame_first/summarize_runs.py \
        --runs tmp/win949/runs [--markdown tmp/win949/summary.md]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ALIAS_MS = 1990.0


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return ordered[index]


def stats(values):
    if not values:
        return {"n": 0, "p50": None, "p95": None, "p99": None}
    return {
        "n": len(values),
        "p50": round(percentile(values, 0.50), 1),
        "p95": round(percentile(values, 0.95), 1),
        "p99": round(percentile(values, 0.99), 1),
    }


def stage_calls(document, names):
    stages = document.get("stage_totals") or {}
    return sum(int((stages.get(name) or {}).get("calls") or 0) for name in names)


NATIVE_D3D11 = ("quick_d3d11_staging_reuse", "quick_d3d11_staging_allocation")
NATIVE_GL = ("quick_gl_gpu_map_copy", "quick_gl_pbo_reuse", "quick_gl_render_callback")


def route_of(document):
    route = document.get("capture_route") or "UNKNOWN"
    if not route.startswith("UNREPORTED"):
        return route
    if stage_calls(document, NATIVE_D3D11):
        return "D3D11_STAGING_SOURCE(stages)"
    if stage_calls(document, NATIVE_GL):
        return "GL_RHI_PBO_SOURCE(stages)"
    if stage_calls(document, ("quick_grab_submit",)):
        return "PUBLIC_QT_FALLBACK(stages)"
    return route


def summarize_document(document):
    samples = [item for item in document.get("samples", []) if not item.get("warmup")]
    upper = [item["upper_ms"] for item in samples]
    valid = [value for value in upper if 0.0 <= value < ALIAS_MS]
    aliased = [value for value in upper if value >= ALIAS_MS]
    invalid = [value for value in upper if value < 0.0]
    cpu = document.get("host_process_cpu_percent") or {}
    return {
        "label": document.get("label"),
        "arm": document.get("arm"),
        "backend": document.get("backend_requested"),
        "route": route_of(document),
        "pixel_truth": document.get("pixel_truth"),
        "framebuffer": document.get("framebuffer_observed"),
        "latency_all": stats(upper),
        "latency_valid": stats(valid),
        "aliased": len(aliased),
        "invalid": len(invalid),
        "cpu_p50": cpu.get("p50"),
        "cpu_p95": cpu.get("p95"),
        "native_calls": stage_calls(document, NATIVE_D3D11) + stage_calls(document, NATIVE_GL),
        "public_grab_calls": stage_calls(document, ("quick_grab_submit",)),
        "fallback": stage_calls(document, ("quick_native_fallback",)),
        "stale_drop": stage_calls(document, ("quick_stale_drop",)),
        "errors": document.get("errors") or [],
        "json": document.get("_path"),
    }


def format_row(entry):
    latency = entry["latency_valid"]
    return (
        f"| {entry['arm']} | {entry['backend']} | {entry['label'].split('-', 2)[-1]} | "
        f"{entry['route']} | {entry['pixel_truth']} | "
        f"{latency['n']} / {latency['p50']} / {latency['p95']} / {latency['p99']} | "
        f"{entry['aliased']} | {entry['invalid']} | "
        f"{entry['cpu_p50']} / {entry['cpu_p95']} | "
        f"{entry['native_calls']} / {entry['public_grab_calls']} | "
        f"{entry['fallback']} / {entry['stale_drop']} |"
    )


HEADER = (
    "| arm | backend | scenario | route | pixels | valid n/p50/p95/p99 (ms) | aliased | invalid | "
    "cpu p50/p95 (%) | native/public calls | fallback/stale |\n"
    "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"
)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runs", required=True, help="directory of win949 probe JSON files")
    parser.add_argument("--markdown", default=None, help="optional markdown output path")
    parser.add_argument("--include", default="", help="only labels containing this substring")
    args = parser.parse_args(argv)

    entries = []
    for path in sorted(Path(args.runs).glob("*.json")):
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if document.get("schema") != "hyremote-win949-probe/1":
            continue
        if args.include and args.include not in (document.get("label") or ""):
            continue
        document["_path"] = str(path)
        entries.append(summarize_document(document))

    order = {"A1": 0, "B": 1, "A2": 2, "QT5A": 3, "QT5B": 4}
    entries.sort(key=lambda item: (order.get(item["arm"], 9), item["backend"] or "",
                                   item["label"] or ""))
    lines = [HEADER] + [format_row(entry) for entry in entries]
    text = "\n".join(lines)
    print(text)
    if args.markdown:
        Path(args.markdown).write_text(text + "\n", encoding="utf-8")
        print(f"\nwritten: {args.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
