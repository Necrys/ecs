#pragma once

// Describes the machine the benchmark runs on, so that the workload can be sized
// to the hardware instead of being hard-coded for one developer's desktop. The
// same binary should produce a meaningful run on a 64 MB-L3 desktop and on a
// Raspberry Pi 1 with 512 MB of RAM and 16 KB of L1.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined( _WIN32 )
// windows.h defines min and max as macros unless told not to, and the library
// uses std::numeric_limits< T >::max(). Defining these before the include keeps
// the library headers compilable in a translation unit that touches Win32.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if defined( _MSC_VER )
#include <intrin.h>
#endif
#else
#include <sched.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace bench {

struct MachineInfo {
  std::string cpu;
  std::string arch;
  std::string os;
  int physical_cores = 0;
  int logical_cores = 0;
  size_t l1_bytes = 0;
  size_t l2_bytes = 0;
  size_t l3_bytes = 0;
  size_t ram_bytes = 0;
  bool cache_sizes_known = false;
  bool ram_known = false;
};

// -----------------------------------------------------------------------------
// CPU identity and cache geometry
// -----------------------------------------------------------------------------

#if ( defined( _M_X64 ) || defined( _M_IX86 ) || defined( __x86_64__ ) || defined( __i386__ ) )

// CPUID leaf 0x80000006 reports the L2 and L3 sizes on AMD and most Intel parts;
// leaf 4 does so only on Intel. Both are tried, and a zero result is reported as
// "unknown" rather than guessed at.
inline void detect_cache_x86( MachineInfo& info ) {
  int regs[ 4 ] = { 0, 0, 0, 0 };

#if defined( _MSC_VER )
  __cpuid( regs, static_cast< int >( 0x80000000u ) );
#else
  __cpuid( 0x80000000, regs[ 0 ], regs[ 1 ], regs[ 2 ], regs[ 3 ] );
#endif

  const unsigned max_extended = static_cast< unsigned >( regs[ 0 ] );
  if ( max_extended >= 0x80000005u ) {
    // AMD reports the L1 geometry in leaf 0x80000005: data cache size in KB in
    // the high half of ECX.
#if defined( _MSC_VER )
    __cpuid( regs, static_cast< int >( 0x80000005u ) );
#else
    __cpuid( 0x80000005, regs[ 0 ], regs[ 1 ], regs[ 2 ], regs[ 3 ] );
#endif
    const uint32_t l1_kb = ( static_cast< uint32_t >( regs[ 2 ] ) >> 24 ) & 0xFFu;
    if ( l1_kb != 0 ) {
      info.l1_bytes = static_cast< size_t >( l1_kb ) * 1024u;
    }
  }

  if ( max_extended >= 0x80000006u ) {
#if defined( _MSC_VER )
    __cpuid( regs, static_cast< int >( 0x80000006u ) );
#else
    __cpuid( 0x80000006, regs[ 0 ], regs[ 1 ], regs[ 2 ], regs[ 3 ] );
#endif
    const uint32_t l2_kb = ( static_cast< uint32_t >( regs[ 2 ] ) >> 16 ) & 0xFFFFu;
    const uint32_t l3_kb = ( ( static_cast< uint32_t >( regs[ 3 ] ) >> 18 ) & 0x3FFFu ) * 512u;
    if ( l2_kb != 0 ) {
      info.l2_bytes = static_cast< size_t >( l2_kb ) * 1024u;
    }
    if ( l3_kb != 0 ) {
      info.l3_bytes = static_cast< size_t >( l3_kb ) * 1024u;
    }
  }

  // Intel's deterministic cache parameters (leaf 4) fill in what is missing.
  for ( unsigned sub = 0; sub < 8; ++sub ) {
#if defined( _MSC_VER )
    __cpuidex( regs, 4, static_cast< int >( sub ) );
#else
    __cpuid_count( 4, sub, regs[ 0 ], regs[ 1 ], regs[ 2 ], regs[ 3 ] );
#endif
    const unsigned type = static_cast< unsigned >( static_cast< uint32_t >( regs[ 0 ] ) & 0x1Fu );
    if ( type == 0 ) {
      break;
    }
    const unsigned level = static_cast< unsigned >( ( static_cast< uint32_t >( regs[ 0 ] ) >> 5 ) & 0x7u );
    const size_t line = static_cast< size_t >( static_cast< uint32_t >( regs[ 1 ] ) & 0xFFFu ) + 1u;
    const size_t ways = static_cast< size_t >( ( static_cast< uint32_t >( regs[ 1 ] ) >> 22 ) & 0x3FFu ) + 1u;
    const size_t sets = static_cast< size_t >( static_cast< uint32_t >( regs[ 2 ] ) ) + 1u;
    const size_t size = line * ways * sets;

    if ( level == 2 && info.l2_bytes == 0 ) {
      info.l2_bytes = size;
    } else if ( level == 3 && info.l3_bytes == 0 ) {
      info.l3_bytes = size;
    }
  }
}

