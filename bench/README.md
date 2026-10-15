# Benchmarks

Measurements of the library, run deliberately and locally. The source is in the
repository because it is portable; the **results are not** - each binary writes
nothing unless told where, so run data belongs wherever the run history is kept.

Performance is the project's first priority, so a change to a hot path is
expected to be backed by numbers: measure, change, measure again, and keep the
difference deliberate rather than accidental.

---

## Building

Benchmarks are off by default. They build with optimisation pinned per compiler
inside `bench/CMakeLists.txt` (`/O2` for MSVC, `-O2` otherwise) rather than
inheriting `CMAKE_BUILD_TYPE`, so a run cannot silently happen against a debug
build.

```powershell
# Windows, one preset per compiler
cmake --preset msvc
cmake --build --preset msvc

cmake --preset clang-cl
cmake --build --preset clang-cl
```

```bash
# Linux and Raspberry Pi
cmake --preset gcc
cmake --build --preset gcc
```

Binaries land in `build/<preset>/bench/`. Run each compiler separately: the point
of having several presets is to compare code generation, and mixing results from
two compilers in one history ruins that.

Nothing here is registered with CTest. Benchmarks are not tests: they are slow,
their results are machine-specific, and a pass/fail verdict on them would be
meaningless.

---

## Running

```powershell
# default: sized to the machine, several seconds to half a minute per binary
build\msvc\bench\bench_iter.exe

# short smoke run, for checking that a change compiles and moves at all
build\msvc\bench\bench_iter.exe --quick

# append results to a CSV kept outside the repository
build\msvc\bench\bench_iter.exe --csv ..\..\bench-data\run.csv
```

The three binaries split by concern:

| Binary | Covers |
|---|---|
| `bench_iter` | iteration: one component, several components, component size, id spread |
| `bench_structure` | structural change: allocate, deallocate, reuse ids, add and remove components |
| `bench_access` | point access: `get` for a present and an absent component |

Each starts by printing the machine it found and the workload it chose, then one
line per measured point, and finishes with a checksum.

---

## How the workload is sized

Nothing about the workload is hard-coded to one machine. Entity counts are
derived at run time from what the machine reports:

- **RAM used** - at most 2% of total, capped at 256 MB. A run on a board with
  512 MB of RAM must not start swapping, because then the benchmark measures the
  disk.
- **Cache sizes** - `machine.hpp` reads the hierarchy through CPUID on x86 and
  through `/sys/devices/system/cpu/cpu0/cache` on Linux, which is what works on
  ARM boards. Where the sizes cannot be determined, the run says so and falls
  back to sizing from RAM.
- **Ladder** - consecutive entity counts differ by 4x, so a cache cliff shows up
  as a change in slope rather than a gradual drift. The ladder is trimmed to a
  bounded number of points, keeping both ends.

On a 64 MB-L3 desktop this produces runs in the millions of entities; on a
Raspberry Pi it produces far smaller ones. That is intended: the curve is
interesting relative to the machine's own cache sizes, not in absolute counts.

Add `--quick` (or build a quick configuration) when iterating; it trades spread
for wall-clock time.

---

## How to read a result

One line per point:

```
join1    Position    n=1048576    median=73.745 ms (4.40 ns/unit)  iqr=2.8%
```

- **median** is the reported figure. It is robust to a slow first sample.
- **per unit** (ns per entity or per operation) is what makes points of different
  sizes comparable.
- **iqr** is the interquartile spread, `(p90 - p10) / median`. It is used instead
  of max-min because the tail of the distribution is dominated by the operating
  system preempting the process, which says nothing about the code. A point where
  the *quartiles* disagree is genuinely unstable.
- Points with an iqr above 10% are marked `<- noisy`. **Do not draw conclusions
  from them.** Either make the sample longer (see below) or leave the point out.

Two mechanisms keep a point trustworthy:

1. **Warmup.** Untimed runs first, so caches, branch predictors and page tables
   are in a steady state. For a large working set the warmup is shortened, since
   building it costs more than the measurement, but it is never removed: without
   one pass the first timed sample pays for every page fault.
2. **Sample length.** A timed sample repeats the work until it lasts at least
   50 ms. A sample lasting a fraction of a millisecond is at the mercy of a
   single preemption. The repetition count is estimated from the *fastest* of a
   few trial runs, because sizing every sample from an outlying first run leaves
   the samples far longer than intended.

If a point stays noisy no matter how long the sample, that is a finding in
itself: report it as unstable and do not build an argument on it.

---

## Recording runs

Write results to CSV with `--csv <path>`; the header is written if the file does
not exist yet, so runs accumulate. Columns:

```
run_id, timestamp, git_rev, cpu, arch, os, compiler, config, scenario, components,
entity_count, live_count, id_span, component_bytes, operations, samples,
p10_ns, median_ns, p90_ns, min_ns, max_ns, iqr_pct, tail_pct, per_unit_ns, notes
```

Alongside `median_ns`, the `cpu`, `arch`, `compiler` and `config` columns are what
make a row comparable at all. Comparing across machines or compilers is only
meaningful when both columns are in view.

The workflow that makes this useful:

1. before touching a hot path, record a baseline run;
2. make the change;
3. run again with the same preset and compare the `median_ns` of the same
   scenarios;
4. treat a difference within the iqr as no difference.

The checksum printed at the end of a run is a guard: if a benchmark is
accidentally optimised away, its checksum stops changing between revisions.

---

## Adding a scenario

A scenario is a `bench::Result` plus a `bench::measure` call:

```cpp
bench::Result result;
result.scenario = "join2";
result.components = "Position,Velocity";
result.entity_count = count;
result.operations = count;                       // what `units` means
result.stats = bench::measure( config, count,    // units
  [] {},                                         // setup, re-run per sample
  [ & ] { /* the measured work */ } );
```

Two rules:

- **Feed the result into the checksum** (`bench::mix_into_checksum`). Without it
  the compiler is entitled to delete the work, and the benchmark measures
  nothing.
- **Keep the measured lambda free of things you do not mean to measure.** Data
  generation, shuffling and allocation belong in the setup or outside the
  measurement.

Benchmarks do not get to change the library. If a scenario cannot be written
against the public interface, that is information about the interface - the same
rule that applies to tests.
