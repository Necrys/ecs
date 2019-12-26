cmake_minimum_required ( VERSION 3.12.1 FATAL_ERROR )
project ( ecs LANGUAGES CXX )

option( ECS_BUILD_TESTS "Build tests" ON )

set ( CMAKE_CXX_STANDARD 11 )
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

if ( ECS_BUILD_TESTS )
  add_subdirectory( tests )
endif()
