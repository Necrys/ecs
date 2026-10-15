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

  # Address space layout randomisation is a documented source of unreproducible
  # noise, and the mechanism fits what was observed here: within one process the
  # samples of a wide id spread are tight, while the medians of separate launches
  # move by tens of percent, and a wide spread is exactly where page placement
  # decides cache set conflicts. Linking without a dynamic base is the standard
  # remedy. On this machine it did not make the medians stable on its own, so it is
  # kept as a reduction in variables rather than as a fix; runs are compared
  # within a session regardless. The library and the tests keep the default.
  set ( ECS_BENCH_LINK_FLAGS /DYNAMICBASE:NO )
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
  set ( ECS_BENCH_OPT_FLAGS -O2 -DNDEBUG )
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
  add_executable ( ${bench} ${source_name}.cpp keep_alive.cpp )
  target_link_libraries ( ${bench} PRIVATE ecs )
  target_compile_options ( ${bench} PRIVATE ${ECS_BENCH_OPT_FLAGS} )
  target_link_options ( ${bench} PRIVATE ${ECS_BENCH_LINK_FLAGS} )
  ecs_set_warnings ( ${bench} )

  # Benchmarks are never tests: running them belongs to a deliberate, local
  # decision, not to a test run.
  set_target_properties ( ${bench} PROPERTIES
    EXCLUDE_FROM_ALL FALSE
  )
endforeach ()
