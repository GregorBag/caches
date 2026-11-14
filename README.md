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

[30 synthetic workloads](benchmarks/README.md) are generated on each run,
with **100,000 possible keys and 200,000 requests** per trace by default.
Each trace is generated once in memory and sent through stdin to every runner
invocation, so all policies, capacities and IDEAL receive identical requests.
Generation is deterministic for the scenario name, generator/Python versions and sizes.
Independent cache budgets run concurrently; `--jobs` limits the number of C++ processes.
Working-set sizes and phase lengths scale with the requested sizes.

```bash
.venv/bin/python benchmarks/benchmark.py \
  --levels 1 2 3 --step 0.05 --max-ratio 0.3 \
  --output results/comparison
```

| Argument | Default | Description |
| --- | --- | --- |
| `--universe-size N` | `1000000` | Number of possible keys. |
| `--requests N` | `2 * universe size` | Requests per generated trace. |
| `--jobs N` | Up to 8 CPUs | Maximum concurrent C++ runners; `1` runs sequentially. |
| `--list-traces` | Off | List matching trace names and exit. |
| `--pattern GLOB` | `*` | Scenario name filter; quote patterns, e.g. `'16_*'` (no `.txt` suffix). |
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

- `manifest.json`: generator/Python versions, sizes, configurations, worker limit and trace hashes.
- `results.csv`: all measurements and IDEAL hit rates.
- `<trace>.png`: hit rate (%) against actual budget / key-space size.
- `ranking.csv` and `ranking.png`: overall strategy scores.
- `ranking_by_ratio.csv` and `ranking_by_ratio.png`: scores across traces at
  each requested ratio.

Line plots select the highest mean curves and show identical curves once.
CSV files keep all strategies. The manifest hashes the exact generated input,
so separate runs can be checked for matching data. Trace files are not saved.
CSV rows and rankings keep a stable order regardless of worker completion order.
The worker limit defaults to `min(8, CPU count)` and cannot exceed the number of
budgets in a trace. Each concurrent runner has its own cache memory.

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
python3 benchmarks/generate_traces.py --trace 09_database_mixed \
  --universe-size 4096 --requests 8192 | \
  ./build/benchmarks/cache_bench_runner --trace - --capacities 64 192
