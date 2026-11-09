#!/usr/bin/env python3
"""Assess real source-property NOTIFY vs Qt render-loop self-trigger signals.

Source = QML root.phase notifySignal, connected through QMetaMethod to a
zero-ms QTimer with no capture side effects. A clean static 1V result can only
establish the safety of THIS PROPERTY, not automatic coverage of arbitrary QML,
custom scenegraph nodes, texture updates, animations or all Qt applications.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
from pathlib import Path
import statistics
import sys

PROPERTY_MARKER = "HYREMOTE_QML_PROPERTY_NOTIFIES_JSON="
RENDER_MARKER = "HYREMOTE_QUICK_SCENE_SIGNALS_JSON="
STAGE_MARKER = "HYREMOTE_DYNAMIC_STAGE_JSON="
CONNECTED_MARKER = "PROPERTY_NOTIFY_PROBE=CONNECTED_ROOT_PHASE"


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def p95(values):
    if not values:
        return None
    values = sorted(values)
    return values[math.ceil(len(values) * .95) - 1]


def load(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    require(CONNECTED_MARKER in text, f"{path}: QMetaProperty connection not confirmed")
    categories = {"property": [], "render": [], "source": []}
    for line in text.splitlines():
        for marker, kind in ((PROPERTY_MARKER, "property"),
                             (RENDER_MARKER, "render"),
                             (STAGE_MARKER, "source")):
            if marker in line:
                try:
                    obj = json.loads(line.split(marker, 1)[1])
                except json.JSONDecodeError as exc:
                    raise RuntimeError(f"{path}: truncated {kind} JSON") from exc
                categories[kind].append(obj)
                break
    for key in ("property", "render"):
        require(len(categories[key]) >= 10, f"{path}: insufficient {key} 1s records")
        require(all(a["utc_ms"] <= b["utc_ms"]
                    for a, b in zip(categories[key], categories[key][1:])),
                f"{path}: out-of-order {key} records")
    require(categories["source"], f"{path}: missing fixture stage changes")
    require(all(x.get("monitored_qml_property") == "root.phase"
                for x in categories["property"]),
            f"{path}: wrong QML property attribution")
    return categories


def summarize(record, dynamic):
    stages = record["source"]
    require((len(stages) >= 8) == dynamic,
            "Fixture did not execute expected static or dynamic mode")
    require(all(a.get("phase") != b.get("phase")
                for a, b in zip(stages, stages[1:])), "Source stages not alternating")
    property_counts = [int(row["phase_notify_1s"])
                       for row in record["property"]]
    render_counts = [int(row["after_animating_1s"])
                     for row in record["render"]]
    require(not any(row.get("phase_notify_overflow")
                    for row in record["property"]),
            "QML property notify ring overflow: invalid source correlation")
    phase_stamps = sorted(int(t)
                          for row in record["property"]
                          for t in row["phase_notify_utc_ms"])
    require(sum(property_counts) == len(phase_stamps),
            "Per-second source-property counter disagrees with timestamp log")
    latest = int(record["property"][-1]["utc_ms"])
    measured = []
    missing = 0
    eligible = 0
    if dynamic:
        for i, item in enumerate(stages[1:], start=1):
            start = int(item["utc_ms"])
            if start >= latest:
                continue  # final partial report window
            end = (int(stages[i + 1]["utc_ms"])
                   if i + 1 < len(stages) else start + 1200)
            end = min(end, latest)
            eligible += 1
            j = bisect.bisect_left(phase_stamps, start)
            if j >= len(phase_stamps) or phase_stamps[j] >= end:
                missing += 1
            else:
                measured.append(phase_stamps[j] - start)
    return {
        "seconds_property_logged": len(property_counts),
        "seconds_render_logged": len(render_counts),
        "source_stages": len(stages),
        "property_notify_total": len(phase_stamps),
        "property_notify_per_sec_median": statistics.median(property_counts),
        "property_notify_per_sec_p95": p95(property_counts),
        "render_after_animating_total": sum(render_counts),
        "render_after_animating_per_sec_median": statistics.median(render_counts),
        "render_after_animating_per_sec_p95": p95(render_counts),
        "eligible_source_phase_changes": eligible if dynamic else None,
        "source_phase_changes_without_property_notify": missing if dynamic else None,
        "source_change_to_property_notify_median_ms":
            statistics.median(measured) if measured else None,
        "source_change_to_property_notify_p95_ms": p95(measured),
    }


def assess(static0, static1, dynamic1):
    a = summarize(static0, False)
    b = summarize(static1, False)
    c = summarize(dynamic1, True)
    if a["property_notify_total"] or b["property_notify_total"]:
        verdict = "PROPERTY_NOTIFY_STATIC_CONTAMINATION_FAIL"
    elif c["eligible_source_phase_changes"] < 8 or c["source_phase_changes_without_property_notify"]:
        verdict = "PROPERTY_NOTIFY_DYNAMIC_INCOMPLETE_FAIL"
    else:
        verdict = "SCOPED_ROOT_PHASE_NOTIFY_CANDIDATE_ONLY"
    return {
        "schema": "hyremote-qml-source-property-notify-provenance/1",
        "verdict": verdict,
        "static_0v": a,
        "static_1v": b,
        "dynamic_1v": c,
        "what_it_proves": (
            "Only QML root.phase NOTIFY delivery on this test app, including "
            "its independence from public render-loop signals if static stayed quiet."
        ),
        "what_it_does_not_prove": (
            "Not generic Qt Quick scene damage: properties without NOTIFY, "
            "custom paint, texture/video, dynamic item trees, animations, "
            "multiple windows and arbitrary third-party widgets are UNTESTED."
        ),
        "direct_after_animating_wake": "REJECTED_FROM_PREVIOUS_A_SIGNAL",
        "runtime_producer": "NOT_CONNECTED",
        "product_pass": False,
        "independent_b": "FAIL_NOT_APPROVED_PREVIOUS_CANDIDATE",
        "site_handoff": "BLOCKED",
    }


def fixture(dynamic, static_render):
    prop = []
    renders = []
    stages = [{"phase": 0, "utc_ms": 1000}]
    if dynamic:
        stages += [{"phase": i % 2, "utc_ms": 1000 + 1200 * i}
                   for i in range(1, 11)]
    stamps = [int(x["utc_ms"]) + 4 for x in stages[1:]]
    for i in range(17):
        a = 1000 + 1000 * i
        included = [x for x in stamps if a <= x < a + 1000]
        prop.append({
            "utc_ms": a + 999,
            "monitored_qml_property": "root.phase",
            "phase_notify_1s": len(included),
            "phase_notify_utc_ms": included,
            "phase_notify_overflow": 0,
        })
        renders.append({
            "utc_ms": a + 999,
            "after_animating_1s": static_render + len(included),
        })
    return {"property": prop, "render": renders, "source": stages}


def self_test():
    base = fixture(False, 0)
    static = fixture(False, 4)
    changed = fixture(True, 4)
    out = assess(base, static, changed)
    require(out["verdict"] == "SCOPED_ROOT_PHASE_NOTIFY_CANDIDATE_ONLY",
            "Valid source QML notify rejected")
    require(out["static_1v"]["render_after_animating_total"] == 68
            and out["static_1v"]["property_notify_total"] == 0,
            "Render-loop self activity was misclassified as source property")
    require(out["dynamic_1v"]["source_change_to_property_notify_p95_ms"] == 4,
            "Source-to-notify timing incorrect")
    bad = fixture(False, 4)
    bad["property"][2]["phase_notify_utc_ms"] = [3001]
    bad["property"][2]["phase_notify_1s"] = 1
    require(assess(base, bad, changed)["verdict"] ==
            "PROPERTY_NOTIFY_STATIC_CONTAMINATION_FAIL",
            "Static contamination did not fail closed")
    missing = fixture(True, 4)
    missing["property"][3]["phase_notify_utc_ms"] = []
    missing["property"][3]["phase_notify_1s"] = 0
    require(assess(base, static, missing)["verdict"] ==
            "PROPERTY_NOTIFY_DYNAMIC_INCOMPLETE_FAIL",
            "Missed dynamic source NOTIFY did not fail closed")
    print("PASS semantic QML property provenance: clean source, render-self noise, missed scene")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--static-0v", type=Path)
    p.add_argument("--static-1v", type=Path)
    p.add_argument("--dynamic-1v", type=Path)
    p.add_argument("--out", type=Path)
    args = p.parse_args()
    if args.self_test:
        self_test()
        return
    require(all((args.static_0v, args.static_1v, args.dynamic_1v, args.out)),
            "Requires all --static-0v, --static-1v, --dynamic-1v and --out")
    result = assess(load(args.static_0v), load(args.static_1v), load(args.dynamic_1v))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")
    print("PROPERTY_NOTIFY_PROVENANCE=" + result["verdict"])
    print("EVIDENCE=" + str(args.out))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as exc:
        print("PROPERTY_NOTIFY_PROVENANCE=BLOCKED_OR_FAILED REASON=" + str(exc),
              file=sys.stderr, flush=True)
        sys.exit(1)
