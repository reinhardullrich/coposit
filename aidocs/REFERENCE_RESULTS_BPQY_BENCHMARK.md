# BPQY Benchmark: Literature Baselines, Adaptive Sponsel/Copomatrix, SAT-B3, and BDD-B3

## Scope

This campaign compares the eight maintained literature baselines, `adaptive_sponsel_copomatrix`, and `sat_b3` on the generated
`bpqy_benchmark` set. The generated flag directly excludes preprocessing-complete matrices and contains 404 model inputs:
298 known strictly copositive matrices and 106 matrices with unknown truth.

Every run used complete preprocessing, diagnostics, a ten-second per-matrix cutoff, CPU 2 for dispatch and serialized SQLite writes,
and persistent workers on CPUs 3 through 9. `--rerun` ensured that every row was measured with the listed binary and cutoff. The ten
campaigns took 5,160.872 seconds, or 86.015 minutes, in total wall time.

Models supporting combined classification ran once in `both` mode. Models without a combined classifier ran once in
`strictly_copositive` mode. A strict-only completion is therefore a predicate result, not a full CP/SCP classification, and should not
be compared as though it proved more than that.

## Main Results

The successful-time median is calculated only over completed matrices. It is useful within a model but is strongly selection-biased
between models because their completed subsets differ. Cutoff-substituted work uses native elapsed time for completions and ten
seconds for every timeout.

### Full combined classification

| Model | Completed | Timeout | Completion rate | Median successful time | Cutoff-substituted work | Wall time |
|---|---:|---:|---:|---:|---:|---:|
| SAT-B3 | 257 | 147 | 63.61% | 215.345 ms | 1,755.021 s | 254.068 s |
| Dickinson 2019 | 98 | 306 | 24.26% | 23.074 ms | 3,165.161 s | 458.531 s |
| Hadeler 1983 | 69 | 335 | 17.08% | 271.524 ms | 3,362.228 s | 482.488 s |
| Danninger 1990 | 10 | 394 | 2.48% | 2,108.177 ms | 3,969.678 s | 571.147 s |

SAT-B3 completed 159 more full classifications than the next model, Dickinson 2019. It is the only full classifier that completed any
selected matrix above order 20, and it completed every selected matrix through order 25.

### Strict predicate only

| Model | Completed | Timeout | Completion rate | Median successful time | Cutoff-substituted work | Wall time |
|---|---:|---:|---:|---:|---:|---:|
| Adaptive Sponsel/Copomatrix | 50 | 354 | 12.38% | 6.316 ms | 3,560.069 s | 513.502 s |
| Copomatrix 2011 | 24 | 380 | 5.94% | 187.378 ms | 3,847.213 s | 554.735 s |
| Bundfuss 2008 | 4 | 400 | 0.99% | 141.775 ms | 4,001.669 s | 582.248 s |
| Sponsel 2012 | 4 | 400 | 0.99% | 2.793 ms | 4,000.020 s | 581.366 s |
| Safi 2021 | 1 | 403 | 0.25% | 6,391.122 ms | 4,036.391 s | 581.709 s |
| Dutour 2018 | 0 | 404 | 0.00% | — | 4,040.000 s | 581.078 s |

Adaptive Sponsel/Copomatrix is the strongest strict-only model in this campaign, but its last completion occurs at order 25. The
strict-only table is not directly comparable to the full-classification table because these models return only the strict predicate.

## SAT-B3 Follow-up at 180 Seconds

SAT-B3 was rerun on the same 404 inputs, with the same binary, complete preprocessing, diagnostics, CPU placement, and combined
classification. Only the per-matrix cutoff changed from 10 to 180 seconds. The rerun completed in 3,210.422 seconds wall time.

| Cutoff | Completed | Timeout | Completion rate | Median successful time | Cutoff-substituted work | Wall time |
|---:|---:|---:|---:|---:|---:|---:|
| 10 s | 257 | 147 | 63.61% | 215.345 ms | 1,755.021 s | 254.068 s |
| 180 s | 294 | 110 | 72.77% | 457.766 ms | 22,202.158 s | 3,210.422 s |

