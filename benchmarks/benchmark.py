#!/usr/bin/env python3
"""Compare cache hit rates on file traces and save CSV + PNG plots."""
import argparse
import csv
import math
import statistics
import subprocess
from collections import defaultdict
from decimal import Decimal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def arguments():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--data-dir", type=Path, default=ROOT / "cache-tests",
                        help="Directory containing trace files")
    parser.add_argument("--pattern", default="*.txt", help="Trace filename glob")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/benchmarks/cache_bench_runner",
                        help="Path to the C++ benchmark runner")
    parser.add_argument("--levels", nargs="+", type=int, choices=[1, 2, 3], default=[1, 2, 3],
                        help="Level counts to benchmark")
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
    args = parser.parse_args()
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


def capacities(budget, weights):
    """Round down proportional sizes; put the remainder in the last level."""
    sizes = [budget * weight // sum(weights) for weight in weights]
    sizes[-1] += budget - sum(sizes)
    if min(sizes) == 0:
        raise ValueError(f"Budget {budget} is too small for split {weights}; "
                         "increase --step or --max-ratio, or select fewer levels")
    return sizes


def run_cache(runner, path, sizes, *, ideal=False):
    command = [str(runner), "--trace", str(path), "--capacities", *map(str, sizes)]
    if ideal:
        command.append("--ideal")
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{path.name}: {result.stderr.strip()}")
    return {name: (int(hits), int(count))
            for name, hits, count in map(str.split, result.stdout.splitlines())}


def read_traces(args, splits):
    traces = {}
    for path in sorted(p for p in args.data_dir.glob(args.pattern) if p.is_file()):
        with path.open() as stream:
            universe = int(stream.readline())
        if not 1 <= universe <= 2**31 - 1:
            raise ValueError(f"{path.name}: universe size must fit a positive 32-bit integer")
        minimum_budget = min(int(Decimal(str(args.step)) * universe + Decimal("0.5")),
                             int(Decimal(str(args.max_ratio)) * universe))
        for weights in splits:
            capacities(minimum_budget, weights)
        traces[path] = universe
    if not traces:
        raise ValueError("No trace files matched --pattern")
    return traces


def budgets_for(universe, step, max_ratio=1):
    """Include the upper endpoint without rounding any budget above the limit."""
    step, limit = Decimal(str(step)), Decimal(str(max_ratio))
    maximum = int(limit * universe)
    ratios = [step * i for i in range(1, math.ceil(limit / step))] + [limit]
    return sorted({min(maximum, int(r * universe + Decimal("0.5"))) for r in ratios})


def benchmark_trace(runner, path, universe, budgets, splits, writer):
    curves, scores = defaultdict(list), defaultdict(list)
    for budget in budgets:
        ideal_hits, count = run_cache(runner, path, [budget], ideal=True)["IDEAL"]
        ideal_rate = 100 * ideal_hits / count
        curves["IDEAL"].append(ideal_rate)
        rates = {}
        for weights in splits:
            sizes = capacities(budget, weights)
            for name, (hits, requests) in run_cache(runner, path, sizes).items():
                if requests != count or not 0 <= hits <= ideal_hits:
                    raise ValueError(f"{path.name}: inconsistent request count or hits exceed IDEAL")
                rate = 100 * hits / count
                rates[name] = rate
                curves[name].append(rate)
                writer.writerow([path.name, universe, budget, budget / universe, name,
                                 ":".join(map(str, sizes)), count, hits, rate, ideal_rate])
        for name, score in relative_scores(rates).items():
            scores[name].append(score)
    return curves, {name: statistics.fmean(values) for name, values in scores.items()}


def relative_scores(rates):
    center = statistics.median(rates.values())
    scale = max(1.0, 1.4826 * statistics.median(abs(r - center) for r in rates.values()))
    result = {}
    for name, rate in rates.items():
        z = max(-3, min(3, (rate - center) / scale))
        result[name] = z + 0.25 * max(z, 0)**2 - 0.5 * max(-z, 0)**2
    return result


def plot_curves(plt, curves, x, top, path):
    fig, ax = plt.subplots(figsize=(9, 5))
    seen = set()
    for name in sorted(curves, key=lambda n: (-statistics.fmean(curves[n]), n)):
        signature = tuple(curves[name])
        if name == "IDEAL" or signature in seen:
            continue
        ax.plot(x, curves[name], linewidth=1, label=name)
        seen.add(signature)
        if len(seen) == top:
            break
    ax.plot(x, curves["IDEAL"], "k--", linewidth=1, label="IDEAL(total budget)")
    ax.set(xlabel="Total cache budget / universe size", ylabel="Hit rate (%)",
           ylim=(0, 101), title=path.stem)
    ax.grid(alpha=0.2)
    ax.legend(fontsize="small")
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def save_ranking(plt, scores, output, top):
    ranking = sorted(((name, statistics.fmean(values)) for name, values in scores.items()),
                     key=lambda item: (-item[1], item[0]))
    with (output / "ranking.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["rank", "strategy", "score"])
        previous, rank = None, 0
        for index, (name, score) in enumerate(ranking, 1):
            if previous is None or not math.isclose(previous, score, rel_tol=0, abs_tol=1e-12):
                rank = index
            writer.writerow([rank, name, score])
            previous = score
    best = ranking[:top]
    fig, ax = plt.subplots(figsize=(9, max(3, len(best) * 0.4)))
    ax.barh([name for name, _ in best], [score for _, score in best])
    ax.invert_yaxis()
    ax.set(xlabel="Relative score (higher is better)", title="Best strategies")
    fig.tight_layout()
    fig.savefig(output / "ranking.png", dpi=150)
    plt.close(fig)
    for name, score in best:
        print(f"{name:20} {score:+.3f}")


def main():
    args = arguments()
    splits = [weights for n, weights in enumerate(([1], args.split_2, args.split_3), 1)
              if n in args.levels]
    traces = read_traces(args, splits)
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    args.output.mkdir(parents=True)  # Require a new directory to avoid mixing experiments.
    scores = defaultdict(list)
    print(f"{len(traces)} traces; levels {args.levels}; "
          f"ratio step {args.step}, maximum {args.max_ratio}", flush=True)
    with (args.output / "results.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["trace", "universe_size", "budget", "ratio", "strategy", "capacities", "requests",
                         "hits", "hit_rate", "ideal_hit_rate"])
        for path, universe in traces.items():
            budgets = budgets_for(universe, args.step, args.max_ratio)
            curves, trace_scores = benchmark_trace(args.runner, path, universe, budgets, splits, writer)
            for name, score in trace_scores.items():
                scores[name].append(score)
            stream.flush()
            plot_curves(plt, curves, [b / universe for b in budgets], args.top,
                        args.output / f"{path.stem}.png")
            print(f"{path.name}: done", flush=True)
    save_ranking(plt, scores, args.output, args.top)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, ImportError) as error:
        raise SystemExit(str(error)) from error
    except KeyboardInterrupt:
        raise SystemExit("Interrupted")
