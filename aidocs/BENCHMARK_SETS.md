# Corpus Benchmark Sets

## Purpose

The maintained `matrices` table has four independent Boolean flags. They are intentionally overlapping: a matrix can serve several
benchmark roles without duplicating its matrix row or expected classification.

| Flag | Rows | Orders | Strict | Boundary | Not copositive | Ordinary unknown | Intended use |
|---|---:|---:|---:|---:|---:|---:|---|
| `smoke_set` | 49 | 1–19 | 23 | 14 | 12 | 0 | Fast correctness and integration checks |
| `representative_core` | 384 | 1–100 | 192 | 110 | 82 | 0 | Routine algorithm comparison on a balanced, diverse corpus |
| `stress_test` | 240 | 5–3,361 | 131 | 96 | 13 | 0 | Branching, equality, exact-arithmetic, and timeout pressure |
| `scale_set` | 364 | 51–3,361 | 88 | 192 | 84 | 0 | Growth with dimension, density, and storage size |

The four flags cover 863 distinct rows. Overlap is deliberate: all 49 Smoke rows are in Representative Core; 100 Core rows are also
Stress rows; and 19 Stress rows are also Scale rows. No Smoke row is in Scale.

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

Those rules produce 179 distinct mandatory rows. Difficulty-ranked family-capped filler brings the set to 240. Of its 221 matrices
with all eight canonical results, 204 were unresolved by at least one baseline and 17 were solved by all eight but retained for family
coverage. The other 19 are high-order anchors outside the original order-100 reference snapshot. All 42 named corpus families occur,
and every row has established ordinary-copositivity truth.

“Difficulty” here means only stored reference evidence: first the number of unresolved canonical baselines, then total elapsed work
with each unresolved run charged its timeout. It is not a new claim about mathematical hardness or a new solver experiment.

## Scale set

Scale contains all 358 corpus matrices above order 100. It also contains the six order-51 matrices from the controlled generated
families, so both complete 90-row generated panels remain intact. Consequently it includes:

- every high-order literature construction already in the corpus; and
- sparse and dense boundary, strict, and explicitly not-copositive triplets at the same 30 irregular dimensions from 51 through 2,997.

No additional matrix was generated. These existing rows already cover the requested density, outcome, coefficient-size, and dimension
strata without introducing another synthetic construction.

## Reproduction and use

New matrix rows default all four flags to zero. The dated assignment is reproduced with:

```bash
sqlite3 testdata/Copos_testdata.sqlite3 < testdata/assign_benchmark_sets_2026_08_10.sql
```

The script freezes the exact eight baseline identities and the pre-consensus sampling classes. It now aborts because ordinary truth
has deliberately changed rather than silently selecting a different set. The flags stored in the database remain the authoritative
2026-08-10 assignment until that sampling decision is deliberately revised.

Select a set directly in SQL, for example:

```sql
SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family
FROM matrices
WHERE representative_core = 1
ORDER BY matrix_id;
```

The current class counts derive directly from stored truth: strict means both flags are one, boundary means strict is zero and ordinary
copositivity is one, and not-copositive means ordinary copositivity is zero. No current row is ordinary-unknown.
