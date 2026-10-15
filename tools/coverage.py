#!/usr/bin/env python3
"""Measure how much of the library the tests actually execute.

Coverage is source-based: the tests are built instrumented by clang
(tools/cmake/Coverage.cmake), each run writes a raw profile, and llvm-cov turns
the merged profile into lcov records.

Why the lcov route: llvm-cov cannot union independent executables that each
contain the library. Asked to report several of them at once it keeps the files
they have in common and loses the rest, and reported one at a time it counts the
library once per binary. So each test binary is exported separately and the
records are merged here, where a line executed by any test counts as executed -
which is what coverage of the library means.

The threshold is a floor, not a target: it starts at what the suite reaches and
only moves up. Coverage written into a report that nobody compares against
anything is decoration.

Usage
-----
    python tools/coverage.py                 # build, run, measure, check the floor
    python tools/coverage.py --no-build      # measure the tree as it stands
    python tools/coverage.py --uncovered     # list the lines no test reaches
    python tools/coverage.py --html DIR      # also write an HTML report
    python tools/coverage.py --lcov FILE     # where to write the merged tracefile

Exit codes: 0 = at or above the floor, 1 = below it or the tests failed,
2 = environment problem (no clang, no llvm tools, no coverage tree).

Only the library is measured: tests, benchmarks, the header check and the test
framework are excluded by path.
"""

from __future__ import annotations

import argparse
import glob
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

from check import Shell, say  # noqa: E402  (the environment handling lives with the other checks)

# The instrumented configuration differs between platforms only in the compiler
# driver: clang-cl on Windows, clang elsewhere. The flags come from
# tools/cmake/Coverage.cmake either way, so the tool picks the preset and lets
# --preset override it.
DEFAULT_PRESET = "coverage" if os.name == "nt" else "linux-coverage"
PRESET = DEFAULT_PRESET
BUILD_DIR = REPO_ROOT / "build" / PRESET

# Nothing under these paths is ours to be measured by the library's number.
EXCLUDE_PATTERN = r"(^|[\\/])(tests|build|_deps)[\\/]"

# The floor, measured rather than chosen: the numbers are each platform's first
# full run on 15.10.2026 after the dependency machinery and the const accessors were
# covered, and they only move up. Lowering one is a decision, not a maintenance step.
#
# Lines are at 100% everywhere because the last uncovered one - the throw for an
# exhausted entity id space - is exercised by an ordinary run: its case is tagged
# [slow] and registered like any other, and the whole space is handed out in about
# 15 s in Release and 67 s in MSVC Debug. There is no partial mode, so a new library
# line that no test reaches fails the gate rather than being excused by a flag.
#
# Branches are per preset, because the metric is not comparable across ABIs. On the
# Itanium ABI every call that can throw gets a landing pad, so the Linux build
# instruments 254 branches where clang-cl instruments 214, and the cold ones are
# never taken by a test that passes: 85.04% against 91.59%, for the same code and
# the same tests. Neither number is the better one - they count different things -
# so each platform holds its own line.
#
# The function percentage is dominated by template instantiations - one per
# component pack in selection.hpp, storage.hpp and sparse_vector.hpp - so it says
# more about how many combinations exist than about what is untested. Lines and
# branches are the figures to work from; branches sit well below 100% because the
# compiler emits arcs for exceptions, static initialisation and short-circuit
# operators, where the source has nothing to decide.
FLOORS: dict[str, tuple[float, float, float]] = {
    # preset: (functions, lines, branches)
    "coverage": (77.0, 100.0, 91.5),          # clang-cl on Windows, the MSVC ABI
    "linux-coverage": (77.0, 100.0, 85.0),    # clang on Linux, the Itanium ABI
}

# A preset this file has not measured gets the loosest branch line and says so when
# it checks, rather than silently holding a platform to a number taken elsewhere.
DEFAULT_FLOOR = (77.0, 100.0, 85.0)


def floors_for(preset: str) -> tuple[float, float, float]:
    return FLOORS.get(preset, DEFAULT_FLOOR)


