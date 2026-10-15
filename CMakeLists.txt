cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs LANGUAGES CXX )

list ( APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake )
include ( CompilerWarnings )

option( ECS_BUILD_TESTS "Build tests" ON )

# Benchmarks are off by default: they are run deliberately, locally, and their
# results are compared against a recorded baseline rather than checked on every
# build.
option( ECS_BUILD_BENCHMARKS "Build benchmarks" OFF )

# C++17 rather than C++11: the test framework (Catch2 v3) requires at least
# C++14, so the standard has to move anyway, and going straight to 17 avoids a
# second migration. Nothing in the library relied on C++11 specifically.
set ( CMAKE_CXX_STANDARD 17 )
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX_EXTENSIONS OFF )

add_library( ecs
  src/registry.cpp
  src/components_storage.cpp
  src/entity.cpp
  src/system_base.cpp
)

target_include_directories ( ecs
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

ecs_set_warnings ( ecs )

# The public headers compiled as their own translation unit, with the template
# surface instantiated. A header warning is otherwise invisible until somebody
# uses that combination, and the templates here are the library's hot path.
# Built as an object library: no main(), no link step, and always part of the
# default build, because the headers matter even when the tests are off.
add_library( ecs_headers OBJECT check/headers.cpp )
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
  add_subdirectory( bench )
endif()
