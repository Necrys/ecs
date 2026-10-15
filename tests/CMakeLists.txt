cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs_ut LANGUAGES CXX )

# -----------------------------------------------------------------------------
# Catch2 v3
#
# Pinned deliberately. An unpinned FetchContent resolves the default branch,
# which Catch2 renamed from master to devel, so the configuration used to fail
# with "invalid reference: master" and the tests could not be built at all.
#
# GIT_TAG is a full commit id rather than the tag name: a commit cannot be moved
# or re-pointed, so the dependency cannot change under us. The tag is recorded
# in ECS_CATCH2_TAG for reference. v3.16.1 is the current stable release.
# -----------------------------------------------------------------------------
set ( ECS_CATCH2_TAG v3.16.1 )
set ( ECS_CATCH2_COMMIT 08092139210881f01a34e60c5f2fd6afa0ea4024 )

include ( FetchContent )

# Stop Catch2 from configuring its own tests, examples and benchmarks.
set ( CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE )
set ( CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set ( CATCH_BUILD_EXTRA_TESTS OFF CACHE BOOL "" FORCE )
set ( CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE )
set ( CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE )

FetchContent_Declare ( catchorg_catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        ${ECS_CATCH2_COMMIT}
  GIT_SHALLOW    FALSE
)

FetchContent_MakeAvailable ( catchorg_catch2 )

# -----------------------------------------------------------------------------
# Test targets
#
# Catch2WithMain supplies the test runner, so the test sources no longer define
# CATCH_CONFIG_MAIN.
# -----------------------------------------------------------------------------
set ( tests
  sparse_vector
  type_enumerator
  storage
  registry
)

foreach ( target ${tests} )

  add_executable ( ${target} ${target}.cpp )

  target_link_libraries ( ${target}
    PRIVATE
      ecs
      Catch2::Catch2WithMain
  )

  if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options ( ${target}
      PRIVATE
        /W3
    )
    target_compile_definitions ( ${target}
      PRIVATE
        _HAS_ITERATOR_DEBUGGING=1
    )
  elseif ( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
    target_compile_options ( ${target}
      PRIVATE
        -Wpedantic
        -pedantic-errors
        -Wextra
        -Wall
        -Winit-self
        -Wold-style-cast
        -Woverloaded-virtual
        -Wuninitialized
        -Wmissing-declarations
    )
  endif ()

endforeach ()

# -----------------------------------------------------------------------------
# CTest registration
#
# enable_testing() is called by the top-level CMakeLists.txt.
#
# No PASS_REGULAR_EXPRESSION on purpose: Catch2 writes its summary to stderr, so
# a pass pattern is never matched and every test would be reported as failed. The
# exit code is the verdict, and [!mayfail] keeps the known layout defect from
# turning that code non-zero.
# -----------------------------------------------------------------------------
foreach ( target ${tests} )
  add_test ( NAME ${target} COMMAND ${target} )
endforeach ()