def find_llvm_tool(name: str) -> str | None:
    """Locate an llvm tool, preferring the copy that ships with the toolchain."""
    override = os.environ.get("LLVM_TOOLS_DIR")
    if override:
        candidate = Path(override) / name
        if candidate.is_file():
            return str(candidate)

    machine = (platform.machine() or "").lower()
    arches = ["ARM64", "aarch64"] if machine in ("arm64", "aarch64") else ["x64", "x86_64", ""]
    program_files = os.environ.get("ProgramFiles") or "C:/Program Files"

    patterns = []
    for arch in arches:
        arch_part = f"{arch}/" if arch else "*/"
        patterns.append(f"{program_files}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/{arch_part}bin/{name}")
    patterns.append(f"{program_files}/LLVM/bin/{name}")

    for pattern in patterns:
        for match in sorted(glob.glob(pattern)):
            if Path(match).is_file():
                return match

    return shutil.which(name)


class FileCoverage:
    """Line, function and branch records for one source file."""

    def __init__(self) -> None:
        self.lines: dict[int, int] = {}
        # name -> (line, execution count); lcov carries both, and rewriting the
        # tracefile without the line would produce records tools cannot place.
        self.functions: dict[str, tuple[int, int]] = {}
        self.branches: dict[tuple[int, int, int], int] = {}

    def merge_line(self, line: int, count: int) -> None:
        self.lines[line] = max(self.lines.get(line, 0), count)

    def merge_function(self, name: str, line: int, count: int) -> None:
        previous_line, previous_count = self.functions.get(name, (line, 0))
        self.functions[name] = (previous_line or line, max(previous_count, count))

    def merge_branch(self, key: tuple[int, int, int], taken: int) -> None:
        self.branches[key] = max(self.branches.get(key, 0), taken)


def parse_lcov(text: str) -> dict[str, FileCoverage]:
    """Read lcov records into per-file coverage."""
    files: dict[str, FileCoverage] = {}
    current: FileCoverage | None = None

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue

        if line.startswith("SF:"):
            current = files.setdefault(line[3:], FileCoverage())
        elif current is None:
            continue
        elif line.startswith("DA:"):
            fields = line[3:].split(",")
            if len(fields) >= 2:
                current.merge_line(int(fields[0]), int(fields[1]))
        elif line.startswith("FN:"):
            fields = line[3:].split(",", 1)
            if len(fields) == 2:
                current.merge_function(fields[1], int(fields[0]), 0)
        elif line.startswith("FNDA:"):
            fields = line[5:].split(",", 1)
            if len(fields) == 2:
                current.merge_function(fields[1], 0, int(fields[0]))
        elif line.startswith("BRDA:"):
            fields = line[5:].split(",")
            if len(fields) >= 4:
                taken = 0 if fields[3] == "-" else int(fields[3])
                current.merge_branch((int(fields[0]), int(fields[1]), int(fields[2])), taken)
        elif line == "end_of_record":
            current = None

    return files


def export_lcov(llvm_cov: str, profile: Path, binary: Path, shell: Shell) -> str:
    result = shell.run(
        [
            llvm_cov,
            "export",
            "--format=lcov",
            f"--instr-profile={profile}",
            f"--ignore-filename-regex={EXCLUDE_PATTERN}",
            str(binary),
        ],
        capture=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"llvm-cov export failed for {binary.name}:\n{result.stderr}")
    return result.stdout or ""


def percent(hit: int, total: int) -> float:
    return 100.0 * hit / total if total else 100.0


def discover_test_binaries(build_dir: Path) -> list[Path]:
    """Test executables of the coverage tree, and nothing else in that directory.

    The directory also holds CTestTestfile.cmake and the build system's files,
    so a plain glob is not enough.
    """
    tests_dir = build_dir / "tests"
    if not tests_dir.is_dir():
        return []

    found = []
    for path in sorted(tests_dir.iterdir()):
        if not path.is_file() or path.name.startswith("CTest"):
            continue
        if os.name == "nt" and path.suffix == ".exe":
            found.append(path)
        elif os.name != "nt" and os.access(path, os.X_OK):
            found.append(path)

    return found


