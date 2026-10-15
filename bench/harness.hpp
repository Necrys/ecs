#pragma once

// Measurement harness: warmup, repeated runs, statistics, machine-sized workloads
// and a machine-readable result line.
//
// The timing policy is deliberately conservative. Performance here is dominated
// by the cache, so a single run measures cache state as much as it measures the
// code: every point is warmed up first, then sampled repeatedly, and the spread
// is reported next to the median. A point whose spread is comparable to the
// effect being investigated cannot support a conclusion.

#include "machine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>
namespace bench {

// -----------------------------------------------------------------------------
// Workload sizing
// -----------------------------------------------------------------------------

// Entity counts are derived, not fixed: the same source has to be meaningful on
// a desktop with a 64 MB L3 and on a Raspberry Pi with 16 KB of L1 and 512 MB of
// RAM. Consecutive steps differ by 4x so that a cache cliff shows up as a change
// in slope rather than as a gradual drift.
//
// The ladder is trimmed to a bounded number of points: past the point where the
// working set no longer fits in any cache the curve is flat, and each extra step
// costs a full build of the world.
inline std::vector< size_t > size_ladder( const size_t largest, const int max_points = 8 ) {
  std::vector< size_t > sizes;
  size_t base = 1024u;
  while ( base <= largest ) {
    sizes.push_back( base );
    base *= 4u;
  }
  if ( sizes.empty() ) {
    sizes.push_back( 1024u );
  }

  // Keep the top of the range: the interesting behaviour is at both ends, and a
  // gap in the middle is less misleading than losing the plateau.
  while ( static_cast< int >( sizes.size() ) > max_points ) {
    sizes.erase( sizes.begin() + 1 );
  }

  return sizes;
}

// Smaller of two values, spelled out because mixing size_t with unsigned
// literals makes std::min deduction ambiguous.
inline size_t smaller( const size_t a, const size_t b ) {
  return a < b ? a : b;
}

// How many entities fit in a given memory budget, rounded down to a ladder step.
// The absolute cap keeps a run bounded: past a few million entities the
// traversal curve is already flat, and building the world costs more than the
// measurement is worth.
inline size_t
entities_for_budget( const size_t budget_bytes, const size_t bytes_per_entity, const size_t cap = 4194304u ) {
  if ( bytes_per_entity == 0 ) {
    return 1024u;
  }
  size_t count = smaller( budget_bytes / bytes_per_entity, cap );
  size_t step = 1024u;
  while ( step <= count / 4u ) {
    step *= 4u;
  }
  count = ( count / step ) * step;
  return count < 1024u ? 1024u : count;
}

// Fraction of RAM that may be allocated for a working set, with a hard cap.
//
// Kept well below the whole of memory so that a run on a small board does not
// start swapping, which would turn a measurement of the code into a measurement
// of the disk. The cap serves the other end: without it a desktop would build a
// working set so large that a single point takes minutes, which is not a useful
// way to spend a local run.
inline size_t memory_budget( const MachineInfo& info, const double fraction = 0.02 ) {
  // 256 MB is already several times the L3 of a mainstream desktop and far more
  // than a small board has, so it is enough to leave the caches behind without
  // making a single point take seconds to build.
  const size_t hard_cap = 256u * 1024u * 1024u;
  if ( !info.ram_known ) {
    return 64u * 1024u * 1024u;
  }
  const size_t budget = static_cast< size_t >( static_cast< double >( info.ram_bytes ) * fraction );
  return budget < hard_cap ? budget : hard_cap;
}

// A cache-sized budget, used where the point of the scenario is the memory
// hierarchy rather than capacity.
inline size_t cache_budget( const MachineInfo& info, const size_t fallback = 8u * 1024u * 1024u ) {
  if ( info.l3_bytes != 0 ) {
    return info.l3_bytes;
  }
  if ( info.l2_bytes != 0 ) {
    return info.l2_bytes;
  }
  return fallback;
}

// -----------------------------------------------------------------------------
// Statistics
// -----------------------------------------------------------------------------

struct Stats {
  size_t samples = 0;
  // Repetitions inside one sample, after calibration. Recorded because a point
  // whose samples are short is measuring the operating system as much as the
  // code, and that is invisible otherwise.
  int repeat = 0;
  double p10_ms = 0.0;
  double median_ms = 0.0;
  double p90_ms = 0.0;
  double min_ms = 0.0;
  double max_ms = 0.0;
  // Interquartile spread relative to the median. Chosen over max-min because the
  // tail of the distribution is dominated by the operating system preempting the
  // process, which says nothing about the code; a genuine instability still shows
  // up because it moves the quartiles.
  double spread_pct = 0.0;
  double tail_pct = 0.0;
  // Coefficient of variation, comparable across magnitudes in a way the absolute
  // spread is not.
  double cv_pct = 0.0;
  double per_unit_ns = 0.0;
  // Cycles per unit of work. Invariant to frequency changes, so it is the figure
  // to compare when the core may have been running at a different clock.
  double per_unit_cycles = 0.0;
  // Every sample, in the order taken, so that any statistic can be re-derived
  // later without re-running and so that a distribution can be looked at rather
  // than guessed at.
  std::vector< double > raw_ms;
};

inline double percentile( const std::vector< double >& sorted, const double fraction ) {
  if ( sorted.empty() ) {
    return 0.0;
  }
  const size_t index = static_cast< size_t >( fraction * static_cast< double >( sorted.size() - 1 ) + 0.5 );
  return sorted[ std::min( index, sorted.size() - 1 ) ];
}

inline Stats summarise( std::vector< double > samples_ms, const size_t units ) {
  Stats stats;
  if ( samples_ms.empty() ) {
    return stats;
  }

  // Kept in the order they were taken, before sorting.
  stats.raw_ms = samples_ms;

  std::sort( samples_ms.begin(), samples_ms.end() );

  stats.samples = samples_ms.size();
  stats.p10_ms = percentile( samples_ms, 0.1 );
  stats.median_ms = percentile( samples_ms, 0.5 );
  stats.p90_ms = percentile( samples_ms, 0.9 );
  stats.min_ms = samples_ms.front();
  stats.max_ms = samples_ms.back();
  stats.spread_pct = stats.median_ms > 0.0 ? ( stats.p90_ms - stats.p10_ms ) / stats.median_ms * 100.0 : 0.0;
  stats.tail_pct = stats.median_ms > 0.0 ? ( stats.max_ms - stats.min_ms ) / stats.median_ms * 100.0 : 0.0;
  stats.per_unit_ns = units > 0 ? stats.median_ms * 1e6 / static_cast< double >( units ) : 0.0;

  // Sample standard deviation over the raw samples, reported relative to the
  // mean as the coefficient of variation.
  double sum = 0.0;
  for ( const double value : samples_ms ) {
    sum += value;
  }
  const double mean = sum / static_cast< double >( samples_ms.size() );

  if ( samples_ms.size() > 1 && mean > 0.0 ) {
    double squares = 0.0;
    for ( const double value : samples_ms ) {
      const double delta = value - mean;
      squares += delta * delta;
    }
    const double deviation = std::sqrt( squares / static_cast< double >( samples_ms.size() - 1 ) );
    stats.cv_pct = deviation / mean * 100.0;
  }

  return stats;
}

// -----------------------------------------------------------------------------
// Run configuration
// -----------------------------------------------------------------------------

struct RunConfig {
  // Seed for anything a scenario randomises. Fixed so that the same run repeated
  // produces the same data: a benchmark whose workload changes between runs
  // cannot be compared with its own baseline.
  uint32_t seed = 20260115u;

