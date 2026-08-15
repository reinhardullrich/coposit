# Corpus Benchmark Sets

## Purpose

The maintained `matrices` table has seven independent Boolean flags. They are intentionally overlapping: a matrix can serve several
benchmark roles without duplicating its matrix row or expected classification.

| Flag | Rows | Orders | Strict | Boundary | Not copositive | Non-strict unknown | Intended use |
|---|---:|---:|---:|---:|---:|---:|---|
| `smoke_set` | 49 | 1–19 | 23 | 14 | 12 | 0 | Fast correctness and integration checks |
| `representative_core` | 384 | 1–100 | 192 | 110 | 82 | 0 | Routine algorithm comparison on a balanced, diverse corpus |
| `stress_test` | 234 | 5–3,361 | 129 | 94 | 11 | 0 | Branching, equality, exact-arithmetic, and timeout pressure |
| `scale_set` | 334 | 43–3,361 | 78 | 182 | 74 | 0 | Growth with dimension, density, and storage size |
| `timeout_5s_strict_set` | 109 | 16–3,361 | 46 | 40 | 23 | 0 | Common five-second strict timeouts of both final pre-checked algorithms |
| `n_le_100` | 3,155 | 1–100 | 827 | 1,119 | 746 | 463 | Every matrix of order at most 100 |
| `n_gt_100_solved` | 94 | 120–5,000 | 6 | 25 | 63 | 0 | Higher-order matrices with at least one literature-reported solve |

The five curated flags cover 833 distinct rows; together with the two generated flags, all seven cover 3,533 rows. Overlap is deliberate:
all 49 Smoke rows are in Representative Core; 100 Core rows are also
Stress rows; and 13 Stress rows are also Scale rows. Every Timeout 5s Strict row is already in Stress or Scale: 45 are Stress rows,
75 are Scale rows, and 11 occur in both; 12 are also Core rows. No Smoke row is in Scale or Timeout 5s Strict.

## N ≤ 100

`n_le_100` is the comprehensive dimension-bounded set rather than a sample. It contains all 3,155 current matrices with
`dimension <= 100`, including 827 strict, 1,119 boundary, 746 non-copositive, and 463 unknown matrices. SQLite generates the Boolean
membership directly from `dimension`, so it cannot become stale and future qualifying rows enter automatically.

## N > 100 Solved in the Literature

`n_gt_100_solved` contains all 94 current matrices above order 100 whose `references_solved` array identifies at least one paper that
reports a completed copositivity, strict-copositivity, or equivalent global-StQP solve. It spans orders 120–5,000 and contains 6
strict, 25 boundary, and 63 non-copositive matrices, with 148 claims from ten sources. The generated membership follows the evidence
field automatically; it is not a project claim that the paper used the same algorithm or exact arithmetic.

## Smoke set

Smoke uses four dimension bands: 1–3, 4–7, 8–12, and 13–20, containing 12, 12, 12, and 13 matrices. Every selected row completed
correctly under each of the eight frozen canonical baseline runs, and no individual stored runtime exceeded 50 ms. That threshold is
a selection fact from the stored reference runs, not a promise about other hardware or future models.

The original assignment balanced strict, boundary, non-copositive, and then-unknown rows using the two stored truth columns. The 14
then-unknown rows are now classified as six boundary and eight non-copositive matrices. Fixed anchors retain the simplest diagonal and off-diagonal decisions, an
order-3 large-integer input, Horn, Hoffman-Pereira, and a strict perfect-copositive example. Family round-robin selection supplies the
remaining rows.

## Representative Core

Representative Core contains only orders at most 100 and contains every Smoke row. Its dimension bands contain 24 matrices at orders
1–3 and 72 matrices in each of 4–7, 8–12, 13–20, 21–50, and 51–100. Exactly half are strictly copositive and half are not strictly
copositive.

Within each order/outcome stratum, selection cycles across families before taking another matrix from the same family. Boundary,
non-copositive, and then-unknown rows received separate quotas. The 75 formerly unknown rows are now classified as 26 boundary and
49 non-copositive matrices. The result contains representatives
from 41 named families plus legacy FracESSA rows. Brock is the only named family absent because its smallest matrix has order 200; it
is present in Stress and Scale.

