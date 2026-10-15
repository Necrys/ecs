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

# Catch2 is a dependency, not code under review: its headers must not be judged
# by our warning set, which under clang is -Weverything. Marking its include
# directories as system keeps the warnings on our own code.
foreach ( catch2_target Catch2 Catch2WithMain Catch2_buildall_interface )
  if ( TARGET ${catch2_target} )
    get_target_property ( catch2_includes ${catch2_target} INTERFACE_INCLUDE_DIRECTORIES )
    if ( catch2_includes )
      set_target_properties ( ${catch2_target}
        PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${catch2_includes}"
      )
    endif()
  endif()
endforeach()

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

  ecs_set_warnings ( ${target} )

  # An instrumented executable has to find its runtime, and the DLL that matches
  # an import library is the one from the compiler that produced it - another
  # toolchain ships a DLL of the same name. Copying it next to the executable
  # makes the run independent of PATH. See cmake/Sanitizers.cmake.
  if ( ECS_ASAN_RUNTIME_DLL )
    add_custom_command( TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ECS_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${target}>"
      COMMENT "Copying the ASan runtime next to ${target}"
    )
  endif()

  if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_definitions ( ${target}
      PRIVATE
        # Iterator debugging is a Debug-only facility of the MSVC standard
        # library; forcing it in a Release build is a hard error there. The
        # library keeps assert() enabled on its own, so the tests still exercise
        # the internal checks that matter.
        $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=1>
    )
  endif ()

endforeach ()

# -----------------------------------------------------------------------------
# CTest registration: one test per Catch2 TEST_CASE, not one per binary. A failure
# names the case that failed instead of the file it lives in, cases can be
# selected by name or tag, and CTest can run and time them individually.
# Discovery asks the binary for its case list once, after it is built.
#
# enable_testing() is called by the top-level CMakeLists.txt.
# -----------------------------------------------------------------------------
include( Catch )

foreach ( target ${tests} )
  catch_discover_tests( ${target}
    # Names are prefixed with the binary: "init" exists in more than one file.
    TEST_PREFIX ${target}.
    PROPERTIES
      LABELS ${target}
      # A case that hangs must not hang the run; the suite takes milliseconds
      # today, so the limit is generous rather than tight.
      TIMEOUT 60
  )
endforeach ()
