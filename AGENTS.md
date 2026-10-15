# AGENTS.md - ecs

A short map of the project for people and agents. The purpose of this file is to
provide enough context to avoid rescanning the whole repository before every
task.

---

## What this is

`ecs` is a small header-heavy **Entity-Component-System** library for C++.
An entity is an identifier; components are ordinary user-defined structs stored
in typed containers; systems are objects with an `update()` method that
subscribe to events and declare dependencies between each other.

**Project priorities, in descending order:**

1. **Performance** - the main goal. Component iteration must scale with the
   number of entities, not with the largest identifier.
2. **Multithreading** - a component access model that allows independent
   systems to run in parallel.
3. **Lightweight** - minimal dependencies, little code, fast builds.
4. **Portability** - Windows (MSVC, Clang), Linux (GCC, Clang),
   Raspberry Pi 1/2 (ARMv6/ARMv7, 32-bit).

Configurations verified against the current code: MSVC (VS 2022), Clang in MSVC
mode (`clang-cl`) and Clang in GNU mode, all on Windows x64; GCC 14.2 on both
Raspberry Pi 1 (ARMv6) and Pi 2 (ARMv7), where the library builds, the header
check compiles and the full test suite passes. Linux x64 remains a target
platform that has not been verified. MinGW-w64 and macOS are currently out of
scope: there is no way to build or check them, and claiming unverified support is
meaningless.

The library is provided as is: this is a non-commercial hobby project and you
use it at your own risk. The license is the Unlicense (public domain), with no
conditions attached. Tests, benchmarks and visualisations are shipped as
test/demo material and are not part of the library itself.

---

## Design principles

These are not incidental choices; they shape the code and new work is expected to
follow them.

### Data and logic stay separate

Component storages exist to hold data, not to contain behaviour. A component is
raw data; whatever operates on it belongs in systems, outside the container. The
containers follow the same rule: they offer storage and traversal and nothing
else.

This is why the design is POD-first. Storing plain, trivially destructible data
is the primary case, and the hot paths are written for it. Supporting types with
destructors is a different problem - it needs explicit lifetime management - and
it is not solved by making the existing containers heavier.

A separate container variant for non-POD types is a reasonable future direction,
where destructor calls can be handled properly in isolation. Until then, treat
POD-first as the contract, and do not mix the two concerns in one container.

### Push work into compile time

Performance is the top priority, and a large part of it is meant to come from
resolving things at compile time rather than at run time. That is why the code is
wrapped in templates fairly heavily: type ids, storage lookups and iteration over
component packs are all meant to collapse into constant code.

Keep it that way. Where a choice exists between a run-time mechanism (virtual
dispatch, type erasure, lookup tables keyed by something computed at run time)
and a compile-time one (templates, `if constexpr`, `static_assert`), prefer the
compile-time one unless it makes the code materially harder to follow. Templates
are a deliberate cost paid for speed; do not pay it and then do the work at run
time anyway.

A caveat: this is an ideal to reach for, not a licence for unreadable
metaprogramming. If a compile-time construction becomes impossible to follow, say
so and discuss it rather than layering more machinery on top.

### Tests observe, they do not accommodate

The library must not know that tests exist. Do not add an accessor, a hook, a
flag or a widened type purely so that a test can reach something. A test that
needs the library changed to be written is pointing at either the wrong thing to
test or a genuine gap in the public surface - and a gap is closed for users, not
for tests.

The architecture is expected to be flexible enough that the behaviour is
testable as it stands, through its public surface. If it is not, that is feedback
about the design.

Each unit test stands on its own:

- it builds its own world and depends on no state left behind by another test;
- it does not rely on the order tests happen to run in;
- it asserts observable behaviour rather than internals, so a refactor that keeps
  the contract does not break it.

### Library first, tests as a separate commit

When fixing a defect or adding behaviour, the change lands in two steps:

1. the library change;
2. the tests that cover it, as a separate commit - typically regression tests for
   a fix, or new coverage for new behaviour.

This keeps the two concerns separately reviewable and makes the tests explicit
rather than an afterthought folded into the implementation. Run the full suite
before a library change and again after it; for a large change, rerun everything
rather than trusting a subset.

---

## Branches and commits

- **`master` is the only development branch.** All work happens here.
- **`2020_last` is frozen history.** The last state of the original 2020 line.
  **Do not commit, merge, rebase or delete it.** The branch exists only for
  comparison and archaeology. It is mirrored by the annotated tag `2020-last`,
  pinned to the same commit.
