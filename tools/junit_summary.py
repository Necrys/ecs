#!/usr/bin/env python3
"""Turn CTest's JUnit output into a markdown block for a run summary.

CI runs the suite in several configurations, and the run page should say what each
of them found without anyone opening a log. This reads the JUnit files CTest
writes (`--output-junit`) and prints one table per file, plus the failing cases by
name when there are any.

Usage:

    python3 tools/junit_summary.py --title "Tests" build/release/junit.xml

The exit code is 0 even when a file is missing or empty: the tests themselves
decide whether a job fails, and a summary that fails the job it describes only
hides the real verdict. A file that could not be read is reported as such instead
of being left out.
"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def number(element: ElementTree.Element, name: str) -> int:
    """A count attribute, treating an absent or unparsable value as zero."""
    try:
        return int(element.get(name, "0") or "0")
    except ValueError:
        return 0


class Totals:
    def __init__(self) -> None:
        self.tests = 0
        self.failures = 0
        self.errors = 0
        self.skipped = 0
        self.seconds = 0.0
        self.failed_cases: list[tuple[str, str]] = []

    def add(self, suite: ElementTree.Element) -> None:
        self.tests += number(suite, "tests")
        self.failures += number(suite, "failures")
        self.errors += number(suite, "errors")
        self.skipped += number(suite, "skipped") + number(suite, "disabled")
        try:
            self.seconds += float(suite.get("time", "0") or "0")
        except ValueError:
            pass

        for case in suite.iter("testcase"):
            for outcome in list(case.findall("failure")) + list(case.findall("error")):
                message = (outcome.get("message") or outcome.text or "").strip().splitlines()
                name = ".".join(part for part in (case.get("classname"), case.get("name")) if part)
                self.failed_cases.append((name, message[0] if message else outcome.tag))


def read(path: Path) -> Totals:
    """Read one JUnit file, which CTest writes as a testsuites document."""
    totals = Totals()
    root = ElementTree.parse(path).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.iter("testsuite"))
    for suite in suites:
        totals.add(suite)
    return totals


def render(title: str, reports: list[tuple[str, Totals]]) -> str:
    lines = [f"### {title}", "", "| Report | Tests | Failures | Errors | Skipped | Time |", "| --- | --- | --- | --- | --- | --- |"]
    for label, totals in reports:
        lines.append(
            f"| {label} | {totals.tests} | {totals.failures} | {totals.errors} | "
            f"{totals.skipped} | {totals.seconds:.1f} s |"
        )

    failing = [(label, case, message) for label, totals in reports for case, message in totals.failed_cases]
    if failing:
        lines += ["", "Failing cases:", ""]
        for label, case, message in failing:
            lines.append(f"- `{case}` ({label}): {message}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarise CTest JUnit reports as markdown.")
    parser.add_argument("--title", default="Tests", help="heading for the block")
    parser.add_argument("reports", nargs="+", metavar="JUNIT", help="JUnit XML files to read")
    args = parser.parse_args()

    reports: list[tuple[str, Totals]] = []
    for name in args.reports:
        path = Path(name)
        if path.is_file():
            try:
                reports.append((relative(path), read(path)))
                continue
            except ElementTree.ParseError as error:
                reports.append((f"{relative(path)} (unreadable: {error})", Totals()))
                continue
        reports.append((f"{relative(path)} (missing)", Totals()))

    sys.stdout.write(render(args.title, reports))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