  // Warmup runs are discarded. Their purpose is to fill the caches, settle the
  // branch predictor and let the allocator reach a steady state, so that the
  // measured runs are not paying for first-touch costs.
  int warmup = 3;

  // Repeated samples per point. More samples tighten the spread; the point of
  // reporting the spread is to see whether a result is usable at all.
  int samples = 15;

  // Bulk allocation of a large working set is far more expensive than the
  // operation under test, so its warmup is reduced separately. It only drops to
  // one, never to zero: a single untimed pass is what makes the pages resident,
  // and without it the first timed run pays for every page fault and shows up as
  // a wide spread rather than as a wrong median.
  int bulk_warmup = 1;

  // Starting repetition count. It is raised automatically so that a single
  // sample lasts at least `min_sample_ms`.
  int scale = 1;

  // A timed sample is repeated until it reaches this duration. Short samples
  // measure the operating system as much as the code.
  double min_sample_ms = 50.0;

  // Upper bound on the automatic repetition, so that a pathologically slow
  // scenario cannot turn one point into minutes.
  int max_scale = 1024;

  // Work above this many entities gets fewer samples: a single run already takes
  // long enough that the median is stable, and the cost of sampling dominates
  // the run otherwise.
  size_t large_threshold = 262144u;
  int large_samples = 7;

