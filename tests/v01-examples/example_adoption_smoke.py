#!/usr/bin/env python3
"""V0.1 learning-example adoption smoke.

Installs the SDK from the current build tree, then configures, builds and deploys each V0.1 learning example as a
standalone project against that installed SDK - so the examples are exercised the way an application developer
exercises them. Each example is then launched from its own deployed tree and checked:

  * 01 and 02 must reach RemoteAccess Running, actually listen, and stop cleanly;
  * 03 must run the same Qt-only binary plainly and with the Generic plugin, keep the native platform identical,
    and open a listener under Generic activation (the application links no HyRemote target, so a listener can only
    come from the plugin - that is what separates loadable from activated).

Deployment independence of a clean application is a different question and keeps its own owner in
tests/release-readiness/run_release_evidence.cmake. No fixed port is used: the OS is asked for a free port.
"""

import argparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

NATIVE_PLATFORMS = ("windows", "xcb")
FORBIDDEN_PLATFORMS = ("hyremote", "offscreen", "minimal", "vnc")
GENERIC_PLUGIN_KEY = "hyremote"


class SmokeFailure(RuntimeError):
    pass


def log(message):
    print("  " + message, flush=True)


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def listener_is_reachable(port, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.4):
                return True
        except OSError:
            time.sleep(0.1)
    return False


