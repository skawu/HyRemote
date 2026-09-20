"""Run one policy-transition observation on an E2/E3 payload, detached and self-recording.

Usage: e23_run.py <exe> <port> [extra payload args...]

Two viewer sessions with a gap between them, so the payload's own acceptance helper
(--policy-transition-ms: stop -> change remoteInputEnabled -> start, application alive) happens in between.
Counts are always taken from THIS instance's log - carrying counters across an application restart produced
meaningless negative deltas earlier today.
"""

import os
import pathlib
import subprocess
import sys
import time

from vncdotool import api

EXE = sys.argv[1]
PORT = int(sys.argv[2])
EXTRA = sys.argv[3:]
NAME = pathlib.Path(EXE).stem
EV = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp/evidence-e23")
LOG = EV / ("%s.log" % NAME)
RECORD = EV / "record.txt"

for part in (r"F:\workspace\hyremote\build-143c\remoteaccess", r"F:\workspace\hyremote\build-143c\qml\HyRemote",
             r"C:\Qt\6.8.3\mingw_64\bin", r"C:\Qt\Tools\mingw1310_64\bin"):
    os.environ["PATH"] = part + os.pathsep + os.environ.get("PATH", "")

EV.mkdir(parents=True, exist_ok=True)


def say(msg):
    print(msg, flush=True)
    with open(RECORD, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def snapshot():
    text = LOG.read_text(encoding="utf-8", errors="replace") if LOG.exists() else ""
    marks = ("KEY", "POINTER", "TEXT", "POLICY_INPUT", "POLICY_STOPPED", "POLICY_RESTART", "REMOTE_STARTED",
             "REMOTE_STOPPED", "REMOTE_INPUT enabled", "REMOTE_INPUT disabled", "CLIENTS", "ERROR", "FAIL")
    return text, {m: text.count(m) for m in marks}, len(text.splitlines())


def connect():
    for attempt in range(1, 13):
        try:
            c = api.connect("127.0.0.1::%d" % PORT, password=None, timeout=5)
            say("  connected on attempt %d" % attempt)
            return c
        except Exception as exc:  # noqa: BLE001 - the payload may still be starting
            say("  attempt %d failed: %s" % (attempt, exc))
            time.sleep(2.0)
    return None


def act(client, label):
    time.sleep(1.0)
    client.keyPress("q")
    time.sleep(0.3)
    client.mouseMove(200, 150)
    time.sleep(0.3)
    client.mouseDown(1)
    time.sleep(0.2)
    client.mouseUp(1)
    time.sleep(1.5)
    try:
        client.disconnect()
    except Exception as exc:  # noqa: BLE001
        say("  [%s] disconnect raised: %s" % (label, exc))


say("=== %s on port %d, extra args: %s ===" % (NAME, PORT, " ".join(EXTRA)))
out = open(LOG, "w", encoding="utf-8")
err = open(EV / ("%s.err" % NAME), "w", encoding="utf-8")
proc = subprocess.Popen([EXE, "-p", str(PORT)] + EXTRA, stdout=out, stderr=err, stdin=subprocess.DEVNULL,
                        creationflags=0x00000008 | 0x00000200)
say("launched PID=%d (detached; this command is not held open by the payload)" % proc.pid)

client = connect()
if client is None:
    say("could not connect; nothing claimed")
    raise SystemExit(1)
_, before1, _ = snapshot()
say("session 1 (expected view-only unless the payload starts with input enabled); before=%s" % before1)
act(client, "session1")
time.sleep(1.0)
_, after1, lines1 = snapshot()
say("after session 1: %s (log lines %d)" % (after1, lines1))

say("waiting 14 s so the payload's own policy-transition helper can run its stop -> configure -> start")
time.sleep(14)
_, mid, lines_mid = snapshot()
say("after the transition window: %s (log lines %d)" % (mid, lines_mid))

client = connect()
if client is None:
    say("could not reconnect after the transition")
else:
    say("session 2 (after the transition); before=%s" % mid)
    act(client, "session2")
    time.sleep(1.0)

text, after2, lines2 = snapshot()
say("after session 2: %s (log lines %d)" % (after2, lines2))
say("delta session1 KEY=%d POINTER=%d | session2 KEY=%d POINTER=%d"
    % (after1["KEY"] - before1["KEY"], after1["POINTER"] - before1["POINTER"],
       after2["KEY"] - mid["KEY"], after2["POINTER"] - mid["POINTER"]))
say("--- full log of this instance ---")
say(text)
