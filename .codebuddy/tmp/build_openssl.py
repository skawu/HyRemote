import hashlib, pathlib, shutil, subprocess, sys, urllib.request

VERSION = "3.6.4"
ROOT = pathlib.Path("f:/workspace/hyremote/third-party")
SRC = ROOT / ("openssl-" + VERSION)
PREFIX = ROOT / ("openssl-" + VERSION + "-mingw64")
TARBALL = ROOT / ("openssl-" + VERSION + ".tar.gz")
URL = "https://www.openssl.org/source/openssl-%s.tar.gz" % VERSION
SHA_URL = URL + ".sha256"

GIT_USR = "C:/Program Files/Git/usr/bin"
MINGW = "C:/Qt/Tools/mingw1310_64/bin"
ENV_PATH = MINGW + ";" + GIT_USR + ";C:/Qt/Tools/CMake_64/bin;" + "C:/Windows/System32"


def run(cmd, cwd=None, env=None, check=True):
    p = subprocess.run(cmd, cwd=cwd, env=env, shell=isinstance(cmd, str), capture_output=True, text=True)
    out = (p.stdout or "") + (p.stderr or "")
    if check and p.returncode != 0:
        print("FAILED: %s" % cmd)
        print("\n".join(out.splitlines()[-25:]))
        sys.exit(1)
    return out


ROOT.mkdir(parents=True, exist_ok=True)

if not TARBALL.exists():
    print("downloading", URL)
    urllib.request.urlretrieve(URL, TARBALL)
print("tarball bytes:", TARBALL.stat().st_size)

try:
    expected = urllib.request.urlopen(SHA_URL, timeout=30).read().decode("utf-8").split()[0].strip().lower()
except Exception as error:  # noqa: BLE001
    print("could not fetch the published sha256:", error)
    sys.exit(1)
actual = hashlib.sha256(TARBALL.read_bytes()).hexdigest()
print("published sha256:", expected)
print("computed  sha256:", actual)
if expected != actual:
    print("ABORT: the downloaded tarball does not match the published digest")
    sys.exit(1)

if SRC.exists():
    shutil.rmtree(SRC)
print("extracting")
run(["tar", "-xf", str(TARBALL)], cwd=ROOT)
if not SRC.exists():
    print("ABORT: extraction did not produce", SRC)
    sys.exit(1)

env = dict(**{k: v for k, v in __import__("os").environ.items()})
env["PATH"] = ENV_PATH

# no-asm: nasm is not installed on this host; shared: the DLLs become part of the deployed payload list, which
# section 10.1 already anticipates. The legacy provider stays enabled on purpose: VNC authentication needs DES,
# and in OpenSSL 3 DES lives in the legacy provider.
configure = [
    str(pathlib.Path(GIT_USR) / "perl.exe"), "Configure", "mingw64",
    "no-asm", "no-tests", "shared",
    "--prefix=" + str(PREFIX),
    "--openssldir=" + str(PREFIX / "ssl"),
]
print("configure:", " ".join(configure))
print(run(configure, cwd=SRC, env=env)[-1500:])

jobs = "-j4"
print("make", jobs)
out = run(["mingw32-make", jobs], cwd=SRC, env=env)
print("make tail:", "\n".join(out.splitlines()[-6:]))
print("install_sw")
out = run(["mingw32-make", "install_sw"], cwd=SRC, env=env)
print("install tail:", "\n".join(out.splitlines()[-6:]))

print("=== verification ===")
for probe in ["include/openssl/des.h", "include/openssl/evp.h", "include/openssl/provider.h"]:
    print(probe, (PREFIX / probe).exists())
libs = sorted(p.name for p in (PREFIX / "lib").glob("*crypto*")) + sorted(p.name for p in (PREFIX / "bin").glob("*.dll"))
print("libs:", libs[:8])
print("prefix:", PREFIX)
