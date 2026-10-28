add_executable(cache_bench_runner runner.cpp)
target_link_libraries(cache_bench_runner PRIVATE caches CLI11::CLI11)
target_compile_options(cache_bench_runner PRIVATE -Wall -Wextra -Wpedantic)