## Stress test

Stress is evidence-driven rather than outcome-balanced. The mandatory part contains:

- all 93 matrices unresolved by all eight frozen five-second canonical baseline runs;
- all 26 matrices from the historical bad-26 targeted runs;
- the two most difficult scored matrices from every named family represented in the canonical run; and
- the maximum-order representative for each strict/non-strict outcome in every named family extending above order 100.

Those rules produced 179 distinct mandatory rows at the frozen assignment point. Difficulty-ranked family-capped filler brought the
set to 240. The later removal of six generated order-2,997 rows reduced the current stored membership to 234. Of the original 221 matrices
with all eight canonical results, 204 were unresolved by at least one baseline and 17 were solved by all eight but retained for family
coverage. The other 19 are high-order anchors outside the original order-100 reference snapshot. All 42 named corpus families occur,
and every row has established non-strict-copositivity truth.

“Difficulty” here means only stored reference evidence: first the number of unresolved canonical baselines, then total elapsed work
with each unresolved run charged its timeout. It is not a new claim about mathematical hardness or a new solver experiment.

## Scale set

At its frozen assignment point, Scale contained all 358 of the then-current corpus matrices above order 100. It also contained the six order-51 matrices from the controlled generated
families. The later corpus reshape removed 120 generated rows above order 1,000 and added 90 generated rows from order 43 through 199;
all additions retain Scale membership. The current set has 334 rows and includes:

- every high-order literature construction already in the corpus; and
- sparse and dense boundary, strict, and explicitly not-copositive triplets at the same 25 irregular dimensions from 43 through 952.

These rows cover the requested density, outcome, coefficient-size, and dimension strata without introducing another mathematical
construction.

## Timeout 5s Strict set

Timeout 5s Strict originally contained the 129 matrices for which both pre-checked final algorithm paths remained unresolved at a five-second
strict-copositivity cutoff. If either Dickinson or Adaptive Sponsel–COPOMATRIX completed the matrix, it was not selected. Both paths
used `preprocessing = 'both'`. Removing the generated rows above order 1,000 reduced the current stored membership to 109.

The 524 Core/Stress rows reuse the runs reported in `research/RESEARCH_REPORT_COPOSIT.md`. Its Dickinson 2019 implementation is the
solver from which `dickinson_final` was copied. Only the remaining 1,918 corpus rows were run afterward with `dickinson_final` and
the current Adaptive Sponsel–COPOMATRIX model. Both missing-only runs completed without classification mismatches or execution errors.

This is dated performance evidence, not a claim that a matrix is intrinsically hard or will time out on another machine or future
binary. Exact hashes, cutoff, intersection rule, and assignment guards are retained in
`testdata/archive/assign_timeout_5s_strict_set_2026_08_12.sql`; new result rows do not change membership automatically.

## Reproduction and use

New matrix rows default the five curated flags to zero. The four original flags are reproduced with:

```bash
sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/assign_benchmark_sets_2026_08_10.sql
```

The timeout set is reproduced separately with:

```bash
sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/assign_timeout_5s_strict_set_2026_08_12.sql
```

The generated order-at-most-100 flag was added with:

```bash
sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/add_n_le_100_set_2026_08_14.sql
```

The generated higher-order literature-solved flag was added with:

```bash
sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/add_n_gt_100_solved_set_2026_08_14.sql
```

The script freezes the exact eight baseline identities and the pre-consensus sampling classes. It now aborts because non-strict truth
has deliberately changed rather than silently selecting a different set. The flags stored in the database remain the authoritative
2026-08-10 assignment until that sampling decision is deliberately revised.

Select a set directly in SQL, for example:

```sql
SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family
FROM matrices
WHERE representative_core = 1
ORDER BY matrix_id;
```

The current class counts derive directly from stored truth: strict means both flags are one, boundary means strict is zero and non-strict
copositivity is one, and not-copositive means non-strict copositivity is zero. The 463 non-strict-unknown rows have not yet received
maintained corpus truth.
