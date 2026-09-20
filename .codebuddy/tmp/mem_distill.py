"""Bounded distillation of MEMORY.md: archive first, then remove only provably superseded sections.

Only sections whose content is an inventory of a state that has already been superseded are removed, and the
original is archived in full so nothing is lost. The script prints every whitelist entry that matched nothing,
so a silent mismatch is impossible.
"""

import pathlib
import shutil

MEM = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/memory")
TMP = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp")
src = MEM / "MEMORY.md"

# Sections removed because this session has first-hand evidence that their state is superseded.
REMOVE = [
    "V1 scope criterion (superseded",
    "What V1.0.0.0 still needs (earlier measurement",
    "[SUPERSEDED 2026-09-19 by PR #147/#157",
    "Documentation system and directory structure: **LANDED**",
    "History of the same matter, as it looked before the merge",
    "Delegated V1 queue: absorbed by the owner's own commits",
    "Delegated V1 queue: branch layout (originally prepared 2026-09-17)",
    "Delegated V1 queue: branch layout (originally prepared 2026-09-17, pre-integration)",
    "Delivered PRs from the 2026-09-17 work session",
    "Hosted-CI defect inventory on the V1 line",
    "V1 status: converged candidate",
    "V1 candidate `6e861e4`: remaining Linux failures",
    "V1 candidate `6e861e4` and the Linux QPA test wiring defect",
    "Structure/docs/examples batch (branch",
    "Review baseline (full A-H review",
]

# A heading that says "OPEN V1 BLOCKER" while its own body says RESOLVED is exactly how this file misled me.
RETITLE = [
    (
        "OPEN V1 BLOCKER: one remote key press reaches the application twice",
        "Remote key press looked doubled: measured to be an observation artifact, not a defect (resolved 2026-09-18)",
    ),
]

text = src.read_text(encoding="utf-8")
lines = text.splitlines()
before_bytes = len(text)

starts = [i for i, ln in enumerate(lines) if ln.startswith("## ")]
preamble = lines[: starts[0]]
sections = []
for idx, s in enumerate(starts):
    end = starts[idx + 1] if idx + 1 < len(starts) else len(lines)
    sections.append(lines[s:end])

removed, kept, retitled, matched = [], [], [], set()
for sec in sections:
    head = sec[0][3:].strip()
    hit = next((p for p in REMOVE if head.startswith(p)), None)
    if hit:
        removed.append(head)
        matched.add(hit)
        continue
    for pre, new in RETITLE:
        if head.startswith(pre):
            sec = ["## " + new] + sec[1:]
            retitled.append(new)
            break
    kept.append(sec)

missing = [p for p in REMOVE if p not in matched]

archive = MEM / "archive"
archive.mkdir(parents=True, exist_ok=True)
shutil.copy2(src, archive / "MEMORY-2026-09-19-pre-distill.md")

current = (TMP / "mem-current-truth.md").read_text(encoding="utf-8").strip()
out = list(preamble) + ["", current, ""]
for sec in kept:
    out += sec + [""]
new_text = "\n".join(out).rstrip() + "\n"
src.write_text(new_text, encoding="utf-8")

print("sections before      : %d" % len(sections))
print("sections removed     : %d" % len(removed))
for h in removed:
    print("   - %s" % h[:96])
print("sections retitled    : %d" % len(retitled))
for h in retitled:
    print("   ~ %s" % h[:96])
print("sections kept        : %d" % len(kept))
print("bytes before         : %d" % before_bytes)
print("bytes after          : %d" % len(new_text))
print("reduction            : %d bytes (%.1f%%)" % (before_bytes - len(new_text), 100.0 * (before_bytes - len(new_text)) / before_bytes))
print("whitelist not matched: %s" % (missing if missing else "none"))
print("archive              : %s" % (archive / "MEMORY-2026-09-19-pre-distill.md"))
