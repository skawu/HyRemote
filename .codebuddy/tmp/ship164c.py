import subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
FILES = ["integrations/qpa/interactive_composite_target.cpp",
         "integrations/qpa/tests/qpa_composite_input_test.cpp"]


def run(*args, cwd=WT):
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


print("probe leftovers:", run("git", "grep", "-n", "SENSITIVITY PROBE")[1] or "(none)")

rc, out = run("cmake", "--build", BUILD, "-j", "6")
if rc != 0:
    print("build failed once - retrying, this host has produced transient build failures today")
    rc, out = run("cmake", "--build", BUILD, "-j", "6")
if rc != 0:
    print("BUILD FAILED - tail:")
    print("\n".join(out.splitlines()[-12:]))
    raise SystemExit(1)
print("build ok")

dll = run("powershell", "-NoProfile", "-Command",
          "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';'; $d")[1]
rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='" + dll + ";C:/Qt/6.8.3/mingw_64/bin;' + $env:PATH; $env:QT_QPA_PLATFORM='offscreen'; cd " + BUILD +
              "; ctest 2>&1 | Select-String -Pattern 'tests passed|tests failed'")
print("ctest:", out)
if not out.strip().startswith("100%"):
    print("ABORT: suite not green")
    raise SystemExit(1)

cmake = "C:/Qt/Tools/CMake_64/bin/cmake.exe"
names = [n for n in run("powershell", "-NoProfile", "-Command",
                        "Get-ChildItem tests/release-readiness -Filter check_*.cmake | Where-Object { $_.Name -ne 'check_release_profile.cmake' } | ForEach-Object { $_.Name }")[1].splitlines()
         if n.strip().endswith(".cmake")]
failed = [n for n in names
          if not (("PASS" in run(cmake, "-DHYREMOTE_SOURCE_DIR=" + WT, "-P", "tests/release-readiness/" + n)[1])
                  or ("passed" in run(cmake, "-DHYREMOTE_SOURCE_DIR=" + WT, "-P", "tests/release-readiness/" + n)[1]))]
print("gates passed = %d / %d" % (len(names) - len(failed), len(names)))
if failed:
    print("ABORT: gates failed", failed)
    raise SystemExit(1)

run("git", "add", *FILES)
print(run("git", "status", "--short")[1])
MSG1 = "test(qpa): document and test composite input exactly-once acceptance (#164)"
MSG2 = ("Acceptance criterion 3 of #164. The behaviour was already correct - `CompositeInputSink::post` accepts an "
        "event only if it can queue it, rejects by throwing without queueing when the bounded mailbox cannot take "
        "it, and surfaces a child adapter's rejection as a deferred error on the *next* post, whose own event is "
        "then not accepted - but none of that was written down or covered.\n\n"
        "The contract is now stated on `post` itself: acceptance means delivered to exactly one child, a rejection "
        "never leaves an event half-delivered, a deferred child failure is always reported before the next event is "
        "taken, and acceptance covers the queued batch rather than the child delivery (so a failure to queue the "
        "drain discards the batch and throws, because those events can no longer be delivered).\n\n"
        "The test gains a deterministic multi-child failure case: one child adapter rejects a single event, and the "
        "test requires that the event is not recorded by any child, that the next post reports the child-adapter "
        "failure and does not accept its own event, and that the next delivery afterwards reaches the previously "
        "failing child exactly once.\n\n"
        "Sensitivity proven rather than assumed: with the deferred-error report replaced by a silent return, the new "
        "case fails with 'the deferred child failure surfaces on the next post instead of being swallowed'. With it "
        "restored, the suite is 76/76 and all ten gates pass.")
rc, out = run("git", "commit", "-q", "-m", MSG1, "-m", MSG2)
print("commit rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-2")[1])
rc, out = run("git", "push")
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print("remote head now:", run("git", "ls-remote", "origin", "refs/heads/fix/164-core-callback-exception-boundary")[1].split()[0][:8])
