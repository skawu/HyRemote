#!/usr/bin/env python3
"""Run ONE bounded, observable local developer case; not a build or release gate.

The local case profile is configured once and never committed. One invocation
runs exactly one child process, never a matrix. Logs/status survive interruption.
"""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


def timestamp():
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def save_status(path, status):
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(status, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temp, path)


def stop_owned_process(process):
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    else:
        process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def run_case(name, config_path, out_root):
    profile = json.loads(config_path.read_text(encoding="utf-8"))
    if not isinstance(profile, dict) or name not in profile:
        raise ValueError(f"case {name!r} not found in {config_path}")
    spec = profile[name]
    cmd = spec.get("command")
    if not isinstance(cmd, list) or not cmd or any(not isinstance(x, str) or not x for x in cmd):
        raise ValueError("command must be a nonempty JSON array of argument strings (no shell)")
    limit = spec.get("timeout_seconds", 120)
    if not isinstance(limit, int) or not 10 <= limit <= 300:
        raise ValueError("timeout_seconds must be an integer between 10 and 300")
    cwd = Path(spec.get("cwd", str(config_path.parent))).expanduser().resolve()
    if not cwd.is_dir():
        raise ValueError(f"working directory not found: {cwd}")
    executable = cmd[0]
    if not Path(executable).is_file() and not shutil.which(executable):
        raise ValueError(f"executable not found: {executable}")
    env = dict(os.environ)
    overrides = spec.get("env", {})
    if not isinstance(overrides, dict) or any(
        not isinstance(k, str) or not isinstance(v, str) for k, v in overrides.items()
    ):
        raise ValueError("env must be a map of strings")
    env.update(overrides)

    folder = out_root / f"{timestamp()}-{name}"
    folder.mkdir(parents=True, exist_ok=False)
    status_path, log_path = folder / "status.json", folder / "output.log"
    status = {"case": name, "started_utc": timestamp(), "state": "STARTING",
              "timeout_seconds": limit, "pid": None, "elapsed_seconds": 0,
              "exit_code": None, "log": str(log_path)}
    save_status(status_path, status)
    print(f"[CASE START] {name} timeout={limit}s log={log_path}", flush=True)
    started = time.monotonic()
    process = None
    read_offset = 0
    try:
        with log_path.open("wb") as output:
            process = subprocess.Popen(
                cmd, cwd=str(cwd), env=env, stdout=output,
                stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
            )
            status.update(state="RUNNING", pid=process.pid)
            save_status(status_path, status)
            print(f"[CASE PID] {name} pid={process.pid}", flush=True)
            while True:
                elapsed = time.monotonic() - started
                status["elapsed_seconds"] = round(elapsed, 1)
                exit_code = process.poll()
                if exit_code is not None:
                    status.update(state="PROCESS_EXIT_0" if exit_code == 0
                                  else "PROCESS_EXIT_NONZERO", exit_code=exit_code)
                    break
                if elapsed >= limit:
                    status["state"] = "TIMEOUT"
                    stop_owned_process(process)
                    status["exit_code"] = process.returncode
                    break
                time.sleep(min(5, max(0.1, limit - elapsed)))
                elapsed = time.monotonic() - started
                status["elapsed_seconds"] = round(elapsed, 1)
                size = log_path.stat().st_size if log_path.exists() else 0
                delta = max(0, size - read_offset)
                if delta:
                    with log_path.open("rb") as source:
                        source.seek(read_offset)
                        new_text = source.read(min(delta, 8192))
                    read_offset = size
                    for line in new_text.decode("utf-8", errors="replace").splitlines()[-3:]:
                        print(f"[CASE LOG] {name}: {line[:250]}", flush=True)
                print(f"[CASE PROGRESS] {name} elapsed={elapsed:.0f}s/{limit}s "
                      f"pid={process.pid} alive={process.poll() is None} "
                      f"new_output_bytes={delta}", flush=True)
                save_status(status_path, status)
    except KeyboardInterrupt:
        status["state"] = "CANCELLED"
        if process:
            stop_owned_process(process)
        status["exit_code"] = process.returncode if process else None
    except Exception as error:
        status["state"] = "START_FAILED"
        status["error"] = f"{type(error).__name__}: {error}"
        if process:
            stop_owned_process(process)
        status["exit_code"] = process.returncode if process else None
    finally:
        status["elapsed_seconds"] = round(time.monotonic() - started, 1)
        save_status(status_path, status)
    print(f"[CASE END] {name} state={status['state']} exit={status['exit_code']} "
          f"elapsed={status['elapsed_seconds']}s status={status_path}", flush=True)
    if status["state"] == "PROCESS_EXIT_0":
        print("[CASE NOTE] Process exit=0 does not prove Viewer or performance PASS.", flush=True)
        return 0
    return 124 if status["state"] == "TIMEOUT" else (
        130 if status["state"] == "CANCELLED" else 1
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", required=True, help="one named case, never a matrix")
    parser.add_argument("--config", type=Path,
                        default=Path(__file__).with_name("cases.local.json"))
    parser.add_argument("--out", type=Path,
                        default=Path(__file__).resolve().parents[2] / "build" / "perf-results")
    options = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", options.case):
        parser.error("case must be 1-64 letters/numbers/dashes/underscores")
    try:
        return run_case(options.case, options.config.expanduser().resolve(),
                        options.out.expanduser().resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"[CASE CONFIG ERROR] {error}", file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
