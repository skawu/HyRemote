#!/usr/bin/env python3
"""Agent-assisted physical RealVNC acceptance for HyRemote remote pointer semantics.

The agent owns identity checks, process orchestration, event-trace collection and evidence output.
A human remains the physical/visual oracle and performs the requested Viewer/local actions.
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
from pathlib import Path
import platform
import socket
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from typing import Callable, Iterable


@dataclass
class StepResult:
    name: str
    machine_pass: bool
    human_pass: bool
    detail: str

    @property
    def passed(self) -> bool:
        return self.machine_pass and self.human_pass


class Trace:
    def __init__(self, process: subprocess.Popen[str], raw_log: Path) -> None:
        self.process = process
        self.raw_log = raw_log
        self.lines: list[str] = []
        self._condition = threading.Condition()
        self._thread = threading.Thread(target=self._read, daemon=True)
        self._thread.start()

    def _read(self) -> None:
        assert self.process.stdout is not None
        with self.raw_log.open("w", encoding="utf-8", newline="\n") as log:
            for raw in self.process.stdout:
                line = raw.rstrip("\r\n")
                stamp = dt.datetime.now(dt.timezone.utc).isoformat()
                log.write(f"{stamp} {line}\n")
                log.flush()
                print(f"[target] {line}")
                with self._condition:
                    self.lines.append(line)
                    self._condition.notify_all()
        with self._condition:
            self._condition.notify_all()

    def mark(self) -> int:
        with self._condition:
            return len(self.lines)

    def wait_for(self, predicate: Callable[[list[str]], bool], start: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        with self._condition:
            while True:
                current = self.lines[start:]
                if predicate(current):
                    return True
                if self.process.poll() is not None:
                    return predicate(current)
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return predicate(current)
                self._condition.wait(timeout=min(0.25, remaining))

    def snapshot(self, start: int) -> list[str]:
        with self._condition:
            return list(self.lines[start:])


def run_git(repo: Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", *args], cwd=repo, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if completed.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} failed: {completed.stderr.strip()}")
    return completed.stdout.strip()


def repository_root() -> Path:
    completed = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if completed.returncode != 0:
        raise RuntimeError("run this helper from a HyRemote git checkout")
    return Path(completed.stdout.strip()).resolve()


def local_ipv4_addresses() -> list[str]:
    values: set[str] = set()
    try:
        for entry in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            address = entry[4][0]
            if address and not address.startswith("127."):
                values.add(address)
    except OSError:
        pass
    return sorted(values)


def ask_yes_no(prompt: str) -> bool:
    while True:
        answer = input(f"{prompt} [y/n]: ").strip().lower()
        if answer in {"y", "yes"}:
            return True
        if answer in {"n", "no"}:
            return False
        print("Please answer y or n.")


def contains(lines: Iterable[str], *needles: str) -> bool:
    return any(all(needle in line for needle in needles) for line in lines)


def event_sequence(lines: list[str], widget: str, types: list[str]) -> bool:
    pos = 0
    for event_type in types:
        found = False
        while pos < len(lines):
            line = lines[pos]
            pos += 1
            if "SHOWCASE_INPUT" in line and f"type={event_type}" in line and f"widget={widget}" in line:
                found = True
                break
        if not found:
            return False
    return True


def wait_action(
    trace: Trace,
    title: str,
    instruction: str,
    predicate: Callable[[list[str]], bool],
    human_question: str,
    timeout: float,
) -> StepResult:
    print("\n" + "=" * 72)
    print(title)
    print(instruction)
    input("Press Enter when you are ready to perform this action in RealVNC Viewer...")
    start = trace.mark()
    machine_pass = trace.wait_for(predicate, start, timeout)
    observed = trace.snapshot(start)
    if machine_pass:
        print("Agent observation: expected Qt event trace detected.")
    else:
        print("Agent observation: expected Qt event trace NOT detected within the timeout.")
        for line in observed[-12:]:
            print(f"  {line}")
    human_pass = ask_yes_no(human_question)
    detail = f"observed_lines={len(observed)}"
    return StepResult(title, machine_pass, human_pass, detail)


def write_summary(
    path: Path,
    *,
    head: str,
    branch: str,
    viewer: str,
    showcase: Path,
    port: int,
    addresses: list[str],
    command: list[str],
    results: list[StepResult],
    notes: str,
    raw_log: Path,
) -> None:
    overall = all(item.passed for item in results)
    now = dt.datetime.now(dt.timezone.utc).isoformat()
    rows = "\n".join(
        f"| {item.name} | {'PASS' if item.machine_pass else 'FAIL'} | "
        f"{'PASS' if item.human_pass else 'FAIL'} | {'PASS' if item.passed else 'FAIL'} | {item.detail} |"
        for item in results
    )
    text = f"""# #400 physical RealVNC input acceptance

