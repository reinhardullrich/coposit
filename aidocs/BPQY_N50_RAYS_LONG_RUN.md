# SAT-Halfspace-Rays Dickinson Long Run On BPQY Order-50 COP Matrices

This note records the long run of `sat_halfspace_rays_dickinson` on three order-50 BPQY matrices from the intended COP generator.
The three matrices use the same random seed, 19, and differ only in the designated support size $\rho_0\in\{12,25,38\}$.

The two unresolved runs were manually stopped on 2026-08-17 at approximately 16:03 EEST. The model ran in combined CP/SCP mode with
complete preprocessing and diagnostics enabled. Its binary SHA-256 was
`f07e50f42d3197ffcc4d8c5ae5328e9d5a0661ad2bebd34d87bae9f33711a76b`.

The stored joint distribution is $(k,d,|U|,\text{frequency})$. Here $k$ is the cardinality of the examined support that generated the
certificate, and $|U|$ is the certificate's upper-endpoint cardinality. The median is the frequency-weighted median over all
certificates generated in that layer. The final column counts certificates attaining that layer's displayed maximum $|U|$.
The time column gives the observed wall-clock interval since
the beginning of that matrix's run, followed by the approximate time spent in the layer. Diagnostics were sampled once per second,
so layers completed before the first sample are shown as taking less than one second. Counts in a manually stopped matrix's final
layer are partial.

## Matrix 12668: $\rho_0=12$, Seed 19

Final outcome: **not copositive and not strictly copositive**, after 05:53:46.8.

| $k$ | Observed interval and time spent in layer | Certificates | Minimum $|U|$ | Mean $|U|$ | Median $|U|$ | Maximum $|U|$ | Certificates at maximum $|U|$ |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | <00:00:01 | 50 | 2 | 30.24 | 30 | 45 | 2 |
| 2 | <00:00:01 | 494 | 3 | 32.86 | 32 | 46 | 5 |
| 3 | <00:00:01 | 2,766 | 5 | 33.79 | 33 | 47 | 10 |
| 4 | 00:00:01–00:00:04 (~3 s) | 10,935 | 6 | 34.89 | 35 | 47 | 59 |
| 5 | 00:00:04–00:00:20 (~16 s) | 32,298 | 7 | 35.96 | 36 | 48 | 39 |
| 6 | 00:00:20–00:01:16 (~56 s) | 74,592 | 8 | 36.97 | 37 | 49 | 1 |
| 7 | 00:01:16–00:04:30 (~3 min 14 s) | 140,521 | 9 | 37.88 | 38 | 49 | 18 |
| 8 | 00:04:30–00:13:55 (~9 min 25 s) | 221,564 | 9 | 38.70 | 39 | 49 | 14 |
| 9 | 00:13:55–00:39:17 (~25 min 22 s) | 300,294 | 10 | 39.43 | 39 | 49 | 9 |
| 10 | 00:39:17–01:34:20 (~55 min 3 s) | 356,755 | 10 | 40.09 | 40 | 49 | 8 |
| 11 | 01:34:20–03:15:42 (~1 h 41 min 22 s) | 377,841 | 11 | 40.68 | 41 | 49 | 9 |
| 12 | 03:15:42–05:53:46.8 (~2 h 38 min 5 s) | 315,111 | 29 | 41.40 | 41 | 48 | 38 |

## Matrix 12693: $\rho_0=25$, Seed 19

Final outcome: **unresolved; manually stopped** at 06:43:29.3, in layer $k=19$.

