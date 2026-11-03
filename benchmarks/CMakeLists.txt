find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(REAL_PATH "${Python3_EXECUTABLE}" BENCHMARK_PYTHON)

set(BENCHMARK_VENV "${PROJECT_SOURCE_DIR}/.venv")
set(BENCHMARK_PYTHON_STAMP "${BENCHMARK_VENV}/.benchmark-requirements.stamp")

add_custom_command(
  OUTPUT "${BENCHMARK_PYTHON_STAMP}"
  COMMAND "${BENCHMARK_PYTHON}" -m venv "${BENCHMARK_VENV}"
  COMMAND "${BENCHMARK_VENV}/bin/python" -m pip install
          --quiet --disable-pip-version-check "matplotlib>=3.8,<4"
  COMMAND "${CMAKE_COMMAND}" -E touch "${BENCHMARK_PYTHON_STAMP}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
  COMMENT "Setting up the benchmark Python environment"
  VERBATIM
)
add_custom_target(benchmark_python DEPENDS "${BENCHMARK_PYTHON_STAMP}")

add_executable(cache_bench_runner runner.cpp)
target_link_libraries(cache_bench_runner PRIVATE caches CLI11::CLI11)
target_compile_options(cache_bench_runner PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(cache_bench_runner benchmark_python)
