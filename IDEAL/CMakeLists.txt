cmake_minimum_required(VERSION 3.16)
project(ideal_in_cxx LANGUAGES CXX)

add_library(ideal_cache INTERFACE)
target_include_directories(ideal_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(ideal_cache INTERFACE cxx_std_17)

add_executable(ideal ideal-cache.cc)
target_link_libraries(ideal PRIVATE ideal_cache)
set_target_properties(ideal PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(ideal_tests ideal-cache-tests.cc)
  target_link_libraries(ideal_tests PRIVATE ideal_cache GTest::gtest_main)
  set_target_properties(ideal_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(ideal_tests)
endif()
