#!/usr/bin/env python3/usr/bin/env python3
"""Run the checks a commit is expected to pass.

This is the gate the repository rules describe: the code is formatted, it builds
with warnings as errors, the tests pass, and - when asked for - it survives the
sanitizers and satisfies clang-tidy. Doing it by hand means remembering four
commands and the preset each one belongs to, which is exactly how a step gets
skipped.

Usage
-----
    python tools/check.py                  # formatting of staged files, build, tests
    python tools/check.py --full           # ... plus sanitizers and clang-tidy
    python tools/check.py --format-all     # check the formatting of every tracked file
    python tools/check.py --sanitizers     # only add the sanitizer run
    python tools/check.py --tidy           # only add the clang-tidy run
    python tools/check.py --preset msvc    # build through another preset

Options
-------
    --preset NAME     configure/build preset (default: tests-only on Windows, linux-gcc elsewhere)
    --sanitizers      also build and run the sanitizer configuration
    --tidy            also run clang-tidy over the library, the header check and the tests
    --full            --sanitizers and --tidy
    --format-all      check every tracked file, not only the staged ones
    --no-format       skip the formatting check
    --no-build        skip the build
    --no-tests        skip ctest
    --jobs N          parallel build jobs

Exit codes: 0 = everything passed, 1 = a check failed, 2 = environment problem
(no cmake, no compiler environment, unknown preset).
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SYSTEMS = {0: "ok", 1: "check failed", 2: "environment problem"}

# The compile-database configuration differs per platform only in the compiler
# driver: clang-cl on Windows, clang elsewhere. See tools/CMakePresets.json.
TIDY_PRESET = "tidy" if os.name == "nt" else "linux-tidy"

# Likewise for the ordinary build: the preset is named for the toolchain, and the
# toolchain is a property of the platform.
DEFAULT_PRESET = "tests-only" if os.name == "nt" else "linux-gcc"

if os.name == "nt":
    # CTest sometimes loses characters through the legacy console code page, and
    # Catch2 prints the assertion text - which is where a failure is explained.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[union-attr]
        except (AttributeError, ValueError):
            pass


def say(message: str) -> None:
    print(message, flush=True)


def find_vcvars() -> str | None:
    """Locate the MSVC environment script.

    The presets drive cl.exe and clang-cl, and neither is on PATH in a plain
    shell: the compiler environment has to be loaded first. vswhere is the
    supported way to ask, the glob is the fallback, and VCVARS overrides both so
    a run can be tied to a specific toolchain.
    """
    override = os.environ.get("VCVARS")
    if override:
        return override if Path(override).is_file() else None

    program_files = os.environ.get("ProgramFiles(x86)") or os.environ.get("ProgramFiles")
    if program_files:
        vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if vswhere.is_file():
            result = subprocess.run(
                [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                capture_output=True,
                text=True,
            )
            for line in result.stdout.splitlines():
                candidate = Path(line.strip()) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if candidate.is_file():
                    return str(candidate)

        candidates = sorted(
            Path(program_files).glob("Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat"),
            reverse=True,
        )
        if candidates:
            return str(candidates[0])

    return None


class Shell:
    """Runs commands with the compiler environment loaded.

    The environment is captured once by running `set` after vcvars64.bat and
    passed to every command: exporting it inside one `cmd /c` call would affect
    that call only, and a fresh configure needs the compiler and the generator on
    PATH - not just the absolute paths an existing cache already holds.
    """

    def __init__(self, jobs: int | None, reconfigure: bool = False) -> None:
        self.jobs = jobs
        self.reconfigure = reconfigure
        self.vcvars: str | None = None
        self._env: dict[str, str] | None = None

    def environment(self) -> dict[str, str] | None:
        """The environment every command should run with, loading it once."""
        if self._env is not None:
            return self._env

        if os.name != "nt":
            return None

        # vcvars sets these two, so their presence is what "the environment is
        # already loaded" means - a developer command prompt, or a build that was
        # started from one. Asking whether some compiler is on PATH is not the same
        # question: a machine with clang-cl installed but no MSVC environment
        # answered yes to that and then had no cl.exe for the presets that pin it.
        if os.environ.get("VCINSTALLDIR") or os.environ.get("VSCMD_VER"):
            return None

        self.vcvars = find_vcvars()
        if self.vcvars is None:
            # No Visual Studio to ask, but a compiler is reachable: a standalone
            # toolchain on PATH is a legitimate way to have one.
            if shutil.which("cl") is not None or shutil.which("clang-cl") is not None:
                return None
            raise EnvironmentError(
                "No MSVC environment found. Install Visual Studio with the C++ workload, "
                "set VCVARS to a vcvars64.bat, or run this from a developer command prompt."
            )

        say(f"   (loading the compiler environment from {self.vcvars})")
        result = subprocess.run(
            f'call "{self.vcvars}" >nul && set',
            shell=True,
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if result.returncode != 0:
            raise EnvironmentError(f"vcvars64.bat failed: {result.stdout}{result.stderr}")

        environment: dict[str, str] = {}
        for line in result.stdout.splitlines():
            name, separator, value = line.partition("=")
            if separator:
                environment[name] = value

        self._env = environment
        return self._env

    def run(self, command: list[str], capture: bool = False) -> subprocess.CompletedProcess:
        environment = self.environment()
        return subprocess.run(
            command,
            cwd=REPO_ROOT,
            env=environment,
            capture_output=capture,
            text=True,
            encoding="utf-8",
            errors="replace",
        )


def step_format(shell: Shell, format_all: bool) -> bool:
    say("== formatting")
    python = sys.executable or "python"
    command = [python, "tools/format.py", "--check"]
    if not format_all:
        # The gate that matters before a commit is the code being committed. The
        # tree as a whole is not formatted yet - the one-off reformat is a
        # separate task - so checking every file here would refuse every commit.
        command.append("--staged")
    result = shell.run(command)
    if result.returncode == 0:
        say("   ok")
        return True
    if result.returncode == 2:
        raise EnvironmentError("clang-format was not found; tools/format.py explains how to point at one.")
    say("   FAILED: run `python tools/format.py` (or `--staged`) to fix it")
    return False


def build_tree(shell: Shell, preset: str, build_dir: Path, reconfigure: bool = False) -> bool:
    # A configured tree keeps the toolchain it was configured with. Re-running
    # the configure step against a different compiler environment makes CMake
    # refuse the stale cache, so it happens only when there is no cache yet or
    # when --reconfigure asks for it.
    need_configure = reconfigure or not (build_dir / "CMakeCache.txt").is_file()

    if need_configure:
        say(f"== configuring {preset}")
        result = shell.run(["cmake", "--preset", preset])
        if result.returncode != 0:
            say(f"   FAILED: cmake --preset {preset}")
            return False

    say(f"== building {preset}")
    command = ["cmake", "--build", "--preset", preset]
    if shell.jobs:
        command += ["--parallel", str(shell.jobs)]
    result = shell.run(command)
    if result.returncode != 0:
        say(f"   FAILED: build of {preset} (see above)")
        return False
    say("   ok")
    return True


def step_tests(shell: Shell, build_dir: Path) -> bool:
    say("== tests")
    # JUnit is written next to the build tree on every run: it costs nothing here
    # and it is what a CI job publishes, so the local gate produces the same
    # artefact the runner would.
    result = shell.run(
        [
            "ctest",
            "--test-dir",
            str(build_dir),
            "--output-on-failure",
            "--output-junit",
            str(build_dir / "junit.xml"),
        ]
    )
    if result.returncode != 0:
        say("   FAILED: ctest")
        return False
    say("   ok")
    return True


def step_sanitizers(shell: Shell, preset: str) -> bool:
    # Windows has one working implementation - MSVC's ASan - and clang refuses
    # both its runtimes there for reasons the module explains. Elsewhere the
    # sanitizers come from the compiler driver, so the ordinary preset is
    # configured with the flags on.
    if os.name == "nt":
        sanitizer_preset = "msvc-asan"
        build_dir = REPO_ROOT / "build" / sanitizer_preset
        return build_tree(shell, sanitizer_preset, build_dir, shell.reconfigure) and step_tests(shell, build_dir)

    sanitizer_preset = "sanitize"
    say("== sanitizers")
    build_dir = REPO_ROOT / "build" / sanitizer_preset
    configure = [
        "cmake",
        "-S",
        ".",
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DECS_BUILD_TESTS=ON",
        "-DECS_BUILD_BENCHMARKS=OFF",
        "-DECS_ENABLE_ASAN=ON",
        "-DECS_ENABLE_UBSAN=ON",
    ]
    if shell.reconfigure or not (build_dir / "CMakeCache.txt").is_file():
        if shell.run(configure).returncode != 0:
            say("   FAILED: sanitizer configuration")
            return False
    if shell.run(["cmake", "--build", str(build_dir)]).returncode != 0:
        say("   FAILED: sanitizer build")
        return False
    if shell.run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"]).returncode != 0:
        say("   FAILED: tests under sanitizers")
        return False
    say("   ok")
    return True


def find_clang_tidy() -> str | None:
    """Locate clang-tidy, preferring an override and then the toolchain copy.

    The host architecture comes first for the same reason tools/format.py
    prefers it there: Visual Studio ships one binary per target, "ARM64" sorts
    before "x64", and an ARM64 binary on an x64 host fails to launch.
    """
    override = os.environ.get("CLANG_TIDY")
    if override:
        return override if Path(override).exists() else None

    import platform

    machine = (platform.machine() or "").lower()
    host_arches = ["ARM64", "aarch64"] if machine in ("arm64", "aarch64") else ["x64", "x86_64", ""]
    program_files = os.environ.get("ProgramFiles") or "C:/Program Files"

    patterns: list[str] = []
    for arch in host_arches:
        arch_part = f"{arch}/" if arch else "*/"
        patterns.append(f"{program_files}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/{arch_part}bin/clang-tidy.exe")
    patterns.append(f"{program_files}/LLVM/bin/clang-tidy.exe")

    import glob

    for pattern in patterns:
        for match in sorted(glob.glob(pattern)):
            if Path(match).is_file():
                return match

    return shutil.which("clang-tidy")


def step_tidy(shell: Shell) -> bool:
    say("== clang-tidy")

    clang_tidy = find_clang_tidy()
    if not clang_tidy:
        raise EnvironmentError("clang-tidy was not found. Install LLVM, or set CLANG_TIDY to its path.")

    database = REPO_ROOT / "build" / TIDY_PRESET
    # Configured on every run, not only when the cache is missing: this step never
    # builds, so nothing regenerates the compile database for us, and a file added
    # since it was written would simply not be checked. The configure itself costs
    # about a second.
    if shell.run(["cmake", "--preset", TIDY_PRESET]).returncode != 0:
        say(f"   FAILED: cmake --preset {TIDY_PRESET}")
        return False

    compile_commands = database / "compile_commands.json"
    if not compile_commands.is_file():
        raise EnvironmentError(f"{compile_commands} was not produced; the tidy preset exports it.")

    import json

    entries = json.loads(compile_commands.read_text(encoding="utf-8"))
    sources: list[str] = []
    for entry in entries:
        path = Path(entry.get("file", ""))
        # Only this repository's own code: the dependencies are built here too,
        # and their findings are not ours to fix.
        if not path.is_file():
            continue
        if "build" in path.parts or "_deps" in path.parts:
            continue
        sources.append(str(path))

    if not sources:
        say("   FAILED: no source files in the compile database")
        return False

    # One process per file: clang-tidy takes a single translation unit, and the
    # per-file invocation keeps a failure from hiding the rest of the report.
    # Findings are counted from the diagnostic tags, which clang-tidy prints for
    # both warnings and errors: .clang-tidy raises them to errors, so the same
    # check name can arrive either way.
    findings: dict[str, int] = {}
    failed = 0
    for source in sorted(set(sources)):
        result = shell.run([clang_tidy, "-p", str(database), source], capture=True)
        output = (result.stdout or "") + (result.stderr or "")
        reported = False
        for line in output.splitlines():
            if not line.rstrip().endswith("]") or ("warning:" not in line and "error:" not in line):
                continue
            names = line.rstrip()[line.rfind("[") + 1 : -1].split(",")
            for name in names:
                findings[name.strip()] = findings.get(name.strip(), 0) + 1
            reported = True
        if result.returncode != 0 and ("error:" in output or reported):
            failed += 1
            if not reported:
                say(f"   {source}: clang-tidy reported an error")

    total = sum(findings.values())
    say(f"   {len(set(sources))} files, {total} findings, {failed} files with errors")

    if failed:
        return False

    if total == 0:
        say("   ok (no findings)")
        return True

    # .clang-tidy raises findings to errors, so reaching this point means the
    # configuration was told otherwise, or a check reported a warning only.
    say("   findings (raised to errors by .clang-tidy)")
    for name, count in sorted(findings.items(), key=lambda item: -item[1])[:10]:
        say(f"     {count:5}  {name}")
    if len(findings) > 10:
        say(f"     ... and {len(findings) - 10} more checks")
    say("   FAILED: the report is meant to be empty - fix the finding or disable the check with a reason")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the checks a commit is expected to pass.")
    parser.add_argument("--preset", default=DEFAULT_PRESET,
                        help=f"configure/build preset (default: {DEFAULT_PRESET})")
    parser.add_argument("--sanitizers", action="store_true", help="also run the sanitizer configuration")
    parser.add_argument("--tidy", action="store_true", help="also run clang-tidy")
    parser.add_argument("--full", action="store_true", help="--sanitizers and --tidy")
    parser.add_argument("--format-all", action="store_true", help="check every tracked file, not only staged ones")
    parser.add_argument("--no-format", action="store_true", help="skip the formatting check")
    parser.add_argument("--no-build", action="store_true", help="skip the build")
    parser.add_argument("--no-tests", action="store_true", help="skip ctest")
    parser.add_argument("--jobs", type=int, default=0, help="parallel build jobs")
    parser.add_argument("--reconfigure", action="store_true", help="re-run cmake configure for every tree used")
    args = parser.parse_args()

    if shutil.which("cmake") is None and not (args.no_format and args.no_build):
        print("error: cmake was not found on PATH", file=sys.stderr)
        return 2

    shell = Shell(args.jobs or None, args.reconfigure)
    build_dir = REPO_ROOT / "build" / args.preset
    results: list[bool] = []

    try:
        if not args.no_format:
            results.append(step_format(shell, args.format_all))
        if not args.no_build:
            results.append(build_tree(shell, args.preset, build_dir, args.reconfigure))
        if not args.no_tests and not args.no_build:
            results.append(step_tests(shell, build_dir))
        if args.sanitizers or args.full:
            results.append(step_sanitizers(shell, args.preset))
        if args.tidy or args.full:
            results.append(step_tidy(shell))
    except EnvironmentError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    failed = results.count(False)
    say("")
    say(f"{len(results) - failed}/{len(results)} checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
