# Benchmarks.
#
# These link the library exactly as it ships and add nothing to it: the rule is
# that tests and measurements do not dictate the API. All of them live in the
# repository because the source is portable, while run data is kept outside it -
# every binary takes a --csv path and writes nowhere unless asked.
#
# Frame sizes and workloads are derived from the machine at run time (see
# machine.hpp), so the same source is meaningful on a desktop and on a Raspberry
# Pi. Optimisation flags are pinned per compiler rather than inherited from
# CMAKE_BUILD_TYPE, so a run cannot silently happen with a debug build.

if ( MSVC )
  set ( ECS_BENCH_OPT_FLAGS /O2 /Ob2 /DNDEBUG )
  set ( ECS_BENCH_WARN_FLAGS /W3 )
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
  set ( ECS_BENCH_OPT_FLAGS -O2 -DNDEBUG )
  set ( ECS_BENCH_WARN_FLAGS -Wall -Wextra )
else ()
  message ( FATAL_ERROR "Unsupported compiler for benchmarks: ${CMAKE_CXX_COMPILER_ID}" )
endif ()

set ( benchmarks
  bench_iter
  bench_structure
  bench_access
)

foreach ( bench ${benchmarks} )
  string ( REPLACE "bench_" "" source_name ${bench} )
  add_executable ( ${bench} ${source_name}.cpp )
  target_link_libraries ( ${bench} PRIVATE ecs )
  target_compile_options ( ${bench} PRIVATE ${ECS_BENCH_OPT_FLAGS} ${ECS_BENCH_WARN_FLAGS} )

  # Benchmarks are never tests: running them belongs to a deliberate, local
  # decision, not to a test run.
  set_target_properties ( ${bench} PROPERTIES
    EXCLUDE_FROM_ALL FALSE
  )
endforeach ()
