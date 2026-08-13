# Copositivity Checker Extraction Handover

Status: historical snapshot of the completed extraction, recorded on 2026-08-07.

Purpose: extract the exact copositivity work from FracESSA into a separate, generic repository without losing the mathematics,
experiments, counterexamples, benchmark corpus, or the distinction between production code and unfinished research.

This document covers the copositivity work performed mainly on August 6 and 7, 2026. Current source code and the SQLite corpus are
authoritative if they disagree with this handover.

Database note: Section 8 documents the preserved FracESSA source snapshot. The maintained coposit corpus now uses the one-table
schema documented in `../PROJECT.md`; the original database remains compressed under `testdata/archive/`.

## 1. Executive summary

1. FracESSA needs **strict** copositivity of an exact symmetric matrix:

   $$
   x^T A x>0\qquad\text{for every nonzero }x\geq0.
   $$

   Non-strict copositivity uses `>= 0` instead. The current production checker decides strict copositivity only.

2. The complete exact stability path now uses FLINT arbitrary-precision integers. Rational denominators are cleared before the final
   checker, and positive scaling does not change either non-strict or strict copositivity.

3. The current production fallback is an optimized exact simplicial-cone checker inspired by Polyhedral Common. It stores only the
   Gram matrix `B = V A V^T`, chooses the most critical negative pair by the exact ratio

   $$
   \frac{b_{ij}^2}{b_{ii}b_{jj}},
   $$

   and creates children by replacing one generator with `v_i + v_j`. It was dramatically faster than both the former Hadeler
   principal-submatrix method and the unchanged upstream Polyhedral Common implementation on the original corpus.

4. The cone method is not uniformly fast. A strictly positive-definite tridiagonal construction of order 15 is extremely difficult
   for it, while direct Danninger recursion solves the same matrix in about 0.4 milliseconds and the adaptive hybrid in about 50
   microseconds. Conversely, matrices 811 and 813 are difficult for direct Danninger recursion and trivial for the cone.

5. The best experimental combination so far is the **adaptive narrow-Danninger/cone hybrid**:

   - at each node, try Danninger pivots in index order;
   - use a Danninger reduction only when it creates at most two children;
   - otherwise perform one optimized cone split;
   - restart the Danninger scan in each child.

   It solved the original 1,078-case corpus without a five-second timeout and handled all four deliberately bad matrices. However,
   the enlarged 1,569-case corpus produced 80 ten-second timeouts. It is therefore promising research code, not a proven uniformly
   practical default.

6. Exact negative-entry connected components are a useful independent reduction. They improved the seven hard strictly
   copositive matrices by a median 17.06% and were neutral on the 81-matrix end-to-end quick set. The current implementation uses
   FracESSA's fixed 64-bit support masks and is therefore **not generic**.

7. The new repository must not inherit FracESSA's dimension-63 limit. The optimized cone itself has no such limit, but the current
   shared sign scan, connected-component helper, experiment parsers, and some runners do.

8. The separate database `testdata/copos_testdata.sqlite3` is the main asset to preserve. It currently contains 1,569 exact integer
   matrices of dimensions 1 through 3,361, exact or partially exact classifications, provenance, published benchmark data, and
   algorithm test runs.

## 2. Mathematical problem and input contract

### 2.1 Strict and non-strict copositivity

For a symmetric matrix `A`:

$$
A\text{ is copositive}
\iff
x^T A x\geq0\quad\text{for every }x\geq0,
$$

and

$$
A\text{ is strictly copositive}
\iff
x^T A x>0\quad\text{for every nonzero }x\geq0.
$$

The distinction is essential. A zero direction rejects strict copositivity but not non-strict copositivity. Several hard examples are
copositive boundary matrices: they are copositive but not strictly copositive.

The quadratic form depends only on the symmetric part `(A + A^T) / 2`. Nevertheless, every current implementation assumes a
symmetric input and often reads only one triangle. The clean generic boundary is therefore to require symmetry explicitly rather
than silently alter an input matrix.

### 2.2 Exact rational input can become exact integer input once

If a rational symmetric matrix has a positive common denominator `d`, then

$$
\widehat A=dA
$$

has integer entries and

$$
x^T\widehat A x=d\,x^TAx.
$$

Since `d > 0`, `A` and `dA` have the same non-strict and strict copositivity classifications. The core algorithms can therefore use
FLINT `fmpz` integers throughout after one denominator-clearing step.

Do not copy FracESSA's circular normalization `A - dJ` into a generic copositivity checker. It preserves the game and its ESS on
the simplex, but it does **not** preserve copositivity on the whole nonnegative orthant. The experiment
`experiments/original_a_copositivity_2026-08-06/` demonstrates this distinction: all 341 compact circular FracESSA matrices had zero
diagonal after strategic normalization and consequently failed strict copositivity in that stored form.

## 3. How FracESSA produces the matrix being checked

This section explains the integration boundary. It belongs in the handover because the generic checker must accept the right
mathematical object, but the new repository should not own FracESSA's candidate solver.

Let `I` be the support of an exact stationary candidate, `J` its extended support of all tied best replies, and

$$
K=J\setminus I.
$$

After choosing one reference strategy in `I`, the reduced extended Hessian has blocks

