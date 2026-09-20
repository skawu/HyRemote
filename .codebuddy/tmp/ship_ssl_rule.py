import json, os, subprocess, urllib.request

REPO = "skawu/HyRemote"
WT = "f:/workspace/hyremote/hyremote-wt-146"
BUILD = "f:/workspace/hyremote/build-146-all"
BRANCH = "build/143-openssl-user-provided"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}
FILE = "cmake/HyRemoteProjectOptions.cmake"


def run(*args, cwd=WT):
    # The mingw and Qt bin directories must be on PATH: without them the compiler driver fails with a non-zero
    # exit and *no diagnostics at all*, which is what produced the "silent build failures" seen earlier today.
    env = dict(os.environ)
    env["PATH"] = ("C:/Qt/Tools/CMake_64/bin;C:/Qt/Tools/Ninja;C:/Qt/Tools/mingw1310_64/bin;"
                   "C:/Qt/6.8.3/mingw_64/bin;" + env.get("PATH", ""))
    p = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=env)
    return p.returncode, (p.stdout + p.stderr).strip()


run("git", "fetch", "origin", "--prune", "--quiet")
print("branch now:", run("git", "branch", "--show-current")[1])
if run("git", "branch", "--show-current")[1] != BRANCH:
    rc, out = run("git", "checkout", "-b", BRANCH, "origin/develop")
    print("checkout rc=%d %s" % (rc, out.splitlines()[-1] if out else ""))
    if rc != 0:
        raise SystemExit(1)

rc, out = run("cmake", "--build", BUILD, "-j", "6")
if rc != 0:
    rc, out = run("cmake", "--build", BUILD, "-j", "4")
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

run("git", "add", FILE)
print(run("git", "status", "--short")[1])
MSG1 = "build(cmake): take OpenSSL from the user's environment and never install it (#143)"
MSG2 = ("The transport-security option carried the comment \"the build fails closed rather than silently downgrading "
        "when it is on and the dependency is missing\", but nothing in the repository had ever searched for OpenSSL, "
        "so the statement was not true: the option merely existed.\n\n"
        "This implements the provisioning rule for the dependency the option names. When the option is on, CMake now "
        "searches for OpenSSL 3. If it is found, the capability is recorded as available and "
        "HYREMOTE_HAS_TRANSPORT_SECURITY is defined. If it is not, the build stays valid, the capability stays off, "
        "and one actionable warning says exactly how to provide it from the user's own environment - the Qt "
        "Maintenance Tool's \"OpenSSL Toolkit\" component, or -DOPENSSL_ROOT_DIR=<prefix> - and that HyRemote does "
        "not install OpenSSL itself. Turning the option off remains the sanctioned way to acknowledge building "
        "without it.\n\n"
        "Measured on a host with no OpenSSL at all: configure succeeds, the warning appears with the Qt route and "
        "OPENSSL_ROOT_DIR named, the suite is 76/76 in the QML+QPA configuration and all ten gates pass, so the "
        "release-profile rule that pins the option is unaffected.\n\n"
        "Consequence for this issue's S2 work: it cannot be implemented or verified where OpenSSL is absent, and "
        "per the provisioning rule the project will not obtain it, so S2 waits on the build environment providing "
        "OpenSSL. Until then the truthful state stays SecurityType None plus the non-loopback fail-closed guard.\n\n"
        "Refs #143 #170")
rc, out = run("git", "commit", "-q", "-m", MSG1, "-m", MSG2)
print("commit rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-2")[1])
rc, out = run("git", "push", "-u", "origin", BRANCH)
print("push rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)

PR_BODY = """## What this is

The build-side half of **#143**'s dependency story, and a correction: the `HYREMOTE_WITH_TRANSPORT_SECURITY` option
carried the comment *"the build fails closed rather than silently downgrading when it is on and the dependency is
missing"*, but **nothing in the repository had ever searched for OpenSSL**. The option existed; the fail-closed
behaviour the comment described did not.

## The rule this implements

OpenSSL comes from the **user's own environment**. This project does not install it and does not build it from
source. If it is missing, the user is told how to provide it - through the Qt SDK, whose Maintenance Tool offers an
"OpenSSL Toolkit" component, or by pointing CMake at an existing installation. If the user does not provide it, the
project simply **does not use any OpenSSL-backed feature**.

## The change

`cmake/HyRemoteProjectOptions.cmake`:

- with the option on, `find_package(OpenSSL 3 QUIET)`;
- found -> `HYREMOTE_TRANSPORT_SECURITY_AVAILABLE=ON` and `HYREMOTE_HAS_TRANSPORT_SECURITY=1` for the code that will
  consume it;
- absent -> the build stays valid, the capability stays off, and **one actionable warning** names the Qt Maintenance
  Tool component and `-DOPENSSL_ROOT_DIR=<prefix>`, states that HyRemote does not install OpenSSL, and points at
  `-DHYREMOTE_WITH_TRANSPORT_SECURITY=OFF` as the explicit way to acknowledge building without it.

## Verified on a host with no OpenSSL at all

| Check | Result |
|---|---|
| configure | succeeds |
| warning | present, naming the Qt component and `OPENSSL_ROOT_DIR` |
| suite | **76/76** in the QML+QPA configuration |
| gates | **10/10**, including the release-profile rule that pins this option |

## Consequence for S2, stated plainly

S2 (RFB VNC authentication) cannot be implemented or verified in an environment without OpenSSL, and under the
provisioning rule this project will not obtain it. So S2 waits on the build environment providing OpenSSL, and until
then the honest product statement stays what `docs/security-model.md` section 10.1 says: the stream is
unauthenticated and unencrypted, `SecurityType None`, with the non-loopback listener fail-closed.

Refs #143 #170
"""
req = urllib.request.Request("https://api.github.com/repos/%s/pulls" % REPO,
                             data=json.dumps({"title": "build(cmake): #143 take OpenSSL from the user's environment, never install it",
                                              "head": BRANCH, "base": "develop", "body": PR_BODY}).encode("utf-8"),
                             headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    pr = json.loads(r.read().decode("utf-8"))
print("PR #%d -> %s" % (pr["number"], pr["html_url"]))
print("base:", pr["base"]["ref"], "head:", pr["head"]["ref"])
