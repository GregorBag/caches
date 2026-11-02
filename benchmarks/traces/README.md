# Cache traces

- 30 synthetic traces, numbered `01`–`30`.
- Line 1: number of keys. Line 2: number of requests.
- Remaining lines: one key per request, from 1 to the number of keys.

| File | Pattern |
| --- | --- |
| `01_uniform_random.txt` | Uniform random keys. |
| `02_zipf_1_2.txt` | Zipf popularity: weight `1/k^1.2`. |
| `03_normal_center.txt` | Normal distribution: mean 512.5, standard deviation 100. |
| `04_discrete_exponential.txt` | Key weight halves every 64 keys. |
| `05_bimodal.txt` | Two equal peaks near 256 and 768. |
| `06_one_hot_90_percent.txt` | 90% key 1; 10% other keys. |
| `07_hot_set_80_percent.txt` | 80% keys 1..32; 20% other keys. |
| `08_sequential_forward.txt` | Eight scans from 1 to 1024. |
| `09_shuffled_full_scans.txt` | Eight full scans, each in a new random order. |
| `10_cycle_257.txt` | Repeating cycle of 257 keys. |
| `11_hot_set_and_scans.txt` | Alternate 512 hot requests and a 512-key scan. |
| `12_popularity_switch.txt` | Switch from keys 1..64 to 513..576 halfway through. |
| `13_rotating_working_set.txt` | A 128-key window shifts by 96 every 1024 requests. |
| `14_web_sessions.txt` | 128 sessions: shared page, session pages, then new pages. |
| `15_local_random_walk.txt` | Steps of -2..2 around a ring; 2% random jumps. |
| `16_database_mixed.txt` | 60% hot keys, 30% moving reads, 10% other keys. |
| `17_three_frequency_tiers.txt` | 50% keys 1..32, 30% 33..256, 20% 257..1024. |
| `18_alternating_hot_regions.txt` | Switch hot regions every 512 requests; 30% background. |
| `19_drifting_popularity.txt` | Popularity peak moves from 128 to 896; 25% background. |
| `20_changing_working_sets.txt` | New 128-key set every 1024 requests; 25% background. |
| `21_returning_working_sets.txt` | Revisit four sets: A-B-C-A-D-B-A-C; 20% background. |
| `22_flash_crowd.txt` | Temporary popularity spike for a new set, then return. |
| `23_short_group_bursts.txt` | Bursts of 16..48 requests to eight keys; 20% background. |
| `24_web_sessions_with_noise.txt` | 35% shared pages, 45% session pages, 20% background. |
| `25_multi_tenant_service.txt` | Four separate key ranges; request shares 45/30/15/10%. |
| `26_recent_history_reuse.txt` | 60% reuse from the last 64 requests; 40% background. |
| `27_delayed_history_reuse.txt` | 35% older reuse, 15% recent reuse, 50% background. |
| `28_periodic_hot_set_load.txt` | Hot-set probability cycles through 20/40/70/40%. |
| `29_background_random_reads.txt` | Repeat blocks of 192 hot reads and 64 other reads. |
| `30_reshuffled_zipf_popularity.txt` | Shuffle Zipf ranks every 1024 requests; 25% background. |