$$
R=
\begin{pmatrix}
H&G\\
G^T&Q
\end{pmatrix}.
$$

The support coordinates represented by `H` are unrestricted. The coordinates indexed by `K` are constrained to be nonnegative.
The exact candidate factorization already gives the inertia of `H`.

- If `H` is not negative definite, the candidate is not an ESS.
- If `H` is negative definite and `K` is empty, the candidate is an ESS.
- Otherwise, eliminate the unrestricted block by a Schur complement.

The final strict-copositivity matrix is a positive multiple of

$$
-\left(Q-G^TH^{-1}G\right).
$$

FracESSA does not form `H^{-1}`. Its retained fraction-free candidate factorization solves the needed multiple right-hand sides and
constructs the integer matrix

$$
M=-\left(\delta\widehat Q-\widehat G^TN\right),
$$

where `dH N = dG delta` and `delta > 0`. Thus `M` is a positive multiple of the mathematical Schur complement and has the same
strict-copositivity classification.

The relevant integration code is:

- `cpp/src/find_candidate_safe.cpp`, especially `build_scaled_reduced_b()`;
- `cpp/include/fracessa/find_candidate_safe.hpp`, which documents its preconditions;
- `cpp/src/checkstab.cpp`, which calls the checker and records the ESS reason.

These files are reference material for integrating the new library back into FracESSA. They are not part of the generic
copositivity core.

## 4. Current exact decision pipeline

FracESSA currently applies the following decisions in order to the integer matrix `M`.

### 4.1 Direct dimensions one through three

The shared exact criteria are in `cpp/include/linalg/copositive_integer.hpp`.

- Order one: `m11 > 0`.
- Order two: both diagonal entries must be positive. A nonnegative off-diagonal entry passes immediately; otherwise require
  `m11*m22 - m12^2 > 0`.
- Order three: first check all two-dimensional principal matrices, then apply the exact determinant/adjugate criterion specialized
  to the six distinct symmetric entries.

These avoid allocation and a general search for the most common small final matrices.

### 4.2 One exact triangular sign scan

For order at least four, `scan_copositivity_signs()` scans positive diagonals first and then one off-diagonal triangle. It collects:

- whether every diagonal is positive;
- whether any off-diagonal entry is negative;
- each row's diagonal plus all negative off-diagonal entries;
- the exact value `1^T M 1`;
- the negative-entry graph used by connected components.

The resulting cheap decisions are:

1. **Reject a nonpositive diagonal.** A coordinate vector is an exact nonpositive witness.
2. **Accept nonnegative off-diagonals with positive diagonals.** Every term is nonnegative and some diagonal term is positive.
3. **Accept negative-part diagonal dominance.** For every row require

   $$
   m_{ii}+\sum_{j:m_{ij}<0}m_{ij}>0.
   $$

   This is Liqun Qi, *Linear Algebra and its Applications* 439 (2013), Theorem 10, equation (12).
4. **Reject a nonpositive all-ones value.** If `1^T M 1 <= 0`, the all-ones vector is an explicit witness.

### 4.3 Negative-entry connected components

Create a graph with one vertex per matrix index and an edge `{i,j}` exactly when `mij < 0`. Entries between distinct connected
components are therefore nonnegative.

If the components are `C1,...,Cr`, then

$$
M\text{ is strictly copositive}
\iff
M[C_t,C_t]\text{ is strictly copositive for every }t.
$$

One direction follows by embedding a bad component vector into the full vector. For the other, split any nonnegative vector over
the components: each component quadratic form is positive when that component is nonzero, while all cross-component terms are
nonnegative.

Singleton components need no further work after the positive-diagonal check. A connected graph is sent to the final algorithm
without copying. A disconnected graph is copied into one principal matrix per non-singleton component.

The mathematics is described in:

- `research/STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md`;
- `research/ZISCHG_2023_COPOSITIVITY_CHECK_RELEVANCE.md`;
- `research/EXACT_STABILITY_EARLY_DECISIONS.md`.

The current implementation uses `std::array<bitset64,64>`. This is ideal for FracESSA's maximum dimension 63 but invalid as a
generic representation. The extracted project needs a dynamically sized adjacency representation or must initially omit this
optimization.

### 4.4 Removed positive-definiteness and Z-matrix routes

The final matrix formerly received another exact positive-definiteness factorization. A failed factorization followed by the
symmetric Z-matrix rule could also reject a matrix with nonpositive off-diagonals.

A complete ablation on all 1,078 matrices showed:

- 1,057 exited through earlier cheap routes;
- 21 reached the compared stage;
- positive definiteness and the Z-matrix condition decided none of those 21;
- removing both reduced the relevant median time by 41.82%, a 1.719x speedup;
- median reductions were 12.19% for seven strict matrices and 49.45% for fourteen rejected matrices.

These checks were deliberately removed. The former general fraction-free LDLT implementation is preserved only in
`archive/fraction_free_ldlt.hpp`. Do not restore it to the default pipeline without new corpus evidence.

## 5. Algorithms evaluated

The algorithms do not all decompose the problem in the same way:

| Method | What changes in a child | Dimension | Practical termination property |
|---|---|---:|---|
| Hadeler | Selects a principal face and tests it | Varies with the selected subset | Finite `2^n - 1` outer enumeration, but exponential |
| Optimized cone | Refines one simplicial cone by replacing a generator with a sum | Unchanged | Exact, often very fast, but no useful observed runtime bound |
| Bundfuss | Refines a simplex through an exact convex-combination split | Unchanged | Boundary cases can refine without exposing the zero direction |
| SNC | Slices and partitions the current simplex | Unchanged | Exact experiment; boundary instances may remain unresolved |
| Danninger | Replaces the problem by lower-dimensional half-cone children | Decreases by one | Finite recursion, but one pivot may create combinatorially many children |

Mixing cone and Danninger steps is mathematically legitimate because each exact step covers the complete region represented by its
parent. A child can therefore be decomposed by a different exact method. The difficulty is performance, not logical compatibility:
one unfortunate first split can create a descendant tree that the other method cannot undo cheaply.

### 5.1 Hadeler principal-submatrix enumeration

This was FracESSA's exact fallback before August 6.

It enumerates every nonempty principal subset in increasing cardinality. When it reaches a principal matrix `C`, all proper
principal matrices have already passed, establishing Hadeler's recursive hypothesis.

- `det(C) > 0`: pass this principal matrix.
- `det(C) < 0`: solve `C y = -1` once and reject exactly when `y > 0`.
- `det(C) = 0`: compute one exact nullspace; reject exactly for a one-dimensional one-sign nullspace.

The last two rules replaced the former complete adjugate construction. They are derived specializations of the classical
Cottle-Habetler-Lemke/Hadeler condition, not separate unproved heuristics.

Evidence:

- all 1,069 original reduced-B classifications matched;
- seven complete safe candidate outputs matched;
- the corpus wall-time median improved from 4.660 seconds to 0.995 seconds over nine alternating uncontended CPU-2 runs, a 4.68x
  speedup;
- the method still has worst-case traversal of `2^n - 1` principal subsets.

The complete proof and literature cross-check are in:

- `aidocs/history/INTEGER_STABILITY_COPOSITIVITY_2026-08-06.md`;
- `research/HADELER_ONE_SOLVE_REPLACEMENT.md`;
- `research/HISTORICAL_HADELER_COPOSITIVITY_CHECK_FLOW.md`.

The archived factorization needed to reproduce it is `archive/fraction_free_ldlt.hpp`.

### 5.2 Optimized exact simplicial-cone checker

This is the current production final algorithm.

Algorithmic origin:

- Mathieu Dutour Sikiric, Polyhedral Common;
- `PairDecomposition` and `TestStrictCopositivity`;
- commit `d2252bc89d991fa6df9750ac9647e19b6a9aca02`;
- `src_copos/Copositivity.h`.

FracESSA's implementation was independently written with FLINT integers. It retains neither the basis `V` nor a witness. Each node
stores only `B = V A V^T`. For every negative off-diagonal pair it computes the exact normalized severity

$$
q_{ij}=\frac{b_{ij}^2}{b_{ii}b_{jj}}.
$$

- If `qij >= 1`, the two-generator span contains an exact zero or negative direction, so reject.
- If every off-diagonal entry is nonnegative, this cone is certified.
- Otherwise split the pair with largest `qij` into the two children obtained by replacing `vi` or `vj` with `vi + vj`.

Only one unsplit Gram matrix is copied for the sibling. Each child then needs one exact row-and-column update.

#### Compared with optimized Hadeler

On the enlarged 1,078-case corpus, 21 matrices reached the final algorithm. The one-shot median `Hadeler / cone` ratio was:

- 1.328x overall;
- 1.155x for 14 rejected matrices;
- 167.38x for seven strictly copositive matrices.

The retained experiment summary reports median speedups of 1.146x overall, 1.112x on rejected matrices, and 343.836x on strict
matrices for an earlier repeated 16-case benchmark. The exact numbers differ because of scope and repetition, but both runs show
the same behavior: Hadeler can find some negative witnesses cheaply, while strict cases force an explosive principal-subset
traversal.

#### Compared with unchanged Polyhedral Common

Both binaries used the same release/native/LTO settings. The upstream checker was called directly with its original GMP types and
unchanged source. On the 16 repeated final-branch matrices, the median `upstream / optimized FLINT` ratio was:

- 350.914x overall;
- 3,759.539x on the six strict matrices;
- 288.599x on the ten rejected matrices.

The optimized checker was faster on every matrix. The large difference comes from the FLINT integer representation, Gram-only
state, direct row/column updates, and removal of general upstream machinery—not from changing exact results.

Full material: `experiments/copositivity_cone_2026-08-06/`.

### 5.3 SNC partition algorithm

The separate experiment implements Safi, Nabavi, and Caron, *A modified simplex partition algorithm to test copositivity*, Journal
of Global Optimization 81 (2021), 645-658, DOI `10.1007/s10898-021-01092-1`.

The paper treats non-strict copositivity. The experiment changed terminal inequalities for strict copositivity, retained the slicing
geometry, and used exact fraction-free FLINT integers.

Results on the 1,078-case corpus:

- every strict classification matched;
- no two-second timeout;
- fraction-free integer SNC was 2.091x faster than the rational implementation by the overall median;
- the optimized cone was faster on every measured matrix, by a 9.253x overall median;
- SNC was 57.690x faster than Hadeler on strict matrices;
- Hadeler was 10.535x faster than SNC on rejected matrices.

SNC is useful as an independent exact implementation and as a possible non-strict-copositivity starting point. It was not selected
for production.

Full material: `experiments/copositivity_snc_2026-08-06/`. The source PDF currently remains outside the repository at
`/home/reinhard/Downloads/s10898-021-01092-1.pdf`.

### 5.4 Exact Bundfuss adaptation

The public repository `josmangarsal/copositivity-detection-bundfuss-faces` was cloned into the local experiment. Its original source
is under `experiments/copositivity_bundfuss_flint_2026-08-07/src/cpp/`. The independent FLINT adaptation is
`flint/BundfussFlint.cpp`.

The adaptation uses exact FLINT rationals, rejects exact zero for strict copositivity, keeps the published split parameters, and
stores the Gram matrix directly. It deliberately does not use connected components.

Results:

- 1,077 of 1,078 cases completed correctly;
- no wrong result;
- ID 9161 timed out after five seconds;
- on seven strict final-branch cases, Bundfuss was 1.98x slower than the cone by the median;
- on 13 completed rejected final-branch cases, Bundfuss was 2.05x faster than the cone by the median;
- on the path-negative-graph family, Bundfuss and the ratio-guided cone generated the same node counts, but Bundfuss was about 1.9x
  slower because rational convex-combination updates cost more than integer generator sums.

ID 9161 is a copositive boundary matrix. Repeated refinement can approach its exact zero direction without exposing it in finite
time through the chosen edge. That makes this exact Bundfuss implementation unsuitable as the only terminating path.

Full material: `experiments/copositivity_bundfuss_flint_2026-08-07/`.

Before publishing any copied third-party source, review its licensing. No license file was captured beside the local cloned source.
FracESSA's independent code is GPL-3.0-or-later, but that does not establish the license of the upstream Bundfuss repository.

### 5.5 Direct Danninger recursion

The implementation was reconstructed from snippets of Danninger's 1990 proceedings article, *A Recursive Algorithm for Determining
(Strict) Copositivity of a Symmetric Matrix*. For

$$
A=
\begin{pmatrix}
a&p^T\\
p&B
\end{pmatrix},\qquad a>0,
$$

it tests `B` on the half-cone `p^T y >= 0` and the division-free Schur matrix

$$
aB-pp^T
$$

on `p^T y <= 0`. Mixed-sign half-cones are triangulated lazily. Every child has dimension one less than its parent, giving a finite
exact recursion, but one pivot can generate a very large staircase family.

The fixed-first-coordinate baseline solved 1,076 of the original 1,078 matrices within five seconds with no mismatch. IDs 811 and
813 timed out.

Two local pivot heuristics were rejected:

- minimizing the predicted immediate child count increased timeouts from two to six and made completed strict cases as much as
  12.7x slower;
- choosing the largest diagonal increased timeouts from two to five and made individual completed cases as much as 365x slower.

The lesson is important: a locally attractive Danninger pivot can create much harder descendants.

References and code:

- `research/papers/Danninger/GOOGLE_BOOKS_SNIPPETS.md`;
- `research/papers/Danninger/danninger_1990_recursive_algorithm_strict_copositivity.md`;
- `experiments/copositivity_danninger_2026-08-07/danninger.cpp`.

### 5.6 Fixed alternation between Danninger and cone

Two experiments alternated algorithms at every child:

- `danninger_cone`: Danninger first, then cone, then Danninger, and so on;
- `cone_danninger`: cone first, then Danninger, then cone, and so on.

Both classified all 1,078 original cases correctly without a five-second timeout. Their stress behavior showed why fixed
alternation is not the final answer:

| Matrix | Pure Danninger | Danninger then cone | Cone then Danninger | Pure cone |
|---|---:|---:|---:|---:|
| ID 811, order 22 | `>5 s` | 3.918 s, 352,717 nodes | 2.750 us, 1 node | immediate |
| ID 813, order 23 | `>5 s` | 1.069 s, 92,379 nodes | 2.458 us, 1 node | immediate |
| SPD tridiagonal, order 15 | 0.398 ms, 13 nodes | 1.102 s, 547,480 nodes | 28.598 s, 11,925,123 nodes | `>5 s` |

The first split determines the geometry of the descendant tree. Alternation can prevent complete failure but can still be millions
of times worse than choosing the favorable method.

Both executables are built from `experiments/copositivity_danninger_2026-08-07/cone_danninger.cpp`; CMake defines
`START_WITH_CONE` only for the cone-first target.

### 5.7 Adaptive narrow-Danninger/cone hybrid

This is the best experimental compromise so far.

For a proposed Danninger pivot, let `r` be the number of positive and `s` the number of negative off-diagonal entries in its row.
The two staircase triangulations contain

$$
\binom{r+s}{r}
$$

children. This is at most two exactly when one sign class is empty or `r = s = 1`. The code determines this from signs without
computing the binomial coefficient.

At every node:

1. scan pivots in index order;
2. take the first pivot producing at most two Danninger children;
3. if none exists, perform one optimized cone split;
4. restart at pivot zero in every child.

Original evidence:

- all 1,078 corpus decisions matched;
- no five-second timeout;
- 400 deterministic random integer matrices of dimensions four through eight matched pure Danninger;
- ID 811: 1.000 us, one node;
- ID 813: 6.042 us, four nodes;
- SPD tridiagonal order 15: 49.292 us, 13 nodes.

Enlarged-corpus evidence is less favorable. The August 7 ten-second run over all 1,569 matrices produced:

- 1,489 completed correct classifications, 94.9%;
- 80 timeouts, 5.1%;
- zero mismatches among completed cases;
- zero process errors;
- completed results: 386 strict and 1,103 not strict;
- timed-out references: 41 strict, 19 copositive but not strict, and 20 non-copositive.

The timeout distribution was:

| Order | Timed out / checked | Rate | Strict | Copositive boundary | Non-copositive |
|---:|---:|---:|---:|---:|---:|
| 7 | 1 / 138 | 0.72% | 1 | 0 | 0 |
| 8 | 3 / 135 | 2.22% | 3 | 0 | 0 |
| 16 | 1 / 44 | 2.27% | 1 | 0 | 0 |
| 18 | 4 / 22 | 18.18% | 3 | 1 | 0 |
| 20 | 2 / 22 | 9.09% | 2 | 0 | 0 |
| 21 | 1 / 11 | 9.09% | 1 | 0 | 0 |
| 22 | 1 / 15 | 6.67% | 1 | 0 | 0 |
| 24 | 2 / 16 | 12.50% | 2 | 0 | 0 |
| 28 | 1 / 9 | 11.11% | 1 | 0 | 0 |
| 45 | 1 / 4 | 25.00% | 1 | 0 | 0 |
| 64 | 2 / 6 | 33.33% | 2 | 0 | 0 |
| 70 | 1 / 3 | 33.33% | 1 | 0 | 0 |
| 120 | 1 / 3 | 33.33% | 1 | 0 | 0 |
| 171 | 3 / 3 | 100.00% | 1 | 1 | 1 |
| 200 | 12 / 12 | 100.00% | 4 | 4 | 4 |
| 256 | 2 / 6 | 33.33% | 2 | 0 | 0 |
| 378 | 3 / 3 | 100.00% | 1 | 1 | 1 |
| 400 | 12 / 12 | 100.00% | 4 | 4 | 4 |
| 496 | 1 / 3 | 33.33% | 1 | 0 | 0 |
| 776 | 3 / 3 | 100.00% | 1 | 1 | 1 |
| 800 | 12 / 12 | 100.00% | 4 | 4 | 4 |
| 1,024 | 6 / 6 | 100.00% | 2 | 2 | 2 |
| 1,035 | 3 / 3 | 100.00% | 1 | 1 | 1 |
| 3,321 | 1 / 1 | 100.00% | 0 | 0 | 1 |
| 3,361 | 1 / 1 | 100.00% | 0 | 0 | 1 |

The three order-171 cases were then run without a timeout on three CPUs. None finished within several minutes, and the user stopped
the run. No result rows were stored for that stopped session.

The conclusion is not that the hybrid is wrong. It is that the original corpus was too easy to establish practical robustness.
There is still no useful dimension-based progress estimate or stopping rule.

Code: `experiments/copositivity_danninger_2026-08-07/adaptive_danninger_cone.cpp`.

## 6. Connected-component benchmark

The connected-component reduction was retained in FracESSA production after three checks:

1. all 30 focused copositivity tests, all nine C++ executables, and all 66 Python tests passed;
2. complete safe output matched the pre-change binary for all 768 FracESSA matrices through dimension 25 with sub-second
   calibrations;
3. focused timing compared direct cone with component decomposition only on the 21 matrices reaching the final checker.

End-to-end 81-matrix quick-set changes were neutral at the median:

- fast: -0.24%;
- safe: -0.07%.

Focused final-checker changes were:

- seven strictly copositive matrices: 15.66% faster on average and 17.06% faster at the median, a 1.206x median speedup;
- fourteen rejected matrices: neutral at the median and 5.91% slower on average, because their direct cone runs usually found a
  witness in only a few microseconds.

The isolated driver is `experiments/connected_component_reduction_2026-08-07/benchmark.cpp`. Its compiled binaries are disposable.
The retained numerical summary is in `aidocs/CHANGES.md`, entry 369; no canonical CSV was saved for this experiment.

## 7. Four deliberately bad matrices

These matrices are tagged with family `bad matrices` in `copos_testdata.sqlite3` and should be the first regression set in the new
repository.

| Database ID | Order | Exact class | Why it matters |
|---:|---:|---|---|
| 9161 | 5 | Copositive, not strict | Brás-Eichfelder-Júdice matrix M5; exact Bundfuss timed out while cone rejected in about 21 us after 34 nodes. |
| 9656 | 15 | Strictly copositive | `D*tridiag(-1,2,-1)*D`, with alternating diagonal scale 1 and 2; pathological for pure cone, easy for Danninger. |
| 811 | 22 | Copositive, not strict | Reduced-B matrix derived from QAPLIB `nug24:A`; direct Danninger timed out, cone rejects immediately. |
| 813 | 23 | Copositive, not strict | Reduced-B matrix derived from QAPLIB `nug25:A`; direct Danninger timed out, cone rejects in a few nodes. |

