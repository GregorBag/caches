cmake_minimum_required(VERSION 3.16)
project(2Q_in_cxx LANGUAGES CXX)

add_library(2Q_cache INTERFACE)
target_include_directories(2Q_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(2Q_cache INTERFACE cxx_std_17)

add_executable(2Q 2q_cache.cc)
target_link_libraries(2Q PRIVATE 2Q_cache)
set_target_properties(2Q PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(2q_tests 2q_tests.cc)
  target_link_libraries(2q_tests PRIVATE 2Q_cache GTest::gtest_main)
  set_target_properties(2q_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(2q_tests)
endif()
