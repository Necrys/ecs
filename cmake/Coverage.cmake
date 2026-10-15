# Coverage instrumentation, off by default: it writes profile data while the
# tests run and slows them down, so it is a deliberate configuration.
#
# What is used where, checked on this machine rather than assumed (15.10.2026):
#
#   * clang and clang-cl - source-based coverage: -fprofile-instr-generate with
#     -fcoverage-mapping, reported by llvm-cov, which ships with LLVM. clang-cl
#     accepts the switches as they are, and the profile counts regions, functions,
#     lines and branches.
#   * GCC - --coverage (gcov), which is what the boards have. It is wired here but
#     has not been exercised from this machine; the report side is tools/coverage.py.
#   * MSVC - refused. Its only supported path is Microsoft.CodeCoverage, whose
#     CodeCoverage.exe belongs to the Enterprise test tooling and is not installed
#     with Community; accepting the option would produce a report of nothing.
#
# Coverage and benchmarks do not combine for the same reason sanitizers do not:
# the profile bookkeeping would be measured instead of the code.

option( ECS_ENABLE_COVERAGE "Build with coverage instrumentation" OFF )

if ( ECS_ENABLE_COVERAGE )
  if ( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    message( FATAL_ERROR
      "Coverage with MSVC needs Microsoft.CodeCoverage (CodeCoverage.exe), which is not part "
      "of Visual Studio Community. Use the clang-cl coverage preset, or GCC on Linux."
    )
  endif()

  if ( ECS_BUILD_BENCHMARKS )
    message( FATAL_ERROR
      "Coverage and benchmarks do not mix: the profile bookkeeping would be measured "
      "instead of the code. Configure with ECS_BUILD_BENCHMARKS=OFF."
    )
  endif()

  if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    set( ECS_COVERAGE_FLAGS -fprofile-instr-generate -fcoverage-mapping )
  else()
    # GCC. The absolute paths keep the .gcno records usable from a build tree that
    # is not the source tree, which is how the boards are driven.
    set( ECS_COVERAGE_FLAGS --coverage -fprofile-abs-path )
  endif()

  string( REPLACE ";" " " ECS_COVERAGE_FLAGS_TEXT "${ECS_COVERAGE_FLAGS}" )

  # Whole build, like the sanitizers: the library is what is being measured, and
  # it is compiled into every test binary.
  string( APPEND CMAKE_CXX_FLAGS " ${ECS_COVERAGE_FLAGS_TEXT}" )
  string( APPEND CMAKE_EXE_LINKER_FLAGS " ${ECS_COVERAGE_FLAGS_TEXT}" )

  message( STATUS "Coverage: ${ECS_COVERAGE_FLAGS_TEXT}" )
endif()
