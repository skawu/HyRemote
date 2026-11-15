#!/usr/bin/env python3
"""PreToolUse guard: keep every write inside the workspace.

Mounted from the project `.codebuddy/settings.json` PreToolUse hook, next to
`guard_scratch_writes.py`.

Policy (owner decision, 2026-11-11):
  - every operation inside the workspace root, including all subdirectories, is
    allowed;
  - every non-read operation outside the workspace root is denied;
  - read-only operations outside the workspace stay allowed.

Non-read means: any file tool (Write/Edit/MultiEdit/NotebookEdit/delete_file),
and any shell command whose destination, deletion target or created directory
resolves outside the workspace root.

Contract:
  - deny  when a file tool targets a path outside the workspace root;
  - deny  when a shell command writes, deletes or creates outside the root;
  - allow read-only operations, inside or outside (`Get-Content`, `cat`, `dir`,
    `Get-ChildItem`, `Test-Path`, `Select-String`, ...);
  - allow on ANY internal error (fail-open). A broken guard must never block
    normal work inside the workspace.

Output follows the documented hook protocol; exit code is always 0.

Modes:
  (no args)   evaluate the hook payload read from stdin
  --self-test run positive/negative classification cases and exit
"""

from __future__ import annotations

import json
import os
import posixpath
import re
import sys
import threading

WORKSPACE = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)
FILE_TOOLS = {
    "Write", "Edit", "MultiEdit", "NotebookEdit", "delete_file",
}
SHELL_TOOLS = {"Bash", "Shell", "run_terminal_cmd", "execute_command"}

# Verbs whose arguments are destinations or deletion targets.
_DESTRUCTIVE_VERBS = {
    "copy-item", "move-item", "cp", "mv", "copy", "rename",
    "set-content", "add-content", "out-file", "tee-object", "tee",
    "new-item", "mkdir", "md", "touch",
    "remove-item", "rm", "del", "erase", "rd", "rmdir",
}
_VERB_RE = re.compile(
    r"(?is)\b(copy-item|move-item|set-content|add-content|out-file|tee-object|"
    r"new-item|remove-item|mkdir|rmdir|rename|cp|mv|copy|tee|del|erase|rd|rm|md|touch)\b([^|;]*)"
)
_REDIRECT_RE = re.compile(r"""(?is)>>?\s*("[^"]+"|'[^']+'|[^\s"'|&;<>()]+)""")
_OPEN_WRITE_RE = re.compile(
    r"""(?is)open\s*\(\s*([^,)]+?)\s*,\s*["'](?:w|a|w\+|a\+|wb|ab|x|xb)"""
)
# `-Path`/`-Force` style flags, and single-letter Windows switches such as
# `rd /s /q`. A longer `/srv/outside` token is a POSIX path, not a switch.
_OPTION_FLAG = re.compile(r"^-[A-Za-z]")
_WIN_SWITCH = re.compile(r"^/[A-Za-z]$")


def _norm(path: str) -> str:
    return os.path.normcase(os.path.normpath(os.path.abspath(path)))


def _inside(path: str, root: str = WORKSPACE) -> bool:
    target = _norm(path)
    base = _norm(root)
    return target == base or target.startswith(base + os.sep)


def _resolve_token(token: str, root: str = WORKSPACE) -> str:
    """Resolve a shell token to a comparable absolute path."""
    raw = token.strip().strip("\"'")
    raw = raw.rstrip(",;)")
    if not raw:
        return ""
    lowered = raw.replace("\\", "/")
    if lowered.startswith("~"):
        return os.path.expanduser(raw)
    if lowered.startswith("/") and not re.match(r"^[a-z]:/", lowered):
        # POSIX-style absolute path. Judge it lexically: it is outside the
        # Windows workspace root by construction.
        return os.path.normpath(raw)
    if re.match(r"^[a-z]:/", lowered):
        return os.path.normpath(raw)
    # Relative to the workspace root (hook cwd).
    return os.path.join(root, posixpath.normpath(lowered))


def _destinations(command: str) -> list[str]:
    """Best-effort list of paths this command writes, deletes or creates."""
    found: list[str] = []
    for match in _REDIRECT_RE.finditer(command):
        found.append(match.group(1))
    for match in _VERB_RE.finditer(command):
        verb = match.group(1).lower()
        args = match.group(2) or ""
        positional = [
            token
            for token in re.findall(r""""[^"]+"|'[^']+'|[^\s"']+""", args)
            if token and not _OPTION_FLAG.match(token) and not _WIN_SWITCH.match(token)
        ]
        if not positional:
            continue
        if verb in {"copy-item", "move-item", "cp", "mv", "copy", "rename"}:
            found.append(positional[-1])
        else:
            found.extend(positional[:1] if verb in {"set-content", "add-content", "out-file"} else positional)
    for match in _OPEN_WRITE_RE.finditer(command):
        found.append(match.group(1))
    return found


def classify(tool_name: str, file_path: str) -> str:
    """File-tool decision. Pure."""
    if tool_name not in FILE_TOOLS:
        return "allow"
    if not file_path:
        return "allow"
    return "allow" if _inside(file_path) else "deny"


