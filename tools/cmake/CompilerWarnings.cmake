# Warning and conformance flags, shared by every target that compiles C++ here:
# the library, the header check, the tests and the benchmarks.
#
# One list on purpose. Before this module the library itself was built with no
# warning flags at all, while the tests and the benchmarks each carried a short
# list of their own - so a defect could sit in the library and be seen, or not,
# depending on who happened to compile it. The class this is meant to catch
# first is a silent narrowing or sign change, which is how D1 appeared.
#
# Warnings are errors by default, locally as well as on a build machine: a
# warning that is allowed to accumulate stops being read.

option( ECS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON )

if ( MSVC )
  # CMake puts /W3 into CMAKE_CXX_FLAGS by default. The warning level belongs to
  # this module instead, and leaving both on the command line makes the compiler
  # report the override (D9025) on every single file.
  string( REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
endif()

# The extra checks Clang has and GCC does not, in one place: both drivers get the
# same list, whether they arrive through the MSVC branch (clang-cl) or the
# GNU-compatible one (clang on Linux). What is disabled is disabled with a reason -
# these are the ones that contradict the design rather than the code.
function( ecs_set_clang_warnings target )
  target_compile_options( ${target} PRIVATE
    -Weverything
    -Wno-c++98-compat                         # the project is C++17
    -Wno-c++98-compat-local-type-template-args
    -Wno-c++98-compat-pedantic
    -Wno-padded                               # padding is a data-layout fact here
    -Wno-global-constructors                  # component_type<T>::id, see D8 in the plan
    -Wno-unsafe-buffer-usage                  # page does deliberate pointer arithmetic, see D15
  )
endfunction()

function( ecs_set_warnings target )
  if ( MSVC )
    target_compile_options( ${target} PRIVATE
      /W4
      /permissive-
      /Zc:__cplusplus
      /utf-8
      /w14640   # construction of a function-local static is not thread-safe
      /w14668   # undefined identifier in #if, treated as 0
    )

    # CMake sets MSVC for clang-cl as well, because it drives the same frontend
    # flags and the same ABI. The warning *sets* are not the same, though: taking
    # this branch alone left clang-cl with /W4 and none of the checks below, so the
    # two Clang drivers disagreed about the code and only the Linux one - GNU-mode
    # clang, which reaches the Clang branch - ever reported the difference. clang-cl
    # understands -Weverything alongside the MSVC flags, so it gets both.
    if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
      ecs_set_clang_warnings( ${target} )
    endif()

    if ( ECS_WARNINGS_AS_ERRORS )
      target_compile_options( ${target} PRIVATE /WX )
    endif()

    return()
  endif()

  if ( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
    target_compile_options( ${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wmissing-declarations   # a non-static function belongs in a header
      -Wold-style-cast
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wnon-virtual-dtor
      -Wdouble-promotion
      -Wformat=2
      -Wundef
    )

    if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
      ecs_set_clang_warnings( ${target} )
    endif()

    if ( ECS_WARNINGS_AS_ERRORS )
      target_compile_options( ${target} PRIVATE -Werror )
    endif()

    return()
  endif()

  message( FATAL_ERROR "Unsupported compiler for warnings: ${CMAKE_CXX_COMPILER_ID}" )
endfunction()
