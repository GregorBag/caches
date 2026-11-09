from collections import defaultdict
import csv
import hashlib
import json
import math
import platform
import statistics

from generate_traces import GENERATOR_VERSION


def geometric_ranking(trace_log_ratios):
    """Weight traces equally, then convert mean log miss ratios to a 0–100 score."""
    return sorted(((name, 100 * math.exp(statistics.fmean(values)))
                   for name, values in trace_log_ratios.items()),
                  key=lambda item: (-item[1], item[0]))


def plot_curves(plt, curves, x, top, path, *, ylabel="Hit rate (%)",
                xlabel="Total cache budget / universe size"):
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
    ax.set(xlabel=xlabel, ylabel=ylabel, ylim=(0, 101), title=path.stem)
    ax.grid(alpha=0.2)
    ax.legend(fontsize="small")
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def save_ranking(plt, trace_log_ratios, output, top):
    ranking = geometric_ranking(trace_log_ratios)
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
    bars = ax.barh([name for name, _ in best], [score for _, score in best])
    ax.bar_label(bars, fmt="%.3f", padding=3)
    ax.invert_yaxis()
    ax.set(xlabel="Geometric score (100 = IDEAL)", xlim=(0, 100), title="Best strategies")
    fig.tight_layout()
    fig.savefig(output / "ranking.png", dpi=150)
    plt.close(fig)
    for name, score in best:
        print(f"{name:20} {score:.3f}")


def save_ratio_ranking(plt, logs_by_ratio, output, top):
    ratios = sorted(logs_by_ratio)
    curves = defaultdict(list)
    with (output / "ranking_by_ratio.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["ratio", "strategy", "score"])
        for ratio in ratios:
            for name, score in geometric_ranking(logs_by_ratio[ratio]):
                curves[name].append(score)
                writer.writerow([float(ratio), name, score])
    curves["IDEAL"] = [100] * len(ratios)
    plot_curves(plt, curves, list(map(float, ratios)), top, output / "ranking_by_ratio.png",
                xlabel="Requested total cache budget / universe size",
                ylabel="Geometric score (100 = IDEAL)")


class BenchmarkReport:
    """Write results on the calling thread; Matplotlib is never used by workers."""

    def __init__(self, options, splits, budget_by_ratio):
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt

        self.plt = plt
        self.options = options
        self.budget_by_ratio = budget_by_ratio
        self.trace_logs = defaultdict(list)
        self.ratio_logs = defaultdict(lambda: defaultdict(list))
        self.manifest = dict(generator_version=GENERATOR_VERSION,
                             python_version=platform.python_version(),
                             universe_size=options.universe_size, requests=options.requests,
                             levels=options.levels, splits=splits, exclusive=options.exclusive,
                             jobs=options.jobs, step=options.step, max_ratio=options.max_ratio,
                             traces=[])

    def __enter__(self):
        self.options.output.mkdir(parents=True)
        self.stream = (self.options.output / 'results.csv').open('w', newline='')
        self.writer = csv.writer(self.stream)
        self.writer.writerow(['trace', 'universe_size', 'budget', 'ratio', 'strategy',
                              'capacities', 'requests', 'hits', 'hit_rate', 'ideal_hit_rate'])
        return self

    def __exit__(self, *_):
        self.stream.close()

    def record_input(self, name, data):
        self.manifest['traces'].append(dict(name=name, sha256=hashlib.sha256(data).hexdigest()))
        (self.options.output / 'manifest.json').write_text(json.dumps(self.manifest, indent=2) + '\n')

    def add_trace(self, name, results):
        curves, logs = defaultdict(list), defaultdict(list)
        by_budget = {result.budget: result for result in results}
        for result in results:
            self.writer.writerows(result.rows)
            for strategy, rate in result.rates.items():
                curves[strategy].append(rate)
            for strategy, value in result.log_ratios.items():
                logs[strategy].append(value)
        self.stream.flush()
        for strategy, values in logs.items():
            self.trace_logs[strategy].append(statistics.fmean(values))
        for ratio, budget in self.budget_by_ratio.items():
            for strategy, value in by_budget[budget].log_ratios.items():
                self.ratio_logs[ratio][strategy].append(value)
        plot_curves(self.plt, curves, [r.budget / self.options.universe_size for r in results],
                    self.options.top, self.options.output / f'{name}.png')

    def finish(self):
        save_ranking(self.plt, self.trace_logs, self.options.output, self.options.top)
        save_ratio_ranking(self.plt, self.ratio_logs, self.options.output, self.options.top)
