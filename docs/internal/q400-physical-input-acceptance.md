# #400 Agent-assisted physical input acceptance

Status: **execution runbook; the file itself is not evidence**.

This runbook is the focused physical gate for #400 after the remote pointer path moved to Qt's window-system ingress. It complements, but does not replace, the broader V1 physical/native acceptance matrix.

The goal is to answer one bounded question on a maintained physical Viewer: after RFB input reaches HyRemote, does Qt itself receive and apply ordinary pointer semantics without a HyRemote mouse-state emulator?

## Entry condition

Execute only on an exact candidate whose hosted/reference validation is already green after the latest product change:

- PR #406 exact head is known and immutable for the run;
- Git Flow policy is green for that head;
- Linux x86_64 / Qt 6.8.3 CI is green for that head;
- Windows x86_64 / Qt 6.8.3 CI and canonical install are green for that head;
- the physical checkout is clean and exactly at that head.

`tests/physical_input_agent.py` enforces the last two checkout properties locally. If the candidate SHA changes, rerun the physical evidence; do not carry a PASS forward by assumption.

## Responsibility split

### Agent owns

- exact SHA and clean-tree verification;
- launching the normal `hyremote-remote-support-showcase` through its public `HyRemote::RemoteAccess` path;
- printing candidate connection endpoints;
- detecting Viewer connection/disconnection through the public client-count signal;
- collecting the application's Qt event trace;
- checking the event facts for each pointer case;
- recording candidate/viewer/host/command/results in `summary.md` plus raw `showcase.log`.

### Human owns

- operating the physical RealVNC Viewer;
- making each requested mouse action exactly once when prompted;
- observing the real local Qt UI and confirming visible behavior;
- exercising one local physical input action while the Viewer remains connected;
- reporting anomalies instead of converting them into an inferred PASS.

A machine trace alone is not physical acceptance. A visual statement without the matching trace is also not enough. Each required cell passes only when both observations pass.

## Build the exact candidate

Build the current clean checkout with Qt 6.8.3 and examples enabled using the repository build authority. Use the same physical-host Qt/toolchain lineage intended for the candidate. Do not point the Agent at an older installed or cached showcase binary.

After the build, identify the exact `hyremote-remote-support-showcase` executable produced from this checkout. The Agent deliberately requires its explicit path rather than guessing a build product.

## Run

Record the installed RealVNC Viewer product/version, then run from the repository checkout:

```text
python tests/physical_input_agent.py \
  --expected-sha <FINAL_PR_406_HEAD_SHA> \
  --viewer "RealVNC Viewer <version>" \
  --showcase <path-to-hyremote-remote-support-showcase> \
  --port 5921
```

On Windows `python` may be replaced by `py -3`. Path quoting should follow the host shell.

The helper launches the showcase with:

```text
hyremote-remote-support-showcase --auto-start --remote-input --port 5921
```

No test-only Runtime API or alternate input backend is used.

## Required #400 cells

The Agent prompts these in order and resets its trace mark before each Human action.

1. **Single click** — Human single-clicks the Asset `QLineEdit`. Agent requires ordinary press/release reaching that line edit; Human confirms one click visibly gives the expected caret/focus behavior.
2. **Double click** — Human double-clicks `Conveyor-01`. Agent requires a Qt `MouseButtonDblClick` on the line edit; Human confirms normal word selection. This is the central proof that Qt, not a HyRemote classifier, owns the double-click semantic.
3. **Held drag / implicit grab** — Human drags the Process load `QSlider`. Agent requires press, held move, then release on the slider; Human confirms continuous tracking through the drag.
4. **Wheel** — Human scrolls while hovering the Command setpoint `QSpinBox`. Agent requires a Qt wheel event on that control; Human confirms the visible value changes in the expected direction.
5. **Right click / context-menu trigger** — Human right-clicks the Asset line edit. Agent requires the Qt context-menu event; Human confirms the ordinary line-edit context menu appears on the **local physical host**.
6. **Local coexistence** — with RealVNC still connected, Human edits the Asset field using the physical local mouse/keyboard and confirms the native UI remains responsive and the Viewer reflects the result.
7. **Disconnect/reconnect** — Human disconnects and reconnects the same Viewer. Agent requires public client count `1 -> 0 -> 1`; Human confirms the application is still visually correct and remotely controllable afterward.

## Scope boundary

This gate intentionally does **not** turn adjacent issues into #400 failures:

- If the local right-click context menu is correctly triggered but a transient popup is absent from the remote framebuffer, record that anomaly against #404. The #400 pointer ingress fact has passed.
- OS foreground/activation behavior remains #362.
- RFB resize/client-framebuffer generation synchronization remains a separate transport correctness concern.
- This focused run does not qualify touch/tablet or desktop-wide native injection.

Conversely, do not waive a failed #400 cell because another synthetic/hosted test is green. The maintained physical Viewer is the point of this gate.

## Evidence and decision

Default evidence directory is `physical-evidence-q400/` and contains:

- `summary.md` — exact candidate, Viewer identity, host facts, launch command, Agent result, Human result and final result for every cell;
- `showcase.log` — timestamped target stdout including `SHOWCASE_CLIENTS` and `SHOWCASE_INPUT` records.

A successful run ends with:

```text
RESULT=Q400_PHYSICAL_PASS
```

A failed or incomplete run ends with `Q400_PHYSICAL_FAIL` (or exits earlier with a non-zero setup/connection code). Attach both evidence files when reporting the outcome. Keep #400 open on any required-cell FAIL; diagnose the first divergent boundary rather than adding control-specific compatibility logic.

A `Q400_PHYSICAL_PASS` is evidence for #400 review, not automatic permission to merge #406 or close unrelated acceptance authorities.
