"""Fold the E5 policy boundary into the release input, so it is a machine check rather than a one-off observation.

Section 5 claims E5 retains a safe initial policy and explicit control enablement. The release input previously
launched with `--remote-input` and never toggled, so it validated delivery and teardown but not the boundary. The
payload now has `--toggle-input-at-ms`, which makes the boundary deterministic and headless.

The new function is self-contained on purpose: the existing verification path is left untouched, because a release
input that stops working is worse than one that covers less. It reuses only the module's existing helpers.
"""
import sys
from pathlib import Path

ROOT = Path(".").resolve()
HARNESS = ROOT / "tests" / "product-e2e" / "showcase_product_fit.py"
APPLY = "--apply" in sys.argv

NEW_FUNCTION = '''

def verify_policy_boundary(executable: Path) -> None:
    """View-only -> explicit enablement -> back to view-only, asserted from the application's own lines.

    Two independent indicators are required in each window - a remote pointer click (`SHOWCASE_POINTER`) and a remote
    key (`SHOWCASE_KEY`) - because a letter alone is focus-dependent and a bare pointer move prints nothing.
    """
    require(executable.exists(), f"showcase executable not found: {executable}")
    port = free_port()
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    process = subprocess.Popen(
        [
            str(executable),
            "--port", str(port),
            "--auto-start",
            "--toggle-input-at-ms", "4000,12000",
            "--test-seconds", "24",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=env,
    )
    lines: list[str] = []
    events: queue.Queue[str] = queue.Queue()

    def reader() -> None:
        assert process.stdout is not None
        for raw in process.stdout:
            line = raw.rstrip("\\r\\n")
            lines.append(line)
            events.put(line)

    threading.Thread(target=reader, daemon=True).start()

    def wait_line(needle: str, timeout: float) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if any(needle in line for line in lines):
                return len(lines)
            if process.poll() is not None:
                raise RuntimeError(f"showcase exited while waiting for {needle!r}: {lines}")
            time.sleep(0.05)
        raise RuntimeError(f"showcase never reported {needle!r}: {lines}")

    def deliver_and_read(start: int) -> tuple[bool, bool]:
        with api.connect(f"127.0.0.1::{port}", password=None, timeout=5) as client:
            wait_for_count(process, lines, 1, 1, "policy-boundary viewer did not connect")
            client.mouseMove(160, 120)
            time.sleep(0.2)
            client.mouseDown(1)
            client.mouseUp(1)
            time.sleep(0.4)
            client.keyDown("a")
            client.keyUp("a")
            time.sleep(0.6)
        window = lines[start:]
        return (any(line.startswith("SHOWCASE_POINTER") for line in window),
                any(line.startswith("SHOWCASE_KEY") for line in window))

    try:
        wait_line("REMOTE_STARTED", 20)
        first = deliver_and_read(0)
        require(not first[0] and not first[1],
                f"view-only leaked remote input before enablement (pointer={first[0]}, key={first[1]}): {lines}")

        wait_line("REMOTE_INPUT enabled", 20)
        second = deliver_and_read(len(lines))
        require(second[0] and second[1],
                f"control mode did not deliver remote input (pointer={second[0]}, key={second[1]}): {lines}")

        wait_line("REMOTE_INPUT disabled", 20)
        third = deliver_and_read(len(lines))
        require(not third[0] and not third[1],
                f"returning to view-only leaked remote input (pointer={third[0]}, key={third[1]}): {lines}")

        print("PASS: remote-support-showcase -> safe initial policy -> explicit enablement -> "
              "return to view-only (pointer click and keyboard indicators on each window)")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)

'''

MAIN_OLD = """    try:
        verify_showcase(args.showcase.resolve())
        return 0
"""
MAIN_NEW = """    try:
        verify_showcase(args.showcase.resolve())
        verify_policy_boundary(args.showcase.resolve())
        return 0
"""


def main() -> int:
    print(("APPLYING" if APPLY else "DRY RUN") + ": policy boundary as a release-input check")
    raw = HARNESS.read_bytes().decode("utf-8")
    newline = "\\r\\n" if "\\r\\n" in raw else "\\n"
    text = raw.replace("\\r\\n", "\\n")
    if "verify_policy_boundary" in text:
        print("  harness: policy boundary check already present")
    else:
        anchor = "def main() -> int:"
        if text.count(anchor) != 1:
            print(f"!! harness: main() definition not found uniquely ({text.count(anchor)})")
            return 1
        text = text.replace(anchor, NEW_FUNCTION.lstrip("\n") + "\n" + anchor, 1)
        if text.count(MAIN_OLD) != 1:
            print("!! harness: main() body anchor not found uniquely")
            return 1
        text = text.replace(MAIN_OLD, MAIN_NEW, 1)
        if APPLY:
            HARNESS.write_bytes(text.replace("\\n", newline).encode("utf-8"))
        print("  harness: verify_policy_boundary added and wired into main()")
    if not APPLY:
        print("\\nre-run with --apply")
    return 0


if __name__ == "__main__":
    sys.exit(main())
