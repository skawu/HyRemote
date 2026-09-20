"""Survey the memory directory and MEMORY.md's section structure before any distillation.

Distillation must be evidence-driven: measure first, then merge by topic, and keep the original
archived so nothing is silently lost.
"""

import pathlib
import re

MEM = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/memory")

print("=== memory directory ===", flush=True)
for p in sorted(MEM.iterdir()):
    kind = "dir " if p.is_dir() else "file"
    size = "" if p.is_dir() else "%8d" % p.stat().st_size
    print("%s %s %s" % (kind, size, p.name), flush=True)

target = MEM / "MEMORY.md"
text = target.read_text(encoding="utf-8")
lines = text.splitlines()
print("", flush=True)
print("=== MEMORY.md: %d chars, %d lines ===" % (len(text), len(lines)), flush=True)

starts = [(i, l[3:].strip()) for i, l in enumerate(lines) if l.startswith("## ")]
print("sections = %d" % len(starts), flush=True)
print("%-4s %-6s %-7s %s" % ("#", "line", "bytes", "heading"), flush=True)
for idx, (start, head) in enumerate(starts):
    end = starts[idx + 1][0] if idx + 1 < len(starts) else len(lines)
    chunk = "\n".join(lines[start:end])
    print("%-4d %-6d %-7d %s" % (idx + 1, start + 1, len(chunk), head[:88]), flush=True)
