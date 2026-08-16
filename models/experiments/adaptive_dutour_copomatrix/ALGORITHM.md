# Adaptive Dutour–COPOMATRIX

Classification: coposit-created experimental CP/SCP predicate model.

Public mode boundary: `solve(A, copositivity_mode::copositive)` tests copositivity (CP), while
`solve(A, copositivity_mode::strictly_copositive)` tests strict copositivity (SCP). The model does not implement combined `both`
classification; callers that need both predicates must select them separately.

## Idea In Plain Language

This model combines two exact geometric operations:

- **Dutour subdivision** keeps the current matrix order and covers the current cone by two smaller cones.
- **COPOMATRIX projection** removes one coordinate and creates one or more problems whose order is one smaller.

COPOMATRIX gives useful guaranteed diagnostics, but a row containing many positive and negative entries can make it create a
combinatorial number of children. Dutour always creates only two children, but it may keep subdividing at the same order for an
unbounded time.

The hybrid therefore uses this rule at every node:

1. If some COPOMATRIX pivot creates at most two immediate children, use the first such pivot.
2. Otherwise use one Dutour split.
3. If a branch reaches 100 consecutive Dutour splits without an order reduction, force COPOMATRIX at pivot zero even when it creates
   more than two children.

The cutoff is branch-local. Every COPOMATRIX child has lower order and restarts the Dutour count at zero.

## Name And Sources

**Adaptive** means the operation is chosen again at every child. **Dutour** identifies Mathieu Dutour Sikirić's maximum-ratio
two-cone subdivision. **COPOMATRIX** is Jia Xu and Yong Yao's published algorithm name.

The COPOMATRIX mathematics comes from:

> Jia Xu and Yong Yao, “An Algorithm for Determining Copositive Matrices,” *Linear Algebra and its Applications* 435(11),
> 2784–2792 (2011), [DOI 10.1016/j.laa.2011.04.038](https://doi.org/10.1016/j.laa.2011.04.038), retained locally as
> [`xu_yao_2011_copomatrix.pdf`](../../../research/papers/xu_yao_2011_copomatrix.pdf).

The exact maintained reconstruction is documented in
[`../../baselines/copomatrix_2011/ALGORITHM.md`](../../baselines/copomatrix_2011/ALGORITHM.md).

The Dutour operation comes from:

- Mathieu Dutour Sikirić's Polyhedral Common `PairDecomposition` implementation;
- pinned strict-copositivity revision
  [`d2252bc89d991fa6df9750ac9647e19b6a9aca02`](https://github.com/MathieuDutSik/polyhedral_common/commit/d2252bc89d991fa6df9750ac9647e19b6a9aca02);
- the maintained [`../../baselines/dutour_2018/ALGORITHM.md`](../../baselines/dutour_2018/ALGORITHM.md).

The direct low-order rules and adaptive routing start from coposit's maintained
[`../adaptive_dutour_danninger/`](../adaptive_dutour_danninger/) model. No paper describes this hybrid, its narrow-pivot rule, or its
100-split cutoff.

## Public Decision Problem

For a nonempty square symmetric integer matrix (A), the selected mode decides either

\[
x^TAx\geq0\quad\text{for every }x\geq0\qquad\text{(CP)},
\]

or strict copositivity, \(x^TAx>0\) for every nonzero \(x\geq0\) (SCP). Equality is accepted by CP and rejected by SCP.

The input parser supplies a nonempty square exactly symmetric matrix; the model assumes that contract without revalidating it. A
cooperative timeout remains unresolved and is never returned as `false`.

## Node State And Invariant

A recursive node stores:

- one exact symmetric integer Gram matrix (C);
- a nonnegative integer `dutour_streak`.

The matrix describes the original quadratic form on the current simplicial cone. If its current ray matrix is (R), then

\[
(Rz)^TA(Rz)=z^T(R^TAR)z,
\qquad z\geq0,
\]

and the stored matrix is (C=R^TAR), possibly after previous coordinate eliminations.

The counter is the number of consecutive Dutour splits on the current root-to-node branch since the last COPOMATRIX reduction. It
starts at zero, increases by one in both Dutour children, and resets to zero in every COPOMATRIX child. It is not a global node count
and activity in one sibling does not change another sibling.

For either selected predicate \(P\in\{\operatorname{CP},\operatorname{SCP}\}\), every transformation is an exact conjunction:

\[
\operatorname{P}(C)
\iff
\operatorname{P}(C_1)\land\cdots\land\operatorname{P}(C_k).
\]

The solver returns false on the first failed child and true only after every required child passes.

## Complete Routing Rule

For a node (C) of order (n):

1. Apply the exact direct criterion when (n\leq3).
2. Reject a negative diagonal in CP mode or a nonpositive diagonal in SCP mode.
3. Scan pivot rows (0,1,\ldots,n-1) and choose the first row whose COPOMATRIX projection has at most two immediate children.
4. If such a row exists, apply COPOMATRIX with that pivot and reset the counter in all children.
5. Otherwise, if `dutour_streak >= 100`, force COPOMATRIX with pivot row zero and reset the counter in all children.
6. Otherwise apply one maximum-ratio Dutour split and give both children `dutour_streak + 1`.

Thus a branch may contain exactly 100 consecutive Dutour splits. At the next node, COPOMATRIX is forced before another Dutour split
can occur.

Exact control-flow pseudocode is:

```text
check(C, streak, mode):
    checkpoint

    if order(C) <= 3:
        return direct_mode_test(C)

    if any diagonal entry fails mode (< 0 for CP, <= 0 for SCP):
        return false

    pivot := first row whose COPOMATRIX child count is <= 2
    if pivot exists:
        return copomatrix(C, pivot)       # every recursive child gets streak 0

    if streak >= 100:
        return copomatrix(C, 0)           # forced even when the child count is large

    (C_left, C_right) := dutour_split(C)
    if check(C_left, streak + 1) is false:
        return false
    return check(C_right, streak + 1)
```

## Direct Orders One Through Three

Order zero is treated internally as vacuously true, although public input cannot be empty.

The shared exact low-order checker uses non-strict comparisons for CP and strict comparisons for SCP. In particular, order one
requires \(c_{11}\geq0\) for CP and \(c_{11}>0\) for SCP.

For order two,

\[
C=\begin{pmatrix}a&b\\b&c\end{pmatrix}
\]

passes CP exactly when \(a,c\geq0\) and either \(b\geq0\) or \(ac-b^2\geq0\). Replacing those three non-strict comparisons by strict
diagonal and determinant comparisons gives SCP.

For order three, the model first applies that exact rule to all three principal pairs. It then uses the determinant and adjugate form
of Hadeler's order-three criterion, exactly as in the existing adaptive model. All comparisons include equality correctly and use
integer arithmetic.

## COPOMATRIX Projection

Choose a pivot coordinate and permute it conceptually to the first position. Write

\[
C=\begin{pmatrix}
a&p^T\\
p&B
\end{pmatrix},
\qquad a\geq0\text{ in CP mode and }a>0\text{ in SCP mode}.
\]

For a nonnegative vector ((t,y)),

\[
q(t,y)=at^2+2t\,p^Ty+y^TBy.
\]

The principal child (B) is always required because (t=0) is allowed. If \(a=0\), which can occur only in CP mode after the
diagonal test, any \(p_i<0\) gives a negative value for \(e_0+\varepsilon e_i\) with sufficiently small \(\varepsilon>0\).
If instead \(p\geq0\), CP of \(C\) is exactly CP of \(B\), so the implementation checks \(B\) and creates no Schur child.

For \(a>0\), completing the square gives

\[
q(t,y)
=a\left(t+\frac{p^Ty}{a}\right)^2
+\frac{1}{a}y^T\left(aB-pp^T\right)y.
\]

Define the division-free Schur matrix

\[
S=aB-pp^T.
\]

When (p^Ty\geq0), the minimum over (t\geq0) occurs at (t=0), which the principal child covers. When (p^Ty\leq0), the feasible
minimum occurs at (t=-p^Ty/a), so (S) must satisfy the selected CP or SCP inequality on

\[
\{y\geq0:p^Ty\leq0\}.
\]

Therefore

\[
\operatorname{P}(C)
\iff
\operatorname{P}(B)
\land
\left[y^TSy\mathrel{\bowtie}0\text{ for every eligible }y\geq0\text{ with }p^Ty\leq0\right],
\]

where \(P\) is CP with \(\bowtie=\geq\), or SCP with \(\bowtie=>\) and nonzero \(y\).

### Pivot sign classes

In the reduced coordinate order, let:

- (P) contain positions where (p_i>0);
- (N) contain positions where (p_i<0);
- (Z) contain positions where (p_i=0).

Let (s=|P|) and (t=|N|). Positive diagonal scaling changes every nonzero (p_i) to its sign without changing CP or SCP. In this
normalized geometry, the negative half-simplex has:

- each negative coordinate ray (e_j), (j\in N);
- a midpoint of every positive-negative edge.

The implementation represents a normalized midpoint by the primitive integer ray

\[
r_{ij}=\frac{|p_j|}{g}e_i+\frac{p_i}{g}e_j,
\qquad g=\gcd(p_i,|p_j|),
\]

for (i\in P) and (j\in N). This is a positive scaling of the paper's rational ray and lies exactly on (p^Tr_{ij}=0).

### Xu–Yao staircase

The negative half-simplex is triangulated depth-first. Starting with ordered positive and negative lists and all zero coordinate
rays already present:

```text
staircase(P[i:], N[j:], rays):
    if all positive labels have been consumed:
        append the remaining negative coordinate rays
        check the resulting transformed Schur child

    else if only N[j] remains:
        append e_N[j]
        append every boundary ray r_(P[k],N[j]) for k >= i
        check the resulting transformed Schur child

    else:
        append r_(P[i],N[j])
        first recurse after consuming P[i]
        then recurse after consuming N[j]
```

For each final ray matrix (R), the child is

\[
R^TSR.
\]

Each ray has at most two nonzero integer coefficients, so one child entry uses at most four coefficient-matrix products.

### COPOMATRIX child count and the narrow gate

The principal child (B) is always checked.

- If (t=0), there is no Schur child: total child count is one.
- If (s=0) and (t>0), the whole negative region is one simplex: total child count is two.
- If (s,t>0), the negative staircase has

  \[
  \binom{s+t-1}{s}
  \]

  Schur children, giving

  \[
  1+\binom{s+t-1}{s}
  \]

  immediate children in total.

This total is at most two exactly when (s=0) or (t\leq1). The adaptive scan therefore chooses the first row with no positive
off-diagonal entry or at most one negative off-diagonal entry. Zero entries do not affect the count.

The source COPOMATRIX algorithm always uses the first coordinate. Selecting another coordinate here is an exact coordinate
permutation before applying the same projection theorem. Only the adaptive route changes; the projection equivalence does not.

### Recursive restart

Every principal or transformed Schur child has order (n-1). If it is entrywise nonnegative, it passes CP immediately; it also
passes SCP when its diagonal is positive. Otherwise it re-enters the complete adaptive algorithm with `dutour_streak = 0`; it does
not continue as pure COPOMATRIX automatically.

## Dutour Split

When no COPOMATRIX pivot is narrow and the cutoff has not been reached, the model examines every negative entry (c_{ij}).

On the two-generator face spanned by the current rays (v_i,v_j), the selected predicate requires

\[
c_{ij}^2\leq c_{ii}c_{jj}\quad\text{for CP},
\qquad
c_{ij}^2<c_{ii}c_{jj}\quad\text{for SCP},
\]

when \(c_{ij}<0\). A strict reverse inequality gives a negative witness and rejects both modes. Equality gives a zero witness and
rejects only SCP.

Otherwise it selects the first pair maximizing

\[
\rho_{ij}=\frac{c_{ij}^2}{c_{ii}c_{jj}}.
\]

Ratios are compared exactly by cross multiplication. If no negative entry exists, entrywise nonnegativity proves CP; after the
strict diagonal test it also proves SCP.

For the selected (i<j), Dutour covers the current cone by replacing one endpoint at a time with the sum ray (v_i+v_j):

- first child: replace (v_i) by (v_i+v_j);
- second child: replace (v_j) by (v_i+v_j).

In Gram-matrix form, replacing generator (i) gives

\[
c'_{ii}=c_{ii}+2c_{ij}+c_{jj},
\qquad
c'_{ik}=c_{ik}+c_{jk},
\]

with all unaffected entries unchanged. The two children have the same order as their parent. Both receive `dutour_streak + 1`.

## Acceptance, Rejection, And Witnesses

The model rejects when any of the following exact events fails the selected mode:

- a diagonal entry is negative for CP or nonpositive for SCP, witnessed by a coordinate ray;
- a direct order-two or order-three criterion fails;
- a negative Dutour pair satisfies \(c_{ij}^2>c_{ii}c_{jj}\), or equality in SCP mode;
- a CP zero-diagonal row contains a negative off-diagonal entry;
- any required principal or Schur child rejects.

It accepts a node when:

- a direct low-order criterion passes;
- no negative entry gives the entrywise-nonnegative CP certificate, and positive diagonal strengthens it to SCP;
- or every child in the exact cone cover or projection conjunction passes.

The current API returns only the Boolean classification. It does not reconstruct a witness in original input coordinates.

## Exact Arithmetic And Representation

All matrix entries, Schur products, determinant tests, ratio cross-products, greatest common divisors, ray coefficients, and Gram
updates use FLINT arbitrary-precision integers.

The implementation uses:

- no floating-point values or tolerances;
- no rational matrix storage;
- no fixed-width sign mask or dimension limit;
- no full generator-history matrix;
- no whole-child content normalization.

Positive scaling of a ray and positive diagonal congruence preserve both CP and SCP, which is why primitive integer midpoint rays
are exact replacements for the paper's normalized rational midpoints.

## Termination

Pure Dutour subdivision does not supply a discrete same-order measure known to reach zero. The cutoff repairs that termination gap.

Along every root-to-leaf branch:

- at most 100 consecutive Dutour steps can retain the current order;
- the next nonterminal node must use COPOMATRIX;
- every COPOMATRIX child lowers the order by one;
- each Dutour node has two children;
- each COPOMATRIX Vmatrix decomposition has a finite number of children.

Therefore the complete recursion tree is finite for every finite input matrix. The theoretical tree can still be extremely large:
the cutoff guarantees eventual diagnostics, not practical speed or memory use. External timeout, process failure, or memory exhaustion
remains an unresolved resource outcome rather than a negative classification.

## Source Behavior And coposit Changes

Source-derived behavior retained locally:

- Dutour's two-ray rejection, maximum-ratio choice, first-tie rule, unscaled sum ray, child construction, and child order;
- COPOMATRIX's principal child, division-free Schur form, sign normalization, normalized midpoint geometry, and Xu–Yao Vmatrix
  decomposition;
- exact depth-first conjunction with immediate short-circuit rejection.

coposit-created mathematical and control-flow choices:

- direct exact terminal criteria through order three;
- mode-aware equality boundaries, including the zero-pivot CP reduction;
- scanning every row and taking the first COPOMATRIX pivot with at most two children;
- using one Dutour split when no narrow COPOMATRIX pivot exists;
- the branch-local cutoff of 100 consecutive Dutour splits;
- forcing pivot zero at the cutoff;
- restarting the complete hybrid with counter zero after every order-reducing child.

Representation-only choices:

- FLINT integer storage;
- primitive integer boundary rays;
- matrix-only node state;
- sparse congruence evaluation;
- cooperative timeout checkpoints.

Changing the cutoff, narrow gate, pivot order, forced pivot, Dutour pair rule, Vmatrix traversal, child order, or counter-reset rule
defines a different model.

## Known Difficult Inputs

### Forced COPOMATRIX explosion

If every pivot row has balanced positive and negative entries, the narrow scan fails. After 100 Dutour steps, pivot zero is forced
even if

\[
1+\binom{s+t-1}{s}
\]

is enormous. The cutoff converts possible endless same-order refinement into a finite but potentially very broad projection.

### Large work before the cutoff

A strictly copositive branch may require both Dutour children to pass at every level. In the worst structural case, 100 consecutive
binary levels can expose an impractically large number of nodes before the forced projection is reached.

### Fixed forced pivot

The cutoff deliberately uses pivot zero. Another row may have fewer COPOMATRIX children, but searching for the best wide pivot is
not part of this model. A poor first row can therefore dominate the forced step.

### High-support boundary zeros

Dutour's local diagonal and two-ray tests do not directly see a zero requiring many generators. COPOMATRIX can also generate a large
negative-half-simplex triangulation before the reduced child exposes that zero. Exceptional circulant matrices with minimal-zero
support near the full dimension are representative structures.

### Exact coefficient growth

Repeated sum-ray updates increase generator coefficients. Schur products and primitive boundary rays can increase matrix-entry bit
length further. Exactness is preserved, but each later multiplication and comparison becomes more expensive.

### Strict inputs require complete coverage

A failing child stops the traversal early. A strictly copositive matrix has no failing child, so every Dutour cone and every required
COPOMATRIX simplex must eventually pass.

## Implementation Map

The complete implementation is local to [`solver.cpp`](solver.cpp).

| Function | Responsibility |
|---|---|
| `solve` | Validate the public matrix and start with streak zero. |
| `adaptive_dutour_copomatrix_checker::check` | Apply direct tests, diagonal rejection, narrow routing, forced cutoff, and Dutour fallback. |
| `first_narrow_copomatrix_pivot` | Find the first row whose COPOMATRIX projection creates at most two children. |
| `check_copomatrix` | Build the principal and Schur problems for the selected pivot and reset descendant streaks. |
| `check_projection` | Apply the mode-aware diagonal test, accept an eligible entrywise-nonnegative child, or restart the adaptive recursion. |
| `check_negative_staircase` | Generate the Xu–Yao negative-half-simplex triangulation depth-first. |
| `make_principal_block` | Delete the chosen pivot row and column. |
| `make_schur_block` | Form (aB-pp^T) exactly. |
| `coordinate_ray`, `pair_ray` | Build sparse integer rays for negative, zero, and boundary vertices. |
| `transform` | Form (R^TMR) from sparse rays. |
| `check_dutour` | Apply the exact pair test, select the maximum ratio, and recurse into two same-order children. |
| `replace_generator_with_sum` | Update one Gram row, column, and diagonal for a sum-ray replacement. |
