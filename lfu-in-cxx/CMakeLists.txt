cmake_minimum_required(VERSION 3.16)
project(lfu_in_cxx LANGUAGES CXX)

add_library(lfu_cache INTERFACE)
target_include_directories(lfu_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(lfu_cache INTERFACE cxx_std_17)

add_executable(lfu lfu_cache.cc)
target_link_libraries(lfu PRIVATE lfu_cache)
set_target_properties(lfu PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(lfu_tests lfu_tests.cc)
  target_link_libraries(lfu_tests PRIVATE lfu_cache GTest::gtest_main)
  set_target_properties(lfu_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(lfu_tests)
endif()
