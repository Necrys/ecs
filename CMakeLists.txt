cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs LANGUAGES CXX )

set ( CMAKE_CXX_STANDARD 11 )
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX_EXTENSIONS OFF )

add_library( ecs INTERFACE )

target_include_directories ( ecs
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory( tests )
