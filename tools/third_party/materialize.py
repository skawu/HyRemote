#!/usr/bin/env python3
import argparse
import json
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX = json.loads((ROOT / "tests" / "third_party" / "matrix.json").read_text(encoding="utf-8"))
APPS = {app["id"]: app for app in MATRIX["applications"]}


def run(*args, cwd=None, capture=True):
    kwargs = {"cwd": cwd, "check": True, "text": True}
    if capture:
        kwargs.update(capture_output=True)
    proc = subprocess.run(args, **kwargs)
    return proc.stdout.strip() if capture else ""


def git(*args, cwd):
    return run("git", *args, cwd=cwd)


def assert_pristine(source: Path) -> None:
    status = git("status", "--porcelain", "--untracked-files=all", cwd=source)
    if status:
        raise RuntimeError(f"refusing to materialize from dirty submodule: {source}\n{status}")


def materialize_archive(source: Path, ref: str, dest: Path) -> None:
    dest.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="hyremote-third-party-") as tmp:
        archive = Path(tmp) / "source.tar"
        run("git", "-C", str(source), "archive", "--format=tar", "-o", str(archive), ref, capture=False)
        with tarfile.open(archive, "r") as tf:
            tf.extractall(dest, filter="data")


def materialize_recursive_clone(app: dict, source: Path, ref: str, dest: Path) -> None:
    # Some real-world applications (OBS is the current example) own nested
    # upstream submodules. `git archive` intentionally omits those gitlinks'
    # contents, so use an independent disposable clone. --reference-if-able
    # reuses objects already present in the pristine tracked submodule without
    # coupling the disposable checkout to its working tree.
    run(
        "git", "clone", "--no-checkout", "--reference-if-able", str(source),
        app["url"], str(dest), capture=False
    )
    try:
        run("git", "checkout", "--detach", ref, cwd=dest, capture=False)
    except subprocess.CalledProcessError:
        run("git", "fetch", "--depth", "1", "origin", ref, cwd=dest, capture=False)
        run("git", "checkout", "--detach", "FETCH_HEAD", cwd=dest, capture=False)
    run("git", "submodule", "sync", "--recursive", cwd=dest, capture=False)
    run("git", "submodule", "update", "--init", "--recursive", "--depth", "1", cwd=dest, capture=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Materialize an upstream ref without modifying its tracked submodule working tree")
    parser.add_argument("--app", required=True, choices=sorted(APPS))
    parser.add_argument("--lane", required=True)
    parser.add_argument("--dest", required=True, type=Path)
    ns = parser.parse_args()

    app = APPS[ns.app]
    if ns.lane not in app["lane_refs"]:
        parser.error(f"unknown lane {ns.lane}")
    configured = app["lane_refs"][ns.lane]
    if configured is None:
        parser.error(f"{ns.app}/{ns.lane} has no qualified/candidate upstream ref yet")
    ref = app["tracking_commit"] if configured == "tracking" else configured
    source = ROOT / app["path"]
    if not source.exists():
        parser.error(f"submodule not initialized: {source}")

    assert_pristine(source)
    git("cat-file", "-e", f"{ref}^{{commit}}", cwd=source)

    dest = ns.dest.resolve()
    if dest.exists():
        shutil.rmtree(dest)

    if (source / ".gitmodules").is_file():
        method = "clone-recursive"
        materialize_recursive_clone(app, source, ref, dest)
    else:
        method = "archive"
        materialize_archive(source, ref, dest)

    # Materialization itself is not allowed to mutate the tracked input.
    assert_pristine(source)
    current = git("rev-parse", "HEAD", cwd=source)
    if current != app["tracking_commit"]:
        raise RuntimeError(
            f"tracked submodule moved while materializing {ns.app}: "
            f"{current} != {app['tracking_commit']}"
        )

    print(f"materialized {ns.app}/{ns.lane} {ref} -> {dest} ({method})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
