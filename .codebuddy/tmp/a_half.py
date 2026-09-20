"""E1 step 9 "A half" on the candidate line: the startup policy is view-only, which must block remote input.

Same payload, same build, same viewer actions as the other halves - only the launch differs (no --remote-input),
so the three halves are comparable and all anchored to the candidate-line tree.

Counts are taken from THIS application instance's log, never from a state file, because state left over from a
previous instance is exactly what produced meaningless negative deltas in the C-half run.
"""

import pathlib
import time

from vncdotool import api

EV = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp/evidence-step9c")
LOG = EV / "app.log"
PORT = 5963


def snapshot():
    text = LOG.read_text(encoding="utf-8", errors="replace") if LOG.exists() else ""
    return text, {
        "KEY": text.count("SHOWCASE_KEY"),
        "POINTER": text.count("SHOWCASE_POINTER"),
        "CLIENTS": text.count("SHOWCASE_CLIENTS"),
        "lines": len(text.splitlines()),
    }


before_text, before = snapshot()
print("A-half before viewer:", before, flush=True)

client = None
for attempt in range(1, 13):
    try:
        client = api.connect("127.0.0.1::%d" % PORT, password=None, timeout=5)
        print("connected on attempt %d" % attempt, flush=True)
        break
    except Exception as exc:  # noqa: BLE001 - the application may still be starting
        print("attempt %d failed: %s" % (attempt, exc), flush=True)
        time.sleep(2.0)
if client is None:
    print("could not connect; nothing claimed", flush=True)
    raise SystemExit(1)
time.sleep(1.0)
client.keyPress("a")
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
    print("disconnect raised:", exc, flush=True)

time.sleep(1.0)
after_text, after = snapshot()
print("A-half after viewer :", after, flush=True)
print("delta KEY=%d POINTER=%d CLIENTS=%d" % (after["KEY"] - before["KEY"], after["POINTER"] - before["POINTER"], after["CLIENTS"] - before["CLIENTS"]), flush=True)
print("A-half verdict: view-only blocked remote input = %s" % (after["KEY"] - before["KEY"] == 0 and after["POINTER"] - before["POINTER"] == 0), flush=True)
print("--- full log of this instance (%d lines) ---" % after["lines"], flush=True)
print(after_text, flush=True)
with open(EV / "record.txt", "a", encoding="utf-8") as f:
    f.write("--- A half: startup policy is view-only (launched without --remote-input) ---\n")
    f.write("before %s after %s\n" % (before, after))
    f.write("delta KEY=%d POINTER=%d CLIENTS=%d verdict_blocked=%s\n"
            % (after["KEY"] - before["KEY"], after["POINTER"] - before["POINTER"],
               after["CLIENTS"] - before["CLIENTS"], after["KEY"] - before["KEY"] == 0 and after["POINTER"] - before["POINTER"] == 0))
    f.write(after_text + "\n")