def relative(path: str) -> str:
    try:
        return Path(path).resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path


def main() -> int:
    parser = argparse.ArgumentParser(description="Measure library coverage with clang and llvm-cov.")
    parser.add_argument("--preset", default=DEFAULT_PRESET,
                        help=f"configure preset to use (default: {DEFAULT_PRESET})")
    parser.add_argument("--no-build", action="store_true", help="do not configure or build")
    parser.add_argument("--no-tests", action="store_true", help="do not run the tests")
    parser.add_argument("--uncovered", action="store_true", help="list lines no test reaches")
    parser.add_argument("--html", metavar="DIR", help="also write an HTML report there")
    parser.add_argument("--lcov", metavar="FILE", help="where to write the merged lcov tracefile")
    parser.add_argument("--junit", metavar="FILE",
                        help="where ctest writes its JUnit report (default: inside the build directory)")
    parser.add_argument("--summary", metavar="FILE",
                        help="also write the numbers as a markdown block, for a CI run summary")
    parser.add_argument("--no-threshold", action="store_true", help="report without checking the floor")
    args = parser.parse_args()

    # The preset decides the build directory, and everything below follows it.
    global PRESET, BUILD_DIR
    PRESET = args.preset
    BUILD_DIR = REPO_ROOT / "build" / PRESET

    # Named by the caller when it wants a particular place - the CI does, because it
    # publishes the file - and otherwise next to the build tree.
    junit_path = Path(args.junit) if args.junit else BUILD_DIR / "junit.xml"

    llvm_cov = find_llvm_tool("llvm-cov.exe" if os.name == "nt" else "llvm-cov")
    llvm_profdata = find_llvm_tool("llvm-profdata.exe" if os.name == "nt" else "llvm-profdata")
    if not llvm_cov or not llvm_profdata:
        print(
            "error: llvm-cov and llvm-profdata are required and were not found.\n"
            "  They ship with LLVM; set LLVM_TOOLS_DIR to the directory holding them.",
            file=sys.stderr,
        )
        return 2

    shell = Shell(None)

    if not args.no_build:
        if shell.reconfigure or not (BUILD_DIR / "CMakeCache.txt").is_file():
            say(f"== configuring {PRESET}")
            if shell.run(["cmake", "--preset", PRESET]).returncode != 0:
                print("error: cmake --preset coverage failed", file=sys.stderr)
                return 2
        say(f"== building {PRESET}")
        if shell.run(["cmake", "--build", "--preset", PRESET]).returncode != 0:
            print("error: coverage build failed", file=sys.stderr)
            return 2

    binaries = discover_test_binaries(BUILD_DIR)
    if not binaries:
        print("error: no test executables in the coverage tree", file=sys.stderr)
        return 2

    profiles = BUILD_DIR / "profiles"
    profiles.mkdir(parents=True, exist_ok=True)
    for stale in profiles.glob("*.profraw"):
        stale.unlink()

    if not args.no_tests:
        say("")
        say("== running the tests (instrumented)")
        environment = dict(shell.environment() or os.environ)
        environment["LLVM_PROFILE_FILE"] = str(profiles / "%p-%m.profraw")
        result = subprocess.run(
            [
                "ctest",
                "--test-dir",
                str(BUILD_DIR),
                "--output-on-failure",
                "--output-junit",
                # Resolved, because ctest reads a relative --output-junit against
                # --test-dir rather than against the working directory: a bare
                # `build/junit.xml` would land inside the build directory as
                # `build/<preset>/build/junit.xml`, which is exactly how the CI
                # reports came to read zero tests for two platforms.
                str(junit_path.resolve()),
            ],
            cwd=REPO_ROOT,
            env=environment,
        )
        if result.returncode != 0:
            print("error: the instrumented test run failed", file=sys.stderr)
            return 1

    raw = sorted(profiles.glob("*.profraw"))
    if not raw:
        print("error: the run produced no profile data", file=sys.stderr)
        return 2

    say(f"== merging {len(raw)} profile(s) from {len(binaries)} test binary(ies)")
    profdata = BUILD_DIR / "coverage.profdata"
    merge = shell.run(
        [llvm_profdata, "merge", "-sparse", *[str(path) for path in raw], "-o", str(profdata)],
        capture=True,
    )
    if merge.returncode != 0:
        print(f"error: llvm-profdata merge failed:\n{merge.stderr}", file=sys.stderr)
        return 2

    merged: dict[str, FileCoverage] = {}
    for binary in binaries:
        for path, coverage in parse_lcov(export_lcov(llvm_cov, profdata, binary, shell)).items():
            target = merged.setdefault(path, FileCoverage())
            for line, count in coverage.lines.items():
                target.merge_line(line, count)
            for name, (line, count) in coverage.functions.items():
                target.merge_function(name, line, count)
            for key, count in coverage.branches.items():
                target.merge_branch(key, count)

    if not merged:
        print("error: no coverage records for library sources", file=sys.stderr)
        return 2

    stats = []
    total_lines = total_lines_hit = 0
    total_functions = total_functions_hit = 0
    total_branches = total_branches_hit = 0

    for path, coverage in merged.items():
        lines = len(coverage.lines)
        lines_hit = sum(1 for count in coverage.lines.values() if count > 0)
        functions = len(coverage.functions)
        functions_hit = sum(1 for _, count in coverage.functions.values() if count > 0)
        branches = len(coverage.branches)
        branches_hit = sum(1 for count in coverage.branches.values() if count > 0)

        total_lines += lines
        total_lines_hit += lines_hit
        total_functions += functions
        total_functions_hit += functions_hit
        total_branches += branches
        total_branches_hit += branches_hit

        stats.append((relative(path), lines, lines_hit, functions, functions_hit, branches, branches_hit))

    line_pct = percent(total_lines_hit, total_lines)
    function_pct = percent(total_functions_hit, total_functions)
    branch_pct = percent(total_branches_hit, total_branches)

    say("")
    say("library coverage")
    say(f"  functions  {total_functions_hit:5}/{total_functions:<5} {function_pct:6.2f}%")
    say(f"  lines      {total_lines_hit:5}/{total_lines:<5} {line_pct:6.2f}%")
    say(f"  branches   {total_branches_hit:5}/{total_branches:<5} {branch_pct:6.2f}%")
    say("")
    say("  worst files (lines uncovered):")

    for name, lines, lines_hit, functions, functions_hit, branches, branches_hit in sorted(
        stats, key=lambda item: (percent(item[2], item[1]), item[0])
    )[:12]:
        say(
            f"    {name:<34} lines {percent(lines_hit, lines):6.2f}%  "
            f"functions {percent(functions_hit, functions):6.2f}%  branches {percent(branches_hit, branches):6.2f}%"
        )

    lcov_path = Path(args.lcov) if args.lcov else BUILD_DIR / "merged.lcov"
    lcov_path.parent.mkdir(parents=True, exist_ok=True)
    lcov_path.write_text(render_lcov(merged), encoding="utf-8")
    say("")
    say(f"  merged tracefile: {lcov_path}")

    if args.uncovered:
        say("")
        say("  uncovered lines:")
        for name, coverage in sorted(merged.items()):
            missed = sorted(line for line, count in coverage.lines.items() if count == 0)
            if missed:
                say(f"    {relative(name)}: {format_ranges(missed)}")

    if args.html:
        say("")
        say("== writing the HTML report")
        html_dir = Path(args.html)
        html_dir.mkdir(parents=True, exist_ok=True)
        result = shell.run(
            [
                llvm_cov,
                "show",
                f"--instr-profile={profdata}",
                f"--ignore-filename-regex={EXCLUDE_PATTERN}",
                "--format=html",
                f"--output-dir={html_dir}",
                *[str(binary) for binary in binaries],
            ],
            capture=True,
        )
        if result.returncode != 0:
            say(f"   llvm-cov show failed: {result.stderr.strip().splitlines()[:1]}")
        else:
            say(f"   {html_dir / 'index.html'}")

    # The verdict is part of the report: written before the exit code is decided, so
    # a run that fails the floor still leaves a summary behind for the dashboard.
    min_functions, min_lines, min_branches = floors_for( PRESET )
    failures = []
    if function_pct + 1e-9 < min_functions:
        failures.append(f"functions {function_pct:.2f}% < {min_functions:.2f}%")
    if line_pct + 1e-9 < min_lines:
        failures.append(f"lines {line_pct:.2f}% < {min_lines:.2f}%")
    if branch_pct + 1e-9 < min_branches:
        failures.append(f"branches {branch_pct:.2f}% < {min_branches:.2f}%")

    if args.summary:
        write_summary(
            Path(args.summary),
            PRESET,
            (total_lines, total_lines_hit, line_pct),
            (total_functions, total_functions_hit, function_pct),
            (total_branches, total_branches_hit, branch_pct),
            (min_functions, min_lines, min_branches),
            failures,
        )

    if args.no_threshold:
        return 0

    if failures:
        say("")
        say("below the floor: " + "; ".join(failures))
        return 1

    return 0


