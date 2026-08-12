# Full Code Review

Date: 2026-08-11
Status: **current completed review**
Scope: shared preprocessing, connected components, exact factorization, adaptive model wiring, Python interfaces, benchmark runners,
SQLite result identity, file-backed corpus storage, tests, and documentation.

The review's identified action items have been resolved or acknowledged in their owning current documentation. The remainder records
the verified state and checks performed during the review.

## Executive Conclusion

No false mathematical certificate was found in the shared preprocessing logic. The exact component theorem, order-one-through-three
tests, exact revalidation of floating Frank-Wolfe candidates, and positive-semidefinite/nullity-one reasoning all preserve the
non-strict-versus-strict equality boundary correctly.

## What Passed Review

### Exact Equality Handling

The non-strict/strict distinction is consistently isolated in exact sign comparisons:

- diagonal and determinant equality passes non-strict and fails strict;
- an exact quadratic value of zero rejects strict but not non-strict;
- positive semidefiniteness accepts non-strict, while positive definiteness accepts strict;
- the combined classification state preserves `strict => non-strict`.

The central state logic is in [`cpp/include/coposit/pre_check.hpp`](../cpp/include/coposit/pre_check.hpp), around line 74.

### Small-Dimensional Copositivity

The order-one, order-two, and order-three rules use exact integer arithmetic in
[`cpp/include/coposit/small_copositivity.hpp`](../cpp/include/coposit/small_copositivity.hpp).

An independent audit enumerated all 1,771,561 symmetric order-three coefficient sextuples with entries in `[-5,5]`. The implemented
decision logic showed no disagreement with the standard order-three criterion.

The larger-matrix triple traversal correctly visits only potentially relevant negative-edge triples, visits a two-negative-edge path
once, and deduplicates a fully negative triangle.

### Floating Frank-Wolfe

Floating point only proposes a candidate. The implementation quantizes the final nonnegative vector to exact nonnegative integer
weights and evaluates the quadratic form exactly before making a decision.

Relevant locations:

- [`cpp/include/coposit/pre_check.hpp`](../cpp/include/coposit/pre_check.hpp), around line 204 - search;
- the same file, around line 295 - exact witness sign.

The regression suite includes a huge-exponent positive matrix whose exact positive contribution underflows in floating point. The
pre-check correctly delegates instead of accepting the rounded zero.

### Connected Components

The negative-entry component theorem is applied correctly. Entries between different negative-graph components are nonnegative, so
non-strict and strict copositivity reduce to the conjunction of the principal component decisions.

The implementation:

- uses the shared dynamic multiword `support` type;
- has no dimension-63 limit;
- reuses component, frontier, discovered, and index storage;
- avoids copying a connected whole matrix;
- scans and processes disconnected component matrices one at a time;
- aggregates non-strict and strict combined classification correctly.

The tests explicitly cross the 64-bit word boundary at dimensions 129 and 130.

### Matrix Scanning and Fused Pipeline

[`cpp/include/coposit/matrix_scan.hpp`](../cpp/include/coposit/matrix_scan.hpp) validates nonempty square symmetric input and collects the
selected signs, row sums, pair results, Frank-Wolfe initialization data, and negative adjacency during one upper-triangle pass.

The fused component pipeline correctly performs cheap globally valid checks at the root while deferring Frank-Wolfe and exact
definiteness work to smaller component matrices.

### Exact Factorization and Nullity-One Logic

The fraction-free LDLT implementation maintains exact rank, inertia, determinant, solve operations, and nullspace recovery. Tests cover
symmetric pivoting, nontrivial kernel vectors, several ranks, complete nullspace bases, and PSD versus indefinite singular matrices.

The nullity-one strict decision is sound:

- a mixed-sign one-dimensional kernel has no nonzero nonnegative vector, so the PSD matrix can still be strictly copositive;
- a one-sided kernel vector gives a nonnegative zero and therefore rejects strict copositivity.

### Adaptive Sponsel-COPOMATRIX Routing

The maintained model consistently uses a 1,000-Sponsel-split streak in source, `ALGORITHM.md`, and the focused test. It evaluates every
COPOMATRIX pivot's exact immediate-child count and keeps the first pivot attaining the minimum. The experimental Zischg comparison
remains separate at 10,000.

### Unresolved Results

Timeout and node-limit outcomes remain distinct from false classifications. The parent process owns SQLite writes, writes are
serialized, and the queue is bounded. Current published Core/Stress batches contain no classification mismatch and no impossible
completed non-strict/strict pair.

## Verification Performed During This Review

- [x] Release build completed successfully.
- [x] All 44 CTest checks passed, including all model tests, 12 public CLI checks, and 19 Python wrapper/runner tests.
- [x] Random differential audit: 1,500 symmetric matrices, dimensions 1-6, coefficients `[-4,4]`, both modes, 9,000 wrapped
  comparisons, no disagreement.
- [x] SQLite `PRAGMA integrity_check`: `ok`.
- [x] Foreign-key check: zero violations.
- [x] Corpus: 2,442 matrices, dimensions 1-3,361.
- [x] Labels: 1,853 copositive, 589 not copositive, 674 strictly copositive.
- [x] Set sizes: Smoke 49, Representative Core 384, Stress 240, Scale 364.
- [x] External storage: 201 references, 201 files, zero missing, zero orphaned, zero malformed shapes.
- [x] Current result database: zero completed `non-strict=false, strict=true` rows.
- [x] Current report aggregates reconcile with the stored current result batches.

## Review Limitation

The workspace currently has an empty `.git` directory. This review therefore covers the complete current state rather than a historical
diff or attribution of individual edits to sessions. A concurrent database storage migration landed during the review and is included
in the findings above.