#endif

inline void detect_cache_linux( MachineInfo& info, int core ) {
  // /sys/devices/system/cpu/cpuN/cache/indexM/{level,size,type} exists on Linux
  // including ARM single-board computers, which is why it is preferred there.
  const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string( core ) + "/cache";

  for ( int index = 0; index < 8; ++index ) {
    const std::string dir = base + "/index" + std::to_string( index );
    std::ifstream level_file( dir + "/level" );
    std::ifstream size_file( dir + "/size" );
    if ( !level_file.is_open() || !size_file.is_open() ) {
      break;
    }

    std::string level_text;
    std::string size_text;
    std::getline( level_file, level_text );
    std::getline( size_file, size_text );

    if ( size_text.empty() ) {
      continue;
    }

    // Sizes are written as "32K" or "2M".
    size_t multiplier = 1;
    const char suffix = size_text.back();
    if ( suffix == 'K' || suffix == 'k' ) {
      multiplier = 1024u;
      size_text.pop_back();
    } else if ( suffix == 'M' || suffix == 'm' ) {
      multiplier = 1024u * 1024u;
      size_text.pop_back();
    }

    size_t value = 0;
    try {
      value = static_cast< size_t >( std::stoul( size_text ) ) * multiplier;
    } catch ( ... ) {
      continue;
    }

    if ( level_text == "1" ) {
      info.l1_bytes = value;
    } else if ( level_text == "2" ) {
      info.l2_bytes = value;
    } else if ( level_text == "3" ) {
      info.l3_bytes = value;
    }
  }
}

inline std::string detect_cpu_name() {
#if defined( _WIN32 )
  char buffer[ 256 ] = { 0 };
  HKEY key = nullptr;
  if (
    RegOpenKeyExA( HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key ) ==
    ERROR_SUCCESS
  ) {
    DWORD size = sizeof( buffer );
    DWORD type = 0;
    RegQueryValueExA( key, "ProcessorNameString", nullptr, &type, reinterpret_cast< LPBYTE >( buffer ), &size );
    RegCloseKey( key );
  }
  return std::string( buffer );
#else
  std::ifstream model( "/proc/cpuinfo" );
  std::string line;
  while ( std::getline( model, line ) ) {
    if ( line.rfind( "model name", 0 ) == 0 || line.rfind( "Model", 0 ) == 0 ) {
      const size_t colon = line.find( ':' );
      if ( colon != std::string::npos ) {
        std::string value = line.substr( colon + 1 );
        while ( !value.empty() && value.front() == ' ' ) {
          value.erase( value.begin() );
        }
        return value;
      }
    }
  }
  return "unknown";
#endif
}

inline size_t detect_ram_bytes() {
#if defined( _WIN32 )
  MEMORYSTATUSEX status = {};
  status.dwLength = sizeof( status );
  if ( GlobalMemoryStatusEx( &status ) ) {
    return static_cast< size_t >( status.ullTotalPhys );
  }
  return 0;
#else
  struct sysinfo info = {};
  if ( sysinfo( &info ) == 0 ) {
    return static_cast< size_t >( info.totalram ) * static_cast< size_t >( info.mem_unit );
  }
  return 0;
#endif
}

inline MachineInfo detect_machine() {
  MachineInfo info;

  info.cpu = detect_cpu_name();
  info.logical_cores = static_cast< int >( std::thread::hardware_concurrency() );

#if defined( _WIN32 )
  SYSTEM_INFO system = {};
  GetNativeSystemInfo( &system );
  info.physical_cores = static_cast< int >( system.dwNumberOfProcessors );
  info.os = "Windows";
#else
  info.physical_cores = info.logical_cores;
  info.os = "Linux";
#endif

#if defined( __aarch64__ ) || defined( _M_ARM64 )
  info.arch = "arm64";
#elif defined( __arm__ ) || defined( _M_ARM )
  info.arch = "arm32";
#elif defined( __x86_64__ ) || defined( _M_X64 )
  info.arch = "x86_64";
#elif defined( __i386__ ) || defined( _M_IX86 )
  info.arch = "x86";
#else
  info.arch = "unknown";
#endif

#if ( defined( _M_X64 ) || defined( _M_IX86 ) || defined( __x86_64__ ) || defined( __i386__ ) )
  detect_cache_x86( info );
#elif !defined( _WIN32 )
  detect_cache_linux( info, 0 );
#endif

  info.cache_sizes_known = ( info.l1_bytes != 0 || info.l2_bytes != 0 || info.l3_bytes != 0 );

  info.ram_bytes = detect_ram_bytes();
  info.ram_known = ( info.ram_bytes != 0 );

  return info;
}

