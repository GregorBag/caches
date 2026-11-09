import argparse
from decimal import Decimal
import fnmatch
import math
import os
from pathlib import Path

from generate_traces import DEFAULT_UNIVERSE, TRACES, validate_sizes

ROOT = Path(__file__).resolve().parents[1]


def arguments(argv=None):
    parser = argparse.ArgumentParser(description="Compare cache hit rates on generated traces.",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--pattern", default="*", help="Generated trace name glob, e.g. '10_*'")
    parser.add_argument("--universe-size", type=int, default=DEFAULT_UNIVERSE,
                        help="Number of possible keys")
    parser.add_argument("--requests", type=int, help="Requests per trace; None means 2 * universe size")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1),
                        help="Maximum concurrent C++ runners; 1 runs sequentially")
    parser.add_argument("--list-traces", action="store_true", help="List matching trace names and exit")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/benchmarks/cache_bench_runner",
                        help="Path to the C++ benchmark runner")
    parser.add_argument("--levels", nargs="+", type=int, choices=[1, 2, 3], default=[1, 2, 3],
                        help="Level counts to benchmark")
    hierarchy = parser.add_mutually_exclusive_group()
    hierarchy.add_argument("--exclusive", action="store_true", default=True,
                           help="Use an exclusive hierarchy")
    hierarchy.add_argument("--inclusive", dest="exclusive", action="store_false",
                           default=argparse.SUPPRESS,
                           help="Use an inclusive hierarchy instead of exclusive")
    parser.add_argument("--split-2", nargs=2, type=int, default=[1, 3],
                        help="Positive nondecreasing weights for two cache levels")
    parser.add_argument("--split-3", nargs=3, type=int, default=[1, 2, 5],
                        help="Positive nondecreasing weights for three cache levels")
    parser.add_argument("--step", type=float, default=0.05,
                        help="Cache/data ratio step, greater than 0 and at most 1")
    parser.add_argument("--max-ratio", type=float, default=1,
                        help="Maximum total cache budget / universe size, greater than 0 and at most 1")
    parser.add_argument("--top", type=int, default=10,
                        help="Maximum strategy curves and ranking bars to plot; must be positive")
    parser.add_argument("--output", type=Path, default=ROOT / "results/benchmark",
                        help="New directory for CSV results and PNG plots")
    args = parser.parse_args(argv)
    if args.requests is None:
        args.requests = 2 * args.universe_size
    try:
        validate_sizes(args.universe_size, args.requests)
    except ValueError as error:
        parser.error(str(error))
    if args.jobs <= 0:
        parser.error("--jobs must be positive")
    if args.top <= 0:
        parser.error("--top must be positive")
    if not math.isfinite(args.step) or not 0 < args.step <= 1:
        parser.error("--step must be greater than 0 and at most 1")
    if not math.isfinite(args.max_ratio) or not 0 < args.max_ratio <= 1:
        parser.error("--max-ratio must be greater than 0 and at most 1")
    for weights in (args.split_2, args.split_3):
        if min(weights) <= 0 or weights != sorted(weights):
            parser.error("Split weights must be positive and nondecreasing")
    args.levels = sorted(set(args.levels))
    args.runner = args.runner.resolve()
    return args


def select_traces(pattern):
    traces = [name for name in TRACES if fnmatch.fnmatchcase(name, pattern)]
    if not traces:
        raise ValueError(f"No traces matched --pattern {pattern!r}")
    return traces


def capacities(budget, weights):
    """Round down proportional sizes; put the remainder in the last level."""
    sizes = [budget * weight // sum(weights) for weight in weights]
    sizes[-1] += budget - sum(sizes)
    if min(sizes) == 0:
        raise ValueError(f"Budget {budget} is too small for split {weights}; "
                         "increase --step or --max-ratio, or select fewer levels")
    return sizes


def budgets_by_ratio(universe, step, max_ratio=1):
    """Map the common ratio grid to rounded budgets capped at the upper limit."""
    step, limit = Decimal(str(step)), Decimal(str(max_ratio))
    maximum = int(limit * universe)
    ratios = [step * i for i in range(1, math.ceil(limit / step))] + [limit]
    return {r: min(maximum, int(r * universe + Decimal("0.5"))) for r in ratios}

