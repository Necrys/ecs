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
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
  double per_unit_ns = 0.0;
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

  return stats;
}

// -----------------------------------------------------------------------------
// Run configuration
// -----------------------------------------------------------------------------

struct RunConfig {
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

// Explains a point whose spread is too wide to draw a conclusion from. The
// overwhelmingly common cause is first-touch: the first timed run walks pages
// that have never been resident, and averages that cost into the median.
inline void print_spread_hint( const double spread_pct ) {
  if ( spread_pct > 15.0 ) {
    std::printf( "%38s hint: wide spread usually means first-touch pages; check the warmup\n", "" );
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

inline void print_csv_header( std::FILE* out ) {
  std::fprintf(
    out,
    "run_id,timestamp,git_rev,cpu,arch,os,compiler,config,scenario,components,"
    "entity_count,live_count,id_span,component_bytes,operations,samples,"
    "p10_ns,median_ns,p90_ns,min_ns,max_ns,iqr_pct,tail_pct,per_unit_ns,notes\n"
  );
}

inline void
print_csv_row( std::FILE* out, const RunContext& context, const Result& result, const std::string& timestamp ) {
  std::fprintf(
    out,
    "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.3f,%s\n",
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
    result.stats.p10_ms * 1e6,
    result.stats.median_ms * 1e6,
    result.stats.p90_ms * 1e6,
    result.stats.min_ms * 1e6,
    result.stats.max_ms * 1e6,
    result.stats.spread_pct,
    result.stats.tail_pct,
    result.stats.per_unit_ns,
    csv_escape( result.notes ).c_str()
  );
}

// -----------------------------------------------------------------------------
// Measurement
// -----------------------------------------------------------------------------

// Runs `work` once and returns its duration in milliseconds. The value returned
// by `work` is accumulated by the caller into a checksum so that the compiler
// cannot conclude the work is unobservable and remove it.
template < typename F >
double time_once_ms( F&& work ) {
  const auto start = std::chrono::steady_clock::now();
  work();
  const auto finish = std::chrono::steady_clock::now();
  return std::chrono::duration< double, std::milli >( finish - start ).count();
}

// Measures `work` at one point: warmup, then `samples` timed runs.
//
// Each timed sample repeats the work until it has run for at least
// `min_sample_ms`. A sample lasting a fraction of a millisecond is at the mercy
// of whatever the operating system does during it: a single preemption then
// dominates the reading and inflates the spread of the point. Lengthening the
// window averages that noise inside the sample instead of letting it move the
// quartiles.
//
// `setup` re-runs before every timed run when the scenario mutates its data.
template < typename Setup, typename Work >
Stats measure( const RunConfig& config, const size_t units, Setup&& setup, Work&& work ) {
  const int warmup = units >= 1000000u ? config.bulk_warmup : config.warmup;
  const int samples = units >= config.large_threshold ? config.large_samples : config.samples;

  // How many repetitions make one sample long enough to be trustworthy.
  //
  // The estimate is taken from the fastest of a few runs, not from the first
  // one: the first run of a point is frequently an outlier because of a page
  // fault or a preemption, and sizing every later sample from it leaves the
  // samples far longer than intended and the point noisier than it needs to be.
  int repeat = config.scale;
  {
    double fastest_ms = 0.0;
    const int trials = 5;
    for ( int t = 0; t < trials; ++t ) {
      const double trial_ms = time_once_ms( work );
      if ( t == 0 || trial_ms < fastest_ms ) {
        fastest_ms = trial_ms;
      }
    }
    while ( fastest_ms * static_cast< double >( repeat ) < config.min_sample_ms && repeat < config.max_scale ) {
      repeat *= 2;
    }
  }

  for ( int i = 0; i < warmup; ++i ) {
    setup();
    for ( int r = 0; r < repeat; ++r ) {
      work();
    }
  }

  std::vector< double > durations;
  durations.reserve( static_cast< size_t >( samples ) );

  for ( int i = 0; i < samples; ++i ) {
    setup();
    durations.push_back( time_once_ms( [ & ] {
      for ( int r = 0; r < repeat; ++r ) {
        work();
      }
    } ) );
  }

  return summarise( durations, units * static_cast< size_t >( repeat ) );
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

} // namespace bench