// -----------------------------------------------------------------------------
// Reporting
// -----------------------------------------------------------------------------

inline std::string human_bytes( const size_t bytes ) {
  if ( bytes == 0 ) {
    return "unknown";
  }
  char buffer[ 64 ];
  if ( bytes >= 1024u * 1024u * 1024u ) {
    std::snprintf( buffer, sizeof( buffer ), "%.1f GB", static_cast< double >( bytes ) / ( 1024.0 * 1024.0 * 1024.0 ) );
  } else if ( bytes >= 1024u * 1024u ) {
    std::snprintf( buffer, sizeof( buffer ), "%.1f MB", static_cast< double >( bytes ) / ( 1024.0 * 1024.0 ) );
  } else {
    std::snprintf( buffer, sizeof( buffer ), "%.1f KB", static_cast< double >( bytes ) / 1024.0 );
  }
  return std::string( buffer );
}

inline void print_machine( const MachineInfo& info ) {
  std::printf( "# machine\n" );
  std::printf( "#   cpu        : %s\n", info.cpu.c_str() );
  std::printf( "#   arch/os    : %s / %s\n", info.arch.c_str(), info.os.c_str() );
  std::printf( "#   cores      : %d logical, %d reported physical\n", info.logical_cores, info.physical_cores );
  std::printf(
    "#   l1/l2/l3   : %s / %s / %s\n",
    human_bytes( info.l1_bytes ).c_str(),
    human_bytes( info.l2_bytes ).c_str(),
    human_bytes( info.l3_bytes ).c_str()
  );
  std::printf( "#   ram        : %s\n", human_bytes( info.ram_bytes ).c_str() );
  if ( !info.cache_sizes_known ) {
    std::printf( "#   note       : cache sizes unavailable; workload sized from RAM only\n" );
  }
}

// -----------------------------------------------------------------------------
// Execution environment
//
// Where a process runs, and how willingly the scheduler moves it, matters as
// much as the code being measured: a benchmark that migrates between cores
// carries its working set to a cold cache each time, and one sharing a core with
// other work is timed alongside it. Both are controlled here, so a run can pin
// itself down and then say in its own output whether it succeeded.
// -----------------------------------------------------------------------------

struct AffinityState {
  bool pinned = false;          // the mask below was applied
  bool priority_raised = false; // the process priority was raised
  int first_cpu = -1;           // lowest core in the applied mask
  int cpu_count = 0;            // number of cores in the applied mask
};

// The cores the process may currently run on.
inline uint64_t current_affinity_mask() {
#if defined( _WIN32 )
  DWORD_PTR process_mask = 0;
  DWORD_PTR system_mask = 0;
  if ( GetProcessAffinityMask( GetCurrentProcess(), &process_mask, &system_mask ) ) {
    return static_cast< uint64_t >( process_mask );
  }
  return 0;
#else
  cpu_set_t set;
  CPU_ZERO( &set );
  if ( sched_getaffinity( 0, sizeof( set ), &set ) == 0 ) {
    uint64_t mask = 0;
    for ( int cpu = 0; cpu < CPU_SETSIZE && cpu < 64; ++cpu ) {
      if ( CPU_ISSET( cpu, &set ) ) {
        mask |= ( 1ull << cpu );
      }
    }
    return mask;
  }
  return 0;
#endif
}

inline void describe_affinity( AffinityState& state, const uint64_t mask ) {
  state.first_cpu = -1;
  state.cpu_count = 0;
  for ( int cpu = 0; cpu < 64; ++cpu ) {
    if ( mask & ( 1ull << cpu ) ) {
      if ( state.first_cpu < 0 ) {
        state.first_cpu = cpu;
      }
      ++state.cpu_count;
    }
  }
}