The longer cutoff recovered 37 of the 147 former timeouts, or 25.17%, and raised the overall completion rate by 9.16 percentage
points. The successful-time median increased because the completed subset now includes harder cases; it does not indicate that the
same matrices ran more slowly.

| Order | Completed at 10 s | Completed at 180 s | Added |
|---:|---:|---:|---:|
| 10 | 26 / 26 | 26 / 26 | 0 |
| 15 | 43 / 43 | 43 / 43 | 0 |
| 20 | 36 / 36 | 36 / 36 | 0 |
| 25 | 40 / 40 | 40 / 40 | 0 |
| 30 | 32 / 42 | 42 / 42 | 10 |
| 35 | 23 / 38 | 31 / 38 | 8 |
| 40 | 14 / 38 | 24 / 38 | 10 |
| 45 | 13 / 36 | 17 / 36 | 4 |
| 50 | 11 / 34 | 12 / 34 | 1 |
| 55 | 14 / 36 | 14 / 36 | 0 |
| 60 | 5 / 35 | 9 / 35 | 4 |

All 294 completions were known strictly copositive matrices and matched the corpus. None of the 106 truth-unknown matrices completed.
Four known-strict matrices still timed out: IDs 13295 and 13296 at order 40, and IDs 13374 and 13385 at order 45. There were no
parse errors, node limits, execution errors, or other non-timeout failures.

Because the diagnostics result identity does not include the cutoff, the 180-second rerun replaced the corresponding ten-second
SAT-B3 rows in `experiments/diagnostics.sqlite3`. The ten-second aggregate and per-order counts above remain the durable record of
that original campaign.

## BDD-B3 Follow-up at 10 Seconds

BDD-B3 was run on the same 404 inputs with complete preprocessing, diagnostics, combined classification, CPU 2 for dispatch, workers
on CPUs 3 through 9, and a ten-second per-matrix cutoff. It completed in 304.216 seconds wall time.

| Model | Completed | Timeout | Completion rate | Median successful time | Cutoff-substituted work | Wall time |
|---|---:|---:|---:|---:|---:|---:|
| SAT-B3 | 257 | 147 | 63.61% | 215.345 ms | 1,755.021 s | 254.068 s |
| BDD-B3 | 232 | 172 | 57.43% | 47.145 ms | 1,927.532 s | 304.216 s |

BDD-B3 completed 25 fewer matrices and took 50.148 seconds more wall time. Its much smaller successful-time median does not reverse
that result: the median is selection-biased, and the additional BDD timeouts dominate total work. The per-order split shows the
change in behavior directly.

| Order | Selected | SAT-B3 completed | BDD-B3 completed | BDD-B3 difference |
|---:|---:|---:|---:|---:|
| 10 | 26 | 26 | 26 | 0 |
| 15 | 43 | 43 | 43 | 0 |
| 20 | 36 | 36 | 36 | 0 |
| 25 | 40 | 40 | 40 | 0 |
| 30 | 42 | 32 | 40 | +8 |
| 35 | 38 | 23 | 28 | +5 |
| 40 | 38 | 14 | 13 | -1 |
| 45 | 36 | 13 | 5 | -8 |
| 50 | 34 | 11 | 0 | -11 |
| 55 | 36 | 14 | 1 | -13 |
| 60 | 35 | 5 | 0 | -5 |

BDD-B3 gains 13 completions at orders 30 and 35, then loses 38 at orders 40 through 60. This is consistent with a BDD backend that
removes Boolean-management overhead on smaller instances but becomes vulnerable to diagram growth and a different selected-support
order on the hard upper-dimensional tail. All 232 BDD-B3 completions were known strictly copositive matrices and matched corpus
truth. None of the 106 truth-unknown matrices completed, and there were no failures other than timeouts.

## Completion Counts by Order

Each cell is `completed / selected` at that order.

### Full combined classification

