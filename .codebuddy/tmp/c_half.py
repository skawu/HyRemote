"""E1 step 9 "C half": the return to view-only, observed on the candidate-line showcase.

Two phases keep the local policy change and the two viewer sessions cleanly separated:
  phase1 - control mode is active before any viewer attaches; a viewer sends a key and a button event,
           which must reach the application;
  (the local policy change to view-only happens between the phases, driven through UI Automation so the
   path is the one a mouse click takes)
  phase2 - a fresh viewer does the same and must NOT reach the application.

Two independent criteria are counted, not one: the showcase's keyboard line and its pointer line. The
pointer line needs a button event on this payload (a bare move prints nothing), which an earlier run
established, so the viewer always presses a button.
"""

import json
import pathlib
import sys
import time

from vncdotool import api

EV = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp/evidence-step9c")
LOG = EV / "app.log"
STATE = EV / "state.json"

PORT = 5963


def counts():
    text = LOG.read_text(encoding="utf-8", errors="replace") if LOG.exists() else ""
    return {
        "SHOWCASE_KEY": text.count("SHOWCASE_KEY"),
        "SHOWCASE_POINTER": text.count("SHOWCASE_POINTER"),
        "SHOWCASE_CLIENTS": text.count("SHOWCASE_CLIENTS"),
        "REMOTE_INPUT disabled": text.count("REMOTE_INPUT disabled"),
        "REMOTE_INPUT enabled": text.count("REMOTE_INPUT enabled"),
        "REMOTE_STARTED": text.count("REMOTE_STARTED"),
        "REMOTE_STOPPED": text.count("REMOTE_STOPPED"),
        "total_lines": len(text.splitlines()),
    }


def tap_viewer(label):
    """One viewer session: a key and a button event, then let the application print."""
    client = api.connect("127.0.0.1::%d" % PORT, password=None, timeout=10)
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
    except Exception as exc:  # noqa: BLE001 - report, never hide
        print("[%s] disconnect raised: %s" % (label, exc), flush=True)


def say(msg):
    print(msg, flush=True)
    with open(EV / "record.txt", "a", encoding="utf-8") as f:
        f.write(msg + "\n")


phase = sys.argv[1] if len(sys.argv) > 1 else "phase1"
EV.mkdir(parents=True, exist_ok=True)

if phase == "phase1":
    before = counts()
    say("--- phase 1: control mode (started with --remote-input --auto-start) ---")
    say("before viewer: %s" % json.dumps(before))
    tap_viewer("phase1")
    time.sleep(1.0)
    after = counts()
    say("after viewer : %s" % json.dumps(after))
    delta_key = after["SHOWCASE_KEY"] - before["SHOWCASE_KEY"]
    delta_ptr = after["SHOWCASE_POINTER"] - before["SHOWCASE_POINTER"]
    say("delta SHOWCASE_KEY=%d delta SHOWCASE_POINTER=%d" % (delta_key, delta_ptr))
    say("phase1 verdict: control mode delivered remote input = %s (both criteria: key=%s pointer=%s)"
        % (delta_key > 0 and delta_ptr > 0, delta_key > 0, delta_ptr > 0))
    STATE.write_text(json.dumps(after), encoding="utf-8")

else:
    before = json.loads(STATE.read_text(encoding="utf-8"))
    say("--- phase 2: after the local policy change back to view-only ---")
    say("before viewer: %s" % json.dumps(before))
    tap_viewer("phase2")
    time.sleep(1.5)
    after = counts()
    say("after viewer : %s" % json.dumps(after))
    delta_key = after["SHOWCASE_KEY"] - before["SHOWCASE_KEY"]
    delta_ptr = after["SHOWCASE_POINTER"] - before["SHOWCASE_POINTER"]
    delta_clients = after["SHOWCASE_CLIENTS"] - before["SHOWCASE_CLIENTS"]
    say("delta SHOWCASE_KEY=%d delta SHOWCASE_POINTER=%d delta SHOWCASE_CLIENTS=%d"
        % (delta_key, delta_ptr, delta_clients))
    say("policy line seen: REMOTE_INPUT disabled count=%d" % after["REMOTE_INPUT disabled"])
    say("phase2 verdict: view-only again blocked remote input = %s (no key and no pointer delta)"
        % (delta_key == 0 and delta_ptr == 0))
