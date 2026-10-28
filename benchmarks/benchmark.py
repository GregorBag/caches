#!/usr/bin/env python3
"""Compare cache hit rates on file traces and save CSV + PNG plots."""
import argparse
import csv
import itertools
import math
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALGORITHMS = ["lru", "lfu", "arc", "lirs", "2q"]


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=ROOT / "cache-tests")
    parser.add_argument("--pattern", default="*.txt", help="Trace filename glob")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/benchmarks/cache_bench_runner")
    parser.add_argument("--levels", nargs="+", type=int, choices=[1, 2, 3], default=[1, 2, 3])
    parser.add_argument("--algorithms", nargs="+", choices=ALGORITHMS, default=ALGORITHMS)
    parser.add_argument("--split-2", nargs=2, type=int, default=[1, 3])
    parser.add_argument("--split-3", nargs=3, type=int, default=[1, 2, 5])
    parser.add_argument("--universe-size", type=int, default=1024)
    parser.add_argument("--ratios", nargs="+", type=float, default=[0.03125, 0.0625, 0.125, 0.25, 0.5, 1])
    parser.add_argument("--top", type=int, default=10)
    parser.add_argument("--output", type=Path, default=ROOT / "results/benchmark")
    args = parser.parse_args()
    if args.universe_size <= 0 or args.top <= 0:
        parser.error("--universe-size and --top must be positive")
    if any(not math.isfinite(r) or r <= 0 or r * args.universe_size > 2**31 - 1 for r in args.ratios):
        parser.error("Ratios must give positive finite budgets fitting a 32-bit integer")
    for weights in (args.split_2, args.split_3):
        if min(weights) <= 0 or weights != sorted(weights):
            parser.error("Split weights must be positive and nondecreasing")
    args.levels = sorted(set(args.levels))
    args.algorithms = list(dict.fromkeys(args.algorithms))
    return args


def capacities(budget, weights):
    """Round down proportional sizes; put the remainder in the last level."""
    sizes = [budget * weight // sum(weights) for weight in weights]
    sizes[-1] += budget - sum(sizes)
    if min(sizes) == 0:
        raise ValueError(f"Budget {budget} is too small for split {weights}; increase --ratios")
    return sizes


def run_cache(runner, path, sizes, algorithms=()):
    command = [str(runner), "--trace", str(path), "--capacities", *map(str, sizes)]
    command += ["--algorithms", *algorithms] if algorithms else ["--ideal"]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{path.name}: {result.stderr.strip()}")
    hits, count = map(int, result.stdout.split())
    return hits, count


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
        ax.plot(x, curves[name], marker="o", markersize=3, label=name)
        seen.add(signature)
        if len(seen) == top:
            break
    ax.plot(x, curves["IDEAL"], "k--", label="IDEAL(total budget)")
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
    paths = sorted(p for p in args.data_dir.glob(args.pattern) if p.is_file())
    if not paths:
        raise ValueError("No trace files matched --pattern")
    budgets = sorted({math.floor(r * args.universe_size + 0.5) for r in args.ratios})
    splits = {1: [1], 2: args.split_2, 3: args.split_3}
    layouts = {(b, n): capacities(b, splits[n]) for b in budgets for n in args.levels}
    combinations = [names for n in args.levels for names in itertools.product(args.algorithms, repeat=n)]
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    args.output.mkdir(parents=True)  # Require a new directory to avoid mixing experiments.
    scores = defaultdict(list)
    print(f"{len(paths)} traces × {len(budgets)} budgets × {len(combinations)} strategies", flush=True)
    with (args.output / "results.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["trace", "budget", "ratio", "strategy", "capacities", "requests",
                         "hits", "hit_rate", "ideal_hit_rate"])
        for path in paths:
            curves = defaultdict(list)
            for budget in budgets:
                ideal_hits, count = run_cache(args.runner.resolve(), path, [budget])
                ideal_rate = 100 * ideal_hits / count
                curves["IDEAL"].append(ideal_rate)
                rates = {}
                for names in combinations:
                    sizes = layouts[budget, len(names)]
                    hits, requests = run_cache(args.runner.resolve(), path, sizes, names)
                    if requests != count or not 0 <= hits <= ideal_hits:
                        raise ValueError(f"{path.name}: inconsistent request count or hits exceed IDEAL")
                    name = ">".join(names)
                    rate = 100 * hits / count
                    rates[name] = rate
                    curves[name].append(rate)
                    writer.writerow([path.name, budget, budget / args.universe_size, name,
                                     ":".join(map(str, sizes)), count, hits, rate, ideal_rate])
                for name, score in relative_scores(rates).items():
                    scores[name].append(score)
            stream.flush()
            plot_curves(plt, curves, [b / args.universe_size for b in budgets], args.top,
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