def classify_shell(command: str) -> str:
    """Shell decision. Deny only when a destination is clearly outside. Pure."""
    if not command:
        return "allow"
    try:
        targets = _destinations(command)
    except Exception:
        return "allow"
    for token in targets:
        resolved = _resolve_token(token)
        if not resolved:
            continue
        if not _inside(resolved):
            return "deny"
    return "allow"


def _read_stdin(timeout: float = 3.0) -> str:
    chunks: list[str] = []
    done = threading.Event()

    def _reader() -> None:
        try:
            chunks.append(sys.stdin.read())
        except Exception:
            pass
        finally:
            done.set()

    thread = threading.Thread(target=_reader, daemon=True)
    thread.start()
    done.wait(timeout)
    return "".join(chunks)


def _payload(raw: str) -> tuple[str, str, str]:
    try:
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        return "", "", ""
    if not isinstance(data, dict):
        return "", "", ""
    tool_name = str(data.get("tool_name") or data.get("toolName") or "")
    tool_input = data.get("tool_input") or data.get("toolInput") or {}
    if not isinstance(tool_input, dict):
        return tool_name, "", ""
    file_path = (
        tool_input.get("file_path")
        or tool_input.get("filePath")
        or tool_input.get("path")
        or tool_input.get("target_file")
        or ""
    )
    command = tool_input.get("command") or tool_input.get("cmd") or ""
    return tool_name, str(file_path), str(command)


def _emit(decision: str, reason: str) -> None:
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": decision,
            "permissionDecisionReason": reason,
        }
    }))
    sys.stdout.flush()


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        file_cases = [
            # inside the workspace: everything allowed
            (os.path.join(WORKSPACE, "tmp", "evidence.json"), "allow"),
            (os.path.join(WORKSPACE, "tmp", "deep", "nested", "x.log"), "allow"),
            (os.path.join(WORKSPACE, "src", "runtime", "src", "main.cpp"), "allow"),
            (os.path.join(WORKSPACE, ".codebuddy", "settings.json"), "allow"),
            (os.path.join(WORKSPACE, "tmp", "..", "src", "ok.cpp"), "allow"),
            # outside the workspace: denied
            (r"C:\Windows\win.ini", "deny"),
            (r"C:\Users\maoxu\.codebuddy\settings.json", "deny"),
            (r"D:\other\repo\file.txt", "deny"),
            (os.path.join(os.path.dirname(WORKSPACE), "sibling", "x.txt"), "deny"),
            (os.path.join(WORKSPACE, "..", "escape.txt"), "deny"),
        ]
        shell_cases = [
            # inside writes stay allowed
            ("echo x > tmp/evidence.json", "allow"),
            ("Set-Content tmp/report.txt DATA", "allow"),
            ("python tools/perf/aggregate_cpu_probe.py tmp/out.json", "allow"),
            ("rm -rf tmp/old", "allow"),
            ("mkdir tmp/newdir", "allow"),
            (r"Copy-Item src\a.cpp build\a.cpp", "allow"),
            # read-only outside stays allowed
            (r"Get-Content C:\Windows\win.ini", "allow"),
            ("cat /etc/hosts", "allow"),
            (r"Get-ChildItem C:\Windows", "allow"),
            (r"Test-Path D:\data", "allow"),
            ("git status", "allow"),
            ("python tools/perf/aggregate_cpu_probe.py --help", "allow"),
            # writes outside are denied
            (r"echo x > C:\Windows\evil.txt", "deny"),
            (r"Set-Content D:\other\a.txt DATA", "deny"),
            (r"Remove-Item C:\Users\maoxu\.codebuddy\settings.json", "deny"),
            (r"Copy-Item src\a.cpp D:\backup\a.cpp", "deny"),
            ("echo x > ../escape.txt", "deny"),
            ("mkdir /srv/outside", "deny"),
            (r"New-Item -ItemType Directory -Path C:\evil", "deny"),
        ]
        failures = []
        for path, expected in file_cases:
            actual = classify("Write", path)
            if actual != expected:
                failures.append(f"file {path!r}: expected {expected}, got {actual}")
        for command, expected in shell_cases:
            actual = classify_shell(command)
            if actual != expected:
                failures.append(f"shell {command!r}: expected {expected}, got {actual}")
        if failures:
            print("SELF-TEST FAIL: " + "; ".join(failures), file=sys.stderr)
            return 1
        print(f"SELF-TEST PASS ({len(file_cases) + len(shell_cases)} cases)")
        return 0

    raw = _read_stdin()
    try:
        tool_name, file_path, command = _payload(raw)
        reason = (
            "Outside the workspace is denied for non-read operations. The only "
            "workspace is " + WORKSPACE + "; writes, edits, deletions and "
            "directory creation must target a path inside it."
        )
        if tool_name in SHELL_TOOLS and classify_shell(command) == "deny":
            _emit("deny", reason)
        elif classify(tool_name, file_path) == "deny":
            _emit("deny", reason)
        else:
            _emit("allow", "")
    except Exception:
        _emit("allow", "")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
