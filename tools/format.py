#!/usr/bin/env python3
"""Format or check the ecs sources with clang-format.

This is intentionally dependency-free: it needs only a clang-format binary and
locates one itself, preferring the copy shipped with the compiler toolchain
(MSVC/LLVM on Windows, the system clang on Linux) over anything on PATH.

The major version is pinned (see REQUIRED_MAJOR below). clang-format makes
different line-breaking decisions between releases, so a verdict from another
major is not the verdict this repository was formatted with: a file would be
reported as needing formatting, and reformatting with that version would put the
file back out of step with the pinned one.

Usage
-----
    python tools/format.py                  # reformat files in place
    python tools/format.py --check          # report files that are not formatted
    python tools/format.py --list           # just list the files that would be handled
    python tools/format.py --staged         # only files staged in git
    python tools/format.py --any-version    # accept another clang-format major

Exit codes: 0 = ok, 1 = check failed (files need formatting), 2 = environment
problem (no clang-format, wrong major, git missing, ...).
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# The version the tree is formatted with. Raising this means reformatting the
# whole tree in the same commit that raises it, or every file fails the check.
REQUIRED_MAJOR = 22

# Directories that never contain library sources.
SKIP_DIRS = {
    ".git",
    "build",
    "build-rel",
    "build-tests",
    "_deps",
    ".vs",
    ".vscode",
    ".fmtcheck",
    "third_party",
    "extern",
    "out",
}

SOURCE_SUFFIXES = {".h", ".hpp", ".hxx", ".cpp", ".cxx", ".cc"}

REPO_ROOT = Path(__file__).resolve().parent.parent


def clang_format_candidates() -> list[str]:
    """Every clang-format worth trying, in the order they should be preferred.

    An explicit CLANG_FORMAT comes first and is then honoured on its own: the
    user pointed at that binary, so a mismatch is reported rather than quietly
    replaced with another one.
    """
    override = os.environ.get("CLANG_FORMAT")
    if override:
        return [override] if Path(override).exists() else []

    import glob
    import platform

    machine = (platform.machine() or "").lower()
    if machine in ("amd64", "x86_64"):
        host_arches = ["x64", "x86_64", "amd64", ""]
    elif machine in ("arm64", "aarch64"):
        host_arches = ["arm64", "aarch64", ""]
    else:
        host_arches = [machine, ""]

    # Toolchain-bundled copies first, host architecture before anything else.
    patterns = []
    for arch in host_arches:
        arch_part = f"{arch}\\" if arch else "*\\"
        patterns.append(
            rf"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\Llvm\{arch_part}bin\clang-format.exe"
        )
        patterns.append(
            rf"C:\Program Files (x86)\Microsoft Visual Studio\*\*\VC\Tools\Llvm\{arch_part}bin\clang-format.exe"
        )
    patterns += [
        r"C:\Program Files\LLVM\bin\clang-format.exe",
        "/usr/lib/llvm-*/bin/clang-format",
        "/usr/lib/llvm*/bin/clang-format",
        "/opt/homebrew/opt/llvm/bin/clang-format",
        "/usr/local/opt/llvm/bin/clang-format",
    ]

    candidates: list[str] = []
    for pattern in patterns:
        for match in sorted(glob.glob(pattern)):
            if Path(match).is_file() and match not in candidates:
                candidates.append(match)

    # Only fall back to PATH after the toolchain locations were considered, so
    # that a stale system-wide install does not shadow the compiler's own copy.
    on_path = shutil.which("clang-format")
    if on_path and on_path not in candidates:
        candidates.append(on_path)

    return candidates


def clang_format_version(binary: str) -> str | None:
    """The `--version` line of a binary, or None when it will not run."""
    try:
        result = subprocess.run([binary, "--version"], capture_output=True, text=True, check=False)
    except OSError:
        return None

    text = (result.stdout or result.stderr or "").strip()
    return text or None


def major_of(version: str | None) -> int | None:
    if not version:
        return None

    match = re.search(r"(\d+)\.(\d+)\.(\d+)", version)
    return int(match.group(1)) if match else None


def choose_clang_format(any_version: bool) -> tuple[str | None, str | None, str | None]:
    """Pick the binary to use.

    Returns (binary, version, error). A binary of the pinned major is preferred
    among the candidates; when none matches, the caller gets the best candidate
    and its version so that the message can name both.
    """
    override = os.environ.get("CLANG_FORMAT")
    candidates = clang_format_candidates()

    if not candidates:
        return None, None, None

    if override:
        # The user was explicit: use it or explain why it cannot be used.
        version = clang_format_version(override)
        if major_of(version) == REQUIRED_MAJOR or any_version:
            return override, version, None
        return None, None, f"{override} is {version or 'not runnable'}"

    first_version = None
    for candidate in candidates:
        version = clang_format_version(candidate)
        if first_version is None:
            first_version = version
        if major_of(version) == REQUIRED_MAJOR:
            return candidate, version, None

    if any_version:
        return candidates[0], first_version, None

    return None, None, f"{candidates[0]} is {first_version or 'not runnable'}"


def git_available() -> bool:
    return shutil.which("git") is not None


def git_files(only_staged: bool) -> list[Path] | None:
    """Return tracked source files, or None when git cannot be used."""
    if not git_available():
        return None
    cmd = ["git", "-C", str(REPO_ROOT), "diff", "--name-only", "--cached", "--diff-filter=ACMR"]
    if not only_staged:
        cmd = ["git", "-C", str(REPO_ROOT), "ls-files"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    except subprocess.CalledProcessError:
        return None
    files = []
    for line in out.splitlines():
        line = line.strip()
        if not line:
            continue
        path = REPO_ROOT / line
        if path.suffix.lower() in SOURCE_SUFFIXES and path.is_file():
            files.append(path)
    return files


def walk_files() -> list[Path]:
    """Fallback discovery when git is unavailable."""
    files = []
    for path in REPO_ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if any(part in SKIP_DIRS for part in path.relative_to(REPO_ROOT).parts):
            continue
        files.append(path)
    return files


def describe(path: Path) -> str:
    """Render a path relative to the repository when possible.

    Files passed explicitly with --files may live outside the checkout, in which
    case the absolute path is the only meaningful description.
    """
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return str(path)


def collect_files(only_staged: bool, explicit: list[str] | None = None) -> list[Path]:
    """Resolve the set of files to process.

    `explicit` takes precedence over discovery: each entry may be a file or a
    directory, given absolute or relative to the current directory, and is
    filtered to source suffixes so that a directory can be passed safely.
    """
    if explicit:
        files: list[Path] = []
        for entry in explicit:
            path = Path(entry)
            if not path.is_absolute():
                path = Path.cwd() / path
            path = path.resolve()
            if path.is_dir():
                for candidate in path.rglob("*"):
                    if candidate.is_file() and candidate.suffix.lower() in SOURCE_SUFFIXES:
                        files.append(candidate)
            elif path.is_file():
                if path.suffix.lower() not in SOURCE_SUFFIXES:
                    print(f"error: not a source file: {entry}", file=sys.stderr)
                    raise SystemExit(2)
                files.append(path)
            else:
                print(f"error: no such file or directory: {entry}", file=sys.stderr)
                raise SystemExit(2)
        if not files:
            print("error: no source files matched", file=sys.stderr)
            raise SystemExit(2)
        return sorted(set(files))

    files = git_files(only_staged)
    if files is None:
        if only_staged:
            print("error: --staged requires git", file=sys.stderr)
            raise SystemExit(2)
        files = walk_files()
    return sorted(set(files))


def main() -> int:
    parser = argparse.ArgumentParser(description="Run clang-format over the ecs sources.")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--check", action="store_true", help="do not modify; fail if formatting is needed")
    group.add_argument("--list", action="store_true", help="list the files that would be processed")
    parser.add_argument("--staged", action="store_true", help="restrict to files staged in git")
    parser.add_argument("--verbose", action="store_true", help="print the clang-format command line")
    parser.add_argument(
        "--any-version",
        action="store_true",
        help=f"accept a clang-format other than {REQUIRED_MAJOR}.x (the verdict may differ)",
    )
    parser.add_argument(
        "--files",
        nargs="+",
        metavar="PATH",
        help="explicit files or directories to process instead of the tracked set",
    )
    args = parser.parse_args()

    clang_format, version, mismatch = choose_clang_format(args.any_version)

    if clang_format is None and mismatch:
        print(
            f"error: clang-format {REQUIRED_MAJOR}.x is required, but {mismatch}.\n"
            f"  The sources are formatted with {REQUIRED_MAJOR}; other majors break lines differently,\n"
            "  so their verdict is not the one this repository was formatted with.\n"
            f"  Install LLVM {REQUIRED_MAJOR}, or point CLANG_FORMAT at a {REQUIRED_MAJOR}.x binary.\n"
            "  Deliberately formatting with another version: --any-version.",
            file=sys.stderr,
        )
        return 2

    if not clang_format:
        print(
            "error: clang-format not found.\n"
            "  Set CLANG_FORMAT=/path/to/clang-format, or install it.\n"
            "  Visual Studio ships one under VC\\Tools\\Llvm\\<arch>\\bin\\.",
            file=sys.stderr,
        )
        return 2

    if args.any_version and major_of(version) != REQUIRED_MAJOR:
        print(f"warning: using {clang_format} ({version}); this is not {REQUIRED_MAJOR}.x", file=sys.stderr)

    files = collect_files(args.staged, args.files)
    if args.list:
        for path in files:
            print(describe(path))
        print(f"\n{len(files)} file(s)")
        return 0

    if not files:
        print("nothing to do")
        return 0

    print(f"using {clang_format} ({version})")
    print(f"{'checking' if args.check else 'formatting'} {len(files)} file(s)")

    if args.check:
        cmd = [clang_format, "--dry-run", "--Werror", "-i", *map(str, files)]
    else:
        cmd = [clang_format, "-i", *map(str, files)]

    if args.verbose:
        print(" ".join(cmd))

    result = subprocess.run(cmd)
    if result.returncode != 0:
        if args.check:
            print(
                "\nFormatting problems found. Run `python tools/format.py` to fix them.",
                file=sys.stderr,
            )
            return 1

        # Not a formatting verdict: clang-format itself failed (bad file,
        # unreadable input, crash). That is an environment problem, not a
        # "please format the code" signal, so it must not share exit code 1.
        print(f"\nclang-format failed with exit code {result.returncode}.", file=sys.stderr)
        return 2

    if args.check:
        print("all files are correctly formatted")
    else:
        print("done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
