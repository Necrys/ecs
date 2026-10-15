# Runtime instrumentation, off by default: it costs time and memory, so it is a
# deliberate configuration for checking a change rather than a property of an
# ordinary build.
#
# What is available where, checked on this machine rather than assumed:
#
#   * MSVC / Windows  - ASan works and the tests pass under it (`msvc-asan`
#     preset). Its runtime is on PATH already, and MSVC's own implementation is
#     the supported one on this platform.
#   * clang on Windows - neither ASan nor UBSan links here. clang's ASan runtime
#     installs interceptors and dies inside `interception_win` with an unhandled
#     instruction, and the UBSan standalone runtime is built against the static C
#     runtime, so it refuses this build's /MD objects. Both are refused with the
#     reason below instead of producing a binary that crashes for a reason
#     unrelated to the code under test.
#   * GCC / clang elsewhere - ASan, UBSan and TSan, added by the compiler driver
#     that performs the link there. This is the configuration the plan means by
#     "ASan/UBSan on every change"; it runs on the Linux and Raspberry Pi side.
#
# Impossible combinations fail the configuration instead of being ignored: a
# silently dropped sanitizer is worse than one that was never requested.

option( ECS_ENABLE_ASAN "Build with AddressSanitizer" OFF )
option( ECS_ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer" OFF )
option( ECS_ENABLE_TSAN "Build with ThreadSanitizer" OFF )

if ( ECS_ENABLE_ASAN AND ECS_ENABLE_TSAN )
  message( FATAL_ERROR "ASan and TSan cannot be combined: both instrument memory access." )
endif()

if ( ( ECS_ENABLE_ASAN OR ECS_ENABLE_UBSAN OR ECS_ENABLE_TSAN ) AND ECS_BUILD_BENCHMARKS )
  message( FATAL_ERROR
    "Sanitizers and benchmarks do not mix: a measurement taken under instrumentation "
    "measures the instrumentation. Configure with ECS_BUILD_BENCHMARKS=OFF."
  )
endif()

# The compiler id, not MSVC: CMake sets MSVC for clang-cl as well, because it
# drives the same command line. Instrumentation is exactly where the two differ.
if ( WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  if ( ECS_ENABLE_ASAN )
    message( FATAL_ERROR
      "clang's ASan cannot intercept on Windows here: the test process dies in interception_win "
      "with an unhandled instruction. Use the msvc-asan preset for ASan on Windows, "
      "or a GCC/clang build on Linux, where ASan, UBSan and TSan all work."
    )
  endif()
  if ( ECS_ENABLE_UBSAN )
    message( FATAL_ERROR
      "clang's UBSan runtime on Windows is built against the static C runtime and refuses "
      "this build's /MD objects. Use a GCC/clang build on Linux for UBSan."
    )
  endif()
endif()

if ( ECS_ENABLE_TSAN AND WIN32 )
  message( FATAL_ERROR "TSan is not available on Windows. Use a GCC or clang build on Linux." )
endif()

if ( ( ECS_ENABLE_UBSAN OR ECS_ENABLE_TSAN ) AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
  message( FATAL_ERROR "Neither UBSan nor TSan exists in MSVC. Use a GCC or clang build on Linux." )
endif()

set( ECS_SANITIZER_FLAGS "" )

if ( ECS_ENABLE_ASAN )
  if ( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    list( APPEND ECS_SANITIZER_FLAGS /fsanitize=address )
  else()
    list( APPEND ECS_SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer )
  endif()
endif()

if ( ECS_ENABLE_UBSAN )
  # Recoverable reports would let a run finish "successfully" after stepping into
  # undefined behaviour, so UBSan stops the program instead.
  list( APPEND ECS_SANITIZER_FLAGS -fsanitize=undefined -fno-sanitize-recover=all )
endif()

if ( ECS_ENABLE_TSAN )
  list( APPEND ECS_SANITIZER_FLAGS -fsanitize=thread )
endif()

if ( ECS_SANITIZER_FLAGS )
  string( REPLACE ";" " " ECS_SANITIZER_FLAGS_TEXT "${ECS_SANITIZER_FLAGS}" )

  # Applied to the whole build rather than to chosen targets. Instrumentation has
  # to agree across every translation unit that ends up in one executable - an
  # uninstrumented dependency is a blind spot, and with MSVC it is also a link
  # error - and the test framework is built here as well.
  string( APPEND CMAKE_CXX_FLAGS " ${ECS_SANITIZER_FLAGS_TEXT}" )

  # The link switch goes to the driver. With GCC and clang the driver performs
  # the link, so it adds the runtime itself; MSVC links through link.exe, which
  # understands the same switch.
  string( APPEND CMAKE_EXE_LINKER_FLAGS " ${ECS_SANITIZER_FLAGS_TEXT}" )

  message( STATUS "Sanitizers: ${ECS_SANITIZER_FLAGS_TEXT}" )
endif()

# An instrumented executable needs its runtime beside it, and PATH is not a
# contract: the DLL that satisfies the import library has to be the one from the
# compiler that produced it, and more than one compiler ships a DLL of the same
# name. The compiler's own directory is the one place that is always right.
if ( ECS_ENABLE_ASAN )
  if ( MSVC )
    set( ECS_ASAN_ARCH "x86_64" )
    if ( CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM64|aarch64" )
      set( ECS_ASAN_ARCH "aarch64" )
    elseif ( CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "X86|i386" )
      set( ECS_ASAN_ARCH "i386" )
    endif()

    get_filename_component( ECS_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY )
    set( ECS_ASAN_RUNTIME_DLL "${ECS_COMPILER_DIR}/clang_rt.asan_dynamic-${ECS_ASAN_ARCH}.dll" )

    if ( NOT EXISTS "${ECS_ASAN_RUNTIME_DLL}" )
      message( FATAL_ERROR "ASan runtime DLL not found next to the compiler: ${ECS_ASAN_RUNTIME_DLL}" )
    endif()

    message( STATUS "ASan runtime: ${ECS_ASAN_RUNTIME_DLL}" )
  endif()
endif()
