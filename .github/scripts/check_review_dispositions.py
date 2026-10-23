#!/usr/bin/env python3
"""Fail a PR when a P1/P2 review thread is unresolved without a disposition.

A disposition is a reply containing `Review-Disposition:` followed by text.
Resolving the GitHub thread also satisfies the gate. The script intentionally
checks review threads rather than ordinary comments, so a generic "LGTM" or a
summary comment cannot silently clear a finding.
"""

from __future__ import annotations

import json
import os
import re
import sys
import urllib.error
import urllib.request

API = "https://api.github.com/graphql"
FINDING = re.compile(r"!\[P[12] Badge\]|\bP[12]\b")
DISPOSITION = re.compile(r"review-disposition\s*:\s*\S+", re.IGNORECASE)


def graphql(token: str, variables: dict) -> dict:
    query = """
    query($owner: String!, $name: String!, $number: Int!, $cursor: String) {
      repository(owner: $owner, name: $name) {
        pullRequest(number: $number) {
          reviewThreads(first: 100, after: $cursor) {
            pageInfo { hasNextPage endCursor }
            nodes {
              isResolved
              comments(first: 50) {
                nodes { body path line author { login } }
              }
            }
          }
        }
      }
    }
    """
    request = urllib.request.Request(
        API,
        data=json.dumps({"query": query, "variables": variables}).encode(),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "User-Agent": "hyremote-review-gate",
        },
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode())


def main() -> int:
    token = os.environ.get("GITHUB_TOKEN", "")
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    event_path = os.environ.get("GITHUB_EVENT_PATH", "")
    if not token or "/" not in repo or not event_path:
        print("review gate: missing token, repository, or event path", file=sys.stderr)
        return 1

    with open(event_path, encoding="utf-8") as handle:
        event = json.load(handle)
    pull_request = event.get("pull_request") or {}
    number = pull_request.get("number")
    if not number:
        print("review gate: event has no pull_request.number", file=sys.stderr)
        return 1

    owner, name = repo.split("/", 1)
    cursor = None
    failures: list[str] = []
    checked = 0
    while True:
        payload = graphql(
            token,
            {"owner": owner, "name": name, "number": int(number), "cursor": cursor},
        )
        if "errors" in payload:
            print(f"review gate: GraphQL error: {payload['errors']}", file=sys.stderr)
            return 1
        threads = payload["data"]["repository"]["pullRequest"]["reviewThreads"]
        for thread in threads["nodes"]:
            comments = thread["comments"]["nodes"]
            finding = next(
                (comment for comment in comments if FINDING.search(comment.get("body") or "")),
                None,
            )
            if not finding:
                continue
            checked += 1
            if thread["isResolved"]:
                continue
            dispositions = [
                comment
                for comment in comments
                if comment is not finding and DISPOSITION.search(comment.get("body") or "")
            ]
            if dispositions:
                continue
            path = finding.get("path") or "unknown file"
            line = finding.get("line")
            location = f"{path}:{line}" if line else path
            author = (finding.get("author") or {}).get("login") or "unknown"
            failures.append(f"{location} (author {author})")

        page = threads["pageInfo"]
        if not page["hasNextPage"]:
            break
        cursor = page["endCursor"]

    if failures:
        print(
            "review gate: unresolved P1/P2 findings need a resolved thread or a "
            "`Review-Disposition:` reply:",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(f"review gate: {checked} P1/P2 finding(s) checked; no blocking thread")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