```

| Argument | Description |
| --- | --- |
| `--trace PATH` | Required: `-` for stdin, or an external trace file. |
| `--capacities N [N ...]` | Required: 1–3 positive, nondecreasing integer capacities, from L1 downwards. |
| `--ideal` | Run only IDEAL; requires exactly one capacity. |
| `--exclusive` | Use an exclusive hierarchy (default). |
| `--inclusive` | Use an inclusive hierarchy. |
| `-h`, `--help` | Show help and exit. |

Input format: positive key-space size, positive request count, then integer
keys in `1..key-space size`, one value per line. Missing, extra or out-of-range
requests are rejected. `--ideal`, `--exclusive`, and `--inclusive` are mutually
exclusive. IDEAL precomputes next uses and maintains an ordered set of resident
next-use positions, taking O(requests × log(capacity + 1)) time after preprocessing.

## Research

Inclusive hierarchies are excluded from this study. In this implementation,
their total hit rate equals that of a standalone cache with the last level's
policy and capacity: upper-level entries are also present in the last level,
and every request updates that level.

The results use the 30 [synthetic workloads](benchmarks/README.md), with
**100,000 keys and 200,000 requests** per trace. Total cache budgets range from
1% to 30% of the key space in steps of 1 percentage point: 30 points per
trace (1,000 to 30,000 entries).

The initial decline in **Rating by ratio** comes from comparison with IDEAL.
Hit rates generally rise with capacity, but the score falls when IDEAL reduces
its misses faster proportionally. For example, from a 1% to a 5% budget,
ARC's mean hit rate across traces rises from **21.5% to 38.2%**, while its
score falls from **79.7 to 71.8**. The best overall strategies for one,
two, and three levels all reach their sampled minimum at **5%** (5,000 entries).
Across all 155 strategies, minima range from 5% to 11%.

### All strategies

Among all 155 configurations, single-level ARC has the highest overall score
(**75.283**), ahead of the best two-level (**74.001**) and three-level
(**73.723**) strategies. ARC also leads the per-ratio ranking at all 30 sampled ratios.

ARC is the best overall choice, but the best strategy for a specific case may
vary with the access pattern, cache budget, number of levels, and capacity split.

Each figure group below shows the overall ranking at the top left and the score
versus cache/data ratio at the top right. The two hit-rate plots below are just
examples, with no special significance attached to the choice of traces.
The rankings use all 30 traces.

<table>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Overall ranking</strong><br>
      <a href="assets/all/ranking.png"><img src="assets/all/ranking.png" alt="all: Overall ranking" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Rating by ratio</strong><br>
      <a href="assets/all/ranking_by_ratio.png"><img src="assets/all/ranking_by_ratio.png" alt="all: Rating by ratio" width="100%"></a>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Trace 17: Periodic hot-set load</strong><br>
      <a href="assets/all/17_periodic_hot_set_load.png"><img src="assets/all/17_periodic_hot_set_load.png" alt="all: Trace 17: Periodic hot-set load" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Trace 23: Variable-length sessions</strong><br>
      <a href="assets/all/23_variable_length_sessions.png"><img src="assets/all/23_variable_length_sessions.png" alt="all: Trace 23: Variable-length sessions" width="100%"></a>
    </td>
  </tr>
</table>

### Single-level algorithms

<table>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Overall ranking</strong><br>
      <a href="assets/one/ranking.png"><img src="assets/one/ranking.png" alt="one: Overall ranking" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Rating by ratio</strong><br>
      <a href="assets/one/ranking_by_ratio.png"><img src="assets/one/ranking_by_ratio.png" alt="one: Rating by ratio" width="100%"></a>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Trace 28: Overlapping working sets</strong><br>
      <a href="assets/one/28_overlapping_working_sets.png"><img src="assets/one/28_overlapping_working_sets.png" alt="one: Trace 28: Overlapping working sets" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Trace 12: Returning working sets</strong><br>
      <a href="assets/one/12_returning_working_sets.png"><img src="assets/one/12_returning_working_sets.png" alt="one: Trace 12: Returning working sets" width="100%"></a>
    </td>
  </tr>
</table>

### Two-level strategies

The 25 combinations use a **1:3** capacity split.

<table>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Overall ranking</strong><br>
      <a href="assets/two/ranking.png"><img src="assets/two/ranking.png" alt="two: Overall ranking" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Rating by ratio</strong><br>
      <a href="assets/two/ranking_by_ratio.png"><img src="assets/two/ranking_by_ratio.png" alt="two: Rating by ratio" width="100%"></a>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Trace 27: Gradual hot-set turnover</strong><br>
      <a href="assets/two/27_gradual_hot_set_turnover.png"><img src="assets/two/27_gradual_hot_set_turnover.png" alt="two: Trace 27: Gradual hot-set turnover" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Trace 12: Returning working sets</strong><br>
      <a href="assets/two/12_returning_working_sets.png"><img src="assets/two/12_returning_working_sets.png" alt="two: Trace 12: Returning working sets" width="100%"></a>
    </td>
  </tr>
</table>

### Three-level strategies

The 125 combinations use a **1:2:5** capacity split.

<table>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Overall ranking</strong><br>
      <a href="assets/three/ranking.png"><img src="assets/three/ranking.png" alt="three: Overall ranking" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Rating by ratio</strong><br>
      <a href="assets/three/ranking_by_ratio.png"><img src="assets/three/ranking_by_ratio.png" alt="three: Rating by ratio" width="100%"></a>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center" valign="top">
      <strong>Trace 12: Returning working sets</strong><br>
      <a href="assets/three/12_returning_working_sets.png"><img src="assets/three/12_returning_working_sets.png" alt="three: Trace 12: Returning working sets" width="100%"></a>
    </td>
    <td width="50%" align="center" valign="top">
      <strong>Trace 16: Delayed history reuse</strong><br>
      <a href="assets/three/16_delayed_history_reuse.png"><img src="assets/three/16_delayed_history_reuse.png" alt="three: Trace 16: Delayed history reuse" width="100%"></a>
    </td>
  </tr>
</table>
