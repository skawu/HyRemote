#!/usr/bin/env python3
"""Report AI-flavored prose markers in docs/.

The default mode is a report only. ``--strict`` turns any warning into a
non-zero exit and is intended for CI after the documentation migration.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from pathlib import Path

MARKERS = {
    "em_dash": re.compile("—"),
    "rather_than": re.compile(r"\brather than\b", re.IGNORECASE),
    "facade": re.compile(r"\bfacade\b", re.IGNORECASE),
    "seam": re.compile(r"\bseams?\b", re.IGNORECASE),
    "truthful": re.compile(r"\btruthful\b", re.IGNORECASE),
    "designed": re.compile(r"\bdesigned\b", re.IGNORECASE),
    "intentionally": re.compile(r"\bintentionally\b", re.IGNORECASE),
    "by_design": re.compile(r"\bby design\b", re.IGNORECASE),
}
FENCE = re.compile(r"```.*?```", re.DOTALL)
CHINESE = re.compile(r"[\u4e00-\u9fff]")
WORDS = re.compile(r"[A-Za-z0-9_]+")
CODE_SPAN = re.compile(r"`[^`]*`")


@dataclass
class Report:
    path: Path
    chinese_chars: int = 0
    english_words: int = 0
    markers: dict[str, int] = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)

    @property
    def marker_total(self) -> int:
        return sum(self.markers.values())

    @property
    def words(self) -> int:
        return max(1, self.english_words + self.chinese_chars)


def prose(text: str) -> str:
    text = FENCE.sub(" ", text)
    text = CODE_SPAN.sub(" ", text)
    return text


def inspect(path: Path, root: Path) -> Report:
    text = path.read_text(encoding="utf-8")
    body = prose(text)
    report = Report(path=path)
    report.chinese_chars = len(CHINESE.findall(body))
    report.english_words = len(WORDS.findall(body))
    for name, pattern in MARKERS.items():
        report.markers[name] = len(pattern.findall(body))

    relative = path.relative_to(root).as_posix()
    is_english = relative.startswith("docs/en/")

    if is_english:
        if report.chinese_chars > 5:
            report.warnings.append("Chinese text in docs/en")
    elif report.chinese_chars == 0 and report.english_words >= 200:
        report.warnings.append("English prose in docs root")
    elif report.chinese_chars > 0 and report.english_words > report.chinese_chars * 2:
        report.warnings.append("mixed Chinese/English root prose")

    density = report.marker_total * 1000.0 / report.words
    if density > 1.0:
        report.warnings.append(f"AI marker density {density:.2f}/1000 words")
    if report.markers["em_dash"] > 2:
        report.warnings.append(f"em dash x{report.markers['em_dash']}")
    if report.markers["rather_than"] > 2:
        report.warnings.append(f"rather than x{report.markers['rather_than']}")
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--limit", type=int, default=25)
    args = parser.parse_args()

    root = args.root.resolve()
    docs = root / "docs"
    if not docs.is_dir():
        print(f"no docs directory under {root}")
        return 1

    reports = [inspect(path, root) for path in sorted(docs.rglob("*.md"))]
    reports.sort(key=lambda item: (len(item.warnings), item.marker_total), reverse=True)

    flagged = [report for report in reports if report.warnings or report.marker_total]
    print(f"docs AI-flavor report: {len(reports)} documents, {len(flagged)} with markers or warnings")
    print(f"{'document':<62} {'em':>3} {'rt':>3} {'other':>6} {'warn':>4}")
    for report in reports[: args.limit]:
        if not report.markers and not report.warnings:
            continue
        other = sum(
            value for key, value in report.markers.items() if key not in ("em_dash", "rather_than")
        )
        relative = report.path.relative_to(root).as_posix()
        print(
            f"{relative:<62} {report.markers['em_dash']:>3} "
            f"{report.markers['rather_than']:>3} {other:>6} {len(report.warnings):>4}"
        )
        for warning in report.warnings:
            print(f"  - {warning}")

    warning_documents = sum(1 for report in reports if report.warnings)
    print(f"warning documents: {warning_documents}")
    if args.strict and warning_documents:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
