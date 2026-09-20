"""Launch the showcase DETACHED and return immediately.

The previous approach (PowerShell Start-Process with -RedirectStandardOutput) gave the child a handle that the
command runner was watching, so the launching command did not return until the application exited - which looked
like a hang for the whole 120-300 s lifetime. Here the child gets its own file handles, its own process group and
no inherited console, and this launcher exits at once.
"""

import os
import pathlib
import subprocess
import sys

EXE = r"F:\workspace\hyremote\build-143c\examples\remote-support-showcase\hyremote-remote-support-showcase.exe"
EV = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp/evidence-step9c")

# The child needs the built runtime next to the Qt libraries on PATH, or it exits silently with an empty log.
for part in (r"F:\workspace\hyremote\build-143c\remoteaccess", r"C:\Qt\6.8.3\mingw_64\bin",
             r"C:\Qt\Tools\mingw1310_64\bin"):
    os.environ["PATH"] = part + os.pathsep + os.environ.get("PATH", "")

EV.mkdir(parents=True, exist_ok=True)
out = open(EV / "app.log", "w", encoding="utf-8")
err = open(EV / "app.err", "w", encoding="utf-8")

DETACHED_PROCESS = 0x00000008
CREATE_NEW_PROCESS_GROUP = 0x00000200

proc = subprocess.Popen(
    [EXE] + sys.argv[1:],
    stdout=out,
    stderr=err,
    stdin=subprocess.DEVNULL,
    creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
)
print("launched PID=%d args=%s" % (proc.pid, " ".join(sys.argv[1:])), flush=True)