| Order | SAT-B3 | Dickinson 2019 | Hadeler 1983 | Danninger 1990 |
|---:|---:|---:|---:|---:|
| 10 | 26 / 26 | 26 / 26 | 26 / 26 | 10 / 26 |
| 15 | 43 / 43 | 43 / 43 | 43 / 43 | 0 / 43 |
| 20 | 36 / 36 | 29 / 36 | 0 / 36 | 0 / 36 |
| 25 | 40 / 40 | 0 / 40 | 0 / 40 | 0 / 40 |
| 30 | 32 / 42 | 0 / 42 | 0 / 42 | 0 / 42 |
| 35 | 23 / 38 | 0 / 38 | 0 / 38 | 0 / 38 |
| 40 | 14 / 38 | 0 / 38 | 0 / 38 | 0 / 38 |
| 45 | 13 / 36 | 0 / 36 | 0 / 36 | 0 / 36 |
| 50 | 11 / 34 | 0 / 34 | 0 / 34 | 0 / 34 |
| 55 | 14 / 36 | 0 / 36 | 0 / 36 | 0 / 36 |
| 60 | 5 / 35 | 0 / 35 | 0 / 35 | 0 / 35 |

### Strict predicate only

| Order | Adaptive | Copomatrix 2011 | Bundfuss 2008 | Sponsel 2012 | Safi 2021 | Dutour 2018 |
|---:|---:|---:|---:|---:|---:|---:|
| 10 | 25 / 26 | 24 / 26 | 4 / 26 | 4 / 26 | 1 / 26 | 0 / 26 |
| 15 | 18 / 43 | 0 / 43 | 0 / 43 | 0 / 43 | 0 / 43 | 0 / 43 |
| 20 | 4 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 |
| 25 | 3 / 40 | 0 / 40 | 0 / 40 | 0 / 40 | 0 / 40 | 0 / 40 |
| 30 | 0 / 42 | 0 / 42 | 0 / 42 | 0 / 42 | 0 / 42 | 0 / 42 |
| 35 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 |
| 40 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 | 0 / 38 |
| 45 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 |
| 50 | 0 / 34 | 0 / 34 | 0 / 34 | 0 / 34 | 0 / 34 | 0 / 34 |
| 55 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 | 0 / 36 |
| 60 | 0 / 35 | 0 / 35 | 0 / 35 | 0 / 35 | 0 / 35 | 0 / 35 |

## Correctness and Unknown Cases

All 517 completed results across the ten campaigns concern matrices with known strict truth, and every one matches the corpus. There
were no parse errors, node limits, execution errors, or other non-timeout failures. None of the 106 truth-unknown matrices completed,
so this campaign adds no new corpus truth values.

The four combined classifiers returned `(copositive, strictly copositive) = (1, 1)` on every completion. Every strict-only completion
returned `strictly_copositive = 1`.

## Native Binary Identities

| Model | Mode | Native SHA-256 |
|---|---|---|
| SAT-B3 | `both` | `ba65cf630189bdc632bf5c5fea990b37e1c292458ad5b8096964470a22f42439` |
| BDD-B3 | `both` | `ddec7485e6b4276920270d4e4b618926b97b9baf7910778396da56832f9f9c2f` |
| Danninger 1990 | `both` | `1f8ec560f5b27d5b8384d62d5654a03fbc2555f8e94368d6c9fa605942f5bdaa` |
| Hadeler 1983 | `both` | `633b6479b0bd28f6718c768c88d956ed385c89747d0206ab680fe4d1b68e15c2` |
| Dickinson 2019 | `both` | `a3bf36ed5dda250dc7ffe45da46e1cd8b13b384634da83e838e022829911d9b8` |
| Dutour 2018 | `strictly_copositive` | `626ca5fd32767f614c882bbff1ea7cb87a5886b4e947867dece72a756f5576d0` |
| Copomatrix 2011 | `strictly_copositive` | `732f0feabc61b9ff160a63b0d11c6279ed68497d8c1b74324fc228bba6d450ef` |
| Safi 2021 | `strictly_copositive` | `0f2b0c69359c337b767c2de4b9aff4a668d45f9f5d9066e7372bbf2d28a56de3` |
| Bundfuss 2008 | `strictly_copositive` | `f8ffca2a1f8aae305cda637abe8eb8596751f07bdd26646b163196bd8ef74f32` |
| Sponsel 2012 | `strictly_copositive` | `becb32fa9944a96f5c7632e9a55184b32259db155ff07b48ccb27148be57d5b8` |
| Adaptive Sponsel/Copomatrix | `strictly_copositive` | `0ccb0f333b2ffada8501a54eca83866b4d3c251846f8b558fc5825cdd165779b` |