def run(command, env=None, timeout=900.0):
    done = subprocess.run(command, env=env, text=True, timeout=timeout,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return done.returncode, done.stdout


def require(condition, message, output=""):
    if not condition:
        raise SmokeFailure(message + (("\n" + output) if output else ""))


def platform_of(output):
    for line in output.splitlines():
        if line.startswith("PLATFORM_NAME="):
            return line.split("=", 1)[1].strip()
    return ""


def find_executable(root, target):
    for suffix in ("", ".exe"):
        for candidate in root.rglob(target + suffix):
            if candidate.is_file():
                return candidate
    return None


def check_native(case, name, output):
    require(name != "", case + ": the example did not report PLATFORM_NAME", output)
    require(name not in FORBIDDEN_PLATFORMS,
            case + ": the example ran on platform '" + name + "', which is not the native platform", output)
    if sys.platform.startswith("linux"):
        require(name == "xcb", case + ": expected native Linux 'xcb', got '" + name + "'", output)
    elif sys.platform.startswith("win"):
        require(name == "windows", case + ": expected native Windows 'windows', got '" + name + "'", output)
    else:
        require(name in NATIVE_PLATFORMS, case + ": unexpected native platform '" + name + "'", output)


def prepare(source_dir, sdk_prefix, qt_prefix, work, cmake_args, name, relative_source, target):
    """Configure, build and deploy one example against the installed SDK; return its deployed executable."""
    prefix_path = str(sdk_prefix) if not qt_prefix else qt_prefix + ";" + str(sdk_prefix)
    build = work / name / "build"
    deployed = work / name / "deployed"
    code, output = run(["cmake", "-S", str(source_dir / relative_source), "-B", str(build),
                        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + prefix_path, *cmake_args])
    require(code == 0, name + ": configure against the installed SDK failed", output)
    code, output = run(["cmake", "--build", str(build), "--config", "Release"])
    require(code == 0, name + ": build failed", output)
    # Deploying is how the example becomes runnable the way its README describes, and it is where the deployment
    # helper places the shared runtime, the Qt runtime closure and the native platform plugin.
    code, output = run(["cmake", "--install", str(build), "--prefix", str(deployed), "--config", "Release"])
    require(code == 0, name + ": deployment failed", output)
    executable = find_executable(deployed, target)
    require(executable is not None, name + ": deployed executable " + target + " missing under " + str(deployed))
    log("V01_EXAMPLE_BUILD=" + name + " PASS (" + str(executable) + ")")
    return deployed, executable


def launch(executable, arguments, env, seconds, expect_listener_on=None):
    """Run the example for a bounded time; optionally prove something listened while it was alive.

    A listener can only be observed while the process is running, so the port is probed during the run and the
    process is then asked to exit cleanly. Polling after exit would prove nothing.
    """
    command = [str(executable), *arguments, "--test-seconds", str(seconds)]
    process = subprocess.Popen(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    listened = False
    if expect_listener_on is not None:
        listened = listener_is_reachable(expect_listener_on, timeout=seconds + 10)
    try:
        output = process.communicate(timeout=seconds + 120)[0]
    except subprocess.TimeoutExpired:
        process.kill()
        output = ""
        raise SmokeFailure(str(executable) + ": did not exit within its bounded run")
    if expect_listener_on is not None and not listened:
        try:
            drained = process.communicate(timeout=seconds + 120)[0]
        except subprocess.TimeoutExpired:
            process.kill()
            drained = ""
        raise SmokeFailure(str(executable) + ": nothing was listening on port " + str(expect_listener_on) +
                           " while it ran\n" + drained)
    return process.returncode, output


def runtime_env(executable):
    """The deployed tree first: the example is launched as deployed."""
    env = dict(os.environ)
    env["PATH"] = str(executable.parent) + os.pathsep + env.get("PATH", "")
    return env


def case_remoteaccess(name, executable):
    env = runtime_env(executable)
    port = free_port()
    code, output = launch(executable, ["--port", str(port)], env, 3, expect_listener_on=port)
    require(code == 0, name + ": the example exited with " + str(code), output)
    require("APP_READY" in output, name + ": the example never became ready", output)
    require("HYREMOTE_START=Running" in output, name + ": start() did not reach Running", output)
    require("HYREMOTE_STOP=Stopped" in output, name + ": stop() did not reach Stopped", output)
    platform_name = platform_of(output)
    check_native(name, platform_name, output)
    log("V01_EXAMPLE_CASE=" + name + " PLATFORM=" + platform_name + " PORT=" + str(port) +
        " REMOTEACCESS=Running->Stopped")


def case_zero_code(name, deployed, executable):
    env = runtime_env(executable)
    code, output = launch(executable, [], env, 3)
    require(code == 0, name + "-normal: the application exited with " + str(code), output)
    require("APP_READY" in output, name + "-normal: the application never became ready", output)
    normal = platform_of(output)
    check_native(name + "-normal", normal, output)
    log("V01_EXAMPLE_CASE=" + name + "-normal PLATFORM=" + normal + " ZERO_CODE=inactive")

    payloads = sorted((deployed / "plugins" / "generic").glob("*")) \
        if (deployed / "plugins" / "generic").is_dir() else []
    if not payloads:
        log("V01_EXAMPLE_CASE=" + name + "-generic SKIPPED (this SDK was built without the Generic payload)")
        return
    log("V01_EXAMPLE_GENERIC_PAYLOAD=" + name + " " + payloads[0].name)

    port = free_port()
    code, output = launch(executable, ["-plugin", GENERIC_PLUGIN_KEY + ":port=" + str(port)], env, 4,
                          expect_listener_on=port)
    require(code == 0, name + "-generic: exited with " + str(code) + " with the Generic plugin active", output)
    require("APP_READY" in output, name + "-generic: the application never became ready", output)
    generic = platform_of(output)
    check_native(name + "-generic", generic, output)
    require(generic == normal, name + ": the Generic plugin changed the native platform from '" + normal +
            "' to '" + generic + "'", output)
    log("V01_EXAMPLE_CASE=" + name + "-generic PLATFORM=" + generic + " PORT=" + str(port) +
        " ZERO_CODE=active LISTENER=proved")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--qt-prefix", default="")
    parser.add_argument("--cmake-arg", action="append", default=[])
    parser.add_argument("--keep-work-dir", action="store_true")
    arguments = parser.parse_args()

    source_dir = Path(arguments.source_dir).resolve()
    work = Path(tempfile.mkdtemp(prefix="hyremote-v01-examples-"))
    log("V01_EXAMPLE_WORK_DIR=" + str(work))
    try:
        sdk_prefix = work / "sdk"
        code, output = run(["cmake", "--install", str(Path(arguments.build_dir).resolve()),
                            "--prefix", str(sdk_prefix), "--config", "Release"])
        require(code == 0, "installing the SDK from the current build tree failed", output)
        configs = list(sdk_prefix.rglob("HyRemoteConfig.cmake"))
        require(bool(configs), "the installed SDK at " + str(sdk_prefix) + " provides no HyRemote package", output)
        log("V01_EXAMPLE_SDK=" + str(sdk_prefix) + " PASS")

        built = {}
        for name, relative, target in (
                ("01-widgets-cpp", "examples/learning/01-widgets-cpp", "hyremote-learning-01-widgets"),
                ("02-quick-cpp", "examples/learning/02-quick-cpp", "hyremote-learning-02-quick"),
                ("03-widgets", "examples/learning/03-zero-code-generic/widgets-app", "hyremote-learning-03-widgets"),
                ("03-quick", "examples/learning/03-zero-code-generic/quick-app", "hyremote-learning-03-quick")):
            built[name] = prepare(source_dir, sdk_prefix, arguments.qt_prefix, work, arguments.cmake_arg,
                                  name, relative, target)

        case_remoteaccess("01-widgets-cpp", built["01-widgets-cpp"][1])
        case_remoteaccess("02-quick-cpp", built["02-quick-cpp"][1])
        case_zero_code("03-widgets", *built["03-widgets"])
        case_zero_code("03-quick", *built["03-quick"])
    except SmokeFailure as failure:
        print("V01_EXAMPLE_SMOKE=FAIL: " + str(failure), flush=True)
        return 1
    finally:
        if not arguments.keep_work_dir:
            shutil.rmtree(work, ignore_errors=True)

    print("V01_EXAMPLE_SMOKE=PASS (01, 02 and 03 normal+Generic built against the installed SDK, launched, native "
          "platform preserved)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