- Result: **{'PASS' if overall else 'FAIL'}**
- Recorded UTC: `{now}`
- Candidate SHA: `{head}`
- Branch: `{branch}`
- Viewer: `{viewer}`
- Host: `{platform.platform()}`
- Python: `{platform.python_version()}`
- Showcase: `{showcase}`
- Listener port: `{port}`
- Host IPv4 candidates: `{', '.join(addresses) if addresses else '<none discovered>'}`
- Launch command: `{' '.join(command)}`
- Raw target trace: `{raw_log.name}`

This is **Agent-assisted Human physical evidence**. The agent verified the immutable checkout,
launched the normal public-API showcase, observed the application's Qt event trace and recorded the
evidence. A human performed the physical RealVNC/local interactions and confirmed the visible control
semantics. Neither side alone is sufficient for a PASS.

| Cell | Agent event trace | Human visual observation | Cell result | Detail |
| --- | --- | --- | --- | --- |
{rows}

## Scope boundary

- Single click, double click, held drag/grab, wheel and right-click/context-menu trigger are #400 evidence.
- The right-click cell requires the local Qt context-menu trigger. A popup missing from the *remote frame*
  is recorded against #404 and does not by itself fail #400.
- OS foreground activation remains #362 and is not inferred from this run.

## Human notes / anomalies

