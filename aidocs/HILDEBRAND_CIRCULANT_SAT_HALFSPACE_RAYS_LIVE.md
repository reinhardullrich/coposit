# Hildebrand Circulants: SAT-Halfspace-Rays Dickinson

This records the 2026-08-17 combined copositivity / strict-copositivity run of `sat_halfspace_rays_dickinson` with
complete preprocessing and a 600-second limit per matrix. Only the Hildebrand circulant family
(`exceptional boundary / Hildebrand circulant support n-2`) is in scope.
The diagnostics retain the exact underlying `(k,d,|U|,frequency)` distribution in
`experiments/diagnostics.sqlite3`; these tables aggregate it by support cardinality `k`.

The sampling clock is whole seconds. `<00:00:01` means that the layer completed before the first diagnostic
sample, rather than that it took literally zero time. The last row of each table was still running when the
snapshot was taken, so its time and certificate statistics are provisional.

## Final requested panel: orders 20 through 24

| Order | Matrix ID | Outcome | Time |
|---:|---:|---|---:|
| 20 | 13029 | timeout | 600 s |
| 21 | 13030 | copositive, not strictly copositive | 456 s |
| 22 | 13031 | timeout | 600 s |
| 23 | 13032 | timeout | 600 s |
| 24 | 13033 | timeout | 600 s |

Order 25 was started before the scope was corrected, then stopped at 228 seconds. It is deliberately excluded:
it has no classification and is not evidence for this panel. The corpus database passed `PRAGMA integrity_check` after the run.

## Matrix 13029, order 20 — timeout at 599.984 s

Final diagnostic snapshot: 00:09:59 of model time, in layer `k=9`.

| k | Observed interval and time spent in layer | Certificates | Minimum \(\lvert U\rvert\) | Mean \(\lvert U\rvert\) | Median \(\lvert U\rvert\) | Maximum \(\lvert U\rvert\) | Certificates at maximum \(\lvert U\rvert\) |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | &lt;00:00:01 | 20 | 10 | 10.00 | 10.0 | 10 | 20 |
| 2 | &lt;00:00:01 | 100 | 11 | 11.00 | 11.0 | 11 | 100 |
| 3 | 00:00:01–00:00:01 (0 s observed) | 239 | 11 | 11.48 | 11.0 | 12 | 115 |
| 4 | 00:00:02–00:00:09 (7 s observed) | 644 | 12 | 12.07 | 12.0 | 13 | 43 |
| 5 | 00:00:10–00:00:36 (26 s observed) | 1,293 | 12 | 12.68 | 13.0 | 14 | 3 |
| 6 | 00:00:37–00:01:43 (66 s observed) | 2,318 | 12 | 13.09 | 13.0 | 14 | 260 |
| 7 | 00:01:44–00:03:47 (123 s observed) | 3,178 | 13 | 13.61 | 14.0 | 15 | 21 |
| 8 | 00:03:48–00:08:01 (253 s observed) | 4,979 | 13 | 14.04 | 14.0 | 15 | 361 |
| 9 | 00:08:02–00:09:59 (117 s observed) | 1,863 | 14 | 14.49 | 14.0 | 16 | 15 |

## Matrix 13030, order 21 — copositive, not strictly copositive at 455.680 s

Final diagnostic snapshot: 00:07:35 of model time, with final reported cardinality `k=21`.

| k | Observed interval and time spent in layer | Certificates | Minimum \(\lvert U\rvert\) | Mean \(\lvert U\rvert\) | Median \(\lvert U\rvert\) | Maximum \(\lvert U\rvert\) | Certificates at maximum \(\lvert U\rvert\) |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | &lt;00:00:01 | 21 | 11 | 11.00 | 11.0 | 11 | 21 |
| 2 | &lt;00:00:01 | 105 | 12 | 12.60 | 12.0 | 15 | 21 |
| 3 | 00:00:01–00:00:02 (1 s observed) | 260 | 13 | 13.73 | 13.0 | 17 | 21 |
| 4 | 00:00:03–00:00:09 (6 s observed) | 428 | 13 | 14.55 | 14.0 | 16 | 105 |
| 5 | 00:00:10–00:00:21 (11 s observed) | 495 | 13 | 15.48 | 15.0 | 17 | 112 |
| 6 | 00:00:22–00:00:43 (21 s observed) | 509 | 14 | 15.86 | 16.0 | 17 | 114 |
| 7 | 00:00:44–00:01:26 (42 s observed) | 702 | 15 | 16.49 | 17.0 | 17 | 411 |
| 8 | 00:01:27–00:02:23 (56 s observed) | 632 | 15 | 16.93 | 17.0 | 18 | 185 |
| 9 | 00:02:24–00:03:21 (57 s observed) | 500 | 15 | 17.29 | 17.0 | 19 | 21 |
| 10 | 00:03:22–00:04:22 (60 s observed) | 380 | 16 | 17.52 | 18.0 | 18 | 229 |
| 11 | 00:04:23–00:05:06 (43 s observed) | 238 | 16 | 17.95 | 18.0 | 19 | 52 |
| 12 | 00:05:07–00:05:46 (39 s observed) | 172 | 17 | 18.35 | 18.0 | 19 | 66 |
| 13 | 00:05:47–00:06:17 (30 s observed) | 101 | 18 | 18.28 | 18.0 | 19 | 28 |
| 14 | 00:06:18–00:06:42 (24 s observed) | 75 | 18 | 18.97 | 19.0 | 19 | 73 |
| 15 | 00:06:43–00:06:51 (8 s observed) | 22 | 19 | 19.00 | 19.0 | 19 | 22 |
| 16 | 00:06:52–00:07:01 (9 s observed) | 21 | 19 | 19.00 | 19.0 | 19 | 21 |
| 17 | 00:07:02–00:07:15 (13 s observed) | 21 | 19 | 19.00 | 19.0 | 19 | 21 |
| 18 | 00:07:16–00:07:31 (15 s observed) | 21 | 19 | 19.00 | 19.0 | 19 | 21 |
| 19 | 00:07:32–00:07:35 (3 s observed) | 21 | 21 | 21.00 | 21.0 | 21 | 21 |

