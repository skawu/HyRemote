import subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "fix/164-core-callback-exception-boundary"
OLD_HEAD = "03463be"


def run(*args, cwd=WT):
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


print("branch:", run("git", "branch", "--show-current")[1])
run("git", "fetch", "origin", "--prune", "--quiet")

rc, out = run("git", "rebase", "origin/develop")
print("rebase rc=%d" % rc)
if rc != 0:
    rc2, status = run("git", "status", "--short")
    print("CONFLICT - reporting instead of guessing:")
    print(out[-1500:])
    print(status[-800:])
    run("git", "rebase", "--abort")
    print("rebase aborted; the branch is unchanged")
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-4")[1])

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
print("boundary test:", out.strip().splitlines()[-1] if out.strip() else "(no output)")
if "0 failure(s)" not in out:
    print("ABORT: the boundary test is not green")
    print(out[-800:])
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

rc, out = run("git", "push", "--force-with-lease=refs/heads/%s:%s" % (BRANCH, OLD_HEAD), "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-3")[1])
