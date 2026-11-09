"""Generate the 30 benchmark workloads in memory, or stream one to stdout."""

import argparse
from array import array
from bisect import bisect_right
import hashlib
import io
import math
import random
import sys

GENERATOR_VERSION = 2
DEFAULT_UNIVERSE = 1_000_000
TRACE_NAMES = (
    'uniform_random', 'zipf_1_2', 'normal_center',
    'discrete_exponential', 'bimodal', 'hot_set_80_percent',
    'popularity_switch', 'local_random_walk', 'database_mixed',
    'three_frequency_tiers', 'drifting_popularity', 'returning_working_sets',
    'short_group_bursts', 'multi_tenant_service', 'recent_history_reuse',
    'delayed_history_reuse', 'periodic_hot_set_load', 'reshuffled_zipf_popularity',
    'lognormal_skewed', 'pareto_popularity', 'beta_two_tails',
    'markov_regions', 'variable_length_sessions', 'interleaved_sessions',
    'interrupted_range_reads', 'decaying_flash_crowds', 'gradual_hot_set_turnover',
    'overlapping_working_sets', 'tenant_popularity_drift', 'irregular_hot_cold_phases',
)
TRACES = tuple(f'{i:02}_{name}' for i, name in enumerate(TRACE_NAMES, 1))


class WeightedRanks:
    """One compact CDF per distribution; no request-sized sampling tables."""

    def __init__(self, rng, weights):
        self.rng = rng
        self.cumulative = array('d')
        total = 0.0
        for weight in weights:
            total += weight
            self.cumulative.append(total)
        self.total = total

    def draw(self):
        # Guard the endpoint against floating-point rounding.
        return min(len(self.cumulative),
                   bisect_right(self.cumulative, self.rng.random() * self.total) + 1)


def validate_sizes(universe, requests):
    if not 1 <= universe <= 2**31 - 1:
        raise ValueError('Universe size must fit a positive 32-bit integer')
    if not 1 <= requests <= 2**31 - 1:
        raise ValueError('Request count must fit a positive 32-bit integer')


