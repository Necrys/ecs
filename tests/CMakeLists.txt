cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs_ut LANGUAGES CXX )

add_executable ( sparse_vector
  sparse_vector.cpp
)

list ( APPEND tests
  sparse_vector
)

include ( FetchContent )
FetchContent_Declare (
  catchorg_catch2
  GIT_REPOSITORY https://github.com/catchorg/catch2
)

FetchContent_GetProperties( catchorg_catch2 )
if ( NOT catchorg_catch2_POPULATED )
  FetchContent_Populate ( catchorg_catch2 )
endif()

foreach ( target ${tests} )

if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
  target_compile_options ( ${target}
    PRIVATE
      /W3
  )
  target_compile_definitions ( ${target}
    PRIVATE
      _HAS_ITERATOR_DEBUGGING=0
  )
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
  target_compile_options ( ${target}
    PRIVATE
      -fprofile-arcs
      -ftest-coverage
      -fno-elide-constructors
      -Wpedantic
      -pedantic-errors
      -ansi
      -Wextra
      -Wall
      -Winit-self
      -Wold-style-cast
      -Woverloaded-virtual
      -Wuninitialized
      -Wmissing-declarations
      -Winit-self
  )
  target_link_libraries( ${target}
    PRIVATE
      ecs
      -fprofile-arcs
      -ftest-coverage
      --coverage
  )

endif ()

target_include_directories( ${target}
  PRIVATE
    ${catchorg_catch2_SOURCE_DIR}/single_include
)

# force NDEBUG for testing reasons
target_compile_definitions( ${target}
  PRIVATE
    NDEBUG
)

endforeach ()