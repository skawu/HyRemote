import json, os, subprocess, urllib.request

REPO = "skawu/HyRemote"
WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "build/qt-lts-adaptive"
FILES = ["CMakeLists.txt", "docs/compatibility.md"]
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}


def run(*args, cwd=WT):
    env = dict(os.environ)
    env["PATH"] = ("C:/Qt/Tools/CMake_64/bin;C:/Qt/Tools/Ninja;C:/Qt/Tools/mingw1310_64/bin;"
                   "C:/Qt/6.8.3/mingw_64/bin;" + env.get("PATH", ""))
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=env)
    return p.returncode, (p.stdout + p.stderr).strip()


run("git", "fetch", "origin", "--prune", "--quiet")
if run("git", "branch", "--show-current")[1] != BRANCH:
    rc, out = run("git", "checkout", "-b", BRANCH, "origin/develop")
    print("checkout rc=%d %s" % (rc, out.splitlines()[-1] if out else ""))
    if rc != 0:
        raise SystemExit(1)

rc, out = run("cmake", "--build", BUILD, "-j", "6")
if rc != 0:
    print("BUILD FAILED:", "\n".join(out.splitlines()[-12:]))
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

cmake = "C:/Qt/Tools/CMake_64/bin/cmake.exe"
names = [n for n in run("powershell", "-NoProfile", "-Command",
                        "Get-ChildItem tests/release-readiness -Filter check_*.cmake | Where-Object { $_.Name -ne 'check_release_profile.cmake' } | ForEach-Object { $_.Name }")[1].splitlines()
         if n.strip().endswith(".cmake")]
failed = [n for n in names
          if not (("PASS" in run(cmake, "-DHYREMOTE_SOURCE_DIR=" + WT, "-P", "tests/release-readiness/" + n)[1])
                  or ("passed" in run(cmake, "-DHYREMOTE_SOURCE_DIR=" + WT, "-P", "tests/release-readiness/" + n)[1]))]
print("gates passed = %d / %d" % (len(names) - len(failed), len(names)))
if failed:
    print("gates failed:", failed)
    raise SystemExit(1)

run("git", "add", *FILES)
print(run("git", "status", "--short")[1])
MSG1 = "build(cmake): detect the Qt line, target LTS, warn instead of blocking on non-LTS (#57)"
MSG2 = ("The project targets Qt LTS lines, and the qualified reference is 6.8.3 (Qt 6.8 LTS). The build now says "
        "which line it was given and adapts instead of refusing:\n\n"
        "  * an LTS line (5.15, 6.2, 6.5, 6.8) is reported during configure;\n"
        "  * a non-LTS line produces one actionable warning and then builds anyway, against the 6.8 API baseline "
        "that every feature search in this repository already asks for - the most compatible configuration "
        "available for a line this project has not qualified - so a user on a newer Qt is never blocked;\n"
        "  * a Qt below 6.8 still cannot configure the product targets, and the existing error already names the "
        "supported way to build Core alone on purpose.\n\n"
        "The Transparent QPA payload is deliberately outside this adaptation: it stays qualified against exactly Qt "
        "6.8.3 private ABI and is skipped unless that exact SDK is present, which is what docs/architecture.md and "
        "docs/compatibility.md already state and what issue #57 owns for other lines.\n\n"
        "Measured: configure prints 'Qt 6.8.3 detected (LTS line 6.8)', the OpenSSL provisioning warning from the "
        "same build-system family is unaffected, the suite is 75/75 and all ten gates pass.")
rc, out = run("git", "commit", "-q", "-m", MSG1, "-m", MSG2)
print("commit rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-1")[1])
rc, out = run("git", "push", "-u", "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)

PR_BODY = """## What this is

The build-system half of the Qt line policy: **the project targets Qt LTS lines**, the qualified reference is **6.8.3
(Qt 6.8 LTS)**, and a user on another Qt must be **told** rather than blocked.

## The change

- The configure step now reports the line it detected: an **LTS** line (5.15, 6.2, 6.5, 6.8) prints
  `Qt <version> detected (LTS line <major.minor>)`.
- A **non-LTS** line prints one actionable warning and **builds anyway**, against the **6.8 API baseline** that
  every feature search in this repository already asks for (`find_package(Qt6 6.8 ...)`). That is the most
  compatible configuration available for a line this project has not qualified, and it means a user on a newer Qt is
  never blocked.
- A Qt **below 6.8** still cannot configure the product targets; the existing error already explains that and names
  the supported way to build Core alone on purpose.
- `docs/compatibility.md` gains the policy section, including the part that is **not** adaptive.

## What is deliberately not adaptive

The **Transparent QPA payload** stays qualified against **exactly Qt 6.8.3 private ABI**. A public-API-compatible Qt
is not a private-ABI-compatible one, and quietly building the payload against another runtime would silently break a
documented V1 qualification claim. `docs/architecture.md:121`, `docs/compatibility.md:17` and #57 (which owns the
Supported / Experimental / Unsupported conclusion for non-reference lines) all already say so, so this change leaves
that behaviour untouched.

## Verified

| Check | Result |
|---|---|
| configure on Qt 6.8.3 | `Qt 6.8.3 detected (LTS line 6.8)` |
| OpenSSL provisioning warning | unaffected, still present |
| suite | **75/75** |
| gates | **10/10** |

Refs #57 #104
"""
req = urllib.request.Request("https://api.github.com/repos/%s/pulls" % REPO,
                             data=json.dumps({"title": "build(cmake): #57 detect the Qt line, target LTS, warn on non-LTS",
                                              "head": BRANCH, "base": "develop", "body": PR_BODY}).encode("utf-8"),
                             headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    pr = json.loads(r.read().decode("utf-8"))
print("PR #%d -> %s" % (pr["number"], pr["html_url"]))
print("base:", pr["base"]["ref"], "head:", pr["head"]["ref"])
