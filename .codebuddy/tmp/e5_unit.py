"""E5 unit, measured deterministically: view-only -> explicit enablement -> return to view-only.

Section 5 requires E5 to retain a safe initial policy, explicit control enablement, true listener-versus-connected
status, reconnect diagnostics and meaningful Widgets/text interaction. The repository's release input covers the
client-count, reconnect, pointer/key and listener-release parts but launches with `--remote-input`, so it never
exercises the policy boundary; and the two-way transition could not be driven deterministically by local keyboard
navigation in earlier attempts.

This run drives the payload itself (`--toggle-input-at-ms`) and measures the boundary in three windows with a fresh
viewer each time, using both the pointer-click line and the keyboard line - the two independent indicators the payload
provides - so the result does not depend on which widget holds focus.
"""
import ctypes
import os
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

from vncdotool import api

user32 = ctypes.windll.user32
WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
OUT = Path(".codebuddy/tmp/evidence-e5unit")
RECORD = OUT / "record.txt"


def say(msg: str) -> None:
    print(msg, flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    with open(RECORD, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def env_with(build: Path) -> dict:
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([r"C:\Qt\6.8.3\mingw_64\bin", r"C:\Qt\Tools\mingw1310_64\bin",
                                   str(build / "remoteaccess"), str(build / "qml" / "HyRemote"),
                                   env.get("PATH", "")])
    env["QT_PLUGIN_PATH"] = str((build / "plugins").resolve())
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    return env


def main() -> int:
    build = Path("build")
    exes = list((build / "examples" / "remote-support-showcase").glob("*.exe"))
    if not exes:
        say("showcase executable not found")
        return 2
    exe, port = exes[0], 5972
    OUT.mkdir(parents=True, exist_ok=True)
    log = OUT / "app.log"
    # enable control at 9s, return to view-only at 24s; the run lasts 40s
    cmd = [str(exe), "--port", str(port), "--auto-start", "--toggle-input-at-ms", "9000,24000",
           "--test-seconds", "40"]
    say("launching: " + " ".join(cmd))
    with open(log, "w", encoding="utf-8", errors="replace") as sink:
        proc = subprocess.Popen(cmd, stdout=sink, stderr=subprocess.STDOUT, env=env_with(build))
    lines: list[str] = []

    def refresh() -> None:
        try:
            lines[:] = log.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return

    def wait(needle: str, budget: float, since: int | None = None) -> bool:
        since = len(lines) if since is None else since
        deadline = time.monotonic() + budget
        while time.monotonic() < deadline:
            refresh()
            if any(needle in l for l in lines[since:]):
                return True
            time.sleep(0.25)
        return False

    def measure(label: str) -> tuple[bool, bool]:
        """Fresh viewer per measurement; returns (pointer click delivered, keyboard line seen)."""
        try:
            v = api.connect(f"127.0.0.1::{port}")
        except Exception as exc:
            say(f"  {label}: could not connect: {exc!r}")
            return False, False
        try:
            connected = wait("SHOWCASE_CLIENTS 1", 12)
            since = len(lines)
            v.mouseMove(160, 120)
            time.sleep(0.3)
            v.mouseDown(1)
            v.mouseUp(1)
            time.sleep(0.6)
            v.keyPress("a")
            time.sleep(1.2)
            refresh()
            pointer = any(l.startswith("SHOWCASE_POINTER") for l in lines[since:])
            keyboard = any(l.startswith("SHOWCASE_KEY") for l in lines[since:])
            say(f"  {label}: connected={connected} pointer_click_delivered={pointer} keyboard_delivered={keyboard}")
            return pointer, keyboard
        finally:
            try:
                v.disconnect()
            except Exception:
                pass
            time.sleep(0.6)

    try:
        started = wait("REMOTE_STARTED", 25, 0)
        say(f"remote access started by the application itself: {started}")
        say("=== window 1: before the first toggle, the policy must still be view-only ===")
        p1, k1 = measure("view-only")
        say(f"=== waiting for the payload's own enablement (toggle at 9s) ===")
        enabled = wait("REMOTE_INPUT enabled", 30)
        say(f"payload reported REMOTE_INPUT enabled: {enabled}")
        say("=== window 2: between the toggles, control must be delivered ===")
        p2, k2 = measure("control mode")
        say("=== waiting for the payload's own return to view-only (toggle at 24s) ===")
        disabled = wait("REMOTE_INPUT disabled", 30)
        say(f"payload reported REMOTE_INPUT disabled: {disabled}")
        say("=== window 3: after the second toggle, the policy must be view-only again ===")
        p3, k3 = measure("view-only again")

        refresh()
        say("application control lines: " +
            str([l for l in lines if l.startswith(("REMOTE_", "SHOWCASE_CLIENTS"))][:14]))
        say("=== E5 boundary status ===")
        say(f"  safe initial policy blocked delivery: {not (p1 or k1)}")
        say(f"  explicit enablement delivered it: {p2 or k2}")
        say(f"  returning to view-only blocked it again: {not (p3 or k3)}")
        say(f"  application still running: {proc.poll() is None}")
        say("  held-state balance across a transition: NOT instrumented on this payload (no held-key probe); "
            "that claim remains with the basic examples' held-holder path")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        say("E5 unit run finished")
    return 0


if __name__ == "__main__":
    sys.exit(main())
