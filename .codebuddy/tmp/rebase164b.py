import subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "fix/164-core-callback-exception-boundary"
TESTFILE = "src/core/tests/test_callback_exception_boundary.cpp"


def run(*args, cwd=WT):
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


MSG1 = "test(core): prove the callback boundary on the overflow path (#164)"
MSG2 = ("The ProducerThrottle overflow branch assigns a diagnostic far longer than the small-string buffer, so it "
        "allocates inside onFrameReady - the one path that reaches the new boundary. This case injects there, "
        "pinned to the delivering thread so an allocation in the harness or on a worker cannot decide the result, "
        "and asserts that the boundary's own message appears: that is what makes the boundary proven rather than "
        "defensive.\n\n"
        "Also fixed in the harness, and this is the lesson from the earlier attempt: the fake's stall gate is now "
        "opened on every path via a scope guard. Without it, a failing assertion left the dispatch worker parked "
        "inside the fake's gate, teardown's join could not return, and the case looked like a product hang. The "
        "harness caused it, not Core.\n\n"
        "The suite is 76/76 in the QML+QPA configuration and all ten gates pass.")

run("git", "add", TESTFILE)
print(run("git", "status", "--short")[1])
rc, out = run("git", "commit", "-q", "-m", MSG1, "-m", MSG2)
print("commit rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-1")[1])

pre_rebase = run("git", "rev-parse", "HEAD")[1]
run("git", "fetch", "origin", "--prune", "--quiet")

rc, out = run("git", "rebase", "origin/develop")
print("rebase rc=%d" % rc)
if rc != 0:
    print("CONFLICT:")
    print(out[-1200:])
    print(run("git", "status", "--short")[1][-600:])
    run("git", "rebase", "--abort")
    print("rebase aborted; the branch is unchanged")
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-5")[1])

rc, out = run("cmake", "--build", BUILD, "-j", "6")
if "error:" in out:
    print("BUILD FAILED")
    print("\n".join([line for line in out.splitlines() if "error:" in line][:8]))
    raise SystemExit(1)
print("build ok")

dll = run("powershell", "-NoProfile", "-Command",
          "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';'; $d")[1]
rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='" + dll + ";C:/Qt/6.8.3/mingw_64/bin;' + $env:PATH; $env:QT_QPA_PLATFORM='offscreen'; cd " + BUILD +
              "; ctest 2>&1 | Select-String -Pattern 'tests passed|tests failed'")
print("ctest:", out)
if not out.strip().startswith("100%"):
    print("ABORT: suite not green after the rebase")
    raise SystemExit(1)

rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.8.3/mingw_64/bin;' + $env:PATH; cd " + BUILD +
              "; & 'core/tests/hyremote-core-test-callback-exception-boundary.exe'")
print("boundary test:", " | ".join(out.strip().splitlines()[-5:]) if out.strip() else "(no output)")
if "0 failure(s)" not in out:
    print("ABORT: the boundary test is not green")
    print(out[-900:])
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

rc, out = run("git", "push", "--force-with-lease=refs/heads/%s:%s" % (BRANCH, pre_rebase), "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-3")[1])
