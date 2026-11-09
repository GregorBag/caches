# Patterns

| Trace | Pattern |
| --- | --- |
| `01_uniform_random` | Uniform random keys. |
| `02_zipf_1_2` | Zipf popularity: weight `1/k^1.2`. |
| `03_normal_center` | Normal distribution: mean 2048.5, standard deviation 400. |
| `04_discrete_exponential` | Key weight halves every 256 keys. |
| `05_bimodal` | Two equal Gaussian peaks near 1024 and 3072, width 180. |
| `06_hot_set_80_percent` | 80% keys 1..128; 20% other keys. |
| `07_popularity_switch` | Switch from keys 1..256 to 2049..2304 halfway through. |
| `08_local_random_walk` | Steps of -2..2 around a ring; 2% random jumps. |
| `09_database_mixed` | 60% hot keys, 30% moving reads, 10% other keys. |
| `10_three_frequency_tiers` | 50% keys 1..128, 30% 129..1024, 20% 1025..4096. |
| `11_drifting_popularity` | Popularity peak moves from 512 to 3584; 25% background. |
| `12_returning_working_sets` | Revisit four sets: A-B-C-A-D-B-A-C; 20% background. |
| `13_short_group_bursts` | Bursts of 16..48 requests to eight keys; 20% background. |
| `14_multi_tenant_service` | Four separate key ranges with Zipf popularity; request shares 45/30/15/10%. |
| `15_recent_history_reuse` | 60% reuse from the last 64 requests; 40% background. |
| `16_delayed_history_reuse` | 35% older reuse, 15% recent reuse, 50% background. |
| `17_periodic_hot_set_load` | Hot-set probability cycles through 20/40/70/40%. |
| `18_reshuffled_zipf_popularity` | Shuffle Zipf ranks every 1024 requests; 25% background. |
| `19_lognormal_skewed` | Independent lognormal key weights, log-space sigma 2. |
| `20_pareto_popularity` | Independent Pareto key weights, shape 1.5. |
| `21_beta_two_tails` | Beta(0.7, 0.7): broad popularity near both ends of the key space. |
| `22_markov_regions` | Persistent random region selection; unequal region widths; 20% background. |
| `23_variable_length_sessions` | Sessions of 12–140 requests, 12–90 private pages, shared pages and background. |
| `24_interleaved_sessions` | Twelve concurrent sessions with independent random lifetimes and private pages. |
| `25_interrupted_range_reads` | Random-start ranges of 12–220 keys, mixed with hot reads and background. |
| `26_decaying_flash_crowds` | Irregular new hot groups with exponentially decaying request share; at least 35% Zipf background. |
| `27_gradual_hot_set_turnover` | A 300-key hot pool gradually replaces random members; 30% background. |
| `28_overlapping_working_sets` | Randomly timed switches between unequal, overlapping regions; 25% background. |
| `29_tenant_popularity_drift` | Five tenants with smoothly changing traffic shares and different Zipf exponents. |
| `30_irregular_hot_cold_phases` | Random phase lengths, hot-set sizes and hot shares; no fixed schedule. |