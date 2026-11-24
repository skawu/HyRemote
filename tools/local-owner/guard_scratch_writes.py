#!/usr/bin/env python3
"""PreToolUse guard: keep executable scripts out of the repository tmp/ directory.

Mounted from the project `.codebuddy/settings.json` PreToolUse hook. It sees only
tool calls issued by the client, which is the intended scope: a human typing
into a terminal is not covered and is not meant to be.

Contract:
  - deny  when tool_name is Write/Edit/MultiEdit and file_path resolves inside
          <repo>/tmp/ with an executable-script extension;
  - allow everything else;
  - allow on ANY internal error (fail-open). A broken guard must never stop
    normal file writes.

Output follows the documented hook protocol:
  {"hookSpecificOutput": {"hookEventName": "PreToolUse",
                          "permissionDecision": "deny|allow",
                          "permissionDecisionReason": "..."}}
Exit code is always 0 so the allow path is never interpreted as a crash.

Modes:
  (no args)   evaluate the hook payload read from stdin
  --self-test run positive/negative classification cases and exit
  --probe     dump the received payload to %TEMP%/cb_guard_probe.json and allow
"""

from __future__ import annotations

import json
import os
import posixpath
import re
import sys
import tempfile
import threading

SCRIPT_SUFFIXES = (".py", ".ps1", ".cmd", ".bat", ".sh", ".pyw", ".vbs")
FILE_TOOLS = {"Write", "Edit", "MultiEdit", "NotebookEdit"}
SHELL_TOOLS = {"Bash", "Shell", "run_terminal_cmd", "execute_command"}
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Shell commands must be judged by their write DESTINATION, not by any script
# name appearing anywhere in the string: `python tmp/probe.py > tmp/out.log`
# reads a script and writes a log, which is allowed, while
# `cp template.ps1 tmp/run.ps1` writes a script, which is denied.
_REDIRECT = re.compile(r"""(?is)>>?\s*("[^"]+"|'[^']+'|[^\s"'|&;<>()]+)""")
_CMDLET_WRITE = re.compile(
    r"(?is)\b(set-content|add-content|out-file|new-item|tee-object|tee|copy-item|"
    r"move-item|cp|mv|copy|rename)\b([^|;]*)"
)
_OPEN_WRITE = re.compile(
    r"""(?is)open\s*\(\s*([^,)]+?)\s*,\s*["'](?:w|a|w\+|a\+|wb|ab)"""
)
_OPTION_FLAG = re.compile(r"^-\w")


def _is_tmp_script(candidate: str, repo_root: str = REPO_ROOT) -> bool:
    """True when candidate resolves under <repo>/tmp/ and names a script."""
    raw = candidate.strip().strip("\"'")
    if not raw:
        return False
    lowered = raw.replace("\\", "/").lower()
    if not lowered.endswith(SCRIPT_SUFFIXES):
        return False

    root = _norm(repo_root).replace("\\", "/").lower().rstrip("/")
    so_far = root + "/tmp"
    if lowered.startswith(so_far + "/"):
        return True

    # Absolute path that names a tmp/ segment (e.g. /srv/repo/tmp or F:/x/tmp).
    is_absolute = lowered.startswith("/") or bool(re.match(r"^[a-z]:/", lowered))
    if is_absolute:
        return "/tmp/" in lowered

    # Relative to the repository root: normalise . and .. segments first so
    # ./tmp/a.py and tmp/sub/../a.py are both caught.
    normalised = posixpath.normpath(lowered)
    return normalised.startswith("tmp/")


def _extract_shell_targets(command: str) -> list[str]:
    """Return every path this command writes to. Best effort, never raises."""
    targets: list[str] = []
    for match in _REDIRECT.finditer(command):
        targets.append(match.group(1))
    for match in _CMDLET_WRITE.finditer(command):
        args = match.group(2) or ""
        positional = [
            token
            for token in re.findall(r""""[^"]+"|'[^']+'|[^\s"']+""", args)
            if token and not _OPTION_FLAG.match(token) and not token.startswith(("-", "/"))
        ]
        if positional:
            # Copy/move style verbs write to the last argument; writers such as
            # Set-Content/Out-File write to the first path-like argument.
            verb = match.group(1).lower()
            chosen = positional[-1] if verb in {
                "copy-item", "move-item", "cp", "mv", "copy", "rename", "tee", "tee-object",
            } else positional[0]
            targets.append(chosen)
    for match in _OPEN_WRITE.finditer(command):
        targets.append(match.group(1))
    return targets


def _norm(path: str) -> str:
    return os.path.normcase(os.path.normpath(os.path.abspath(path)))