ID 9161 is small enough to retain directly:

```text
5#1,-1,1,2,-3,2,-3,-3,4,5,6,-4,5,-8,16
```

Retrieve the other exact upper triangles from the database rather than copying another textual duplicate.

## 8. Copositivity test database

### 8.1 Location and current contents

Copy:

```text
testdata/copos_testdata.sqlite3
```

Current counts:

- 1,569 permutation-inequivalent exact integer matrices;
- dimensions 1 through 3,361;
- 427 strictly copositive;
- 56 copositive but not strictly copositive;
- 55 non-copositive;
- 1,031 legacy rows known only to be not strictly copositive, with non-strict copositivity still null.

`is_copositive = NULL` is meaningful. It means non-strict copositivity was not established, not that the matrix is non-copositive.

### 8.2 Tables

`matrices` stores:

- exact dimension and upper-triangle text;
- SHA-256 identity;
- strict and nullable non-strict classification;
- optional link to the originating FracESSA matrix;
- size class and creation date.

`matrix_sources` stores:

- source name and URL;
- exact revision or archive identity;
- source entry and family;
- denominator removed during exact import;
- classification proof or basis;
- optional link to a published benchmark row.

`published_benchmarks` stores 56 rows from Tables 2 and 3 of Julius Žilinskas, *Copositive Programming by Simplicial Partition*:
graph data, clique status, search counts, maximum level, time or allowed time, and whether the published run solved the instance.

`tests` stores reproducible local algorithm runs:

- session and timestamp;
- machine and CPU;
- algorithm name and description;
- build label, source reference, Git revision, and binary SHA-256;
- `ok`, `timeout`, or `error`;
- nullable strict and non-strict outputs;
- nullable correctness;
- target, iterations, wall time, median elapsed nanoseconds, and timeout;
- optional node count and diagnostic.

The schema is embedded in the SQLite file. `testdata/schema.sql` belongs to the FracESSA database and is not the schema for this
corpus. Export the copositivity schema explicitly in the new repository instead of copying the wrong file.

### 8.3 Imported sources

The general corpus includes:

- 78 representatives from the 81-file `Copositivity/Matrices` graph corpus;
- 329 representatives from 330 exact rational inputs in `AlexOertel/MinCOP_LDLT`;
- 83 matrices generated from 29 official Second DIMACS Challenge clique graphs;
- nine Brás-Eichfelder-Júdice reference matrices;
- 1,069 original FracESSA reduced-B representatives before later deduplication/import changes;
- the four local stress matrices listed above.

Duplicates and simultaneous row-and-column permutations are collapsed to one representative. Positive scalar multiples should also
be considered the same copositivity problem, but this database's current identity constraint is dimension plus exact matrix hash,
not projective normalization.

### 8.4 Stored August 7 test sessions

The important complete session is:

```text
adaptive-danninger-cone-all-10s-2026-08-07T05-43-43+0300
```

It contains 1,489 correct completions and 80 timeouts.

The earlier session

```text
current-production-all-5s-2026-08-07T05-35-15+0300
```

must not be interpreted as a clean generic production screen. It has 1,457 correct completions, 35 timeouts, and 77 errors. Those
77 errors came from the experiment driver's artificial `dimension <= 63` parser, not from mathematical checker failures.

The full adaptive run initially used seven concurrent workers on CPUs 3 through 9. Large searches exhausted memory before 22 cases
were recorded. At the same time, the laptop's RAM-backed `/tmp` held about 6 GiB of stale files. After cleanup, the 22 missing cases
were rerun with only three workers; every one timed out. The operational lesson is to serialize SQLite writes, commit every matrix,
and sharply bound concurrency for memory-explosive searches.

## 9. Exact file-copy map

The `research/` and `experiments/` directories are ignored by Git. They exist only in the maintainer worktree. They must be copied
manually before the FracESSA worktree is removed; a fresh GitHub clone will not contain them.

### 9.1 Core files to copy as the starting point

| File | What it contains | Extraction note |
|---|---|---|
| `cpp/include/linalg/integer.hpp` | Thin owning and non-owning C++ wrappers around FLINT `fmpz`. | Copy. It is the zero-overhead integer boundary used by every exact kernel. |
| `cpp/include/linalg/matrix_integer.hpp` | Owning C++ wrapper around FLINT `fmpz_mat`. | Copy, then remove the FracESSA-only `matrix_fraction.hpp` dependency and `set_from_fraction_matrix()` if the new parser clears denominators itself. |
| `cpp/include/linalg/copositive_integer.hpp` | Direct 1x1/2x2/3x3 rules, shared sign scan, optimized cone, and connected-component helper. | Copy as source material, **not unchanged**. Separate the dimension-independent cone from the fixed-64 sign/component code. |
| `cpp/tests/test_copositivity.cpp` | Focused exact regressions for small rules, arbitrary precision, sign scan, cone, and components. | Copy and split generic tests from FracESSA-bitset tests. Add the four database stress matrices. |
| `testdata/copos_testdata.sqlite3` | Canonical exact corpus, provenance, literature rows, and algorithm test results. | Copy unchanged first; perform schema migrations only in the new repository. |
| `aidocs/history/COPOSITIVITY_CHECKER_EXTRACTION_HANDOFF.md` | This extraction record and file map. | Copy so the new repository retains the decisions and evidence summarized here. |

