add_executable(
  cache_tests
  arc_tests.cpp
  ideal_cache_tests.cpp
  lfu_tests.cpp
  lirs_tests.cpp
  lru_tests.cpp
  multi_level_tests.cpp
  two_queue_tests.cpp
)

target_link_libraries(
  cache_tests
  PRIVATE
  caches
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cache_tests)