def classify(tool_name: str, file_path: str, repo_root: str = REPO_ROOT) -> str:
    """Return "deny" or "allow" for one tool call. Pure, no side effects."""
    if tool_name not in FILE_TOOLS:
        return "allow"
    if not file_path:
        return "allow"
    target = _norm(file_path)
    tmp_root = _norm(os.path.join(repo_root, "tmp"))
    if not target.startswith(tmp_root + os.sep):
        return "allow"
    lowered = target.lower()
    if lowered.endswith(SCRIPT_SUFFIXES):
        return "deny"
    return "allow"


def classify_shell(command: str, repo_root: str = REPO_ROOT) -> str:
    """Deny a shell command whose write destination is a script under tmp/. Pure."""
    if not command:
        return "allow"
    try:
        targets = _extract_shell_targets(command)
    except Exception:
        return "allow"
    for target in targets:
        if _is_tmp_script(target, repo_root):
            return "deny"
    return "allow"


def _read_stdin(timeout: float = 3.0) -> str:
    """Read stdin with a hard timeout so a silent pipe cannot stall a write."""
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
    """Return (tool_name, file_path, command). Never raises."""
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
        cases = [
            ("Write", "tmp/probe.py", "deny"),
            ("Write", "tmp/sem/observer.py", "deny"),
            ("Edit", "tmp/run.cmd", "deny"),
            ("MultiEdit", "tmp/x.ps1", "deny"),
            ("Write", "tmp/evidence.json", "allow"),
            ("Write", "tmp/viewer.log", "allow"),
            ("Write", "tmp/report.md", "allow"),
            ("Write", "tools/local-owner/helper.py", "allow"),
            ("Write", "src/runtime/src/main.cpp", "allow"),
            ("Read", "tmp/probe.py", "allow"),
            ("Write", "", "allow"),
            ("Write", "tmpfoo/probe.py", "allow"),
        ]
        failures = []
        for tool, path, expected in cases:
            actual = classify(tool, os.path.join(REPO_ROOT, path) if path else "")
            if actual != expected:
                failures.append(f"{tool} {path}: expected {expected}, got {actual}")

        shell_cases = [
            ("echo x > tmp/a.py", "deny"),
            ("python -c \"open('tmp/b.py','w').write('x')\"", "deny"),
            ("cp template.ps1 tmp/run.ps1", "deny"),
            ("powershell -c \"Set-Content tmp/c.cmd 'x'\"", "deny"),
            # Relative prefixes and absolute destinations must not bypass.
            ("echo x > ./tmp/a.py", "deny"),
            ("echo x > ./tmp/nested/a.py", "deny"),
            ("echo x > tmp/sub/../a.py", "deny"),
            ("echo x > /srv/HyRemote/tmp/a.py", "deny"),
            ("echo x > C:/work/HyRemote/tmp/a.py", "deny"),
            ("Set-Content -Path tmp/d.ps1 -Value x", "deny"),
            ("Move-Item template.py tmp/e.py", "deny"),
            # Reading a script and writing a non-script output stays allowed.
            ("python tmp/sem/observer.py --once > tmp/observer.log", "allow"),
            ("python tmp/sem/observer.py --once", "allow"),
            ("cp tmp/sem/observer.py tools/local-owner/observer.py", "allow"),
            ("python tools/perf/aggregate_cpu_probe.py tmp/data.json", "allow"),
            ("Get-ChildItem tmp -Recurse", "allow"),
            ("rm -rf tmp/old", "allow"),
            ("echo evidence > tmp/report.json", "allow"),
            ("git status", "allow"),
        ]
        for command, expected in shell_cases:
            actual = classify_shell(command)
            if actual != expected:
                failures.append(f"shell {command!r}: expected {expected}, got {actual}")

        if failures:
            print("SELF-TEST FAIL: " + "; ".join(failures), file=sys.stderr)
            return 1
        print(f"SELF-TEST PASS ({len(cases) + len(shell_cases)} cases)")
        return 0

    raw = _read_stdin()

    if "--probe" in argv:
        probe_path = os.path.join(tempfile.gettempdir(), "cb_guard_probe.json")
        try:
            with open(probe_path, "w", encoding="utf-8") as handle:
                handle.write(raw or "<EMPTY>")
        except Exception:
            pass
        _emit("allow", "probe")
        return 0

    try:
        tool_name, file_path, command = _payload(raw)
        reason = (
            "Executable scripts are not allowed under tmp/. tmp/ holds run outputs only. "
            "Declare the script in the task contract and place it in a planned tracked "
            "directory (tools/, tests/, .github/scripts/, cmake/, src/**/tests/), then "
            "register it in tests/release-readiness/script_inventory.txt"
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
