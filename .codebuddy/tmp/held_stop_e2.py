"""E2 held-input cells on Quick: explicit runtime stop while a modifier is held, plus no late queued input.

Design: the holder stays connected while the payload's own watchdog performs the explicit transition
(POLICY_STOPPED -> POLICY_INPUT true -> POLICY_RESTART_REQUESTED -> READY), so the stop really happens while a
modifier is held and undelivered-to-release. The first key from a fresh viewer afterwards must arrive with
shift=0, which is the balance evidence the E1 payload could not instrument.
"""

import os
import pathlib
import subprocess
import sys
import time

from vncdotool import api

PORT = 5968
EXE = r"F:\workspace\hyremote\build-143c\examples\quick-basic\hyremote-quick-basic.exe"
EV = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp/evidence-held-e2")
LOG = EV / "quick-basic.log"
RECORD = EV / "record.txt"

for part in (r"F:\workspace\hyremote\build-143c\remoteaccess", r"F:\workspace\hyremote\build-143c\qml\HyRemote",
             r"C:\Qt\6.8.3\mingw_64\bin", r"C:\Qt\Tools\mingw1310_64\bin"):
    os.environ["PATH"] = part + os.pathsep + os.environ.get("PATH", "")

EV.mkdir(parents=True, exist_ok=True)


def say(msg):
    print(msg, flush=True)
    with open(RECORD, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def log_text():
    return LOG.read_text(encoding="utf-8", errors="replace") if LOG.exists() else ""


def connect():
    for attempt in range(1, 10):
        try:
            return api.connect("127.0.0.1::%d" % PORT, password=None, timeout=5)
        except Exception:  # noqa: BLE001 - payload may still be starting
            time.sleep(2.0)
    return None


say("=== E2 held-input: explicit stop while held (port %d) ===" % PORT)
out = open(LOG, "w", encoding="utf-8")
err = open(EV / "quick-basic.err", "w", encoding="utf-8")
proc = subprocess.Popen([EXE, "-p", str(PORT), "--remote-input", "--test-seconds", "150",
                         "--policy-transition-ms", "20000"], stdout=out, stderr=err,
                        stdin=subprocess.DEVNULL, creationflags=0x00000008 | 0x00000200)
say("launched PID=%d (control mode; watchdog will stop+configure+start at ~20 s)" % proc.pid)

holder = connect()
if holder is None:
    say("no listener; nothing claimed")
    raise SystemExit(1)
time.sleep(1.0)
holder.keyDown("shift")
time.sleep(0.5)
keys_before = log_text().count("APP_KEY")
say("holder sent Shift (keyDown only) and stays connected; APP_KEY lines so far = %d" % keys_before)

# Wait for the payload's own explicit transition to happen while the modifier is still held.
# Print progress every five seconds: a wait that prints nothing reads as a hang, which is what made an earlier command
# look stuck for its whole 45 s.
deadline = time.time() + 45
last_report = time.time()
while time.time() < deadline and "POLICY_STOPPED" not in log_text():
    time.sleep(1.0)
    if time.time() - last_report >= 5.0:
        last_report = time.time()
        say("  ... waiting for POLICY_STOPPED (%.0fs of 45s, %d log lines)"
            % (45 - (deadline - time.time()), len(log_text().splitlines())))
text = log_text()
say("explicit transition observed = %s" % ("POLICY_STOPPED" in text))
keys_at_stop = text.count("APP_KEY")
say("APP_KEY lines at the stop = %d (no late delivery while held: %s)"
    % (keys_at_stop, keys_at_stop == keys_before))

holder_ok = True
try:
    holder.disconnect()
except Exception:
    holder_ok = False
time.sleep(1.0)

after = connect()
if after is None:
    say("could not reconnect after the transition")
    raise SystemExit(1)
time.sleep(1.0)
after.keyPress("q")
time.sleep(2.0)
after.disconnect()
time.sleep(1.0)

text = log_text()
say("--- full log of this instance ---")
say(text)
tail = [l for l in text.splitlines() if l.startswith("APP_KEY")]
say("first key after the stop: %s" % (tail[0] if tail else "(none)"))
say("verdict: balanced after explicit stop = %s" % (bool(tail) and "shift=0" in tail[0]))
say("holder disconnect after the stop raised: %s" % (not holder_ok))