{notes if notes else 'None.'}
"""
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Agent-assisted Human RealVNC validation for HyRemote #400 pointer semantics"
    )
    parser.add_argument("--expected-sha", required=True, help="exact candidate commit (full SHA or resolvable ref)")
    parser.add_argument("--viewer", required=True, help='viewer identity/version, e.g. "RealVNC Viewer 7.13.0"')
    parser.add_argument("--showcase", required=True, type=Path, help="built hyremote-remote-support-showcase executable")
    parser.add_argument("--port", type=int, default=5921)
    parser.add_argument("--evidence-dir", type=Path, default=Path("physical-evidence-q400"))
    parser.add_argument("--timeout", type=float, default=60.0, help="seconds allowed for each Human action")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("--port must be in 1..65535")

    repo = repository_root()
    head = run_git(repo, "rev-parse", "HEAD")
    expected = run_git(repo, "rev-parse", f"{args.expected_sha}^{{commit}}")
    if head != expected:
        print(f"FAIL: checkout HEAD {head} != expected candidate {expected}", file=sys.stderr)
        return 2

    dirty = run_git(repo, "status", "--porcelain", "--untracked-files=no")
    if dirty:
        print("FAIL: tracked checkout is dirty; physical evidence must be tied to an immutable tree:", file=sys.stderr)
        print(dirty, file=sys.stderr)
        return 2

    showcase = args.showcase
    if not showcase.is_absolute():
        showcase = (repo / showcase).resolve()
    if not showcase.is_file():
        print(f"FAIL: showcase executable not found: {showcase}", file=sys.stderr)
        return 2

    evidence_dir = args.evidence_dir
    if not evidence_dir.is_absolute():
        evidence_dir = (repo / evidence_dir).resolve()
    evidence_dir.mkdir(parents=True, exist_ok=True)
    raw_log = evidence_dir / "showcase.log"
    summary = evidence_dir / "summary.md"

    branch = run_git(repo, "branch", "--show-current") or "<detached>"
    addresses = local_ipv4_addresses()
    command = [str(showcase), "--auto-start", "--remote-input", "--port", str(args.port)]

    print(f"Candidate: {head}")
    print(f"Viewer:    {args.viewer}")
    print(f"Target:    {showcase}")
    print(f"Evidence:  {evidence_dir}")
    print("Connection endpoints:")
    if addresses:
        for address in addresses:
            print(f"  {address}:{args.port}")
    else:
        print(f"  <this host IPv4>:{args.port}")

    process = subprocess.Popen(
        command,
        cwd=repo,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=os.environ.copy(),
    )
    trace = Trace(process, raw_log)
    results: list[StepResult] = []

    try:
        start = trace.mark()
        if not trace.wait_for(lambda lines: contains(lines, "REMOTE_STARTED"), start, 20.0):
            print("FAIL: showcase did not report REMOTE_STARTED", file=sys.stderr)
            return 3

        print("\nOpen RealVNC Viewer and connect to one endpoint above.")
        connect_mark = trace.mark()
        connected = trace.wait_for(lambda lines: contains(lines, "SHOWCASE_CLIENTS 1"), connect_mark, args.timeout)
        results.append(StepResult("Viewer connect", connected, connected, "client-count 0->1"))
        if not connected:
            print("FAIL: Viewer connection was not observed.", file=sys.stderr)
            return 4

        results.append(wait_action(
            trace,
            "Single click",
            "In RealVNC Viewer, single-click once inside the Asset field containing 'Conveyor-01'.",
            lambda lines: event_sequence(lines, "QLineEdit", ["MouseButtonPress", "MouseButtonRelease"]),
            "Did the Asset field visibly receive focus/caret from exactly one remote click?",
            args.timeout,
        ))

        results.append(wait_action(
            trace,
            "Double click",
            "In RealVNC Viewer, double-click the word 'Conveyor-01' inside the Asset field.",
            lambda lines: contains(lines, "SHOWCASE_INPUT", "type=MouseButtonDblClick", "widget=QLineEdit"),
            "Did Qt visibly perform the normal line-edit double-click behavior (word selection)?",
            args.timeout,
        ))

        results.append(wait_action(
            trace,
            "Held drag / implicit grab",
            "In RealVNC Viewer, press the Process load slider handle, drag it a clear distance, then release.",
            lambda lines: event_sequence(lines, "QSlider", ["MouseButtonPress", "MouseMove", "MouseButtonRelease"]),
            "Did the slider track the remote drag continuously and finish at the released position?",
            args.timeout,
        ))

        results.append(wait_action(
            trace,
            "Wheel",
            "In RealVNC Viewer, place the pointer over Command setpoint (the spin box showing 42) and scroll the wheel.",
            lambda lines: contains(lines, "SHOWCASE_INPUT", "type=Wheel", "widget=QSpinBox"),
            "Did the spin-box value visibly change in the expected wheel direction?",
            args.timeout,
        ))

        results.append(wait_action(
            trace,
            "Right click / context-menu trigger",
            "In RealVNC Viewer, right-click once inside the Asset field. Judge the local Qt menu trigger; remote popup capture belongs to #404.",
            lambda lines: contains(lines, "SHOWCASE_INPUT", "type=ContextMenu", "widget=QLineEdit"),
            "Did the normal QLineEdit context menu appear on the LOCAL host (even if the remote frame does not show that popup)?",
            args.timeout,
        ))

        print("\n" + "=" * 72)
        print("Local coexistence")
        print("Keep RealVNC connected. On the physical host, use the local mouse/keyboard to edit the Asset field once.")
        local_ok = ask_yes_no("Did local input remain responsive and did the Viewer reflect the resulting UI change?")
        results.append(StepResult("Local input coexistence", True, local_ok, "Human physical observation"))

        print("\n" + "=" * 72)
        print("Disconnect / reconnect")
        print("Disconnect/close the RealVNC Viewer connection now.")
        disconnect_mark = trace.mark()
        disconnected = trace.wait_for(lambda lines: contains(lines, "SHOWCASE_CLIENTS 0"), disconnect_mark, args.timeout)
        if disconnected:
            print("Agent observed client count return to 0. Reconnect the same Viewer.")
            reconnect_mark = trace.mark()
            reconnected = trace.wait_for(lambda lines: contains(lines, "SHOWCASE_CLIENTS 1"), reconnect_mark, args.timeout)
        else:
            reconnected = False
        human_reconnect = disconnected and reconnected and ask_yes_no(
            "After reconnect, is the application still visually correct and controllable from the Viewer?"
        )
        results.append(StepResult(
            "Viewer disconnect/reconnect",
            disconnected and reconnected,
            human_reconnect,
            f"disconnect={disconnected} reconnect={reconnected}",
        ))

        notes = input("\nKnown anomaly / note (blank for none; mention remote-popup visibility as #404 if applicable): ").strip()
        write_summary(
            summary,
            head=head,
            branch=branch,
            viewer=args.viewer,
            showcase=showcase,
            port=args.port,
            addresses=addresses,
            command=command,
            results=results,
            notes=notes,
            raw_log=raw_log,
        )

        print("\n" + "=" * 72)
        for item in results:
            print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}")
        overall = all(item.passed for item in results)
        print(f"\nRESULT={'Q400_PHYSICAL_PASS' if overall else 'Q400_PHYSICAL_FAIL'}")
        print(f"Evidence summary: {summary}")
        print(f"Raw trace:        {raw_log}")
        return 0 if overall else 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
