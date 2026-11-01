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

GCC may print `note: unable to represent further imported source locations`
when importing C++ modules. This concerns source-location information in compiler
diagnostics; the note alone is not a build failure. If the build finishes with
exit code 0 and links the executable, no source changes are needed for this note.

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

Check Python benchmark budget generation and argument validation (no Matplotlib
installation needed):

```bash
python3 -m unittest discover -s benchmarks -p 'test_*.py'
```

## Hit-rate benchmark

Build the C++ runner from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build --target cache_bench_runner -j2
```

The Python frontend additionally needs Python 3 and Matplotlib from
`benchmarks/requirements.txt`. `.venv` is a local directory containing an isolated
Python environment and its installed packages. It keeps these dependencies
separate from system Python. It is optional: another Python environment with
the required Matplotlib version also works. The C++ library, tests, and runner
do not need Python or `.venv` to run.

Set up the Python environment **once**. On Debian/Ubuntu, install venv support
before creating it:

```bash
sudo apt update
sudo apt install python3-venv
python3 -m venv .venv && .venv/bin/python -m pip install -r benchmarks/requirements.txt
```

If `venv` reports that `ensurepip` is unavailable, install the venv package for
the Python version used to create it: for Python 3.12,
`sudo apt install python3.12-venv`. Then repeat the environment creation and
dependency installation command above; an existing `.venv` can be reused.

Check the environment:

```bash
.venv/bin/python -m pip check
.venv/bin/python -c "import matplotlib; print(matplotlib.__version__)"
```

`Requirement already satisfied` means a dependency is already installed; it is
not an error. Once these checks pass, use the existing environment for subsequent
runs. There is no need to recreate it or install dependencies each time.

Run from the repository root (list arguments are **space-separated**).
Calling `.venv/bin/python` directly selects the environment; activation with
`source .venv/bin/activate` is optional
([Python documentation](https://docs.python.org/3/library/venv.html#how-venvs-work)):

```bash
.venv/bin/python benchmarks/benchmark.py \
  --levels 1 2 3 \
  --split-2 1 3 --split-3 1 2 5 \
  --step 0.05 --max-ratio 0.3 \
  --output results/comparison
