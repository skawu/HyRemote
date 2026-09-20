"""The two facts that decide the backlog review: why no workflow ran, and which overlapping PR supersedes which."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
CAND = "origin/feature/104-v1-ga-acceptance-matrix"


def sh(args):
    p = subprocess.run(args, cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


def gh_json(args):
    out = sh(["gh"] + args)
    try:
        return json.loads(out)
    except Exception:
        print("!! unreadable: %s" % out[:160], flush=True)
        return None


print("=== #106: draft? state? (draft PRs do not run pull_request workflows) ===", flush=True)
pr = gh_json(["pr", "view", "106", "--repo", REPO, "--json",
              "number,isDraft,state,mergeable,mergeStateStatus,headRefName,baseRefName,updatedAt,title"])
if pr:
    print(json.dumps(pr, indent=2)[:700], flush=True)

print("", flush=True)
print("=== containment between the two overlapping docs PRs ===", flush=True)
for a, b in (("docs/zones-and-qpa-correctness", "examples/project-logo"),
             ("examples/project-logo", "docs/zones-and-qpa-correctness")):
    code = subprocess.run(["git", "merge-base", "--is-ancestor", "origin/%s" % a, "origin/%s" % b],
                          cwd=str(WT), capture_output=True).returncode
    print("  %-34s contained in %-34s -> %s" % (a, b, "YES" if code == 0 else "no"), flush=True)

print("", flush=True)
print("=== is #150's hygiene content already in the candidate? ===", flush=True)
gitattrs = WT / ".gitattributes"
gitignore = WT / ".gitignore"
print("  .gitattributes exists in candidate: %s" % gitattrs.exists(), flush=True)
if gitattrs.exists():
    print("  .gitattributes (%d lines): %s" % (len(gitattrs.read_text(encoding="utf-8").splitlines()),
                                               " | ".join(gitattrs.read_text(encoding="utf-8").splitlines()[:6])), flush=True)
text = gitignore.read_text(encoding="utf-8") if gitignore.exists() else ""
for rule in ("__pycache__", "*.pyc", "aqtinstall"):
    print("  .gitignore already contains %-14s : %s" % (rule, rule in text), flush=True)

print("", flush=True)
print("=== does #150 add .gitattributes that the candidate lacks? ===", flush=True)
diff = sh(["git", "diff", "--stat", "%s...origin/chore/v1-repo-hygiene" % CAND])
print(diff.strip()[:600], flush=True)
for path in (".gitattributes", ".gitignore"):
    out = sh(["git", "show", "origin/chore/v1-repo-hygiene:%s" % path])
    if "fatal" in out:
        print("  %s: absent on that branch" % path, flush=True)
    else:
        print("  %s on that branch (%d lines): %s" % (path, len(out.splitlines()), " | ".join(out.splitlines()[:8])), flush=True)