// Pins the process to one core, or to the fastest cores the platform reports.
//
// `core` selects a specific core; a negative value asks for the fastest cores
// available, which on a hybrid part means the performance cores. When the
// platform refuses, the run continues unpinned and reports that rather than
// failing: a benchmark that cannot pin still produces a number, just a noisier
// one.
inline AffinityState pin_process( const int core ) {
  AffinityState state;

#if defined( _WIN32 )
  DWORD_PTR target = 0;

  if ( core >= 0 ) {
    target = ( static_cast< DWORD_PTR >( 1 ) << core );
  } else {
    DWORD length = 0;
    GetLogicalProcessorInformationEx( RelationProcessorCore, nullptr, &length );
    if ( length != 0 ) {
      std::vector< unsigned char > buffer( length );
      if (
        GetLogicalProcessorInformationEx(
          RelationProcessorCore, reinterpret_cast< PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX >( buffer.data() ), &length
        )
      ) {
        // One logical processor per physical core. The first core is taken from
        // each entry rather than the whole mask: sharing a physical core with a
        // sibling thread would reintroduce exactly the interference pinning is
        // meant to remove.
        DWORD offset = 0;
        while ( offset < length ) {
          const auto* entry =
            reinterpret_cast< const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* >( buffer.data() + offset );
          if ( entry->Relationship == RelationProcessorCore ) {
            const DWORD_PTR mask = static_cast< DWORD_PTR >( entry->Processor.GroupMask[ 0 ].Mask );
            // Lowest set bit: the first logical processor of this core.
            const DWORD_PTR single = mask & ( ~mask + 1 );
            if ( single != 0 ) {
              target |= single;
            }
          }
          offset += entry->Size;
        }
      }
    }
  }

  if ( target != 0 && SetProcessAffinityMask( GetCurrentProcess(), target ) ) {
    state.pinned = true;
    describe_affinity( state, static_cast< uint64_t >( target ) );
  }

  if ( SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS ) ) {
    state.priority_raised = true;
  }
#else
  cpu_set_t set;
  CPU_ZERO( &set );

  if ( core >= 0 ) {
    CPU_SET( core, &set );
  } else {
    // One logical processor per physical core, on the same reasoning as above.
    const int reported = static_cast< int >( sysconf( _SC_NPROCESSORS_ONLN ) );
    const int physical =
      std::thread::hardware_concurrency() != 0 ? static_cast< int >( std::thread::hardware_concurrency() ) : reported;
    for ( int cpu = 0; cpu < physical && cpu < CPU_SETSIZE; ++cpu ) {
      CPU_SET( cpu, &set );
    }
  }

  if ( sched_setaffinity( 0, sizeof( set ), &set ) == 0 ) {
    state.pinned = true;
    describe_affinity( state, current_affinity_mask() );
  }

  // Best effort: an unprivileged process may not be allowed to raise this, which
  // is not a reason to abandon a benchmark.
  if ( nice( -10 ) != -1 ) {
    state.priority_raised = true;
  }
#endif

  return state;
}

inline void print_affinity( const AffinityState& state, const bool requested ) {
  if ( !requested ) {
    AffinityState current;
    describe_affinity( current, current_affinity_mask() );
    std::printf( "#   affinity   : not requested; free to run on %d core(s)\n", current.cpu_count );
    return;
  }

  if ( !state.pinned ) {
    std::printf( "#   affinity   : pinning requested but refused by the platform\n" );
    return;
  }

  std::printf(
    "#   affinity   : pinned to %d core(s), lowest cpu%d; priority %s\n",
    state.cpu_count,
    state.first_cpu,
    state.priority_raised ? "raised to high" : "left unchanged"
  );
}

// -----------------------------------------------------------------------------
// Time base
//
// Wall-clock time is what a frame costs, but it moves when the core changes
// frequency, which a boost-happy part does constantly. A cycle count does not.
// Both are recorded so that a session can be checked for frequency drift: if
// nanoseconds per cycle changes between two phases of a session, the phases are
// not comparable whatever the ratios say.
// -----------------------------------------------------------------------------

inline uint64_t read_cycles() {
#if defined( _MSC_VER ) || defined( __i386__ ) || defined( __x86_64__ )
#if defined( _MSC_VER )
  return __rdtsc();
#else
  return __builtin_ia32_rdtsc();
#endif
#else
  // No cheap cycle counter on this target; nanoseconds stay the only measure.
  return 0;
#endif
}

// True when read_cycles() returns something meaningful, so that a report does not
// print a column of zeroes as if it were data.
inline bool cycles_available() {
  return read_cycles() != 0;
}

// Nanoseconds per cycle over a short interval. Comparing this between the first
// and last point of a session shows whether the core was running at a consistent
// frequency throughout.
inline double measure_ns_per_cycle() {
  if ( !cycles_available() ) {
    return 0.0;
  }

  const auto start_time = std::chrono::steady_clock::now();
  const uint64_t start_cycles = read_cycles();

  // A short spin, long enough that the clock reading is not the measurement.
  volatile uint64_t spin = 0;
  for ( uint64_t i = 0; i < 2000000u; ++i ) {
    spin += i;
  }

  const uint64_t end_cycles = read_cycles();
  const auto end_time = std::chrono::steady_clock::now();

  const double cycles = static_cast< double >( end_cycles - start_cycles );
  if ( cycles <= 0.0 ) {
    return 0.0;
  }

  const double ns = std::chrono::duration< double, std::nano >( end_time - start_time ).count();
  return ns / cycles;
}

} // namespace bench
