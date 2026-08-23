# Corpus Benchmark Sets

## Purpose

The maintained `matrices` table has six overlapping Boolean flags. A matrix can serve several benchmark roles without duplicating
its matrix row or expected classification.

| Flag | Rows | Orders | Strict | Boundary | Not copositive | Unknown | Intended use |
|---|---:|---:|---:|---:|---:|---:|---|
| `smoke_set` | 46 | 4–15 | 20 | 26 | 0 | 0 | Fast correctness and integration checks |
| `core_and_stress_test` | 469 | 4–3,361 | 215 | 245 | 9 | 0 | Routine comparisons and difficult model-search cases |
| `n_le_100` | 920 | 4–100 | 505 | 309 | 0 | 106 | Every matrix of order at most 100 not solved by preprocessing |
| `n_gt_100_solved` | 0 | — | 0 | 0 | 0 | 0 | Higher-order literature-solved matrices not solved by preprocessing |
| `bpqy_benchmark` | 404 | 10–60 | 298 | 0 | 0 | 106 | BPQY COP exact lifts that are strict or still unknown |
| `bpqy_quick_test` | 6 | 30–60 | 6 | 0 | 0 | 0 | Short, medium-duration SAT-B3 development panel |

All six sets exclude `preprocessing_solved = 1` when selected by the benchmark runner: a benchmark member must reach the selected
model rather than end in the shared preprocessing pipeline. The three stored curated flags and three generated flags still cover 947
distinct effective rows because the BPQY panel is contained in N ≤ 100. All 46 Smoke rows belong to
Core and Stress and to N ≤ 100. Of the 469 Core and Stress rows, 442 belong to N ≤ 100 and 27 belong to neither generated set. The
two dimension-generated sets are disjoint by definition.

## BPQY Benchmark

`bpqy_benchmark` is generated from source 51's normalized `BPQY COP ...` family rows. It contains every exact COP construction that
is not already preprocessing-complete and is either strictly copositive or still has both truth fields unknown. Known non-copositive,
copositive-boundary, and preprocessing-complete lifts are excluded. The generated flag contains the 404 rows shown above: 298 strict
and 106 unknown. Because the flag is generated, later exact classifications and preprocessing decisions automatically update
membership without a manual refresh.

Select it with `python/run_results.py MODEL --matrix-set bpqy_benchmark ...`.

## BPQY Quick Test

`bpqy_quick_test` is a stored six-matrix subset of the BPQY benchmark for development runs that need more model work than Smoke but
must remain much shorter than the complete BPQY campaign. Selection used the exact SAT-B3 combined run of 2026-08-22 with complete
preprocessing, diagnostics, and binary SHA-256
`ba65cf630189bdc632bf5c5fea990b37e1c292458ad5b8096964470a22f42439`:

| Matrix ID | Order | SAT-B3 time |
|---:|---:|---:|
| 13173 | 30 | 17.672 s |
| 15436 | 60 | 20.073 s |
| 13226 | 35 | 32.793 s |
| 13318 | 40 | 45.079 s |
| 13377 | 45 | 87.198 s |
| 13387 | 45 | 96.798 s |

The requested 15–100-second band has qualifying completions at only five distinct stored orders: 30, 35, 40, 45, and 60. Order 50
jumps from 5.725 seconds to 152.946 seconds, while order 55 has no completion above 8.758 seconds. The quick set therefore repeats
order 45 rather than admitting a case outside the requested runtime band. These times are selection evidence, not a promise for a
later SAT-B3 binary or another machine.

Select it with `python/run_results.py MODEL --matrix-set bpqy_quick_test ...`.

## N ≤ 100

`n_le_100` is comprehensive rather than sampled within the matrices that require model work. SQLite generates membership directly
from `dimension <= 100 AND preprocessing_solved = 0`, so it cannot become stale and every future qualifying matrix enters
automatically.

## N > 100 Solved in the Literature

`n_gt_100_solved` contains every matrix above order 100 that is not solved by preprocessing and whose `references_solved` array
identifies at least one paper-reported completed copositivity, strict-copositivity, or equivalent global-standard-quadratic-program
solve. It is currently empty because preprocessing completely classifies all 75 higher-order matrices carrying such a claim.
Membership follows the evidence and preprocessing fields automatically; it does not claim that the paper used coposit's model or
exact arithmetic.

## Smoke Set

The current Smoke set contains 26 matrices at orders 4–7, 15 at orders 8–12, and 5 at orders 13–15. Every selected row has a stored
eligible completion no slower than 50 ms. That threshold is selection evidence, not a promise about other hardware or future models.

The dated refresh assigned 49 stored members after replacing 21 preprocessing-complete rows. Three of those assignments are now also
resolved by later preprocessing, so the effective selector retains 20 strict and 26 boundary cases. The boundary cases still exercise
a negative strict-copositivity result. Selection required order at most 20 and an eligible completion no slower than 50 ms, then
preferred underrepresented families, nearby order, source diversity, and timing. No order-1–3 row remains because preprocessing
classifies every such corpus matrix.

## Core And Stress Test