- Feature branches are cut from `master` and merged back; do not keep
  long-lived side branches.

Commit messages: a short line in the imperative mood, without prefixes such as
`feat:` / `fix:`, matching the historical line of the repository. One task, one
logical commit. Everything in the repository - code, comments, documentation,
commit messages - is written in **English**.

---

## Current state (important to understand before starting)

`master` is in an intermediate state: part of what is planned is not
implemented, part is outdated, and part has known defects. **Current behaviour
is not a contract.** The list of defects and unfinished work, with rationale, is
kept in the plan's registry; check it before touching an area so as not to
rediscover what is known and not to polish what is going to be rewritten anyway.

The broad picture:

- the library itself (component storage, registry, systems, events) exists but
  needs to be brought to a working state;
- tests build and run, but cover only part of the library;
- benchmarks exist and are used to keep performance honest, but they are run by
  hand rather than in CI, and there is no recorded baseline in the repository;
- multithreading, resources and deferred operations are not implemented;
- there is no user-facing documentation beyond `README.md`.

**Rule: do not polish code that is going to be rewritten.** Before changing an
architectural area, make sure it is not marked as pending replacement.

---

## Repository layout

```
ecs/
|-- .clang-format          formatting rules (verified, idempotent)
|-- CMakeLists.txt         library build
|-- CODE_STYLE.md          code style: required reading before editing code
|-- AGENTS.md              this file
|-- README.md              introduction to the project
|-- LICENSE                Unlicense (public domain)
|-- include/               public headers
|   |-- types.h                eid_t and common types
|   |-- sparse_vector.h/.hpp   paged sparse array (the storage core)
|   |-- storage.h/.hpp         component storages, join/exclude
|   |-- entity.h/.hpp          entity handle
|   |-- registry.h/.hpp        registry: entities, systems, events
|   |-- system.h/.hpp          systems, dependencies, configuration
|   |-- events.h/.hpp          event queues
|   `-- type_enumerator.h      assigns ids to component/system/event types
|-- src/                   non-template .cpp implementations
|-- cmake/                 build modules (warnings, sanitizers)
|-- check/                 the public headers compiled as their own unit
|-- tests/                 unit tests (Catch2)
|-- bench/                 benchmarks (off by default, see bench/README.md)
`-- tools/                 helper scripts (formatting, checks)
```

File naming convention: `*.h` holds declarations, `*.hpp` holds template and
inline implementations, and `src/*.cpp` holds non-template implementations. New
code should be laid out the same way.

---

## Building and running tests

The library is static and has no external dependencies. It is built as C++17:
Catch2 v3, which the tests use, requires at least C++14, and the standard was
moved straight to 17 to avoid a second migration.

Warnings are part of the build, not an optional extra: `cmake/CompilerWarnings.cmake`
gives every target the same set (`/W4 /permissive-` on MSVC, `-Wall -Wextra
-Wconversion -Wsign-conversion` and the rest on GCC and Clang, plus `-Weverything`
under Clang), and `ECS_WARNINGS_AS_ERRORS` (on by default) turns them into errors.
`check/headers.cpp` compiles the public headers as their own translation unit and
instantiates the template surface, so a warning in a header is not hidden until
somebody uses that combination. Turn the option off only to see the whole list at
once, e.g. `-DECS_WARNINGS_AS_ERRORS=OFF`.

The tests pull Catch2 through `FetchContent`, pinned to an exact commit of
v3.16.1 (see `tests/CMakeLists.txt`), so the dependency cannot shift under us.
`ECS_BUILD_TESTS` is on by default, which means the first configure downloads
Catch2 and therefore needs network access; use `-DECS_BUILD_TESTS=OFF` for an
offline library-only build.

### Before a commit: one command that runs the checks

```powershell
./tools/check.ps1              # formatting of what is staged, build, tests
./tools/check.ps1 -Full        # the same, plus sanitizers and clang-tidy
```

`tools/check.py` is the same thing without PowerShell (`tools/check.sh` on
POSIX), and it loads the compiler environment itself, so it works from an
ordinary shell. Everything is also reachable by hand:

