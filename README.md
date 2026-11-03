# Caches

C++23 library with LRU, LFU, ARC, LIRS, 2Q, inclusive/exclusive multi-level
caches, and an offline IDEAL reference.

## Requirements

- Linux
- GCC 14+
- CMake 3.28+
- Ninja 1.11+
- Git and internet access to fetch GoogleTest (tests) and CLI11 (runner)
  through CPM.cmake
- Python 3 with `venv` when `BUILD_BENCHMARKS=ON` (`python3-venv` on Debian/Ubuntu)

## Build

Configure from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
```

For library only, use `-DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF`.

Build:

```bash
cmake --build build -j2
```

With `BUILD_BENCHMARKS=ON`, the build creates `.venv` and installs Matplotlib
via pip. Setup reruns when `benchmarks/CMakeLists.txt` changes or `.venv` is
removed.

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Benchmark

Python calls the C++ runner to compare hit rates across traces and cache budgets.
It tests all ordered policy combinations, including repetitions: 5, 25, or 125
strategies for 1, 2, or 3 levels. IDEAL uses one cache with the same total budget.

Exclusive hierarchies are the default. With `--inclusive`, total hit rate
depends on the last level.

[30 synthetic traces](benchmarks/traces/README.md) are included.
Trace format: positive key-space size, positive request count, then the integer
keys, one value per line.

```bash
.venv/bin/python benchmarks/benchmark.py \
  --levels 1 2 3 --step 0.05 --max-ratio 0.3 \
  --output results/comparison
```

| Argument | Default | Description |
| --- | --- | --- |
| `--data-dir PATH` | `benchmarks/traces` | Input trace directory. |
| `--pattern GLOB` | `*.txt` | Trace filename filter; quote patterns, e.g. `'16_*'`. |
| `--runner PATH` | `build/benchmarks/cache_bench_runner` | C++ runner executable. |
| `--levels N [N ...]` | `1 2 3` | Level counts: 1, 2, or 3. |
| `--exclusive` | On | Use an exclusive hierarchy. |
| `--inclusive` | Off | Use an inclusive hierarchy; excludes `--exclusive`. |
| `--split-2 W1 W2` | `1 3` | Capacity weights for two levels, L1 first. |
| `--split-3 W1 W2 W3` | `1 2 5` | Capacity weights for three levels, L1 first. |
| `--step RATIO` | `0.05` | Cache/data ratio step; `0 < RATIO <= 1`. |
| `--max-ratio RATIO` | `1` | Maximum total budget / key-space size; `0 < RATIO <= 1`. |
| `--top N` | `10` | Positive limit on distinct curves and ranking bars; line plots also show IDEAL. |
| `--output PATH` | `results/benchmark` | Output directory; must not already exist. |
| `-h`, `--help` | — | Show help and exit. |

### Output

- `results.csv`: all measurements and IDEAL hit rates.
- `<trace>.png`: hit rate (%) against actual budget / key-space size.
- `ranking.csv` and `ranking.png`: overall strategy scores.
- `ranking_by_ratio.csv` and `ranking_by_ratio.png`: scores across traces at
  each requested ratio.

Line plots select the highest mean curves and show identical curves once.
CSV files keep all strategies.

### Score

Scores compare miss counts with IDEAL:

```text
miss_ratio      = (requests - ideal_hits) / (requests - hits)
trace_log_ratio = mean(log(miss_ratio) over distinct budgets in one trace)
score           = 100 * exp(mean(trace_log_ratio over traces))
score_at_ratio  = 100 * exp(mean(log(miss_ratio_at_ratio) over traces))
```

Traces have equal weight, as do distinct budgets within each trace.
100 matches IDEAL; 50 means twice as many misses on a weighted geometric average.
Scores are not hit-rate percentages. IDEAL is excluded from the ranking.

## C++ runner

Prints `strategy hits requests` per combination. Policy names run from L1
downwards, e.g. `lru>arc`.

```bash
./build/benchmarks/cache_bench_runner \
  --trace benchmarks/traces/16_database_mixed.txt --capacities 64 192
```

| Argument | Description |
| --- | --- |
| `--trace PATH` | Required trace file in the format above. |
| `--capacities N [N ...]` | Required: 1–3 positive, nondecreasing integer capacities, from L1 downwards. |
| `--ideal` | Run only IDEAL; requires exactly one capacity. |
| `--exclusive` | Use an exclusive hierarchy (default). |
| `--inclusive` | Use an inclusive hierarchy. |
| `-h`, `--help` | Show help and exit. |

`--ideal`, `--exclusive`, and `--inclusive` are mutually exclusive.
