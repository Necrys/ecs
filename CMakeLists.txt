cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs LANGUAGES CXX )

option( ECS_BUILD_TESTS "Build tests" ON )

# Benchmarks are off by default: they are run deliberately, locally, and their
# results are compared against a recorded baseline rather than checked on every
# build.
option( ECS_BUILD_BENCHMARKS "Build benchmarks" OFF )

# The build modules are included after the options they depend on: the sanitizer
# and coverage configurations refuse to combine with benchmarks, so
# ECS_BUILD_BENCHMARKS has to be known by the time they run.
list ( APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/tools/cmake )
include ( CompilerWarnings )
include ( Sanitizers )
include ( Coverage )

# C++17 rather than C++11: the test framework (Catch2 v3) requires at least
# C++14, so the standard has to move anyway, and going straight to 17 avoids a
# second migration. Nothing in the library relied on C++11 specifically.
set ( CMAKE_CXX_STANDARD 17 )
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX_EXTENSIONS OFF )

add_library( ecs
  lib/src/registry.cpp
  lib/src/components_storage.cpp
  lib/src/entity.cpp
  lib/src/events.cpp
  lib/src/sparse_vector.cpp
  lib/src/system_base.cpp
)

target_include_directories ( ecs
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/lib/include
)

ecs_set_warnings ( ecs )

# The public headers compiled as their own translation unit, with the template
# surface instantiated. A header warning is otherwise invisible until somebody
# uses that combination, and the templates here are the library's hot path.
# Built as an object library: no main(), no link step, and always part of the
# default build, because the headers matter even when the tests are off - which is
# why the target is defined here rather than in tests/, even though its source
# lives with the tests.
add_library( ecs_headers OBJECT tests/check/headers.cpp )
target_link_libraries( ecs_headers PRIVATE ecs )
ecs_set_warnings ( ecs_headers )

# enable_testing() belongs to the top-level directory: called only in a
# subdirectory it produces a CTestTestfile there, and `ctest` run from the build
# root reports "No tests were found".
enable_testing()

if ( ECS_BUILD_TESTS )
  add_subdirectory( tests )
endif()

if ( ECS_BUILD_BENCHMARKS )
  add_subdirectory( tests/bench )
endif()
