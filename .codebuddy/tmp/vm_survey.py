"""Survey the Linux x86 VM before collecting anything: what OS, what desktop session, what Qt kits, what toolchain.

The password is read from the environment (HYR_VM_PASS) and is never written to disk, to the repository or to memory.
"""
import os
import sys

import paramiko

HOST = "192.168.244.128"
USER = "hdzk"

SURVEY = r"""
echo "--- identity ---"
uname -m
. /etc/os-release 2>/dev/null && echo "PRETTY=$PRETTY_NAME"
echo "CPU=$(nproc)"
echo "--- session ---"
echo "DISPLAY=${DISPLAY:-<unset>}"
echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-<unset>}"
echo "XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-<unset>}"
loginctl list-sessions 2>/dev/null | head -4
echo "--- desktop / graphics ---"
ls /usr/share/xsessions/ 2>/dev/null
ls /usr/share/wayland-sessions/ 2>/dev/null
ls /dev/dri 2>/dev/null || echo "no /dev/dri"
glxinfo -B 2>/dev/null | head -4 || echo "glxinfo not installed"
echo "--- Qt kits ---"
ls -d /opt/Qt* 2>/dev/null || echo "no /opt/Qt*"
ls -d /opt/qt* 2>/dev/null || echo "no /opt/qt*"
ls -d "$HOME"/Qt* 2>/dev/null || echo "no ~/Qt*"
dpkg -l 2>/dev/null | awk '/qt6|qt5/ {print $2}' | head -12
echo "--- toolchain ---"
for t in git cmake ninja g++ make qmake6 qmake python3; do printf "%s=" "$t"; command -v "$t" || echo "missing"; done
echo "--- display/input device visibility ---"
ls /dev/input 2>/dev/null | head -5 || echo "no /dev/input"
echo "--- repo present? ---"
ls -d "$HOME"/HyRemote /srv/HyRemote /opt/HyRemote 2>/dev/null || echo "no HyRemote checkout yet"
"""


def main() -> int:
    pw = os.environ.get("HYR_VM_PASS")
    if not pw:
        print("HYR_VM_PASS is not set in the environment")
        return 2
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=USER, password=pw, timeout=15, banner_timeout=15,
                   look_for_keys=False, allow_agent=False)
    try:
        _, stdout, stderr = client.exec_command(SURVEY, timeout=60)
        out = stdout.read().decode("utf-8", errors="replace")
        err = stderr.read().decode("utf-8", errors="replace")
        print(out)
        if err.strip():
            print("--- stderr ---")
            print(err[:1200])
    finally:
        client.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