```

Python benchmark arguments:

| Argument | Default | Description |
| --- | --- | --- |
| `--data-dir PATH` | `cache-tests` | Directory containing input traces. |
| `--pattern GLOB` | `*.txt` | Filename pattern inside `--data-dir`; quote it, e.g. `--pattern '18_*'` for one trace. |
| `--runner PATH` | `build/benchmarks/cache_bench_runner` | Compiled C++ runner to execute. |
| `--levels N [N ...]` | `1 2 3` | Level counts to measure; each value must be 1, 2, or 3. Duplicates are removed. All algorithm combinations run for each selected count. |
| `--split-2 W1 W2` | `1 3` | Relative shares of the total budget for two levels, from L1 downwards; positive nondecreasing integers. |
| `--split-3 W1 W2 W3` | `1 2 5` | Relative shares for three levels; positive nondecreasing integers. |
| `--step RATIO` | `0.05` | Spacing between requested cache/data ratios; a finite number greater than 0 and at most 1. |
| `--max-ratio RATIO` | `1` | Upper limit on total cache budget divided by key-space size; a finite number greater than 0 and at most 1. `0.3` limits measurements to 30%. |
| `--top N` | `10` | Positive maximum number of distinct strategy curves per trace and ranking bars. IDEAL is drawn additionally; CSV files keep all strategies. |
| `--output PATH` | `results/benchmark` | New directory for CSV files and PNG plots; must not already exist. Parent directories are created automatically. |
| `-h`, `--help` | — | Show argument descriptions and defaults, then exit. |

Default paths are relative to the repository root; paths supplied explicitly
are relative to the current working directory unless absolute.

Each trace starts with its positive key-space size on the first line, then a
positive request count, followed by that many integer keys. For example:

```text
1024
4
512
512
1
512
```

All five algorithms (LRU, LFU, ARC, LIRS, and 2Q) always participate.
Python calls C++ once per selected level count; the runner enumerates every
ordered algorithm combination **with repetitions**, reading the trace once per
invocation. With 1–3 levels there are 155 configurations per trace/budget.
Each configuration starts empty and counts only the
boolean result of `lookup_update()`. IDEAL runs once per trace/budget using the
entire budget. The current inclusive hierarchy's total hit rate depends on its
last level; all combinations are nevertheless executed.

`--step` sets the cache/data ratio spacing, while `--max-ratio` sets the upper
limit. The defaults (`0.05` and `1`) give 20 requested points. Ratios increase
by the step up to the limit, and the upper endpoint is always included:
`--step 0.3` gives `0.3, 0.6, 0.9, 1`;
`--step 0.1 --max-ratio 0.3` gives `0.1, 0.2, 0.3`;
`--step 0.2 --max-ratio 0.3` gives `0.2, 0.3`.
If the step exceeds the limit, only the upper endpoint is measured.

The total budget is `ratio * universe_size`, using each file's first line,
rounded to the nearest integer (halves up), then capped at
`floor(max_ratio * universe_size)` so the actual budget never exceeds the limit.
For 32 keys and `--max-ratio 0.3`, the maximum budget is 9 entries, not 10.
Repeated rounded budgets are merged.
No `--universe-size` argument is needed. 1024 in the supplied traces is the key-space size,
not necessarily the number of different keys observed in a file. Sizes are in
resident entries, excluding policy metadata. Each level's proportional share
is rounded down; the remainder goes to the last level. Zero-sized levels are
rejected: increase `--step` or `--max-ratio`, or select fewer levels.
Splits must be positive and nondecreasing.
For budget 256 the default sizes are `256`, `64:192`, and `32:64:160`.

The output directory must be new. It receives `results.csv` (all measurements,
key-space size, actual cache sizes and hit rates), one PNG per trace,
`ranking.csv`, and `ranking.png`.
Curves show top mean-hit-rate strategies; identical curves are shown once.
CSV keeps every configuration. Rates are percentages, and the X axis uses the
actual budget divided by that file's key-space size.

The overall ranking rewards positive outliers and penalizes negative ones.
For each trace/budget, using hit rates in percentage points:

```text
center = median(rates)
scale  = max(1, 1.4826 * median(abs(rate - center)))
z      = clamp((rate - center) / scale, -3, 3)
score  = z + 0.25 * max(z, 0)^2 - 0.5 * max(-z, 0)^2
```

Scores are averaged over each trace's budget points, then equally over traces; denser
sampling therefore gives a region more weight. All selected configurations
participate in the reference distribution, including equivalent ones. IDEAL is
excluded. Equal scores receive equal ranks; plotted top-K ties are selected
alphabetically. This is a configurable experiment's relative ranking, not a
claim of a universally optimal strategy.

The runner is also usable directly; it prints one `strategy hits requests` line
per combination (5, 25, or 125 lines for 1, 2, or 3 capacities).
With `--ideal`, it prints a single `IDEAL hits requests` line:

```bash
./build/benchmarks/cache_bench_runner --trace cache-tests/18_database_mixed.txt \
  --capacities 64 192
./build/benchmarks/cache_bench_runner --trace cache-tests/18_database_mixed.txt \
  --ideal --capacities 256
```

C++ runner arguments:

| Argument | Default | Description |
| --- | --- | --- |
| `--trace PATH` | Required | Existing trace file in the format described above. |
| `--capacities N [N ...]` | Required | 1–3 positive integer capacities, from L1 downwards, in nondecreasing order. Their count selects the number of levels. With `--ideal`, supply exactly one capacity. |
| `--ideal` | Off | Run only IDEAL with the supplied capacity instead of all algorithm combinations. |
| `-h`, `--help` | — | Show help and exit. |

The runner accepts absolute capacities. `--step`, `--max-ratio`, and the other
experiment and plotting options belong to the Python frontend.

Only Matplotlib is needed beyond the Python standard library. There are no
subcommands, resume/checkpoints, configuration files, parallel workers or
report-only mode. The simple Python → C++ invocation follows the approach used
in [nhwc](https://github.com/lon3ry/nhwc/tree/main/bench/source).