The current per-matrix rows and diagnostics are stored in `experiments/diagnostics.sqlite3`.

## Current-Support Comparison at 5 Seconds

On 2026-08-24, the eight literature baselines and Improved NBC-B7 were rerun after the shared `Support`/`SupportContext` migration.
The campaign used the current 404-matrix `bpqy_benchmark`, complete preprocessing, diagnostics, a five-second per-matrix cutoff,
CPU 4 for dispatch and database writes, and workers on CPUs 5 through 9. Combined-capable models ran in `both` mode; the other five
baselines ran the strict predicate only. Total campaign wall time was 3,244.380 seconds (54 minutes 4.380 seconds).

| Model | Mode | Completed | Timeout | Completion rate | Median successful time | Cutoff-substituted work | Wall time |
|---|---|---:|---:|---:|---:|---:|---:|
| Improved NBC-B7 | both | 263 | 141 | 65.10% | 111.878 ms | 874.581 s | 179.048 s |
| Dickinson 2019 | both | 101 | 303 | 25.00% | 19.674 ms | 1,574.085 s | 316.233 s |
| Hadeler 1983 | both | 69 | 335 | 17.08% | 269.829 ms | 1,687.792 s | 338.709 s |
| Copomatrix 2011 | strict | 18 | 386 | 4.46% | 49.425 ms | 1,938.802 s | 390.926 s |
| Danninger 1990 | both | 7 | 397 | 1.73% | 1,578.531 ms | 1,994.981 s | 401.591 s |
| Bundfuss 2008 | strict | 4 | 400 | 0.99% | 142.368 ms | 2,001.640 s | 403.620 s |
| Sponsel 2012 | strict | 4 | 400 | 0.99% | 2.790 ms | 2,000.020 s | 401.185 s |
| Dutour 2018 | strict | 0 | 404 | 0.00% | — | 2,020.000 s | 406.278 s |
| Safi 2021 | strict | 0 | 404 | 0.00% | — | 2,020.000 s | 406.790 s |

The campaign stored exactly 3,636 final rows. Every non-completion was a timeout, every completed known case matched corpus truth,
and none of the 93 truth-unknown matrices completed. Improved NBC-B7 completed 162 more full classifications than the strongest
literature baseline, Dickinson 2019. Strict-only results remain predicate results and must not be presented as full CP/SCP
classifications.

The native SHA-256 identities were `a90323e07737f527f32e8f2c0610bbe72c86fbaf67a26ceec8dc8df4c38ce161` (Improved NBC-B7),
`2b93424b2c4394f7674182987aa160363e7ddff2857aacb7f779a5a4cf50da24` (Dickinson),
`8245019385b76635933564d0ae9cbb639b5196d5f209c700bd24cac9eb79f3e0` (Hadeler),
`a916c7dce91bc85614782c95f45b9be3e20c1346bb95222feda2f1cdc3e53c1e` (Danninger),
`c8e0ff60e6fe28b2c62dc51841e1a4d44a42f560131b025df829258cb1b7795b` (Dutour),
`625ebdee15c835a7cdfd90ed061d9f79fa6e09b03bc081db45a01f56c9fd4aba` (Copomatrix),
`6cc232c80ca36a44ab65ca10926e85fcf031197410755c3365fa65a3887fba49` (Safi),
`fafa09f09510adfb0bdd0d234f775fcbcaff92d9805a378acd490f13395dfdab` (Bundfuss), and
`156f64b684a2946476e4b198ec5c073e1fe909a055f307d6282024d9c0d70fef` (Sponsel).
