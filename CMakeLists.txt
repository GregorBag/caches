cmake_minimum_required(VERSION 3.28)
project(caches LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

include(cmake/CPM.cmake)
include(cmake/packages.cmake)

add_library(caches STATIC)
target_sources(caches
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      source/abstract_cache.cppm
      source/arc_cache.cppm
      source/ideal_cache.cppm
      source/lfu_cache.cppm
      source/lirs_cache.cppm
      source/lru_cache.cppm
      source/two_queue_cache.cppm
)
target_compile_features(caches PUBLIC cxx_std_23)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()