def generate_trace(name, universe=DEFAULT_UNIVERSE, requests=None):
    """Return one compact array of requests; never read or write trace files.

    Spatial sizes scale from 4096 keys, temporal lengths from 8192 requests.
    Probabilities and the elementary +/-2 random-walk steps stay unchanged.
    """
    requests = 2 * universe if requests is None else requests
    validate_sizes(universe, requests)
    if name not in TRACES:
        raise ValueError(f'Unknown trace: {name}')
    kind = name[3:]
    # Numbering is presentation only; each scenario has an independent stream.
    digest = hashlib.sha256(f'{GENERATOR_VERSION}:{kind}'.encode()).digest()
    rng = random.Random(int.from_bytes(digest[:8], 'big'))
    key_range = range(1, universe + 1)
    result = array('I')
    space_scale, time_scale = universe / 4096, requests / 8192

    def space(value):
        return min(universe, max(1, round(value * space_scale)))

    def duration(value):
        return max(1, round(value * time_scale))

    def uniform():
        return rng.randint(1, universe)

    def ring(value):
        return int(value) % universe + 1

    def pool(size):
        return rng.sample(key_range, space(size))

    def normal(center, sigma):
        while True:
            key = round(rng.gauss(0.5 + (center - 0.5) * space_scale, sigma * space_scale))
            if 1 <= key <= universe:
                return key

    def recent(horizon):
        return result[-rng.randint(1, min(len(result), horizon))] if result else uniform()

    # Stationary distributions: compute only the selected distribution's CDF.
    weights = None
    if kind == 'zipf_1_2':
        weights = (k ** -1.2 for k in key_range)
    elif kind == 'discrete_exponential':
        weights = (2 ** (-(k - 1) / space(256)) for k in key_range)
    elif kind == 'lognormal_skewed':
        weights = (rng.lognormvariate(0, 2) for _ in key_range)
    elif kind == 'pareto_popularity':
        weights = (rng.paretovariate(1.5) for _ in key_range)
    if weights is not None:
        sampler = WeightedRanks(rng, weights)
        return array('I', (sampler.draw() for _ in range(requests)))

    if kind == 'uniform_random':
        return array('I', (uniform() for _ in range(requests)))
    if kind == 'normal_center':
        return array('I', (normal(2048.5, 400) for _ in range(requests)))
    if kind == 'bimodal':
        return array('I', (normal(rng.choice((1024, 3072)), 180) for _ in range(requests)))
    if kind == 'beta_two_tails':
        return array('I', (min(universe, 1 + int(rng.betavariate(0.7, 0.7) * universe))
                           for _ in range(requests)))

    # Initialize only state needed by this workload. Retain at most one trace.
    remaining, position, region = 0, uniform() - 1, 0
    hot, shared, sessions, working_sets = [], [], [], []
    hot_members = set()
    if kind in ('interrupted_range_reads', 'gradual_hot_set_turnover'):
        hot = pool(300)
        hot_members = set(hot)
    if kind in ('variable_length_sessions', 'interleaved_sessions'):
        shared = pool(180)

    def session():
        return [pool(rng.randint(12, 90)), rng.randint(duration(12), duration(140))]

    if kind in ('variable_length_sessions', 'interleaved_sessions'):
        sessions = [session() for _ in range(12 if kind == 'interleaved_sessions' else 1)]
    if kind == 'returning_working_sets':
        working_sets = [pool(512) for _ in range(4)]
    zipf = None
    labels = None
    if kind in ('reshuffled_zipf_popularity', 'decaying_flash_crowds'):
        zipf = WeightedRanks(rng, (k ** -0.95 for k in key_range))
    if kind == 'reshuffled_zipf_popularity':
        labels = array('I', key_range)
    tenants, tenant_samplers = [], []
    if kind in ('multi_tenant_service', 'tenant_popularity_drift'):
        count = min(universe, 4 if kind == 'multi_tenant_service' else 5)
        for j in range(count):
            start, end = j * universe // count, (j + 1) * universe // count
            tenants.append((start, end))
            tenant_samplers.append(WeightedRanks(rng, (k ** -(0.6 + j * 0.15)
                                                       for k in range(1, end - start + 1))))
        phases = [rng.uniform(0, 2 * math.pi) for _ in tenants]
    regions = ((0, 190), (520, 370), (1200, 730), (2600, 1100))
    order = (0, 1, 2, 0, 3, 1, 0, 2)
    for i in range(requests):
        u = rng.random()
        phase = min(7, 8 * i // requests)
        if kind == 'hot_set_80_percent':
            size = space(128)
            key = rng.randint(1, size) if u < 0.8 or size == universe else rng.randint(size + 1, universe)
        elif kind == 'popularity_switch':
            start = 0 if i < requests // 2 else round(2048 * space_scale)
            key = ring(start + rng.randrange(space(256)))
        elif kind == 'local_random_walk':
            position = uniform() - 1 if u < 0.02 else position + rng.randint(-2, 2)
            key = ring(position)
        elif kind == 'database_mixed':
            key = rng.randint(1, space(64)) if u < 0.6 else ring(position) if u < 0.9 else uniform()
            if 0.6 <= u < 0.9:
                position += 1
        elif kind == 'three_frequency_tiers':
            low, high = (0, space(128)) if u < 0.5 else (space(128), space(1024)) if u < 0.8 else (space(1024), universe)
            key = rng.randint(min(low + 1, universe), max(min(low + 1, universe), high))
        elif kind == 'drifting_popularity':
            center = 512 + 3072 * i / max(1, requests - 1)
            key = normal(center, 256) if u < 0.75 else uniform()
        elif kind == 'returning_working_sets':
            key = rng.choice(working_sets[order[phase]]) if u < 0.8 else uniform()
        elif kind == 'short_group_bursts':
            if remaining == 0:
                hot, remaining = pool(8), rng.randint(duration(16), duration(48))
            key = rng.choice(hot) if u < 0.8 else uniform()
            remaining -= 1
        elif kind == 'multi_tenant_service':
            tenant = rng.choices(range(len(tenants)), (45, 30, 15, 10)[:len(tenants)])[0]
            key = tenants[tenant][0] + tenant_samplers[tenant].draw()
        elif kind == 'recent_history_reuse':
            key = recent(duration(64)) if u < 0.6 else uniform()
        elif kind == 'delayed_history_reuse':
            if u < 0.35 and len(result) >= duration(256):
                key = result[-rng.randint(duration(256), min(len(result), duration(1024)))]
            else:
                key = recent(duration(64)) if 0.35 <= u < 0.5 else uniform()
        elif kind == 'periodic_hot_set_load':
            share = (0.2, 0.4, 0.7, 0.4)[i // duration(512) % 4]
            key = rng.randint(1, space(256)) if u < share else uniform()
        elif kind == 'reshuffled_zipf_popularity':
            if i == 0 or phase != previous_phase:
                rng.shuffle(labels)
            previous_phase = phase
            key = labels[zipf.draw() - 1] if u < 0.75 else uniform()
        elif kind == 'markov_regions':
            if rng.random() < min(1, 0.008 / time_scale):
                region = rng.randrange(len(regions))
            start, width = regions[region]
            key = ring(round(start * space_scale) + rng.randrange(space(width))) if u < 0.8 else uniform()
        elif kind in ('variable_length_sessions', 'interleaved_sessions'):
            slot = rng.randrange(len(sessions))
            if sessions[slot][1] == 0:
                sessions[slot] = session()
            pages, _ = sessions[slot]
            key = rng.choice(shared) if u < 0.25 else rng.choice(pages) if u < 0.8 else uniform()
            sessions[slot][1] -= 1
        elif kind == 'interrupted_range_reads':
            if remaining == 0:
                position = uniform() - 1
                remaining = rng.randint(duration(12), duration(220))
            key = ring(position) if u < 0.6 else rng.choice(hot) if u < 0.85 else uniform()
            if u < 0.6:
                position += 1
                remaining -= 1
        elif kind == 'decaying_flash_crowds':
            if remaining == 0:
                hot = pool(rng.randint(80, 500))
                remaining = rng.randint(duration(400), duration(1600))
                decay, crowd_age = rng.uniform(duration(180), duration(650)), 0
            key = rng.choice(hot) if u < 0.65 * math.exp(-crowd_age / decay) else zipf.draw()
            remaining -= 1
            crowd_age += 1
        elif kind == 'gradual_hot_set_turnover':
            if rng.random() < 0.08 and len(hot) < universe:
                replacement = uniform()
                while replacement in hot_members:
                    replacement = uniform()
                slot = rng.randrange(len(hot))
                hot_members.remove(hot[slot])
                hot[slot] = replacement
                hot_members.add(replacement)
            key = rng.choice(hot) if u < 0.7 else uniform()
        elif kind == 'overlapping_working_sets':
            if remaining == 0:
                position = round(rng.choice((200, 550, 950, 1400, 2200, 2800)) * space_scale)
                width = space(rng.randint(300, 1100))
                remaining = rng.randint(duration(150), duration(950))
            key = ring(position + rng.randrange(width)) if u < 0.75 else uniform()
            remaining -= 1
        elif kind == 'tenant_popularity_drift':
            weights = [1.2 + math.sin(i / duration(850 + 250 * j) + phases[j]) for j in range(len(tenants))]
            tenant = rng.choices(range(len(tenants)), weights)[0]
            key = tenants[tenant][0] + tenant_samplers[tenant].draw() if u < 0.85 else uniform()
        elif kind == 'irregular_hot_cold_phases':
            if remaining == 0:
                hot, share = pool(rng.randint(80, 1000)), rng.uniform(0.25, 0.85)
                remaining = rng.randint(duration(120), duration(1100))
            key = rng.choice(hot) if u < share else uniform()
            remaining -= 1
        else:
            raise ValueError(f'Unimplemented trace: {name}')
        result.append(key)
    return result


def write_trace(stream, universe, keys):
    """Write the runner's text protocol in chunks without a giant string list."""
    stream.write(f'{universe}\n{len(keys)}\n'.encode())
    for offset in range(0, len(keys), 16384):
        stream.write(('\n'.join(map(str, keys[offset:offset + 16384])) + '\n').encode())


def trace_bytes(universe, keys):
    stream = io.BytesIO()
    write_trace(stream, universe, keys)
    return stream.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trace', choices=TRACES, required=True)
    parser.add_argument('--universe-size', type=int, default=DEFAULT_UNIVERSE)
    parser.add_argument('--requests', type=int, help='Request count (default: twice the universe size)')
    args = parser.parse_args()
    try:
        keys = generate_trace(args.trace, args.universe_size, args.requests)
        write_trace(sys.stdout.buffer, args.universe_size, keys)
    except ValueError as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