| $k$ | Observed interval and time spent in layer | Certificates | Minimum $|U|$ | Mean $|U|$ | Median $|U|$ | Maximum $|U|$ | Certificates at maximum $|U|$ |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | <00:00:01 | 50 | 5 | 32.24 | 35 | 41 | 6 |
| 2 | <00:00:01 | 444 | 7 | 36.33 | 37 | 44 | 6 |
| 3 | <00:00:01 | 1,981 | 7 | 37.75 | 38 | 46 | 5 |
| 4 | 00:00:01–00:00:03 (~2 s) | 6,128 | 8 | 38.61 | 39 | 47 | 5 |
| 5 | 00:00:03–00:00:08 (~5 s) | 14,372 | 9 | 39.28 | 39 | 47 | 15 |
| 6 | 00:00:08–00:00:23 (~15 s) | 27,258 | 10 | 39.76 | 40 | 48 | 5 |
| 7 | 00:00:23–00:00:57 (~34 s) | 44,623 | 11 | 40.09 | 40 | 48 | 5 |
| 8 | 00:00:57–00:02:05 (~1 min 8 s) | 66,847 | 11 | 40.21 | 41 | 48 | 4 |
| 9 | 00:02:05–00:04:25 (~2 min 20 s) | 95,965 | 12 | 40.17 | 41 | 48 | 29 |
| 10 | 00:04:25–00:08:59 (~4 min 34 s) | 133,773 | 13 | 40.11 | 40 | 49 | 2 |
| 11 | 00:08:59–00:17:07 (~8 min 8 s) | 178,475 | 14 | 40.20 | 40 | 49 | 2 |
| 12 | 00:17:07–00:31:55 (~14 min 48 s) | 221,161 | 15 | 40.49 | 40 | 48 | 89 |
| 13 | 00:31:55–00:55:45 (~23 min 50 s) | 248,218 | 15 | 40.93 | 41 | 49 | 1 |
| 14 | 00:55:45–01:32:27 (~36 min 42 s) | 248,043 | 16 | 41.47 | 41 | 49 | 19 |
| 15 | 01:32:27–02:22:52 (~50 min 25 s) | 220,665 | 17 | 42.05 | 42 | 49 | 12 |
| 16 | 02:22:52–03:22:05 (~59 min 13 s) | 175,704 | 18 | 42.61 | 43 | 49 | 21 |
| 17 | 03:22:05–04:35:02 (~1 h 12 min 57 s) | 128,714 | 19 | 43.02 | 43 | 49 | 13 |
| 18 | 04:35:02–05:56:05 (~1 h 21 min 3 s) | 91,859 | 19 | 43.23 | 44 | 49 | 30 |
| 19 | 05:56:05–06:43:29.3 (~47 min 24 s; partial) | 35,550 | 20 | 43.33 | 44 | 49 | 19 |

## Matrix 12718: $\rho_0=38$, Seed 19

Final outcome: **unresolved; manually stopped** at 06:43:30.1, in layer $k=9$.

| $k$ | Observed interval and time spent in layer | Certificates | Minimum $|U|$ | Mean $|U|$ | Median $|U|$ | Maximum $|U|$ | Certificates at maximum $|U|$ |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | <00:00:01 | 50 | 10 | 27.48 | 31 | 35 | 8 |
| 2 | <00:00:01 | 563 | 13 | 31.79 | 33 | 38 | 6 |
| 3 | <00:00:01 | 3,394 | 13 | 33.42 | 34 | 40 | 6 |
| 4 | 00:00:01–00:00:06 (~5 s) | 14,665 | 15 | 34.43 | 35 | 41 | 7 |
| 5 | 00:00:06–00:00:36 (~30 s) | 49,180 | 15 | 35.25 | 35 | 42 | 11 |
| 6 | 00:00:36–00:03:22 (~2 min 46 s) | 133,991 | 16 | 35.95 | 36 | 42 | 252 |
| 7 | 00:03:22–00:18:19 (~14 min 57 s) | 306,028 | 17 | 36.57 | 37 | 43 | 262 |
| 8 | 00:18:19–01:39:01 (~1 h 20 min 42 s) | 605,906 | 18 | 37.15 | 37 | 44 | 122 |
| 9 | 01:39:01–06:43:30.1 (~5 h 4 min 29 s; partial) | 799,109 | 19 | 37.81 | 38 | 45 | 16 |
