from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import math
import subprocess
from threading import Lock

from benchmark_config import capacities


@dataclass
class BudgetResult:
    budget: int
    rates: dict
    log_ratios: dict
    rows: list


class Runner:
    """Own child processes so an error or Ctrl+C stops outstanding calculations."""

    def __init__(self, executable):
        self.executable = executable
        self._lock = Lock()
        self._active = set()
        self._closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        with self._lock:
            self._closed = True
            for process in self._active:
                if process.poll() is None:
                    process.terminate()

    def run(self, name, data, sizes, *, ideal=False, exclusive=True):
        command = [str(self.executable), '--trace', '-', '--capacities', *map(str, sizes)]
        command.append('--ideal' if ideal else '--exclusive' if exclusive else '--inclusive')
        with self._lock:
            if self._closed:
                raise RuntimeError('Benchmark cancelled')
            process = subprocess.Popen(command, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self._active.add(process)
        try:
            stdout, stderr = process.communicate(input=data)
        except BaseException:
            process.kill()
            process.wait()
            raise
        finally:
            with self._lock:
                self._active.remove(process)
        if process.returncode:
            raise RuntimeError(f'{name}: {stderr.decode(errors="replace").strip()} '
                               f'(runner exit {process.returncode})')
        result = {strategy: (int(hits), int(count))
                  for strategy, hits, count in map(str.split, stdout.decode().splitlines())}
        if len(result) != (1 if ideal else 5 ** len(sizes)) or (ideal and 'IDEAL' not in result):
            raise ValueError(f'{name}: incomplete runner output')
        return result


def benchmark_budget(runner, name, data, universe, requests, budget, splits, exclusive):
    """Compute one budget without touching report files or shared accumulators."""
    ideal_hits, count = runner.run(name, data, [budget], ideal=True)['IDEAL']
    if count != requests or not 0 <= ideal_hits < count:
        raise ValueError(f'{name}: invalid IDEAL hit or request count')
    ideal_rate = 100 * ideal_hits / count
    result = BudgetResult(budget, {'IDEAL': ideal_rate}, {}, [])
    for weights in splits:
        sizes = capacities(budget, weights)
        measurements = runner.run(name, data, sizes, exclusive=exclusive)
        for strategy, (hits, measured_count) in measurements.items():
            if measured_count != count or not 0 <= hits <= ideal_hits:
                raise ValueError(f'{name}: inconsistent request count or hits exceed IDEAL')
            rate = 100 * hits / count
            result.rates[strategy] = rate
            result.log_ratios[strategy] = math.log((count - ideal_hits) / (count - hits))
            result.rows.append([name, universe, budget, budget / universe, strategy,
                                ':'.join(map(str, sizes)), count, hits, rate, ideal_rate])
    return result


def benchmark_trace(executable, name, data, universe, requests, budgets, splits, *, jobs=1, exclusive=True):
    """Keep the payload shared; return results ordered by budget, not finish time."""
    # Exit Runner first on failure, terminating children before joining threads.
    with ThreadPoolExecutor(max_workers=min(jobs, len(budgets))) as pool, Runner(executable) as runner:
        futures = [pool.submit(benchmark_budget, runner, name, data, universe, requests,
                               budget, splits, exclusive) for budget in budgets]
        results = [future.result() for future in as_completed(futures)]
    return sorted(results, key=lambda result: result.budget)
