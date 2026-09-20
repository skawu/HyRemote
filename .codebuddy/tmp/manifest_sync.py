"""Apply the product-owner mandatory-set correction (#170/#174/#175/#176) plus the anti-leak regression check.

Edits are asserted: every replacement must match exactly once, or nothing is written for that file.
"""

import json
import pathlib

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
MANIFEST = WT / ".github/release/v1-mandatory-issues.json"
GATE = WT / "tests/release-readiness/check_release_authority_policy.cmake"

NEW_BLOCKERS = [170, 174, 175, 176]
CLASSIFIED = {
    14: "governance adoption gate; referenced as a process authority, its own acceptance is tracked by #14 itself",
    74: "CI runner infrastructure; referenced as an evidence caveat, not a product work item",
    90: "fixed defect referenced by the release documents as history",
    91: "fixed defect referenced by the release documents as history",
    95: "release-process authority (Git Flow, tags, release gates), not a product work item",
    106: "the V1 integration line itself; its completion is judged by the section-10 checklist, not as a work item",
    134: "qualification infrastructure draft track; explicitly not a V1 support claim",
    147: "the merged scope-convergence change, referenced as history",
    156: "handoff/continuation note, not a product work item",
    166: "post-V1 process improvement for bounded release-train windows",
}

# --- 1) the manifest -------------------------------------------------------------------------------
data = json.loads(MANIFEST.read_text(encoding="utf-8"))
required = [n for n in data["required_issue_numbers"] if n not in NEW_BLOCKERS] + NEW_BLOCKERS
required = sorted(required)
deferred = data["embedded_deferred_issue_numbers"]
overlap = set(required) & set(deferred)
assert not overlap, "an issue cannot be both mandatory and deferred: %s" % sorted(overlap)

lines = []
lines.append("{")
lines.append('  "schema": %d,' % data["schema"])
lines.append('  "version": "%s",' % data["version"])
lines.append('  "scope_authority": %d,' % data["scope_authority"])
lines.append('  "candidate_freeze_authority": %d,' % data["candidate_freeze_authority"])
lines.append('  "required_issue_numbers": [')
for i, n in enumerate(required):
    lines.append("    %d%s" % (n, "," if i + 1 < len(required) else ""))
lines.append("  ],")
lines.append('  "embedded_deferred_issue_numbers": [%s],' % ", ".join(str(n) for n in deferred))
lines.append('  "classified_referenced_issue_numbers": [')
keys = sorted(CLASSIFIED)
for i, n in enumerate(keys):
    lines.append("    %d%s" % (n, "," if i + 1 < len(keys) else ""))
lines.append("  ],")
notes = data.get("notes", {})
for n in NEW_BLOCKERS:
    notes.setdefault(str(n), "V1.0.0.0 blocker confirmed by the product-owner scope review of 2026-09-19.")
lines.append('  "notes": {')
note_items = list(notes.items())
for i, (k, v) in enumerate(note_items):
    lines.append('    %s: %s%s' % (json.dumps(k), json.dumps(v, ensure_ascii=False),
                                   "," if i + 1 < len(note_items) else ""))
lines.append("  }")
lines.append("}")
MANIFEST.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("manifest: required now %d items, newest %s" % (len(required), required[-4:]), flush=True)

# --- 2) the pinned expected list in the gate --------------------------------------------------------
text = GATE.read_text(encoding="utf-8")
old_list = """set(expected_v1_issues
    9 30 31 32 33 39 41 57 101 104 107 109 143 144 157 158 159 162 163 164 165)"""
assert text.count(old_list) == 1, "expected_v1_issues block not found exactly once"
new_list = """set(expected_v1_issues
    %s)""" % " ".join(str(n) for n in required)
text = text.replace(old_list, new_list, 1)

# --- 3) the anti-leak check -------------------------------------------------------------------------
anchor = 'set(expected_embedded_deferred 7 10 17 18)'
assert text.count(anchor) == 1, "embedded-deferred anchor not found exactly once"
anti_leak = '''set(expected_classified_referenced %s)
string(JSON classified_count LENGTH "${authority_json}" classified_referenced_issue_numbers)
math(EXPR classified_last "${classified_count} - 1")
set(actual_classified_referenced)
foreach(index RANGE 0 ${classified_last})
    string(JSON issue GET "${authority_json}" classified_referenced_issue_numbers ${index})
    list(APPEND actual_classified_referenced "${issue}")
endforeach()
if(NOT "${actual_classified_referenced}" STREQUAL "${expected_classified_referenced}")
    message(FATAL_ERROR
        "release-authority-policy: classified-reference list drifted. "
        "expected='${expected_classified_referenced}' actual='${actual_classified_referenced}'")
endif()

# Anti-leak: every issue number mentioned by an in-tree release authority must be classified, so a
# future V1-labelled blocker cannot silently exist outside the mandatory manifest.
set(declared_authority_documents
    docs/release-candidate-checklist.md
    docs/v1-ga-acceptance.md
    docs/development-roadmap.md
    docs/known-limitations.md
    docs/compatibility.md
    docs/v1-physical-acceptance.md
    docs/security-model.md)
set(known_issue_numbers ${expected_v1_issues} ${expected_embedded_deferred} ${expected_classified_referenced})
foreach(document IN LISTS declared_authority_documents)
    set(document_path "${HYREMOTE_SOURCE_DIR}/${document}")
    if(NOT EXISTS "${document_path}")
        message(FATAL_ERROR "release-authority-policy: declared authority document is missing: ${document}")
    endif()
    file(READ "${document_path}" document_text)
    string(REGEX MATCHALL "#[0-9]+" mentioned "${document_text}")
    foreach(token IN LISTS mentioned)
        string(REGEX REPLACE "#" "" number "${token}")
        if(NOT number IN_LIST known_issue_numbers)
            message(FATAL_ERROR
                "release-authority-policy: unclassified issue reference #${number} in ${document}; "
                "either add it to the mandatory set or classify it in "
                ".github/release/v1-mandatory-issues.json")
        endif()
    endforeach()
endforeach()
''' % " ".join(str(n) for n in keys)
text = text.replace(anchor, anchor + "\n\n" + anti_leak, 1)
GATE.write_text(text, encoding="utf-8")
print("gate: expected list updated and the anti-leak block inserted", flush=True)