`core_and_stress_test` replaces the two former flags because project comparisons used their union in practice. The former flags had
384 and 234 members with 100 overlaps, hence 518 distinct post-reshape matrices. Six order-51 rows belonged to the later-removed
generated panel; deleting them gives the current 512 model-search matrices.

The former Representative Core was balanced and limited to orders at most 100. It contained every Smoke row, used six order bands,
cycled across families within each order/outcome stratum, and selected 384 matrices. The former Stress Test was evidence-driven. Its
mandatory portion included common baseline failures, the historical bad-26 cohort, difficult representatives from each canonical
family, and maximum-order family/outcome anchors; difficulty-ranked, family-capped filler completed the original selection.

The dated refresh retained 286 members and replaced 226 preprocessing-complete rows, restoring 512 stored assignments. Later additive
preprocessing evidence now excludes 43 of those assignments at selection time, leaving 215 strict, 245 boundary, and 9 non-copositive
matrices with no unknown truth. The stored selection prefers underrepresented families before nearby order, source and
literature-failure diversity, and difficult stored timing; this avoids filling nearly every available boundary slot with the large
Hoffman-Pereira catalog family.

The Hildebrand panel migration removed six redundant same-order boundary variants while retaining one order-15–25 representative per
order. Six already eligible boundary matrices at orders 16, 18, and 20 replaced those Core memberships, so at that stage the 512-row
size and 256/175/81 outcome composition did not change. The current preprocessing refresh supersedes that historical composition.

## Nine-Matrix Development Benchmark

This fixed panel is the short development comparison for experimental support-lattice models. It is small enough for frequent
multi-model runs while retaining easy, intermediate, and difficult exact cases:

| Order | Matrix IDs | Selection role |
|---:|---|---|
| 25 | 12625, 12628, 12631 | Established BPQY COP representatives with designated support size 19 |
| 30 | 13145, 13147, 13152 | Established BPQY COP-extension representatives with designated support size 15 |
| 40 | 13279 | Closest available substitute for a third medium-duration order-50 case |
| 50 | 12090, 12091 | Preprocessing-unresolved Chen--Burer `globallib/qp1` and `qp2` cases |

At selection time, 12090 and 12091 were the only order-50 matrices that were not solved by preprocessing and had a completed best
runtime between 5 and 90 seconds: 82.357 and 82.399 seconds. Matrix 13279 satisfies the same conditions at order 40 with an
85.899-second best runtime. The only other preprocessing-unresolved order-50 matrix with any completed timing took about 2
milliseconds, while the remaining order-50 candidates had no completed timing, so none served the intended intermediate-duration
role.

Run the panel with `python/run_results.py --matrix-ids 12625 12628 12631 13145 13147 13152 13279 12090 12091`. Preprocessing may be
enabled: none of these nine rows is marked `preprocessing_solved`.

## Retired Sets

Scale and Timeout 5s Strict were removed from the maintained schema on 2026-08-16. Their dated assignment scripts and historical
result reports remain available as evidence, but neither is a current runner selector.

Historical result reports using the former 524-matrix Core/Stress snapshot remain valid for that named snapshot. They do not describe
the current 512-matrix fused set.

## Reproduction And Use

The original 2026-08-10 assignment script remains historical because it creates the two superseded columns. The dated migration
`testdata/archive/fuse_core_and_stress_test_2026_08_16.sql` records their union, exclusion of generated sources 93 and 94, and guarded
expected counts. `testdata/archive/remove_generated_stress_2026_08_16.py` records the later removal of those two generated sources and
all 150 of their matrices. `testdata/archive/retire_scale_and_timeout_sets_2026_08_16.sql` records removal of the two retired columns.
`testdata/archive/exclude_preprocessing_solved_from_benchmarks_2026_08_16.py` records the initial 2,115-row preprocessing exclusion
and changes both generated flags to require `preprocessing_solved = 0`.
`testdata/archive/refresh_benchmark_sets_after_motzkin_straus_2026_08_16.sql` records the current 2,708-row refresh and exact curated
replacement IDs. New rows default both stored curated flags to zero.
`testdata/archive/refresh_corpus_from_diagnostics_2026_08_18.sql` adds later exact classifications and preprocessing flags without
rewriting either curated membership list.
`testdata/archive/replace_hildebrand_circulants_2026_08_16.py` records the later one-per-order Hildebrand replacement and six Core
refills.
`testdata/archive/add_independent_angle_hildebrand_2026_08_19.py` adds the separate exact order-15--30 independent-angle panel; its
16 preprocessing-unresolved rows enter the generated N ≤ 100 set automatically and leave both curated flags unchanged.

The latest benchmark refresh was applied with:

```bash
/usr/bin/sqlite3 testdata/copos_testdata.sqlite3 ".read testdata/archive/refresh_benchmark_sets_after_motzkin_straus_2026_08_16.sql"
```

Select the fused set directly:

```sql
SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family
FROM matrices
WHERE core_and_stress_test = 1
ORDER BY matrix_id;
```

Strict means both truth fields are one, boundary means strict is zero and copositive is one, and not copositive means the copositive
field is zero. `NULL` means the corresponding truth has not been established.