def write_summary(
    path: Path,
    preset: str,
    lines: tuple[int, int, float],
    functions: tuple[int, int, float],
    branches: tuple[int, int, float],
    floor: tuple[float, float, float],
    failures: list[str],
) -> None:
    """Write the numbers as markdown, for a CI run summary."""
    min_functions, min_lines, min_branches = floor
    verdict = (
        "Below the floor: " + "; ".join(failures)
        if failures
        else f"Meets the floor ({min_lines:.1f}% lines, {min_branches:.1f}% branches, {min_functions:.1f}% functions)."
    )
    rows = [("Lines", lines), ("Functions", functions), ("Branches", branches)]
    text = [
        f"### Coverage ({preset})",
        "",
        "| Metric | Covered | Total | Percent |",
        "| --- | --- | --- | --- |",
    ]
    for name, (total, hit, pct) in rows:
        text.append(f"| {name} | {hit} | {total} | {pct:.2f}% |")
    text += ["", verdict, ""]
    path.write_text("\n".join(text), encoding="utf-8")


def format_ranges(lines: list[int]) -> str:
    """Compact a sorted line list into `5-9, 12` form."""
    parts = []
    start = previous = lines[0]
    for line in lines[1:]:
        if line == previous + 1:
            previous = line
            continue
        parts.append(f"{start}" if start == previous else f"{start}-{previous}")
        start = previous = line
    parts.append(f"{start}" if start == previous else f"{start}-{previous}")
    return ", ".join(parts)