The final diagnostic records `k=21`, but neither `k=20` nor `k=21` emitted a certificate. They therefore have no
\(\lvert U\rvert\) distribution to tabulate.

## Matrix 13031, order 22 — timeout at 599.977 s

Final diagnostic snapshot: 00:09:59 of model time, in layer `k=8`.

| k | Observed interval and time spent in layer | Certificates | Minimum \(\lvert U\rvert\) | Mean \(\lvert U\rvert\) | Median \(\lvert U\rvert\) | Maximum \(\lvert U\rvert\) | Certificates at maximum \(\lvert U\rvert\) |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | &lt;00:00:01 | 22 | 12 | 12.00 | 12.0 | 12 | 22 |
| 2 | &lt;00:00:01 | 110 | 13 | 13.00 | 13.0 | 13 | 110 |
| 3 | 00:00:01–00:00:03 (2 s observed) | 291 | 13 | 13.47 | 13.0 | 14 | 136 |
| 4 | 00:00:04–00:00:18 (14 s observed) | 886 | 13 | 14.02 | 14.0 | 15 | 93 |
| 5 | 00:00:19–00:00:58 (39 s observed) | 1,573 | 13 | 14.51 | 15.0 | 16 | 6 |
| 6 | 00:00:59–00:02:48 (109 s observed) | 2,701 | 13 | 14.88 | 15.0 | 16 | 410 |
| 7 | 00:02:49–00:06:40 (231 s observed) | 3,698 | 14 | 15.42 | 15.0 | 17 | 55 |
| 8 | 00:06:41–00:09:59 (198 s observed) | 2,009 | 14 | 15.94 | 16.0 | 17 | 244 |

## Matrix 13032, order 23 — timeout at 599.995 s

Final diagnostic snapshot: 00:09:59 of model time, in layer `k=7`.

| k | Observed interval and time spent in layer | Certificates | Minimum \(\lvert U\rvert\) | Mean \(\lvert U\rvert\) | Median \(\lvert U\rvert\) | Maximum \(\lvert U\rvert\) | Certificates at maximum \(\lvert U\rvert\) |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | &lt;00:00:01 | 23 | 13 | 13.00 | 13.0 | 13 | 23 |
| 2 | <00:00:01 | 115 | 14 | 14.00 | 14.0 | 14 | 115 |
| 3 | 00:00:02–00:00:06 (4 s observed) | 258 | 14 | 14.50 | 14.0 | 15 | 128 |
| 4 | 00:00:07–00:00:48 (41 s observed) | 819 | 14 | 15.10 | 15.0 | 16 | 109 |
| 5 | 00:00:49–00:02:39 (110 s observed) | 1,515 | 15 | 15.47 | 15.0 | 17 | 2 |
| 6 | 00:02:40–00:08:02 (322 s observed) | 3,038 | 15 | 15.97 | 16.0 | 17 | 243 |
| 7 | 00:08:03–00:09:59 (116 s observed) | 825 | 15 | 16.39 | 16.0 | 18 | 1 |

## Matrix 13033, order 24 — timeout at 599.995 s

Final diagnostic snapshot: 00:09:59 of model time, in layer `k=6`.

| k | Observed interval and time spent in layer | Certificates | Minimum \(\lvert U\rvert\) | Mean \(\lvert U\rvert\) | Median \(\lvert U\rvert\) | Maximum \(\lvert U\rvert\) | Certificates at maximum \(\lvert U\rvert\) |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | &lt;00:00:01 | 24 | 11 | 11.00 | 11.0 | 11 | 24 |
| 2 | 00:00:01–00:00:01 (0 s observed) | 156 | 12 | 12.46 | 12.0 | 15 | 24 |
| 3 | 00:00:02–00:00:08 (6 s observed) | 547 | 13 | 13.37 | 13.0 | 16 | 30 |
| 4 | 00:00:09–00:00:41 (32 s observed) | 1,433 | 13 | 14.51 | 14.0 | 20 | 5 |
| 5 | 00:00:42–00:02:26 (104 s observed) | 2,753 | 14 | 15.45 | 15.0 | 20 | 40 |
| 6 | 00:02:27–00:09:59 (452 s observed) | 2,567 | 14 | 16.31 | 16.0 | 21 | 2 |
