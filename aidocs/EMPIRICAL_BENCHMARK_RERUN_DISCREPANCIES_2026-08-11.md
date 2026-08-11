# Empirical Benchmark Rerun Discrepancies — 2026-08-11

## Scope

The complete 524-matrix Representative Core and Stress Test union was rerun once under uniform conditions:

- five-second cutoff per matrix;
- parent dispatcher and SQLite writer pinned to CPU 3;
- three persistent native workers pinned to CPUs 4, 5, and 6;
- strict and ordinary modes;
- preprocessing disabled and both preprocessing stages enabled; and
- all eight literature baselines plus Adaptive Sponsel–COPOMATRIX.

This produced 36 complete batches and 18,864 newly measured result rows. Every completed result matched the stored corpus truth.
There were no execution errors in the rerun and no changed `true` or `false` classification.

## Completion Counts

Each cell is strict / ordinary and shows `previous → rerun`. Bold cells changed.

| Model | No pre-check | Both preprocessing stages |
|---|---:|---:|
| Dutour 2018 | 299 / 250 → 299 / 250 | 336 / 274 → 336 / 274 |
| Danninger 1990 | **296 / 257 → 298 / 257** | 316 / 278 → 316 / 278 |
| COPOMATRIX 2011 | 356 / 327 → 356 / 327 | 369 / 342 → 369 / 342 |
| Safi 2021 | 330 / 286 → 330 / 286 | 353 / 320 → 353 / 320 |
| Bundfuss 2008 | **302 / 251 → 303 / 252** | **356 / 289 → 357 / 291** |
| Sponsel 2012 | 333 / 293 → 333 / 293 | 381 / 308 → 381 / 308 |
| Hadeler 1983 | 366 / 322 → 366 / 322 | 370 / 335 → 370 / 335 |
| Dickinson 2019 | 376 / 343 → 376 / 343 | 380 / 353 → 380 / 353 |
| Adaptive Sponsel–COPOMATRIX | 458 / 431 → 458 / 431 | **464 / 439 → 465 / 440** |

Seven configurations changed completion count. In total, nine previously timed-out calls completed in the rerun; no previous
completion became unresolved.

## Exact Status Transitions

| Model | Mode | Preprocessing | Matrix | Order | Family | Previous | Rerun |
|---|---|---|---:|---:|---|---|---|
| Adaptive Sponsel–COPOMATRIX | Ordinary | Both | 10044 | 26 | Exceptional boundary / Baston cyclic | Timeout | Completed in 4.554583 s |
| Adaptive Sponsel–COPOMATRIX | Strict | Both | 10489 | 89 | Strict exceptional perfect copositive / Dannenberg–Schürmann lift | Timeout | Completed in 4.861603 s |
| Bundfuss 2008 | Ordinary | None | 9177 | 14 | ggen | Timeout | Completed in 4.924178 s |
| Bundfuss 2008 | Ordinary | Both | 9177 | 14 | ggen | Timeout | Completed in 4.878296 s |
| Bundfuss 2008 | Ordinary | Both | 9186 | 14 | Sanchis | Timeout | Completed in 2.384203 s |
| Bundfuss 2008 | Strict | None | 9177 | 14 | ggen | Timeout | Completed in 4.885826 s |
| Bundfuss 2008 | Strict | Both | 9177 | 14 | ggen | Timeout | Completed in 4.929198 s |
| Danninger 1990 | Strict | None | 9200 | 16 | cisqrg | Timeout | Completed in 4.605604 s |
| Danninger 1990 | Strict | None | 9219 | 15 | Johnson | Timeout | Completed in 3.975387 s |
| Danninger 1990 | Ordinary | None | 10244 | 999 | Exceptional boundary / Johnson–Reams generalized Horn | Worker crash (`SIGSEGV`) | Node limit |

The five Adaptive and prechecked Bundfuss transitions used exactly the same native-module hashes as the previous measurements.
They therefore measure cutoff and system-load variability rather than a changed algorithm. Matrix 9186 is the strongest example:
the previous six-worker run timed out, while the isolated three-worker rerun completed in 2.384203 seconds.

The no-precheck Bundfuss and Danninger rows used newer native modules than the historical no-precheck rows. Their mathematical
algorithms are unchanged, but the current implementations include documented allocation and traversal optimizations. Danninger
matrix 10244 also confirms the intended safety change: its former recursive stack crash is now an explicit unresolved 50,000-open-node
limit in the iterative traversal.

## Substituted One-CPU Time Differences

Substituted time is the sum of completed native-call times plus five seconds for every unresolved row. The table lists only changes
larger than one second over a complete 524-matrix batch; smaller changes are intentionally omitted.

| Model | Mode | Preprocessing | Previous | Rerun | Difference |
|---|---|---|---:|---:|---:|
| Adaptive Sponsel–COPOMATRIX | Strict | Both | 371.672 s | 370.182 s | −1.490 s |
| Adaptive Sponsel–COPOMATRIX | Ordinary | Both | 511.156 s | 506.895 s | −4.261 s |
| Bundfuss 2008 | Strict | None | 1,137.198 s | 1,135.372 s | −1.826 s |
| Bundfuss 2008 | Strict | Both | 863.674 s | 858.850 s | −4.824 s |
| Bundfuss 2008 | Ordinary | Both | 1,195.662 s | 1,188.741 s | −6.922 s |
| COPOMATRIX 2011 | Ordinary | None | 1,007.445 s | 1,005.817 s | −1.628 s |
| Danninger 1990 | Strict | None | 1,152.787 s | 1,148.480 s | −4.307 s |
| Danninger 1990 | Strict | Both | 1,047.334 s | 1,050.155 s | +2.820 s |
| Danninger 1990 | Ordinary | Both | 1,242.443 s | 1,240.764 s | −1.679 s |
| Dickinson 2019 | Strict | Both | 733.786 s | 732.559 s | −1.228 s |
| Dickinson 2019 | Ordinary | Both | 872.944 s | 870.825 s | −2.119 s |
| Hadeler 1983 | Strict | Both | 808.865 s | 806.986 s | −1.879 s |
| Hadeler 1983 | Ordinary | Both | 998.250 s | 995.813 s | −2.437 s |
| Safi 2021 | Ordinary | None | 1,222.570 s | 1,223.870 s | +1.299 s |
| Sponsel 2012 | Strict | None | 984.513 s | 983.373 s | −1.141 s |

These differences are small relative to batch totals of roughly 370–1,400 seconds. Most are explained directly by near-cutoff
status transitions; the rest are ordinary timing variation. Previous dispatcher wall times are not compared because the historical
runs used four or six workers, whereas the uniform rerun deliberately used three.

## Conclusion

The rerun found no correctness discrepancy. The changed completion counts are real measurements but are not evidence of changed
mathematics: they are either near the five-second boundary, system-load sensitive, or associated with documented implementation-only
optimizations. The refreshed reference and research tables use the uniform three-worker rerun.
