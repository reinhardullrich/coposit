# Dickinson Final

Classification: selected coposit-created final model copied from the maintained `dickinson_2019` baseline. Its solver logic, tests,
and trace behavior are currently unchanged; its trace helper is local so the model remains self-contained. The separate model
identity allows later deliberate changes without altering the literature baseline.

## Decision Modes

Both modes build the same finite Dickinson certificate and use the same subset order, coverage relation, solve, nullspace choice, and
stored signatures. A nonsingular solve with $w\leq0$ gives a genuinely negative nonnegative witness and rejects both predicates. The
difference is a generated nonnegative null vector $w\geq0$, $w\neq0$: non-strict mode stores its valid certificate signature and
continues, because $w^TAw=0$ is allowed; strict mode returns `false` immediately.

Completing non-strict mode proves $A$ copositive by Theorem 4.6. Completing strict mode without generating a nonnegative zero proves
strict copositivity through Lemma 5.2 and Corollary 5.3. `solve(A, mode)` returns only the selected Boolean and defaults to
`strictly_copositive`. `classify(A)` instead performs one non-strict-complete certificate traversal and reports both
`is_copositive` and `is_strictly_copositive`. It does not run the two modes separately. Strict-only `solve` remains cheaper on a
boundary matrix because it stops at the first generated nonnegative zero, while combined classification must retain that
certificate and finish the non-strict proof.

As coposit's public `safe` method, this solver follows the shared fused component/pre-check pipeline. Globally valid checks run during
the root scan; Frank–Wolfe and exact definiteness are deferred to each negative-entry component. Those stages may decide a matrix
before Dickinson enumeration begins; otherwise every surviving component follows the algorithm below. The standalone model target
and explicitly configured Python calls may still invoke the final model without that composition.

## What The Algorithm Does

Dickinson's algorithm builds a finite collection of vectors that certifies copositivity. It considers supports: sets of coordinates
on which a possible critical nonnegative vector could live. For a support that has not already been explained by an earlier
certificate vector, it solves one exact linear system, or takes one nullspace vector when the corresponding principal matrix is
singular.

One certificate vector can cover many larger supports, so the algorithm may skip a substantial part of the theoretical $2^n-1$
search space. The worst case still visits every nonempty principal subset. Unlike the geometric models, it does not divide the
simplex or the nonnegative cone.

## Final Model Identity

The model is named `dickinson_final` because it is coposit's selected Dickinson-based implementation rather than the frozen
literature baseline. It was copied in full from `models/baselines/dickinson_2019/`; every retained certificate, coverage test,
nonsingular solve, singular-vector choice, and final proof is initially identical. Its source-trace helper is copied locally so the
final model is self-contained. The baseline remains separately buildable for faithful comparisons.

The paper reports that the algorithm had not been implemented, so there is no author executable or author timing path to reproduce.
Increasing-cardinality subset order, numeric-mask order, the admissible null-vector selection, packed signatures, and exact LDLT are
documented implementation choices left open by the paper or lossless representations of its decisions.

## Name And Source

The identifier follows coposit's `<first-author>_<year>` rule. It names Peter J. C. Dickinson and the publication year of:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

The model implements the paper's Algorithms 1 and 2. The non-strict-copositivity certificate is Theorem 4.6. The strict conclusion
used by coposit follows from Lemma 5.2 and Corollary 5.3. Dickinson explicitly reports that the paper's algorithm had not been
implemented; there is therefore no original program whose traversal order can be copied.

## Terms Used Below

For a vector $u$:

- its **support**, written $\operatorname{supp}(u)$, is the set of indices where $u_i\neq0$;
- its nonnegative-product set is
  \[
  N_A(u)=\{i:(Au)_i\geq0\};
  \]
- $A_I$ is the principal matrix formed by keeping the rows and columns in an index set $I$.

The implementation embeds every vector calculated for $A_I$ into the full space by putting zeros outside $I$.

## When A Subset Is Already Covered

A previously generated vector $u$ covers a subset $I$ exactly when

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

This is exactly the coverage condition in Theorem 4.6. A certificate vector must also lie outside $-\mathbb R_+^n$, meaning that
it has at least one positive component. In the singular branch the implementation orients the vector to ensure this.

When the displayed containment holds, the certificate already accounts for the subset $I$, so the algorithm skips $A_I$. coposit
stores only $\operatorname{supp}(u)$ and the sign of every component of $Au$, because those are
the only facts needed for later coverage tests.

The current subset $I$, $\operatorname{supp}(u)$, and $N_A(u)$ use coposit's shared packed support representation. Each support stores
$\lceil n/64\rceil$ unsigned 64-bit words, so coverage is exactly two wordwise subset tests. The traversal also keeps its ordered
index vector because forming $A_I$ requires those indices directly; maintaining both views avoids converting the packed support on
every uncovered subset.

Retained signatures are partitioned by the lowest index in $\operatorname{supp}(u)$. Any signature that covers $I$ must have that
index in $I$, so coverage checks inspect only the buckets named by the indices of $I$. Each bucket is searched newest first. The
result remains the same Boolean existence test over all eligible signatures; only signatures that cannot cover $I$ are omitted from
the scan.

## Processing An Uncovered Subset

Let $C=A_I$.

### Nonsingular principal matrix

Solve the single system

\[
Cw=\mathbf 1
\]

exactly. If $w\leq0$, then $z=-w\geq0$ and

\[
z^TCz=w^TCw=w^T\mathbf1<0.
\]

Thus $z$, embedded in the full coordinate space, is an explicit negative witness and the complete matrix is not copositive.

If $w$ has a positive component, embed it as the next certificate vector.

### Singular principal matrix

Choose one nonzero exact vector in the nullspace of $C$:

\[
Cw=0.
\]

Orient its sign so that it has a positive component. This gives the certificate vector required by the paper's singular branch. If
the resulting embedded vector is nonnegative, it is also a zero of the quadratic form and will matter for the final strict test.

The fraction-free LDLT factorization stops with exact rank $r<m$. The implementation sets one free coordinate in its transformed
system, solves the completed triangular equations backwards, and reverses the factorization's coordinate operations. The result is
one nonzero exact integer vector $w$ with $Cw=0$, for every positive nullity $m-r$. It does not construct a nullspace basis.

### Information retained from the vector

For every accepted $w$, the implementation calculates:

1. the support of its full embedded vector $u$;
2. the sign pattern of $Au$, used for future coverage;
3. whether $u\geq0$ and $u^TAu=0$.

The full vector is then discarded. Keeping only this signature saves memory without changing any later decision.

## Mode-Dependent Zero Termination

The published algorithms decide non-strict copositivity. coposit must distinguish a strictly copositive matrix from a copositive
matrix that has a nonnegative zero.

Any generated nonnegative zero is already a direct proof that strict copositivity fails; Corollary 5.3 is not needed for that
direction. The converse is the important part: if the matrix has a nonnegative zero, Lemma 5.2 and Corollary 5.3 ensure that a
completed non-strict certificate contains a minimal one, up to positive scaling. Consequently:

\[
A\text{ is strictly copositive}
\iff
\text{the completed certificate contains no nonnegative zero}.
\]

In non-strict mode a generated nonnegative zero is retained as a non-strict Dickinson certificate and traversal continues. In strict
mode it returns `false` immediately. Corollary 5.3 is needed for the strict converse: if the finite traversal finishes without
encountering such a zero, its completed certificate proves that no minimal nonnegative zero exists.

### One-pass combined result

The combined operation starts with `{is_copositive = true, is_strictly_copositive = true}` and follows the non-strict traversal.
The first generated nonnegative zero changes only the strict field to `false`; its signature is retained, and certificate
construction continues. A later negative witness changes both fields to `false` and stops. If traversal completes, non-strict
copositivity is proved and the remembered zero flag distinguishes strict copositivity from its boundary.

The only possible completed pairs are therefore `{false, false}`, `{true, false}`, and `{true, true}`. `{false, true}` is
impossible because strict copositivity implies copositivity. All coverage tests, exact solves, null-vector choices, signatures, and
subset order are shared with non-strict mode.

## Complete Control Flow

1. Visit subset sizes $1,2,\ldots,n$.
2. Within one size, visit subsets in increasing numeric-bit-mask order.
3. Skip $I$ when a retained certificate signature covers it.
4. Otherwise form $A_I$ and either solve $A_Iw=\mathbf1$ or take a nullspace vector.
5. Reject immediately if the nonsingular solve produces $w\leq0$.
6. When the new vector is a nonnegative zero, retain it in non-strict or combined mode, recording strict failure in combined mode;
   return `false` immediately in strict mode. Otherwise retain its certificate signature.
7. After all uncovered subsets have been processed, return `true`.

The traversal is finite because there are only $2^n-1$ nonempty subsets.

## Known Difficult Inputs

Dickinson avoids subsets only when an existing vector covers a wide interval

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

It struggles when generated vectors have $N_A(u)$ only slightly larger than their own support. Such a vector covers few additional
subsets, so the algorithm approaches the full $2^n-1$ traversal and performs an exact factorization for many of them.

On boundary matrices whose first nonnegative zero has large support, the algorithm may still enumerate many earlier supports before
generating that decisive zero. Once it is generated, strict mode stops without processing any remaining supports, whereas non-strict
mode keeps the signature and finishes the copositivity certificate.

Matrices with many singular principal submatrices are also unfavorable. They still require exact factorization and can produce
kernel vectors with weak coverage, although recovering one vector from the retained partial factorization avoids a second
elimination.

## Exact Arithmetic And Fidelity

The nonsingular solve uses fraction-free LDLT. It stores the numerator of $w$ together with a positive denominator. Multiplying a
vector by a positive value preserves its signs, support, coverage, witness status, and zero status, so the denominator need not be
carried into later checks. Singular vectors are exact integers recovered from the same partial factorization.

The principal-matrix, one-column solution, and full-product storage are reused between uncovered subsets. Only the lower triangle
of each principal matrix is copied because the symmetric fraction-free LDLT implementation reads and overwrites that triangle
exclusively. These are representation and allocation optimizations and do not change the generated vectors or certificate.

The paper fixes the coverage theorem and both branches for every uncovered subset but leaves the subset order and admissible
singular nullspace vector open. Increasing cardinality, numeric-mask order, and one vector obtained from the first free coordinate
of coposit's LDLT factorization are deterministic implementation choices. For nullity greater than one this vector can differ from
another valid basis choice and therefore cover different later subsets; Dickinson's Algorithm 2 explicitly permits any nonzero
null vector outside the nonpositive orthant. Retaining only the coverage signature and storing its two sets as packed words are
lossless representation optimizations.

The strict zero-termination rule is a coposit mode layered on the paper's non-strict Algorithm 1. The model itself adds no direct
low-order test, cone subdivision, connected-component reduction, or other solver's certificate vectors or pruning rules. The public
`safe` composition applies the independent shared prechecks once before entering this solver; analysis callers that select no
preprocessing run the Dickinson traversal directly.

Timed native-module builds observe a shared signal flag at principal-subset, certificate, factorization, and matrix-row boundaries
and return a distinct timeout outcome. Standalone model and test builds compile those checkpoints to no-ops, so they add no timer
thread, clock read, signal handler, or changed certificate decision.