  bool verbose = true;
};

inline RunConfig quick_config() {
  RunConfig config;
  config.warmup = 1;
  config.samples = 5;
  config.bulk_warmup = 0;
  config.large_samples = 3;
  return config;
}

// Command line shared by every benchmark:
//
//   --quick          short run, for checking that a change moves at all
//   --csv <path>     append results to a CSV, outside the repository
//   --pin            pin the process to the fastest cores and raise its priority
//   --core <n>       pin to one specific core instead
//
// Pinning is opt-in because its effect is machine-specific, but it is reported
// either way: a number whose scheduling conditions are unknown cannot be
// compared with anything.
struct CommandLine {
  RunConfig config;
  std::string csv_path;
  bool pin = false;
  int core = -1;
};

inline CommandLine parse_command_line( const int argc, char** argv ) {
  CommandLine line;

  for ( int i = 1; i < argc; ++i ) {
    const std::string argument = argv[ i ];
    if ( argument == "--quick" ) {
      line.config = quick_config();
    } else if ( argument == "--pin" ) {
      line.pin = true;
    } else if ( argument == "--core" && i + 1 < argc ) {
      line.core = std::atoi( argv[ ++i ] );
      line.pin = true;
    } else if ( argument == "--csv" && i + 1 < argc ) {
      line.csv_path = argv[ ++i ];
    }
  }

  return line;
}

// Applies the requested scheduling conditions and reports them. Called before
// anything is measured, because sharing a core or migrating between cores changes
// the numbers by more than most code changes do.
inline AffinityState apply_scheduling( const CommandLine& line ) {
  if ( !line.pin ) {
    print_affinity( AffinityState(), false );
    return AffinityState();
  }

  const AffinityState state = pin_process( line.core );
  print_affinity( state, true );
  return state;
}

// Records the time base at the start of a session. Nanoseconds per cycle moving
// between the first and last point means the core was running at a different clock
// during the two, and then the two are not comparable however careful the rest of
// the harness is.
struct FrequencyCheck {
  double ns_per_cycle = 0.0;
};

inline FrequencyCheck begin_frequency_check() {
  FrequencyCheck check;
  check.ns_per_cycle = measure_ns_per_cycle();
  if ( check.ns_per_cycle > 0.0 ) {
    std::printf( "#   time base  : %.4f ns/cycle at session start\n", check.ns_per_cycle );
  } else {
    std::printf( "#   time base  : no cycle counter available; nanosecond figures only\n" );
  }
  return check;
}

inline void end_frequency_check( const FrequencyCheck& check ) {
  const double now = measure_ns_per_cycle();
  if ( check.ns_per_cycle <= 0.0 || now <= 0.0 ) {
    return;
  }

  const double drift = ( now - check.ns_per_cycle ) / check.ns_per_cycle * 100.0;
  std::printf( "#   time base  : %.4f ns/cycle at session end (%+.1f%%)\n", now, drift );
  if ( drift > 5.0 || drift < -5.0 ) {
    std::printf(
      "#   warning    : the core changed clock during this run; comparisons across\n"
      "#                this session are unreliable\n"
    );
  }
}

// -----------------------------------------------------------------------------
// Result records
// -----------------------------------------------------------------------------

struct Result {
  std::string scenario;
  std::string components;
  std::string notes;
  size_t entity_count = 0;
  size_t live_count = 0;
  size_t id_span = 0;
  size_t component_bytes = 0;
  size_t operations = 0;
  Stats stats;
};

inline std::string csv_escape( const std::string& value ) {
  if ( value.find( ',' ) == std::string::npos && value.find( '"' ) == std::string::npos ) {
    return value;
  }
  std::string escaped = "\"";
  for ( const char character : value ) {
    if ( character == '"' ) {
      escaped += "\"\"";
    } else {
      escaped += character;
    }
  }
  escaped += "\"";
  return escaped;
}

inline std::FILE* open_for_read( const std::string& path ) {
#if defined( _MSC_VER )
  std::FILE* file = nullptr;
  if ( fopen_s( &file, path.c_str(), "rb" ) != 0 ) {
    return nullptr;
  }
  return file;
#else
  return std::fopen( path.c_str(), "rb" );
#endif
}

// True when the path names an existing regular file, used to decide whether a
// CSV being appended to still needs its header row.
inline bool file_exists( const std::string& path ) {
  if ( std::FILE* file = open_for_read( path ) ) {
    std::fclose( file );
    return true;
  }
  return false;
}

// Opens a file for appending. Wrapped so that the MSVC deprecation of fopen does
// not turn into a warning in every benchmark.
inline std::FILE* open_for_append( const std::string& path ) {
#if defined( _MSC_VER )
  std::FILE* file = nullptr;
  if ( fopen_s( &file, path.c_str(), "ab" ) != 0 ) {
    return nullptr;
  }
  return file;
#else
  return std::fopen( path.c_str(), "ab" );
#endif
}

inline std::string compiler_description() {
#if defined( _MSC_VER )
  char buffer[ 64 ];
  std::snprintf( buffer, sizeof( buffer ), "msvc-%d", static_cast< int >( _MSC_VER ) );
  return buffer;
#elif defined( __clang__ )
  char buffer[ 128 ];
  std::snprintf( buffer, sizeof( buffer ), "clang-%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__ );
  return buffer;
#elif defined( __GNUC__ )
  char buffer[ 128 ];
  std::snprintf( buffer, sizeof( buffer ), "gcc-%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
  return buffer;
#else
  return "unknown";
#endif
}

struct RunContext {
  std::string run_id;
  std::string git_rev;
  std::string config;
  MachineInfo machine;
};

// Wall-clock time of the run, for the history file. It has to be the moment the
// measurement was taken, not the moment the binary was built: the same binary is
// run for days, and a build stamp in every row would date the whole session to
// whenever it was last compiled.
inline std::string run_timestamp() {
  const std::time_t now = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );

  std::tm local{};
#if defined( _MSC_VER )
  localtime_s( &local, &now );
#else
  localtime_r( &now, &local );
#endif

  char buffer[ 32 ] = "unset";
  if ( std::strftime( buffer, sizeof( buffer ), "%Y-%m-%d %H:%M:%S", &local ) == 0 ) {
    return std::string( "unset" );
  }
  return std::string( buffer );
}

inline void print_csv_header( std::FILE* out ) {
  std::fprintf(
    out,
    "run_id,timestamp,git_rev,cpu,arch,os,compiler,config,scenario,components,"
    "entity_count,live_count,id_span,component_bytes,operations,samples,repeat,"
    "p10_ns,median_ns,p90_ns,min_ns,max_ns,iqr_pct,tail_pct,cv_pct,per_unit_ns,"
    "per_unit_cycles,notes\n"
  );
}

inline void
print_csv_row( std::FILE* out, const RunContext& context, const Result& result, const std::string& timestamp ) {
  std::fprintf(
    out,
    "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%d,"
    "%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.3f,%.3f,%s\n",
    csv_escape( context.run_id ).c_str(),
    csv_escape( timestamp ).c_str(),
    csv_escape( context.git_rev ).c_str(),
    csv_escape( context.machine.cpu ).c_str(),
    csv_escape( context.machine.arch ).c_str(),
    csv_escape( context.machine.os ).c_str(),
    csv_escape( compiler_description() ).c_str(),
    csv_escape( context.config ).c_str(),
    csv_escape( result.scenario ).c_str(),
    csv_escape( result.components ).c_str(),
    result.entity_count,
    result.live_count,
    result.id_span,
    result.component_bytes,
    result.operations,
    result.stats.samples,
    result.stats.repeat,
    result.stats.p10_ms * 1e6,
    result.stats.median_ms * 1e6,
    result.stats.p90_ms * 1e6,
    result.stats.min_ms * 1e6,
    result.stats.max_ms * 1e6,
    result.stats.spread_pct,
    result.stats.tail_pct,
    result.stats.cv_pct,
    result.stats.per_unit_ns,
    result.stats.per_unit_cycles,
    csv_escape( result.notes ).c_str()
  );
}

// Writes every sample of every point to a separate file, one row per sample.
//
// The summary is enough to compare two runs; the raw samples are what makes it
// possible to look at a distribution later, re-derive any statistic without
// re-running, and tell a genuine two-state distribution from ordinary scatter.
// Files with a samples suffix live next to the CSV and are only written when a
// path is given, so nothing lands in the repository by accident.
inline void print_samples( std::FILE* out, const RunContext& context, const Result& result ) {
  for ( size_t i = 0; i < result.stats.raw_ms.size(); ++i ) {
    std::fprintf(
      out,
      "%s,%s,%s,%zu,%zu,%zu,%zu,%zu,%d,%d,%.1f\n",
      csv_escape( context.run_id ).c_str(),
      csv_escape( result.scenario ).c_str(),
      csv_escape( result.components ).c_str(),
      result.entity_count,
      result.live_count,
      result.id_span,
      result.operations,
      i,
      result.stats.repeat,
      static_cast< int >( result.stats.samples ),
      result.stats.raw_ms[ i ] * 1e6
    );
  }
}

inline void print_samples_header( std::FILE* out ) {
  std::fprintf(
    out, "run_id,scenario,components,entity_count,live_count,id_span,operations,sample_index,repeat,samples,ns\n"
  );
}

// -----------------------------------------------------------------------------
// Measurement
// -----------------------------------------------------------------------------

// Runs `work` once and returns its duration in milliseconds. The value returned
// by `work` is accumulated by the caller into a checksum so that the compiler
// cannot conclude the work is unobservable and remove it.
// -----------------------------------------------------------------------------
// Keeping work alive
//
// A checksum printed at the end of a run is not enough on its own: the compiler
// may still hoist the work out of the timed region or sink it past the clock
// reading, and then the benchmark times nothing. These helpers put an explicit
// escape inside the measured region, following what Google Benchmark does.
// -----------------------------------------------------------------------------

#if defined( _MSC_VER )
// Forces a value into memory the compiler cannot reason about. An out-of-line
// function whose parameter is a volatile pointer is the portable way to do this
// on MSVC; the pointer is never dereferenced.
extern "C" void bench_keep_alive( void const volatile* value );
#pragma comment( linker, "/INCLUDE:bench_keep_alive" )
#endif

// Makes the value look used, so the computation that produced it cannot be
// discarded.
template < typename T >
inline void keep( T&& value ) {
#if defined( _MSC_VER )
  bench_keep_alive( &reinterpret_cast< char const volatile& >( value ) );
#else
  asm volatile( "" : "+r,m"( value ) : : "memory" );
#endif
}

// A compiler barrier: nothing may move across it. Used after timing so that work
// cannot sink out of the measured region.
inline void clobber_memory() {
#if defined( _MSC_VER )
  std::atomic_signal_fence( std::memory_order_acq_rel );
#else
  asm volatile( "" : : : "memory" );
#endif
}

// Cycles accumulated by the most recent timed run, and the run that reports them.
// Recorded alongside nanoseconds because they answer different questions: a core
// that changes frequency moves one without touching the other.
inline uint64_t& last_cycles() {
  static uint64_t value = 0;
  return value;
}

// Runs `work` once and returns its duration in milliseconds, accumulating the
// cycle count into last_cycles(). The value the work produced is fed to the
// checksum by the caller, which is what stops it from being discarded.
//
// Declared before `calibrate`, which calls it: with two-phase lookup the call
// inside a template is not found later, and MSVC under /permissive- rejects it
// exactly as GCC and clang do.
template < typename F >
double time_once_ms( F&& work ) {
  const auto start = std::chrono::steady_clock::now();
  const uint64_t start_cycles = read_cycles();
  work();
  const uint64_t end_cycles = read_cycles();
  const auto finish = std::chrono::steady_clock::now();
  clobber_memory();

  last_cycles() += static_cast< uint64_t >( end_cycles > start_cycles ? end_cycles - start_cycles : 0 );
  return std::chrono::duration< double, std::milli >( finish - start ).count();
}

// Frozen calibration for one point: how many repetitions make a single sample
// long enough to be worth timing, decided once and reused for every sample.
struct Calibration {
  int repeat = 1;
  double typical_ms = 0.0; // median cost of one repetition
  int attempts = 0;        // how many times the estimate had to be raised
};

// Decides `repeat` such that one sample lasts at least `min_sample_ms`.
//
// The estimate uses the median of several single runs rather than the fastest.
// Taking the minimum biases the count downwards, and a sample that is too short
// is at the mercy of single scheduler events: on this machine a point measured in
// ~0.1 ms came out bimodal, its median jumping between two states, while the same
// point batched into milliseconds held to a few percent.
//
// The choice is then confirmed on real samples and raised if the observed duration
// falls short, because even a median-based estimate can be optimistic when the
// distribution is wide.
template < typename Setup, typename Work >
Calibration calibrate( const RunConfig& config, Setup&& setup, Work&& work ) {
  Calibration result;

  {
    std::vector< double > trials;
    trials.reserve( 7 );
    for ( int t = 0; t < 7; ++t ) {
      trials.push_back( time_once_ms( work ) );
    }
    std::sort( trials.begin(), trials.end() );
    result.typical_ms = trials[ trials.size() / 2 ];
  }

  result.repeat = config.scale;
  for ( int attempt = 0; attempt < 6; ++attempt ) {
    result.attempts = attempt + 1;

    while ( result.typical_ms * static_cast< double >( result.repeat ) < config.min_sample_ms &&
            result.repeat < config.max_scale ) {
      result.repeat *= 2;
    }

    // Confirm on real samples; the median of three again, since a single run is
    // exactly what cannot be trusted here.
    setup();
    std::vector< double > confirm;
    for ( int c = 0; c < 3; ++c ) {
      confirm.push_back( time_once_ms( [ & ] {
        for ( int r = 0; r < result.repeat; ++r ) {
          work();
        }
      } ) );
    }
    std::sort( confirm.begin(), confirm.end() );

    if ( confirm[ 1 ] >= config.min_sample_ms || result.repeat >= config.max_scale ) {
      break;
    }

    // Still too short: scale in proportion to what was actually observed.
    const double measured = confirm[ 1 ] > 0.0 ? confirm[ 1 ] : config.min_sample_ms;
    int next =
      static_cast< int >( static_cast< double >( result.repeat ) * ( config.min_sample_ms / measured ) * 1.3 ) + 1;
    if ( next <= result.repeat ) {
      next = result.repeat + 1;
    }
    result.repeat = next < config.max_scale ? next : config.max_scale;
  }

  return result;
}

// Measures `work` at one point: warmup, then `samples` timed runs.
//
// The repetition count is calibrated so that one sample lasts at least
// `min_sample_ms`, then frozen for every sample of the point. Two details matter,
// and both were established by measurement on this machine:
//
//   * the estimate uses the MEDIAN of a few runs, not the fastest one. Taking the
//     minimum biases the count downwards, and a sample that is too short is at
//     the mercy of single scheduler events: a point measured in ~0.1 ms came out
//     bimodal, while the same point batched into milliseconds held to a few
//     percent;
//   * the chosen count is confirmed on real samples and raised if the observed
//     duration falls short, because the estimate can still be optimistic when the
//     distribution is wide.
//
// `setup` re-runs before every timed run when the scenario mutates its data.
template < typename Setup, typename Work >
Stats measure( const RunConfig& config, const size_t units, Setup&& setup, Work&& work ) {
  const int warmup = units >= 1000000u ? config.bulk_warmup : config.warmup;
  const int samples = units >= config.large_threshold ? config.large_samples : config.samples;

  const Calibration calibration = calibrate( config, setup, work );
  const int repeat = calibration.repeat;

  for ( int i = 0; i < warmup; ++i ) {
    setup();
    for ( int r = 0; r < repeat; ++r ) {
      work();
    }
  }

  std::vector< double > durations;
  durations.reserve( static_cast< size_t >( samples ) );

  const uint64_t cycles_before = last_cycles();

  for ( int i = 0; i < samples; ++i ) {
    setup();
    durations.push_back( time_once_ms( [ & ] {
      for ( int r = 0; r < repeat; ++r ) {
        work();
      }
    } ) );
  }

  const uint64_t cycles_taken = last_cycles() - cycles_before;
  const size_t total_units = units * static_cast< size_t >( repeat );

  Stats stats = summarise( durations, total_units );
  stats.repeat = repeat;
  if ( cycles_taken != 0 && total_units != 0 ) {
    stats.per_unit_cycles = static_cast< double >( cycles_taken ) / static_cast< double >( total_units );
  }

  return stats;
}

// -----------------------------------------------------------------------------
// Reporting
// -----------------------------------------------------------------------------

inline void print_result( const Result& result ) {
  std::printf( "%-34s %-24s", result.scenario.c_str(), result.components.c_str() );
  if ( result.live_count != 0 && result.live_count != result.entity_count ) {
    std::printf( " live=%-9zu span=%-10zu", result.live_count, result.id_span );
  } else {
    std::printf( " n=%-9zu               ", result.entity_count );
  }
  std::printf(
    " median=%9.3f ms (%8.2f ns/unit)  iqr=%5.1f%%%s\n",
    result.stats.median_ms,
    result.stats.per_unit_ns,
    result.stats.spread_pct,
    result.stats.spread_pct > 10.0 ? "  <- noisy" : ""
  );
  if ( result.stats.per_unit_cycles > 0.0 ) {
    // Cycles per unit and the coefficient of variation, on their own line so that
    // the main line stays readable at this width.
    std::printf( "%-60s cycles/unit=%7.2f  cv=%5.1f%%\n", "", result.stats.per_unit_cycles, result.stats.cv_pct );
  }
  if ( !result.notes.empty() ) {
    std::printf( "%38s note: %s\n", "", result.notes.c_str() );
  }
  std::fflush( stdout );
}

// A checksum is printed at exit. If a benchmark is accidentally optimised away,
// the checksum stops changing between revisions, which is visible in the log.
inline uint64_t& checksum() {
  static uint64_t value = 0;
  return value;
}

inline void mix_into_checksum( const uint64_t value ) {
  checksum() ^= value + 0x9e3779b97f4a7c15ull + ( checksum() << 6 ) + ( checksum() >> 2 );
}

// Writes the raw samples of every point, next to the summary CSV.
inline void
write_samples_file( const std::string& csv_path, const RunContext& context, const std::vector< Result >& results ) {
  if ( csv_path.empty() ) {
    return;
  }

  const std::string samples_path = csv_path + ".samples.csv";
  const bool needs_header = !file_exists( samples_path );

  if ( std::FILE* out = open_for_append( samples_path ) ) {
    if ( needs_header ) {
      print_samples_header( out );
    }
    for ( const auto& result : results ) {
      print_samples( out, context, result );
    }
    std::fclose( out );
    std::printf( "# raw samples written to %s\n", samples_path.c_str() );
  }
}

// Explains a point whose spread is too wide to draw a conclusion from. The
// overwhelmingly common cause is first-touch: the first timed run walks pages
// that have never been resident, and averages that cost into the median.
inline void print_spread_hint( const double spread_pct ) {
  if ( spread_pct > 15.0 ) {
    std::printf( "%38s hint: wide spread usually means first-touch pages; check the warmup\n", "" );
  }
}

} // namespace bench
