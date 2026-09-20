"""Watch a log for a pattern, printing progress every interval - the fix for one-long-sleep commands.

A command that does `Start-Sleep -Seconds 240` and only then looks is silent for four minutes while doing nothing, which
reads as a hang and wastes the operator's attention. This prints a bounded line per interval so progress is visible, and it
stops as soon as the pattern appears.

Usage:
    poll.py --log <path> --expect <regex> [--timeout 240] [--interval 10] [--also <regex>]...
"""

import argparse
import pathlib
import re
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--expect", required=True)
    parser.add_argument("--also", action="append", default=[], help="additional patterns to report as they appear")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--interval", type=float, default=10.0)
    args = parser.parse_args()

    log = pathlib.Path(args.log)
    expect = re.compile(args.expect)
    others = [(p, re.compile(p)) for p in args.also]
    seen_others: set[str] = set()

    started = time.time()
    while time.time() - started < args.timeout:
        text = ""
        if log.exists():
            text = log.read_text(encoding="utf-8", errors="replace")
        for raw, pattern in others:
            if raw not in seen_others and pattern.search(text):
                seen_others.add(raw)
                print("  + %.0fs: /%s/ appeared" % (time.time() - started, raw), flush=True)
        if expect.search(text):
            print("MATCH /%s/ after %.0fs (%d log lines)"
                  % (args.expect, time.time() - started, len(text.splitlines())), flush=True)
            return 0
        print("  ... %.0fs, %d log lines, no /%s/ yet" % (time.time() - started, len(text.splitlines()), args.expect),
              flush=True)
        time.sleep(args.interval)

    print("TIMEOUT after %.0fs without /%s/; log=%s" % (args.timeout, args.expect, log), flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
