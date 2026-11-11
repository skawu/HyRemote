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
import re
import sys
import tempfile
import threading

SCRIPT_SUFFIXES = (".py", ".ps1", ".cmd", ".bat", ".sh", ".pyw", ".vbs")
FILE_TOOLS = {"Write", "Edit", "MultiEdit", "NotebookEdit"}
SHELL_TOOLS = {"Bash", "Shell", "run_terminal_cmd", "execute_command"}
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# A shell command that writes a script under tmp/ must also be denied, otherwise
# the file tools can simply be bypassed through the shell.
_TMP_TARGET = r"""(?is)(?:^|[\s"'(=])tmp[\\/](?:[^\s"'`;|&<>]*[\\/])?([^\s"'`;|&<>]+?\.(?:py|ps1|cmd|bat|sh|pyw|vbs))\b"""
_WRITE_HINT = re.compile(
    r"(?is)(?:>>?|tee\b|set-content\b|add-content\b|out-file\b|copy-item\b|move-item\b|"
    r"new-item\b|cp\s|mv\s|copy\s|rename\b|open\s*\([^)]*['\"]w|\bwritetext\b|writes\b)"
)
_TMP_SCRIPT = re.compile(_TMP_TARGET)


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
    """Deny a shell command that writes a script file under tmp/. Pure."""
    if not command:
        return "allow"
    if not _TMP_SCRIPT.search(command):
        return "allow"
    if not _WRITE_HINT.search(command):
        return "allow"
    # Resolving the raw token is not required: any write-hinted command that
    # names a tmp/ script is denied regardless of how the path is spelled.
    return "deny"


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
            ("python tmp/sem/observer.py --once", "allow"),
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
