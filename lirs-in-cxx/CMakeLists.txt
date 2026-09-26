cmake_minimum_required(VERSION 3.16)
project(lirs_in_cxx LANGUAGES CXX)

add_library(lirs_cache INTERFACE)
target_include_directories(lirs_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(lirs_cache INTERFACE cxx_std_17)

add_executable(lirs lirs_cache.cc)
target_link_libraries(lirs PRIVATE lirs_cache)
set_target_properties(lirs PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(lirs_tests lirs_tests.cc)
  target_link_libraries(lirs_tests PRIVATE lirs_cache GTest::gtest_main)
  set_target_properties(lirs_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(lirs_tests)
endif()
