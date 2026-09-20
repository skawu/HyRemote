import os, subprocess

WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
CMAKE = "C:/Qt/Tools/CMake_64/bin/cmake.exe"


def run(*args, cwd=WT):
    env = dict(os.environ)
    env["PATH"] = ("C:/Qt/Tools/CMake_64/bin;C:/Qt/Tools/Ninja;C:/Qt/Tools/mingw1310_64/bin;"
                   "C:/Qt/6.8.3/mingw_64/bin;" + env.get("PATH", ""))
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=env)
    return p.returncode, (p.stdout + p.stderr).strip()


rc, out = run(CMAKE, "-G", "Ninja", "-S", ".", "-B", BUILD,
              "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64",
              "-DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe",
              "-DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe",
              "-DHYREMOTE_BUILD_TESTS=ON", "-DHYREMOTE_BUILD_EXAMPLES=ON",
              "-DHYREMOTE_BUILD_QML_API=ON", "-DHYREMOTE_WITH_QPA_PROXY=ON")
print("configure rc=%d" % rc)
for line in out.splitlines():
    if "LTS line" in line or "is not an LTS line" in line or "OpenSSL Toolkit" in line:
        print("  ", line.strip())
if rc != 0:
    print("\n".join(out.splitlines()[-12:]))
    raise SystemExit(1)

rc, out = run(CMAKE, "--build", BUILD, "-j", "6")
if rc != 0:
    print("BUILD FAILED:", "\n".join(out.splitlines()[-14:]))
    raise SystemExit(1)
print("build ok")

dll = run("powershell", "-NoProfile", "-Command",
          "$d=(Get-ChildItem " + BUILD + " -Recurse -Filter *.dll | Select-Object -ExpandProperty DirectoryName -Unique) -join ';'; $d")[1]
rc, out = run("powershell", "-NoProfile", "-Command",
              "$env:PATH='" + dll + ";C:/Qt/6.8.3/mingw_64/bin;' + $env:PATH; $env:QT_QPA_PLATFORM='offscreen'; cd " + BUILD +
              "; ctest 2>&1 | Select-String -Pattern 'tests passed|tests failed'")
print("ctest:", out)
if not out.strip().startswith("100%"):
    raise SystemExit(1)

names = [n for n in run("powershell", "-NoProfile", "-Command",
                        "Get-ChildItem tests/release-readiness -Filter check_*.cmake | Where-Object { $_.Name -ne 'check_release_profile.cmake' } | ForEach-Object { $_.Name }")[1].splitlines()
         if n.strip().endswith(".cmake")]
failed = []
for n in names:
    out2 = run(CMAKE, "-DHYREMOTE_SOURCE_DIR=" + WT, "-P", "tests/release-readiness/" + n)[1]
    if not (("PASS" in out2) or ("passed" in out2)):
        failed.append(n)
print("gates passed = %d / %d" % (len(names) - len(failed), len(names)))
if failed:
    print("gates failed:", failed)
    raise SystemExit(1)
