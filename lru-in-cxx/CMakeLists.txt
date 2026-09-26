cmake_minimum_required(VERSION 3.16)
project(lru_in_cxx LANGUAGES CXX)

add_library(lru_cache INTERFACE)
target_include_directories(lru_cache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(lru_cache INTERFACE cxx_std_17)

add_executable(lru lru_cache.cc)
target_link_libraries(lru PRIVATE lru_cache)
set_target_properties(lru PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(lru_tests lru_tests.cc)
  target_link_libraries(lru_tests PRIVATE lru_cache GTest::gtest_main)
  set_target_properties(lru_tests PROPERTIES CXX_EXTENSIONS OFF)

  include(GoogleTest)
  gtest_discover_tests(lru_tests)
endif()
