#!/usr/bin/env python3
"""Format or check the ecs sources with clang-format.

This is intentionally dependency-free: it needs only a clang-format binary and
locates one itself, preferring the copy shipped with the compiler toolchain
(MSVC/LLVM on Windows, the system clang on Linux) over anything on PATH.

Usage
-----
    python tools/format.py            # reformat files in place
    python tools/format.py --check    # report files that are not formatted
    python tools/format.py --list     # just list the files that would be handled
    python tools/format.py --staged   # only files staged in git

Exit codes: 0 = ok, 1 = check failed (files need formatting), 2 = environment
problem (no clang-format found, git missing, ...).
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

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


def find_clang_format() -> str | None:
    """Locate a clang-format binary, preferring toolchain-bundled copies.

    Visual Studio ships one binary per target architecture under
    VC/Tools/Llvm/<arch>/bin. The host-architecture copy is preferred because a
    cross-target binary (for example x86 on an x64 host) will be found first
    alphabetically and then fail to launch.
    """
    override = os.environ.get("CLANG_FORMAT")
    if override:
        return override if Path(override).exists() else None

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
    for pattern in patterns:
        for match in sorted(glob.glob(pattern)):
            if Path(match).is_file():
                return match

    # Only fall back to PATH after the toolchain locations were considered, so
    # that a stale system-wide install does not shadow the compiler's own copy.
    return shutil.which("clang-format")


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
        "--files",
        nargs="+",
        metavar="PATH",
        help="explicit files or directories to process instead of the tracked set",
    )
    args = parser.parse_args()

    clang_format = find_clang_format()
    if not clang_format:
        print(
            "error: clang-format not found.\n"
            "  Set CLANG_FORMAT=/path/to/clang-format, or install it.\n"
            "  Visual Studio ships one under VC\\Tools\\Llvm\\<arch>\\bin\\.",
            file=sys.stderr,
        )
        return 2

    files = collect_files(args.staged, args.files)
    if args.list:
        for path in files:
            print(describe(path))
        print(f"\n{len(files)} file(s)")
        return 0

    if not files:
        print("nothing to do")
        return 0

    version = subprocess.run([clang_format, "--version"], capture_output=True, text=True).stdout.strip()
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
