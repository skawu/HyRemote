import subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "fix/164-core-callback-exception-boundary"
REMOTE_HEAD = "03463be"


def run(*args, cwd=WT):
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


rc, out = run("cmake", "--build", BUILD, "--target", "hyremote-core-test-callback-exception-boundary", "-j", "6")
print("build rc=%d" % rc)
if "error:" in out:
    print("\n".join([line for line in out.splitlines() if "error:" in line][:8]))
    raise SystemExit(1)

rc, out = run("powershell", "-NoProfile", "-Command",
              "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';';"
              " $env:PATH='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.8.3/mingw_64/bin;' + $d + ';' + $env:PATH;"
              " cd " + BUILD + "; (Get-Item 'core/tests/hyremote-core-test-callback-exception-boundary.exe').LastWriteTime.ToString('HH:mm:ss')"
              " + ' :: ' + ((& 'core/tests/hyremote-core-test-callback-exception-boundary.exe' 2>&1 | Select-Object -Last 6) -join ' | ')")
print("exe:", out)

if "coreFrameCallbackBoundaryReportsOnOverflow" not in out and "4 case(s)" not in out:
    print("NOTE: the new case still does not appear in the run output; not pushing until that is understood")
    raise SystemExit(1)

rc, out = run("powershell", "-NoProfile", "-Command",
              "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';';"
              " $env:PATH='" + "C:/Qt/6.8.3/mingw_64/bin;" + "' + $d + ';' + $env:PATH; cd " + BUILD +
              "; $env:QT_QPA_PLATFORM='offscreen'; ctest 2>&1 | Select-String -Pattern 'tests passed|tests failed'")
print("ctest:", out)
if not out.strip().startswith("100%"):
    print("ABORT: suite not green")
    raise SystemExit(1)

rc, out = run("git", "push", "--force-with-lease=refs/heads/%s:%s" % (BRANCH, REMOTE_HEAD), "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print("remote head is now:", run("git", "ls-remote", "origin", "refs/heads/" + BRANCH)[1].split()[0][:8])
