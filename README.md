# ecs

A small header-heavy **Entity-Component-System** library for C++.

An entity is just an identifier. A component is an ordinary struct holding your
data - plain, trivially destructible data is the case the containers are built
for. A system is an object with an `update()` method that walks the components
it cares about. The library takes care of storing components, finding them,
ordering systems and delivering events. No external dependencies, no code
generation, no magic - just headers and a little bit of `.cpp`.

Data and logic are kept apart on purpose: containers store, systems compute. The
same reasoning drives the implementation towards compile-time work - type ids,
storage lookups and iteration over a set of components are resolved by the
compiler, which is where much of the performance comes from.

Project goals, in order of importance:

1. **Performance** - walking components should cost time proportional to the
   number of matching entities, not to the size of the identifier space.
2. **Multithreading** - independent systems should be able to run in parallel.
3. **Lightweight** - minimal code and dependencies, fast builds.
4. **Portability** - Windows, Linux, Raspberry Pi 1/2.

---

## Status

**The library is being reworked and is not ready for use.**

The current code is a work in progress: part of what is planned is not
implemented, part is outdated, and part has known defects. We are fixing and
developing it deliberately, so the current behaviour should not be treated as a
contract: both the API and the internals will change.

There are no usable releases yet. Development happens on the `master` branch;
the `2020_last` branch (and the `2020-last` tag) is a frozen snapshot of the
original 2020 line, kept for comparison. Once there is a version worth relying
on, it will be tagged and the README will describe the stable API.

We are not pretending this is production-ready: it is an experimental project
that still has a way to go before it works reliably.

---

## Building

You need a C++17 compiler and CMake 3.12+. The library is static and has no
dependencies; the tests fetch Catch2 v3 (pinned to an exact commit), so the
first configure needs network access. Pass `-DECS_BUILD_TESTS=OFF` to build the
library on its own.

Verified configurations:

| Platform | Compiler | State |
|---|---|---|
| Windows x64 | MSVC 19.38 (VS 2022) | builds |
| Windows x64 | Clang (`clang-cl`), Clang (GNU mode) | builds |
| Linux x64 | GCC, Clang | target platform, build not verified yet |
| Raspberry Pi 1/2 (ARMv6/ARMv7) | GCC | target platform, build not verified yet |

MinGW-w64 and macOS are not on the list: there is no tooling available to build
and check them, and claiming unverified support is meaningless. They will be
added once it becomes possible to build and test on them, including through CI.

```bash
# Linux / Raspberry Pi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

```powershell
# Windows, MSVC + Ninja (from a Developer Command Prompt)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Tests and benchmarks are built separately and serve as a demonstration; the
library itself does not depend on them. The tests build and pass, and are
registered with CTest:

```bash
ctest --test-dir build --output-on-failure
```

---

## Documentation

Usage examples, both basic and tutorial-style, will appear later - once the API
stops changing and they no longer have to be rewritten along with the code. For
now the repository contains only general information about the project and the
rules for working with the code.

| File | Contents |
|---|---|
| `AGENTS.md` | project map and working rules, including for agents |
| `CODE_STYLE.md` | code style and commenting requirements |
| `bench/README.md` | how to build, run and read the benchmarks |
| `README.md` | this file - an introduction to the project |
| `LICENSE` | license |

Generated-from-source documentation will appear only if there is a real need for
it; plans, history and design rationale do not belong in the repository.

Performance is the first priority here, so the repository ships a small benchmark
suite. It is off by default, sizes its workload from the machine it runs on, and
deliberately keeps its results out of the repository - see `bench/README.md`.

---

## License

**Unlicense** - software released into the public domain. See `LICENSE`.

In short: the code may be used however you like - copied, modified, published,
sold, distributed in source or compiled form, for commercial or non-commercial
purposes, without any conditions and without attribution. No warranty is
provided, and the risk of using it is yours.
