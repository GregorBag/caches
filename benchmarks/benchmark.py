"""Compare cache hit rates on generated traces and save CSV + PNG plots."""

from benchmark_config import arguments, budgets_by_ratio, capacities, select_traces
from benchmark_report import BenchmarkReport
from benchmark_runner import benchmark_trace
from generate_traces import generate_trace, trace_bytes


def run_benchmark(options, traces):
    splits = [weights for n, weights in enumerate(([1], options.split_2, options.split_3), 1)
              if n in options.levels]
    budget_by_ratio = budgets_by_ratio(options.universe_size, options.step, options.max_ratio)
    budgets = sorted(set(budget_by_ratio.values()))
    for weights in splits:
        capacities(budgets[0], weights)
    jobs = min(options.jobs, len(budgets))
    print(f'{len(traces)} traces; {options.universe_size} keys; {options.requests} requests; '
          f'levels {options.levels}; {"exclusive" if options.exclusive else "inclusive"} hierarchy; '
          f'{jobs} parallel budgets; ratio step {options.step}, maximum {options.max_ratio}', flush=True)
    with BenchmarkReport(options, splits, budget_by_ratio) as report:
        for name in traces:
            print(f'{name}: generating', flush=True)
            data = trace_bytes(options.universe_size,
                               generate_trace(name, options.universe_size, options.requests))
            report.record_input(name, data)
            results = benchmark_trace(options.runner, name, data, options.universe_size,
                                      options.requests, budgets, splits,
                                      jobs=jobs, exclusive=options.exclusive)
            del data
            report.add_trace(name, results)
            print(f'{name}: done', flush=True)
        report.finish()


def main():
    options = arguments()
    traces = select_traces(options.pattern)
    if options.list_traces:
        print('\n'.join(traces))
    else:
        run_benchmark(options, traces)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, ImportError) as error:
        raise SystemExit(str(error)) from error
    except KeyboardInterrupt:
        raise SystemExit('Interrupted')
