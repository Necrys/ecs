cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs LANGUAGES CXX )

option( ECS_BUILD_TESTS "Build tests" ON )

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

# enable_testing() belongs to the top-level directory: called only in a
# subdirectory it produces a CTestTestfile there, and `ctest` run from the build
# root reports "No tests were found".
enable_testing()

if ( ECS_BUILD_TESTS )
  add_subdirectory( tests )
endif()