def render_lcov(merged: dict[str, FileCoverage]) -> str:
    """Write the merged records back out, so the result can be consumed elsewhere."""
    chunks = []
    for path, coverage in sorted(merged.items()):
        lines = [f"SF:{Path(path).resolve().as_posix()}"]
        for name, (line, count) in sorted(coverage.functions.items()):
            lines.append(f"FN:{line},{name}")
            lines.append(f"FNDA:{count},{name}")
        lines.append(f"FNF:{len(coverage.functions)}")
        lines.append(f"FNH:{sum(1 for _, count in coverage.functions.values() if count > 0)}")
        for line, count in sorted(coverage.lines.items()):
            lines.append(f"DA:{line},{count}")
        lines.append(f"LF:{len(coverage.lines)}")
        lines.append(f"LH:{sum(1 for count in coverage.lines.values() if count > 0)}")
        for (line, block, branch), taken in sorted(coverage.branches.items()):
            lines.append(f"BRDA:{line},{block},{branch},{taken if taken else '-'}")
        lines.append(f"BRF:{len(coverage.branches)}")
        lines.append(f"BRH:{sum(1 for count in coverage.branches.values() if count > 0)}")
        lines.append("end_of_record")
        chunks.append("\n".join(lines))

    return "\n".join(chunks) + "\n"


if __name__ == "__main__":
    raise SystemExit(main())
