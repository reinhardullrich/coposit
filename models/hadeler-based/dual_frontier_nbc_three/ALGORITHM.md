# Dual-Frontier NBC Three

## Idea

`dual_frontier_nbc_three` is a coposit-created experiment based on `dual_frontier_nbc_two`. Before walking from an NBC seed, it
processes that seed with the complete exact Improved NBC-B7 logic and stores the resulting Halfspace–Rays Dickinson interval even
when the interval does not reach the full support. It then performs the same coverage-aware dual-frontier walk:

- a large support whose principal matrix is positive definite, giving downward pruning; and
- a small support whose reduced Hessian is not positive definite, giving upward pruning.

The walk also evaluates the traditional Dickinson right-hand side at every step. It retains only candidates whose upper endpoint
appears to be the full index set. Floating point never proves anything: every retained candidate is recomputed with exact integer
arithmetic before it changes the search.

The name writes out `three` to identify the third dual-frontier experiment. NBC is the model's cardinality-aware SAT support
generator inherited from `improved_nbc_b7`.

## Source and classification

This is a coposit-created variant, not a literature baseline. Its implementation is copied from
`models/hadeler-based/dual_frontier_nbc_two`. Its only new policy is exact Dickinson processing of every emitted seed before the
walk begins.

## Notation

Let $A\in\mathbb Z^{n\times n}$ be symmetric and let $I\subseteq[n]$ be nonempty. Write $A_I$ for the principal submatrix on
$I$. On the simplex face with support $I$, let $H_I$ denote the reduced Hessian of $x^TAx$.

Two hereditary facts drive the walk:

1. If $A_I\succ0$, then every principal submatrix $A_J$, $J\subseteq I$, is positive definite. Hence $I$ gives a valid
   downward closure.
2. If $H_I\not\succ0$, then no superset can restore strict positive curvature on all tangent directions. Hence $I$ gives a
   valid upward closure in the Hadeler traversal.

Between these regions one may have

\[
H_I\succ0,\qquad A_I\not\succ0.
\]

Those middle supports are handled by the ordinary exact Dickinson fallback.

## Initial pair pruning

Before the walk starts, every pair \(\{i,j\}\) is tested exactly. Its one-dimensional tangent curvature is

\[
A_{ii}+A_{jj}-2A_{ij}.
\]

If this is nonpositive, every superset containing the pair is excluded from the Hadeler search. The pair rule is installed directly
in the NBC generator.

## Exact seed processing

NBC first emits an uncovered support $I$ from the smallest unfinished cardinality. The model immediately applies the complete
exact Improved NBC-B7 decision flow to $I$: exact LDLT, curvature tests, the traditional solve $A_Iu=\mathbf1$, recursive
half-space improvement, and the ray sweep. A negative witness ends the classification. Otherwise every valid resulting Dickinson
interval $[L,U]$ is installed in NBC, including a bounded interval with $U\ne[n]$.

The walk still starts from the original $I$. Its new seed interval may cover $I$ and some immediate extensions, but extensions
outside $U$ can remain uncovered. A ceiling interval covers every upward continuation and therefore ends the walk immediately.

## Floating dual-frontier walk

The NBC generator emits the next uncovered low-frontier support $I$. For every proposed extension $I\cup\{j\}$, the model first asks
NBC whether that support is covered by an active or pending exact certificate. Covered extensions are excluded before comparing
Schur pivots. Thus the walk chooses the best-scoring uncovered extension, not merely the best-scoring extension.

If no uncovered one-index extension remains, the path stops. This loses no correctness: every skipped support is already certified,
and NBC will later emit any still-uncovered higher support independently. If the terminal support differs from the already processed
seed, it is checked exactly for both a downward and an upward closure.

### Principal-positive-definite phase

For the current support, the model forms a floating-point $LDL^T$ factorization of $A_I$. For each unused index $j$, it
computes the one-index Schur pivot

\[
s_j=A_{jj}-A_{jI}A_I^{-1}A_{Ij}.
\]

Among candidates with a safely positive pivot, it adds the index with the largest pivot. This continues until no proposed
one-index extension remains positive definite. The last floating-point positive-definite support is saved as a downward candidate.

### Reduced-Hessian phase

Keeping one support index as the simplex anchor, the model factors the floating reduced Hessian. For each unused index it computes
the corresponding tangent Schur pivot. It adds the index with the smallest pivot, because that is the cheapest local direction
toward loss of positive curvature. The first support whose proposed pivot is nonpositive is an upward candidate.

