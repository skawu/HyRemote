"""Launch a payload DETACHED, prove it actually started, and return in a couple of seconds.

This is the mechanism behind two fixes that were previously only rules:

1. **No process-lifetime coupling.** The child gets its own file handles, its own process group and no inherited console,
   so the command that launches it returns immediately instead of waiting for the payload to exit (the failures that made
   the operator skip commands were `Start-Process -RedirectStandardOutput` holding the runner open for 120-300 s).
2. **No silent child failure mistaken for buffering.** Immediately after launching, this probes for the expected first
   output, and if the process is already gone it prints the exit code and the *stderr* stream, which is where Qt writes
   QML `console.log` markers - reading only stdout is how a QML run once looked like it produced nothing at all.

Usage:
    run_detached.py <exe> --log <path> [--expect <regex>] [--timeout 15] [--path <dir>]... -- [payload args...]

Exit code is 0 when the payload is up (or merely still alive when no --expect was given), 1 when it failed to start.
"""

import argparse
import os
import pathlib
import re
import subprocess
import sys
import time

DETACHED_PROCESS = 0x00000008
CREATE_NEW_PROCESS_GROUP = 0x00000200


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("exe")
    parser.add_argument("--log", required=True)
    parser.add_argument("--expect", default=None, help="regex that must appear in stdout+stderr")
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--path", action="append", default=[], help="directory to prepend to the child's PATH")
    # parse_known_args, not REMAINDER: a REMAINDER positional placed after the first positional swallowed every option
    # that followed the executable path, so --log was reported missing. Verified by running the failure case.
    args, extra = parser.parse_known_args()
    payload = [a for a in extra if a != "--"]

    # Qt's bin and the build-tree runtime directories are what the loader needs; missing them is the silent-exit cause.
    for directory in args.path:
        os.environ["PATH"] = directory + os.pathsep + os.environ.get("PATH", "")

    log = pathlib.Path(args.log)
    log.parent.mkdir(parents=True, exist_ok=True)
    err = log.with_suffix(log.suffix + ".err")
    out_handle = open(log, "w", encoding="utf-8")
    err_handle = open(err, "w", encoding="utf-8")

    proc = subprocess.Popen([args.exe] + payload, stdout=out_handle, stderr=err_handle,
                            stdin=subprocess.DEVNULL,
                            creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP)
    print("launched pid=%d (detached)" % proc.pid, flush=True)

    if not args.expect:
        print("no --expect given; returning immediately, payload pid=%d" % proc.pid, flush=True)
        return 0

    pattern = re.compile(args.expect)
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        text = log.read_text(encoding="utf-8", errors="replace")
        text += err.read_text(encoding="utf-8", errors="replace")
        if pattern.search(text):
            print("probe OK: /%s/ seen within %.1fs" % (args.expect, args.timeout - (deadline - time.time())), flush=True)
            return 0
        if proc.poll() is not None:
            print("probe FAILED: payload exited with code %s before matching /%s/" % (proc.returncode, args.expect), flush=True)
            print("--- stdout ---\n%s" % log.read_text(encoding="utf-8", errors="replace")[:800], flush=True)
            print("--- stderr ---\n%s" % err.read_text(encoding="utf-8", errors="replace")[:800], flush=True)
            return 1
        time.sleep(1.0)

    print("probe TIMED OUT after %.0fs waiting for /%s/; payload pid=%s still alive"
          % (args.timeout, args.expect, proc.pid), flush=True)
    print("--- stderr so far ---\n%s" % err.read_text(encoding="utf-8", errors="replace")[:800], flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
