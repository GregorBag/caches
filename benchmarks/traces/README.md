# Cache traces

- 50 synthetic traces, numbered `01`–`50`.
- Line 1: number of keys. Line 2: number of requests.
- Remaining lines: one key per request, from 1 to the number of keys.

| File | Pattern |
| --- | --- |
| `01_uniform_random.txt` | Uniform random keys. |
| `02_zipf_1_2.txt` | Zipf popularity: weight `1/k^1.2`. |
| `03_normal_center.txt` | Normal distribution: mean 2048.5, standard deviation 400. |
| `04_discrete_exponential.txt` | Key weight halves every 256 keys. |
| `05_bimodal.txt` | Two equal peaks near 1024 and 3072. |
| `06_hot_set_80_percent.txt` | 80% keys 1..128; 20% other keys. |
| `07_popularity_switch.txt` | Switch from keys 1..256 to 2049..2304 halfway through. |
| `08_rotating_working_set.txt` | A 512-key window shifts by 384 every 1024 requests. |
| `09_local_random_walk.txt` | Steps of -2..2 around a ring; 2% random jumps. |
| `10_database_mixed.txt` | 60% hot keys, 30% moving reads, 10% other keys. |
| `11_three_frequency_tiers.txt` | 50% keys 1..128, 30% 129..1024, 20% 1025..4096. |
| `12_alternating_hot_regions.txt` | Switch hot regions every 512 requests; 30% background. |
| `13_drifting_popularity.txt` | Popularity peak moves from 512 to 3584; 25% background. |
| `14_changing_working_sets.txt` | New 512-key set every 1024 requests; 25% background. |
| `15_returning_working_sets.txt` | Revisit four sets: A-B-C-A-D-B-A-C; 20% background. |
| `16_flash_crowd.txt` | Temporary popularity spike for a new set, then return. |
| `17_short_group_bursts.txt` | Bursts of 16..48 requests to eight keys; 20% background. |
| `18_web_sessions_with_noise.txt` | 35% shared pages, 45% session pages, 20% background. |
| `19_multi_tenant_service.txt` | Four separate key ranges; request shares 45/30/15/10%. |
| `20_recent_history_reuse.txt` | 60% reuse from the last 64 requests; 40% background. |
| `21_delayed_history_reuse.txt` | 35% older reuse, 15% recent reuse, 50% background. |
| `22_periodic_hot_set_load.txt` | Hot-set probability cycles through 20/40/70/40%. |
| `23_background_random_reads.txt` | Repeat blocks of 192 hot reads and 64 other reads. |
| `24_reshuffled_zipf_popularity.txt` | Shuffle Zipf ranks every 1024 requests; 25% background. |
| `25_zipf_0_7.txt` | Zipf ranks with exponent 0.7; randomly permuted key labels. |
| `26_zipf_0_95.txt` | Zipf ranks with exponent 0.95; randomly permuted key labels. |
| `27_zipf_1_05.txt` | Zipf ranks with exponent 1.05; randomly permuted key labels. |
| `28_zipf_mandelbrot.txt` | Rank weight (rank + 40)^-1.25; a broad popularity head. |
| `29_lognormal_moderate.txt` | Independent lognormal key weights, log-space sigma 1. |
| `30_lognormal_skewed.txt` | Independent lognormal key weights, log-space sigma 2. |
| `31_pareto_popularity.txt` | Independent Pareto key weights, shape 1.5. |
| `32_stretched_exponential.txt` | Rank weight exp(-(rank / 350)^0.6). |
| `33_triangular_popularity.txt` | Triangular key distribution with mode at 35% of the key space. |
| `34_beta_two_tails.txt` | Beta(0.7, 0.7): broad popularity near both ends of the key space. |
| `35_beta_skewed.txt` | Beta(2, 5): asymmetric, smoothly varying popularity. |
| `36_trimodal_popularity.txt` | Gaussian peaks at 650/2000/3400, widths 180/420/250, shares 20/50/30%. |
| `37_markov_regions.txt` | Persistent random region selection; unequal region widths; 20% background. |
| `38_variable_length_sessions.txt` | Sessions of 12–140 requests, 12–90 private pages, shared pages and background. |
| `39_interleaved_sessions.txt` | Twelve concurrent sessions with independent random lifetimes and private pages. |
| `40_interrupted_range_reads.txt` | Random-start ranges of 12–220 keys, mixed with hot reads and background. |
| `41_variable_stride_reads.txt` | Random-start runs of 8–100 reads with strides 1/2/3/5/8; 40% noise. |
| `42_decaying_flash_crowds.txt` | Irregular new hot groups with exponentially decaying request share; at least 35% Zipf background. |
| `43_gradual_hot_set_turnover.txt` | A 300-key hot pool gradually replaces random members; 30% background. |
| `44_overlapping_working_sets.txt` | Randomly timed switches between unequal, overlapping regions; 25% background. |
| `45_variable_recent_history.txt` | 55% historical reuse with a fresh horizon of 24–700 per request. |
| `46_lognormal_reuse_delays.txt` | 50% reuse attempts at lognormal delays (median 180, sigma 1); uniform fallback and background. |
| `47_mixed_read_streams.txt` | Interleave Zipf point reads, variable range scans, recent reuse and uniform reads. |
| `48_tenant_popularity_drift.txt` | Five tenants with smoothly changing traffic shares and different Zipf exponents. |
| `49_correlated_random_walk.txt` | Random-length local walks with varying Gaussian step scales; 12% random jumps. |
| `50_irregular_hot_cold_phases.txt` | Random phase lengths, hot-set sizes and hot shares; no fixed schedule. |