The upward candidate is greedily polished: an index is removed whenever the smaller support still appears to have a non-positive-
definite reduced Hessian. This produces one inclusion-minimal candidate for the chosen path, not every minimal frontier point.

### Floating Dickinson nomination

At every support visited by the path, the model solves in floating point

\[
A_Iu=\mathbf1.
\]

It computes $Au$ after embedding $u$ in the full coordinate space. A support is retained as a ceiling candidate only when

\[
u\notin-\mathbb R_+^I
\quad\text{and}\quad
Au\ge0

\]

within the floating screening tolerance. The second condition says that the proposed Dickinson upper endpoint is all of $[n]$.
Partial floating Dickinson intervals are deliberately ignored by the walk.

The floating solve uses scaled dense Gaussian elimination with partial row pivoting. A small or nonfinite pivot makes the proposal
inconclusive; it cannot produce a certificate or a negative answer.

## Exact verification

After the complete floating path has been fixed, its candidates are replayed with exact arbitrary-precision integer arithmetic.
This delay prevents a certificate found early on the path from changing which later frontier candidate is examined.

When coverage stops a path, the terminal support is checked in both exact directions. The model installs a downward closure if its
principal matrix is positive definite (or the inherited singular PSD rule applies), and an upward closure if its reduced Hessian is
not positive definite. The seed itself is not processed twice.

### Downward candidate

The model exactly factorizes $A_I$. It installs the downward closure only if exact LDLT proves $A_I\succ0$. If floating point
mistook a singular matrix for a positive-definite matrix, the existing exact positive-semidefinite consistent rule may still certify
the face; otherwise the candidate is discarded.

### Upward candidate

The model exactly factorizes $A_I$ and applies the inherited exact reduced-Hessian test. It installs the upward closure only if
exact arithmetic proves $H_I\not\succ0$. Otherwise the proposal is discarded.

### Ceiling Dickinson candidate

For a nonsingular candidate, exact LDLT solves $A_Iu=\mathbf1$ and computes every entry of $Au$. The model accepts the candidate
only if all products are exactly nonnegative. It then installs the upward closure rooted at

\[
L=\operatorname{supp}(u).
\]

If $u\le0$, then $-u\ge0$ is an exact negative witness and the matrix is not copositive. If $u\ge0$ and $u^TA_Iu=0$, the
matrix is not strictly copositive. Singular floating candidates are discarded; the ordinary exact singular branch remains the
fallback.

## Exact seed decision flow

Before walking, every seed is processed by the copied exact `improved_nbc_b7` logic:

1. factor $A_I$ exactly;
2. reject an exact negative witness;
3. install an upward curvature closure if $H_I\not\succ0$;
4. otherwise construct and optimize the exact Dickinson vector using the copied half-space and ray steps.

Unlike the first two dual-frontier experiments, this model sends every valid optimized seed interval to NBC. Floating Dickinson
candidates encountered later on the walk are still retained only when their exact upper endpoint is $[n]$.

There is no separate high-frontier traversal. Supports are emitted only in increasing cardinality. Downward closures come from the
verified principal-positive-definite frontier candidates found by the walk. `both` performs CP/SCP classification in one traversal.

## Termination and correctness

All decisive tests are exact. Floating-point false positives are discarded by verification; floating-point false negatives only
lose an optional pruning opportunity. Every support of the finite Boolean lattice is either removed by an exact closure or
eventually emitted once by the upward NBC iterator. The traversal therefore remains a complete fallback.

Timeout checks occur in matrix conversion, candidate scoring, exact products, and support enumeration. A timeout remains unresolved
and is never returned as a negative classification.

## Known Difficult Inputs

The walk finds only one greedy path per uncovered seed. A matrix may have many incomparable maximal positive-definite supports and
minimal non-positive-curvature supports, so the chosen Schur order can miss more useful frontier points. Different walks may still
revisit the same uncovered intermediate support; this model removes redundant travel through covered regions, not all duplicate
floating work. Nearly singular floating pivots deliberately become inconclusive and reduce the walk to the exact fallback.

Retaining every seed interval can make NBC expensive when many bounded certificates overlap without covering much of the remaining
lattice. Conversely, forbidding the walk from crossing those intervals can hide a useful frontier point beyond them, although the
ordinary NBC traversal remains complete. In that regime certificate storage and support generation can dominate runtime.