| What | How |
|---|---|
| Formatting | `python tools/format.py --check` (all files) or `--staged` (what is about to be committed) |
| Warnings | `-DECS_WARNINGS_AS_ERRORS=ON` is the default; `OFF` prints the whole list instead of stopping |
| Sanitizers | `cmake --preset msvc-asan` on Windows. Elsewhere `-DECS_ENABLE_ASAN=ON -DECS_ENABLE_UBSAN=ON`; `ECS_ENABLE_TSAN` exists for the day there are threads |
| clang-tidy | `cmake --preset tidy` produces a compile database, then `clang-tidy -p build/tidy <file>`; the checks and the reasons for what is off are in `.clang-tidy` |
| Tests | `ctest --test-dir build/<preset> --output-on-failure` |

Sanitizers on Windows come from MSVC only: clang's ASan runtime dies in
`interception_win` here and its UBSan runtime refuses `/MD` objects, so both are
refused with an explanation rather than half-wired. The module comment in
`cmake/Sanitizers.cmake` records what was measured. Findings from clang-tidy are
a report, not a gate: the C++17 review works through them.

```powershell
# Windows / MSVC, from the repository root
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

```bash
# Linux / GCC or Clang
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

With MSVC it is convenient to call `vcvars64.bat` once, otherwise `cmake` will
not find the compiler. The test binaries can also be run directly; they take the
usual Catch2 command line (`sparse_vector.exe --list-tests`, `-s` for verbose
output, and so on).

Benchmarks are a separate build, off by default and deliberately not registered
with CTest, with one preset per compiler (`msvc`, `clang-cl`, `clang`, `gcc`):

```bash
cmake --preset msvc
cmake --build --preset msvc
build/msvc/bench/bench_iter.exe --csv <path outside the repository>
```

They size their workload from the machine they run on and report a median with a
spread, so they are usable on a desktop and on a Raspberry Pi alike. See
`bench/README.md` before running or changing them, and measure before and after
any change to `join`, `sparse_vector` or `get_*`.

---

## Rules for agents

### General

- **Develop on `master` only.** Do not touch `2020_last` (see "Branches and commits").
- **Do not add third-party dependencies** without explicit agreement: the goal
  is to stay lightweight and build using nothing but the compiler.
- **Do not change `LICENSE`** on your own - that is the author's decision.
- **Do not create documentation nobody reads.** See "Documentation: what to write and where".
- **Write in English**: code, identifiers, comments, documentation and commit
  messages.

### Before editing code

1. Read `CODE_STYLE.md`.
2. Make sure the area is not pending replacement: a local fix in a place that is
   going to be rewritten anyway is pointless.
3. For changes on the hot path (`join`, `sparse_vector`, `get_*`), measure
   first, change second. A change without a measurement is not accepted.

### After editing code

1. Run the formatter. On Windows: `./tools/format.ps1`; on any platform:
   `python tools/format.py`. To check without changing anything, use `-Check` /
   `--check`.
2. Build without warnings at the highest warning level.
3. Run the tests. If functionality was added, add a test; if a defect was fixed,
   add a regression test.
4. Do not leave commented-out code or debug output behind.

`./tools/check.ps1` does 1-3 in one command, and `-Full` adds the sanitizers and
clang-tidy. Run it before a commit rather than trusting memory.

### What not to do

- Do not refactor unrelated things along the way.
- Do not "improve" style by hand: `clang-format` owns style.
- Do not write essay-length comments (see "Documentation: what to write and where").

---

## Documentation: what to write and where

Priority order, from most to least important.

1. **Self-documenting code.** Names of types, functions and parameters should
   explain their purpose without a comment. If a signature needs a comment to be
   understood, rename first and comment only if it is still necessary.
2. **Comments.** Lightweight, one or two lines, only for what is not obvious:
   invariants, the reason behind an unusual decision, pitfalls. No history, no
   restating the code, no research rationale. A comment that repeats the code is
   deleted.
3. **Separate documentation.** The README is general project information; usage
   examples will appear later, once the API stabilises. Documentation generated
   from source will be added only if there is a real need; there is no reason to
   set it up in advance.

**The repository must not reference material that is not in it.** Anyone who
clones the project should see nothing extraneous; plans, history and design
rationale live separately from the code and do not go into the repository.

---

## Quick reference

| Question | Where to look |
|---|---|
| How to lay out code | `CODE_STYLE.md` |
| What is built and how | `CMakeLists.txt`, `README.md` |
| How to format | `tools/format.ps1` (Windows), `tools/format.py` (anywhere), `.clang-format` |
| How to benchmark, and how to read a result | `bench/README.md` |
| API entry points | `include/registry.h`, `include/entity.h`, `include/storage.h` |
