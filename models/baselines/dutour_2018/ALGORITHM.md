# Dutour 2018

Classification: exact non-strict- and strict-copositivity implementation of Dutour Sikirić's source traversal.

## Decision Modes

The same cone partition serves two predicates. In `copositive` mode a node rejects a diagonal only when $g_{ii}<0$ and rejects a
negative two-generator restriction only when $g_{ij}^2>g_{ii}g_{jj}$. Equality is allowed, so an equality edge is subdivided and
its zero direction may become a child generator. In `strictly_copositive` mode the comparisons are $g_{ii}\leq0$ and
$g_{ij}^2\geq g_{ii}g_{jj}$ because a zero already disproves strict positivity. Both modes use the identical maximum-ratio pair,
sum ray, child order, and LIFO traversal. An entrywise-nonnegative Gram matrix is safe in non-strict mode with nonnegative diagonal;
strict mode reaches that acceptance only after checking that every diagonal is positive.

The public call is `solve(A, mode)` and defaults to `strictly_copositive`. It returns the Boolean value of only the requested
predicate; it is not a three-state classification. Non-strict boundary inputs can still exhaust the node limit when the preserved
same-dimensional subdivision approaches an interior zero without generating it exactly.

## What The Algorithm Does

The algorithm looks for troublesome directions in the nonnegative orthant. Instead of testing every nonnegative vector directly,
it divides the orthant into simpler cones. Each cone is described by a small set of generating rays. If all pairs of rays interact
nonnegatively under the quadratic form, the complete cone is safe. If one pair is too negative, it is an immediate counterexample.
Otherwise the algorithm splits that pair through their sum and continues with the two smaller cones.

This is a geometric search. It does not enumerate principal submatrices or solve linear systems. Its practical behavior is
determined by how many cone subdivisions are needed before every remaining cone is easy to certify.

## Name And Sources

The model identifier follows Coposit's `<first-author>_<year>` rule. `dutour` names Mathieu Dutour Sikirić, whose Polyhedral
Common repository supplies the exact source algorithm, and `2018` is the year in which its `PairDecomposition` implementation
entered that repository. The year identifies the implementation revision, not a separately published 2018 paper.

Primary implementation sources:

- [`PairDecomposition` introduction, commit `33ce96e4d0589f340a0fbfd7824ff70f9a2ce093`](https://github.com/MathieuDutSik/polyhedral_common/commit/33ce96e4d0589f340a0fbfd7824ff70f9a2ce093);
- the later `TestStrictCopositivity` path at pinned source commit
  [`d2252bc89d991fa6df9750ac9647e19b6a9aca02`](https://github.com/MathieuDutSik/polyhedral_common/commit/d2252bc89d991fa6df9750ac9647e19b6a9aca02).

Related published descriptions of the simplicial-partition family are:

- Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
  Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035);
- Julius Žilinskas and Mirjam Dür, “Depth-First Simplicial Partition for Copositivity Detection, with an Application to
  MaxClique,” *Optimization Methods and Software* 26(3), 499–510 (2011);
- Julius Žilinskas, [“Copositive Programming by Simplicial Partition”](../../../research/papers/zilinskas-2011-copositive-programming-by-simplicial-partition.pdf),
  *Informatica* 22(4), 601–614 (2011).

## Problem And Node Representation

The selected mode decides whether a symmetric integer matrix $A$ is copositive or strictly copositive:

\[
x^T A x\geq0
\quad\text{or}\quad
x^T A x>0
\qquad\text{for every nonzero }x\geq0.
\]

A node represents a simplicial cone with generators $v_1,\ldots,v_n$. It stores only the exact Gram matrix

\[
B_{ij}=v_i^T A v_j.
\]

The initial generators are the coordinate rays, so the initial Gram matrix is $B=A$. Generator coordinates are unnecessary:
every subdivision replaces one generator by a sum of two existing generators, and the corresponding Gram row and column are sums
of existing rows and columns.

## Decision Flow

For every pending Gram matrix $B$, the model performs the following steps.

### 1. Apply the mode-dependent generator test

If some diagonal value satisfies

\[
b_{ii}\leq0,
\]

then $v_i\geq0$ is a nonzero direction with nonpositive quadratic value. Strict copositivity fails immediately.
Non-strict mode rejects only $b_{ii}<0$.

### 2. Inspect every negative generator pair

For each $i<j$ with $b_{ij}<0$, compute the normalized negative coupling

\[
\rho_{ij}=\frac{b_{ij}^2}{b_{ii}b_{jj}}.
\]

The implementation compares these ratios by cross multiplication; it never constructs a rational number.

Strict mode rejects when

\[
b_{ij}^2\geq b_{ii}b_{jj},
\]

then the two-generator restriction is not strictly copositive. Equality gives a nonnegative zero direction and a strict inequality
gives a negative direction, so the complete matrix is rejected. More explicitly, with
$a=b_{ii}>0$, $b=b_{jj}>0$, and $c=b_{ij}<0$, the nonnegative coefficient vector $(b,-c)$ has value

\[
\begin{pmatrix}b&-c\end{pmatrix}
\begin{pmatrix}a&c\\c&b\end{pmatrix}
\begin{pmatrix}b\\-c\end{pmatrix}
=b\left(ab-c^2\right)\leq0.
\]

This is an actual direction in the current cone, not merely a heuristic warning.
Non-strict mode rejects only $b_{ij}^2>b_{ii}b_{jj}$; equality is retained and subdivided.

### 3. Accept an entrywise nonnegative Gram matrix

If no negative off-diagonal entry exists, then every $b_{ij}\geq0$. For any nonzero coefficient vector
$\lambda\geq0$,

\[
\lambda^T B\lambda
=\sum_i b_{ii}\lambda_i^2+2\sum_{i<j}b_{ij}\lambda_i\lambda_j>0,
\]

because all diagonal entries have already been proved positive. The entire cone is certified and removed from the work list.

### 4. Choose the split pair

Otherwise, select the negative pair with the largest exact ratio ρ. The first pair in index order wins an exact tie. This is the
source implementation's split heuristic: it refines the two-generator face closest to losing strict copositivity.

### 5. Split through the sum ray

For the selected pair, introduce

\[
w=v_i+v_j.
\]

The original cone is the union of two child cones:

\[
\operatorname{cone}(v_i,w,\{v_k:k\neq i,j\})
\quad\text{and}\quad
\operatorname{cone}(w,v_j,\{v_k:k\neq i,j\}).
\]

Their intersection is the common face containing $w$ and every unchanged generator. Each child Gram matrix is produced by
replacing one row and column with its sum with the other selected row and column. In particular,

\[
w^TAw=b_{ii}+2b_{ij}+b_{jj},\qquad w^TAv_k=b_{ik}+b_{jk}.
\]

The union statement follows directly from the two selected coefficients. If a point uses
$\alpha v_i+\beta v_j$ and $\alpha\geq\beta$, then

\[
\alpha v_i+\beta v_j=(\alpha-\beta)v_i+\beta w
\]

lies in the first child; if $\beta\geq\alpha$, the analogous expression lies in the second. The unchanged generators can be
carried along in either child. This is why checking both children is exactly equivalent to checking the parent cone.

Both children must be strictly copositive. The implementation uses a LIFO work list and visits the child replacing $v_i$ before
its sibling.

## Exact Arithmetic And Complexity

All signs, products, and ratio comparisons use FLINT arbitrary-precision integers. A split updates one row and column rather than
reconstructing a basis or multiplying full matrices. This is a representation optimization only; the selected pair, new ray,
children, and traversal are unchanged from the pinned source.

The cone dimension does not decrease. Runtime depends on how many cones the repeated subdivisions create. Coposit allows at most
50,000 open nodes: before a split, the current node is replaced by its two unfinished children, and the run stops with the distinct
unresolved `node_limit` outcome if that would create 50,001 open nodes. A timed native-module run may likewise observe the shared
signal flag at node and matrix-row boundaries and return a distinct timeout outcome. Neither resource outcome is reported as
`false`; neither changes a mathematical test or traversal decision.

## Known Difficult Inputs

The central weakness is that every split keeps the same dimension. A matrix can have many negative generator pairs that are all too
weak to trigger the two-generator rejection. The Gram matrix is then not entrywise nonnegative, but no split makes permanent
dimension-reducing progress. The algorithm can build a large tree of closely related cones.

Sparse positive-definite chain matrices are a concrete example. Corpus matrix **9656** is

\[
A=D\,\operatorname{tridiag}(-1,2,-1)D,
\qquad
D=\operatorname{diag}(1,2,1,2,\ldots),
\]

of order 15. Every negative interaction is local to two adjacent coordinates and passes the two-generator test, while the complete
Gram matrix remains nonnegative only after extensive refinement. The sum-ray split repeatedly changes the cone without exploiting
the chain's easy one-coordinate elimination structure.

Matrices close to the strict-copositive boundary can cause the same problem: negative pair ratios remain just below the rejection
threshold, so the algorithm must keep subdividing instead of obtaining either an immediate witness or an entrywise-nonnegative
certificate.

## Fidelity Boundary

The maintained implementation was compared decision by decision with the pinned Polyhedral Common source. It preserves the source
implementation's mode-dependent diagonal and two-generator tests, maximum-ratio pair choice and first-tie rule, unscaled sum ray,
two-child split, and depth-first child order. Storing only the Gram matrix is Coposit's representation optimization: it removes
generator coordinates that the source decisions never inspect and changes no generated cone.

The baseline deliberately contains no low-dimensional shortcut, SNC slice, Danninger reduction, Hadeler test, graph decomposition,
or Coposit-created hybrid rule.

Timeout checkpoints are enabled only in timed native-module builds. Standalone `coposit` and model-test builds compile the same calls
to no-ops, so the baseline has no timer thread, clock read, or signal handler. The fixed 50,000-open-node guard applies to every
build and is a Coposit resource boundary, not part of the pinned source algorithm.