`matrix_integer.hpp` currently includes `matrix_fraction.hpp` only because FracESSA converts its parsed rational game there. The
copositivity algorithms themselves need only integer and integer-matrix storage. Do not drag the complete FracESSA fraction type into
the new core merely to preserve this convenience method.

### 9.2 FracESSA integration files to copy as references, not library source

| File | Why keep it |
|---|---|
| `cpp/src/checkstab.cpp` | Shows the exact cheap-route order and final checker call. |
| `cpp/src/find_candidate_safe.cpp` | Contains `build_scaled_reduced_b()`, the Schur-complement producer used by FracESSA. |
| `cpp/include/fracessa/find_candidate_safe.hpp` | Documents the producer's state and preconditions. |
| `cpp/include/linalg/fraction_free_ldlt_kkt.hpp` | Active specialized candidate solver and retained-factor solve used before the copositivity boundary. Not needed by a standalone checker. |
| `cpp/include/fracessa/bitset64.hpp` | Needed only to understand the current component optimization. Do not make it the generic graph representation. |
| `cpp/CMakeLists.txt` | Reference for finding and linking FLINT, MPFR, and GMP. Do not copy the whole FracESSA build. |

The clean integration later should be one call from `checkstab.cpp` into the new library with a `matrix_int` or equivalent exact
integer matrix. The new library should know nothing about supports, candidates, ESS reason strings, or FracESSA logging.

### 9.3 Historical and mathematical files to copy

| File | Purpose |
|---|---|
| `aidocs/history/INTEGER_STABILITY_COPOSITIVITY_2026-08-06.md` | Complete retired integer Hadeler proof, implementation, and benchmark record. |
| `research/HADELER_ONE_SOLVE_REPLACEMENT.md` | Detailed one-solve/nullspace derivation and independent literature links. |
| `research/HISTORICAL_HADELER_COPOSITIVITY_CHECK_FLOW.md` | Historical complete stability/copositivity flow; useful for understanding what was removed. |
| `research/STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md` | Full negative-entry-component proof and unimplemented duplicate-row ideas. |
| `research/ZISCHG_2023_COPOSITIVITY_CHECK_RELEVANCE.md` | Assessment of Zischg's thesis and exact one-row reductions. |
| `research/EXACT_STABILITY_EARLY_DECISIONS.md` | Cheap exact certificates and their costs. |
| `research/papers/bomze_1992.md` and `.pdf` | Source for the specialized low-dimensional strict-copositivity criteria. |
| `research/papers/Hadeler_1983.md` and `.pdf` | Foundational strict-copositivity theorem. |
| `research/papers/Danninger/` | Reconstructed Danninger algorithm and retained Google Books snippets. |
| `research/papers/Zischg_Johannes_2023_Copositivity_Testing.pdf` | Source for graph and row-reduction ideas. |
| `/home/reinhard/Downloads/s10898-021-01092-1.pdf` | Safi-Nabavi-Caron SNC paper; currently outside the repository. |
| `/home/reinhard/Downloads/žilinskas-2011-copositive-programming-by-simplicial-partition.pdf` | Source of the published benchmark tables. |

The large historical flow document describes earlier positive-definiteness, Z-matrix, and Hadeler paths. It is not current
production truth; retain its status label when copying it.

### 9.4 Experiment directories to copy

| Directory | Keep |
|---|---|
| `experiments/copositivity_cone_2026-08-06/` | `README.md`, `CMakeLists.txt`, `cone_benchmark.cpp`, `upstream_polyhedral_benchmark.cpp`, `run.py`, and `results/`. |
| `experiments/copositivity_snc_2026-08-06/` | `README.md`, `CMakeLists.txt`, `snc_benchmark.cpp`, `run.py`, and `results/`. |
| `experiments/copositivity_bundfuss_flint_2026-08-07/` | `FLINT_RESULTS.md`, `flint/CMakeLists.txt`, `flint/BundfussFlint.cpp`, runner, result CSVs, and upstream source only after license review. |
| `experiments/copositivity_danninger_2026-08-07/` | `README.md`, `CMakeLists.txt`, the three algorithm `.cpp` files, `run.py`, and `results/`. |
| `experiments/copositivity_cone_progress_2026-08-07/` | `progress.cpp` and `hadeler.cpp`; evidence that raw node progress gives no useful stopping rule. |
| `experiments/connected_component_reduction_2026-08-07/` | `benchmark.cpp`; use the documented results from this handover and `aidocs/CHANGES.md`. |
| `experiments/copositivity_subset_generation_2026-07-31/` | Historical Hadeler subset-generator comparison; useful only if principal-submatrix enumeration is retained. |
| `experiments/original_a_copositivity_2026-08-06/` | Demonstrates why FracESSA's strategic circular normalization must not be treated as copositivity-preserving. |

