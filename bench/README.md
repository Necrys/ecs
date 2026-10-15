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

# pin to one physical core per core, and raise the process priority
build\msvc\bench\bench_iter.exe --pin

# pin to one specific core instead
build\msvc\bench\bench_iter.exe --core 4
```

**Use `--pin` for anything you intend to compare.** Where a process runs matters as
much as the code: migration between cores carries the working set to a cold cache,
and a sibling thread on the same physical core steals cache and execution slots.
Pinning removes both, and it is reported either way so that a number whose
scheduling conditions are unknown does not get compared with anything. A negative
side effect is that the benchmark then measures one core rather than the machine.

The three binaries split by concern:

| Binary | Covers |
|---|---|
| `bench_iter` | iteration: one component, several components, component size, id spread |
| `bench_structure` | structural change: allocate, deallocate, reuse ids, add and remove components |
| `bench_access` | point access: `get` for a present and an absent component |

Each starts by printing the machine it found, its cache hierarchy and its
scheduling conditions, then the workload it chose, then one line per measured
point, and finishes with a checksum.

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
2. **Sample length.** One timed sample repeats the work until it lasts at least
   50 ms, and that repetition count is then frozen for every sample of the point.
   The count is calibrated from the *median* of several single runs and confirmed
   on real samples, because estimating it from the fastest run biases it
   downwards. This is not a detail: a point measured in ~0.1 ms came out bimodal
   on this machine, its median jumping between two states, while the same point
   batched into milliseconds held to a few percent.

If a point stays noisy no matter how long the sample, that is a finding in
itself. **Cache set conflicts from page placement are real and are not fixed by
pinching the harness**: within one process the samples of a wide id spread are
tight (a few percent), while the medians of separate launches of the same binary
move by tens of percent, and a wide spread is exactly where page placement decides
which cache sets collide. The remedies are to compare within a session and to treat
a small number of wide-span points as approximate. The benchmarks are linked
without a dynamic base, which removes one source of layout variation, but on the
machine measured here it did not make those medians stable on its own.

---

## Comparing two revisions

Absolute figures drift between sessions on this machine — the same binary can move
by more than ten percent while its own interquartile spread stays under one
percent. Comparing a new run against a file recorded yesterday therefore proves
nothing. The comparison that works is **interleaved and paired**: build both
revisions, run them alternately in one session, and compare each pair.

`ab-interleaved.ps1` (kept with the run history, not in the repository) does this:

```powershell
# build the other revision into its own tree
git worktree add ..\base-checkout <commit>
cmake --preset msvc ; cmake --build --preset msvc          # in that tree

# then, from the tree holding the script
powershell -File ab-interleaved.ps1 `
    -A ..\base-checkout\build\msvc\bench\bench_iter.exe `
    -B .\build\msvc\bench\bench_iter.exe `
    -Tag a1 -Rounds 5 -Pin
```

It alternates A B, then B A, and so on. **Alternating the order is not a detail**:
running A before B in every round gives B a systematic advantage, and a control run
of one binary against itself measured 2.4 % in B's favour before the order was
swapped. For each point it reports the per-round ratio, the geometric mean of those
ratios, and the number of rounds that agreed on a direction. A point is called only
when every round agrees *and* the effect exceeds five percent; everything else is
reported as unresolved, which is the honest answer rather than a small number.

Control runs of one binary against itself, pinned, four rounds: 1.020, 0.996,
0.999, with no point called in the two later runs. Treat an aggregate within about
two percent as no difference.

The scripts live in the workspace next to the run history:

| File | Purpose |
|---|---|
| `compare.ps1` | quick comparison against a recorded baseline, point by point |
| `ab-interleaved.ps1` | paired interleaved A/B, the one to trust for a verdict |

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
