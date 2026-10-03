cmake_minimum_required(VERSION 3.16)
project(arc_in_cxx LANGUAGES CXX)

add_library(arc_cache INTERFACE)
target_include_directories(arc_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arc_cache INTERFACE cxx_std_17)

add_executable(arc arc_cache.cc)
target_link_libraries(arc PRIVATE arc_cache)
set_target_properties(arc PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(arc_tests arc_tests.cc)
  target_link_libraries(arc_tests PRIVATE arc_cache GTest::gtest_main)
  set_target_properties(arc_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(arc_tests)
endif()