Do not copy generated `build/`, `build-*`, binaries, object files, caches, or `upstream-build/`. Rebuild from source. Do not copy a
nested `.git` directory into the new repository.

### 9.5 Archived code

Copy `archive/fraction_free_ldlt.hpp` only if retaining the Hadeler baseline or exact positive-definiteness experiments. The current
optimized cone and adaptive Danninger/cone code do not need it.

### 9.6 Ownership and licensing boundary

The extracted coposit repository should own `integer.hpp`, `matrix_integer.hpp`, and the generic copositivity API. FracESSA should
consume those headers from coposit and call the checker directly. coposit must not include FracESSA headers. This one-way dependency
avoids both duplicated wrapper types and a circular dependency, while preserving the current inline C++/FLINT calls and their speed.
The exact CMake consumption mechanism can be chosen when the repository is created; it does not require a process, serialization,
or shared-library boundary.

Copy FracESSA's `LICENSE` as the starting GPL-3.0-or-later license material. Review `THIRD_PARTY_NOTICES.md`, then retain only the
notices for dependencies actually distributed by the new repository. This does not resolve the separate licensing uncertainty for
the cloned Bundfuss source.

## 10. Generic-repository changes required before claiming unrestricted dimensions

The following are not optional cleanup; they are correctness boundaries.

1. **Remove the fixed sign-scan arrays.** `CopositivitySignScan` currently owns 64 negative-neighbor masks and 64 row sums. Calling
   it with a larger matrix would overrun those arrays.
2. **Replace or omit `bitset64` connected components.** The current component helper supports FracESSA's order at most 63 only.
3. **Remove experiment parser caps.** `adaptive_danninger_cone.cpp`, `connected_component_reduction/benchmark.cpp`, and other drivers
   reject dimensions above 63 even when the algorithm itself could process them.
4. **Do not pass giant matrices through one command-line argument.** The last unbounded experiment used a temporary stdin-capable
   build because upper triangles of order hundreds or thousands exceed practical argument limits. That temporary source and runner
   were not retained. A new CLI should read stdin or a file.
5. **Bound parallelism by memory, not only CPU count.** A search stores many arbitrary-precision child matrices. Seven simultaneous
   large searches exhausted memory.
6. **Keep exact status separate from timeout.** A timeout is `unknown`, never `false`.
7. **Keep strict and non-strict outputs separate.** The present algorithms usually return only strict copositivity. Do not fill the
   non-strict result field by guessing from a strict failure.

The `pending.reserve(64)` call inside the optimized cone is only an initial vector capacity. It is not a dimension limit.

## 11. Minimal proposed shape of the extracted repository

Do not begin with a framework of interchangeable abstract solvers. The smallest useful repository is:

```text
include/
  coposit/integer.hpp
  coposit/matrix_integer.hpp
  coposit/strict_copositivity.hpp
src/
  main.cpp
tests/
  test_strict_copositivity.cpp
testdata/
  copos_testdata.sqlite3
research/
  ...copied notes and papers...
experiments/
  ...copied isolated algorithms and results...
```

The first public API can require a symmetric exact integer matrix and return a strict-copositivity Boolean. Rational parsing can
clear denominators at the boundary. Non-strict copositivity, witnesses, progress reporting, and algorithm selection should be added
only when their exact contracts are decided.

The production FracESSA integration should remain a direct exact call. A separate repository does not require a runtime process,
serialization, or shared library boundary: CMake can consume the checker source directly, preserving inlining and current speed.
The dependency direction must be `FracESSA -> coposit`, never the reverse.

## 12. Open mathematical and algorithmic questions

1. Can the adaptive Danninger/cone rule be given a termination proof, or must it retain a finite direct-Danninger fallback?
2. Can cone pair selection use a condition or direction-quality measure that avoids the positive-definite tridiagonal pathology
   without destroying its behavior on IDs 811 and 813?
3. Is there a useful dimension- and geometry-dependent resource estimate? Raw node count and matrix order were both inadequate.
4. Can non-strict and strict copositivity share most of the engine while preserving exact boundary behavior?
5. Can Danninger choose a pivot using descendant information cheaply enough to beat fixed-first recursion? The two tested local
   heuristics made results worse.
6. Can duplicate-row reduction be made exact and cheap enough to add after connected components? It is proved in the research note
   but was not implemented.
7. Which imported legacy strict-false matrices are non-strict copositive and which are non-copositive? The database intentionally
   leaves 1,031 non-strict classifications unknown.
8. What licensing terms apply to the copied Bundfuss repository? Resolve this before redistribution.

## 13. Current recommendation

Preserve three layers separately:

1. **Current exact core:** FLINT integer wrappers, direct small criteria, cheap sign certificates, optimized cone, focused tests, and
   the SQLite corpus.
2. **Optional exact reduction:** dynamically sized negative-entry connected components.
3. **Research engines:** Hadeler, SNC, Bundfuss, direct Danninger, fixed alternation, and adaptive narrow-Danninger/cone.

Do not silently replace FracESSA's production cone with the adaptive hybrid yet. The hybrid was excellent on the old corpus but
timed out on 80 matrices in the expanded corpus. The new repository is the right place to make these engines independently runnable,
continue exact corpus testing, and decide the default with evidence.
