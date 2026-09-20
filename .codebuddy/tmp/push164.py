import subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "fix/164-core-callback-exception-boundary"
REMOTE_HEAD = "03463be"


def run(*args, cwd=WT):
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


dll = run("powershell", "-NoProfile", "-Command",
          "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';'; $d")[1]
rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='" + dll + ";C:/Qt/6.8.3/mingw_64/bin;' + $env:PATH; $env:QT_QPA_PLATFORM='offscreen'; cd " + BUILD +
              "; ctest 2>&1 | Select-String -Pattern 'tests passed|tests failed'")
print("ctest:", out)
if not out.strip().startswith("100%"):
    print("ABORT: suite not green")
    raise SystemExit(1)

rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.8.3/mingw_64/bin;' + $dll + ';' + $env:PATH; cd " + BUILD +
              "; & 'core/tests/hyremote-core-test-callback-exception-boundary.exe' 2>&1 | Select-Object -Last 2")
print("boundary test:", out.strip())

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

rc, out = run("git", "push", "--force-with-lease=refs/heads/%s:%s" % (BRANCH, REMOTE_HEAD), "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print("remote head now:", run("git", "ls-remote", "origin", "refs/heads/" + BRANCH)[1].split()[0][:8])
print("local head:", run("git", "rev-parse", "--short", "HEAD")[1])
