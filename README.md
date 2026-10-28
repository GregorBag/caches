# Caches

C++23 cache library implementing ARC, IDEAL, LFU, LIRS, LRU, and 2Q.

## Requirements

- Linux
- CMake 3.28+
- GCC 14+ (C and C++)
- Ninja 1.11+
- Git and internet access to download dependencies

GoogleTest is fetched automatically via CPM.cmake.

## Building

Configure and build from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=Release
```
Tests are enabled by default. Add `-DBUILD_TESTS=OFF` to configure a library-only build.

```bash
cmake --build build -j2
```

## Running

The project builds a library. Run the test executable directly:

```bash
./build/tests/cache_tests
```

## Testing

Run all tests through CTest, showing output on failure:

```bash
ctest --test-dir build --output-on-failure
```

## Hit-rate benchmark

Build the C++ runner and install the Python plotting dependency:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build --target cache_bench_runner -j2
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r benchmarks/requirements.txt
```

If creating the venv fails because `ensurepip` is missing, an installed system
pip can finish the setup without installing system packages:

```bash
python3 -m venv --without-pip .venv
python3 -m pip --python .venv/bin/python install pip -r benchmarks/requirements.txt
source .venv/bin/activate
```

Run from the repository root (list arguments are **space-separated**):

```bash
python benchmarks/benchmark.py \
  --levels 1 2 3 \
  --algorithms lru lfu arc lirs 2q \
  --split-2 1 3 --split-3 1 2 5 \
  --universe-size 1024 \
  --ratios 0.03125 0.0625 0.125 0.25 0.5 1 \
  --output results/comparison
```

`--data-dir` defaults to `cache-tests`, `--pattern` to `*.txt`, and `--runner`
to `build/benchmarks/cache_bench_runner`. Use e.g. `--pattern '18_*'` for one
trace. `--top 10` limits plotted curves and ranking bars. See `--help` for defaults.
Each trace starts with a positive request count followed by that many integer keys.

Python enumerates every ordered algorithm combination **with repetitions**, then
calls C++ for each configuration. With five algorithms and 1–3 levels there are
155 configurations per trace/budget. Each run starts empty and counts only the
boolean result of `lookup_update()`. IDEAL runs once per trace/budget using the
entire budget. The current inclusive hierarchy's total hit rate depends on its
last level; all combinations are nevertheless executed.

The total budget is `ratio * universe-size`, rounded to the nearest integer
(halves up). Repeated rounded budgets are merged. 1024 is the key-space size,
not necessarily the number of different keys observed in a file. Sizes are in
resident entries, excluding policy metadata. Each level's proportional share
is rounded down; the remainder goes to the last level. Zero-sized levels are
rejected: increase the budget. Splits must be positive and nondecreasing.
For budget 256 the default sizes are `256`, `64:192`, and `32:64:160`.

The output directory must be new. It receives `results.csv` (all measurements,
actual sizes and hit rates), one PNG per trace, `ranking.csv`, and `ranking.png`.
Curves show top mean-hit-rate strategies; identical curves are shown once.
CSV keeps every configuration. Rates are percentages, and the X axis uses the
actual budget divided by `universe-size`.

The overall ranking rewards positive outliers and penalizes negative ones.
For each trace/budget, using hit rates in percentage points:

```text
center = median(rates)
scale  = max(1, 1.4826 * median(abs(rate - center)))
z      = clamp((rate - center) / scale, -3, 3)
score  = z + 0.25 * max(z, 0)^2 - 0.5 * max(-z, 0)^2
```

Scores are averaged equally over the selected budget points and traces; denser
sampling therefore gives a region more weight. All selected configurations
participate in the reference distribution, including equivalent ones. IDEAL is
excluded. Equal scores receive equal ranks; plotted top-K ties are selected
alphabetically. This is a configurable experiment's relative ranking, not a
claim of a universally optimal strategy.

The runner is also usable directly; it prints `hits requests`:

```bash
./build/benchmarks/cache_bench_runner --trace cache-tests/18_database_mixed.txt \
  --algorithms lru arc --capacities 64 192
./build/benchmarks/cache_bench_runner --trace cache-tests/18_database_mixed.txt \
  --ideal --capacities 256
```

Only Matplotlib is needed beyond the Python standard library. There are no
subcommands, resume/checkpoints, configuration files, parallel workers or
report-only mode. The simple Python → C++ invocation follows the approach used
in [nhwc](https://github.com/lon3ry/nhwc/tree/main/bench/source).
