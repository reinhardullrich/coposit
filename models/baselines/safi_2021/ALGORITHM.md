# Safi 2021

Classification: exact non-strict implementation and strict adaptation of Safi, Nabavi, and Caron's simplex algorithm.

## Decision Modes

The simplex state, maximal SNC slice, intersection vertices, child triangulation, and depth-first traversal are identical in both
modes. Only mathematically meaningful boundary comparisons change. Non-strict mode rejects a vertex or centre only when its value is
negative and accepts the centre-radius certificate when its exact lower bound is nonnegative. Strict mode rejects a nonpositive
vertex or centre and requires a strictly positive lower bound. A non-strict node with a zero-diagonal vertex and a negative incident
edge is rejected immediately because that two-vertex face contains negative values; this also avoids a degenerate zero-pivot slice.

`solve(A, mode)` defaults to `strictly_copositive` and returns only that predicate's Boolean. In non-strict mode an exact zero is not a
failure, so a boundary input may keep refining until a certificate is reached or a timeout/node limit leaves the result unresolved.

## What The Algorithm Does

SNC works on a simplex inside the standard nonnegative simplex. When the complete simplex cannot be certified immediately, it
chooses one vertex with positive quadratic value and cuts every troublesome edge incident to that vertex. This creates a maximal
inner simplex around the chosen vertex. The paper proves that the inner simplex never needs to be searched separately: proving the
remaining polyhedron safe also proves the removed simplex safe.

The remaining polyhedron is divided into standard simplices and processed recursively. SNC therefore uses a more aggressive slice
than a binary edge split, but every child still has the same dimension as its parent.

## Name And Sources

The model identifier follows coposit's `<first-author>_<year>` rule. `safi` names the paper's first author, Mohammadreza Safi, and
`2021` is the publication year. The paper calls the algorithm **SNC**, formed from the authors' surnames Safi, Nabavi, and Caron.

Primary reference:

- Mohammadreza Safi, Seyed Saeed Nabavi, and Richard J. Caron, “A Modified Simplex Partition Algorithm to Test Copositivity,”
  *Journal of Global Optimization* 81, 645–658 (2021),
  [DOI 10.1007/s10898-021-01092-1](https://doi.org/10.1007/s10898-021-01092-1).

The [paper PDF](../../../research/papers/safi-nabavi-caron-2021.pdf) is retained locally. The implementation ports the independently
written fraction-free experiment at
[`experiments/copositivity_snc_2026-08-06/snc_benchmark.cpp`](../../../experiments/copositivity_snc_2026-08-06/snc_benchmark.cpp),
SHA-256 `12b4e89ef6ae14cdafee8e6eb103695f818e538be9f9fa502458b99ba367c084`.

## Simplex And Gram Matrix

A node is a simplex

\[
\Delta=\operatorname{conv}(v_1,\ldots,v_n)
\]

inside the standard simplex $x\geq0, \mathbf1^Tx=1$. Its Gram matrix is

\[
G_{ij}=v_i^TAv_j.
\]

The initial vertices are the coordinate vectors, so initially $G=A$. Because every point of Δ is a convex combination of its
vertices, the signs of $G$ give immediate witnesses and certificates.

## Cheap Node Checks

### Vertex rejection

If

\[
G_{ii}=v_i^TAv_i\leq0
\]

for any vertex, strict copositivity fails. A negative value is a negative witness; zero is enough to fail the strict inequality.

### Entrywise Gram certificate

If there is no negative off-diagonal entry, then $G\geq0$ entrywise. With the already checked positive diagonal, every nonzero
convex combination has positive quadratic value. The complete simplex is certified.

### Centre-radius certificate

If negative cross terms remain, let

\[
\bar v=\frac1n\sum_i v_i,
\qquad
\delta=\max_i\lVert v_i-\bar v\rVert_1,
\qquad
\lVert A\rVert_1=\sum_{i,j}|a_{ij}|.
\]

The paper bounds how far the quadratic form can change between the centre and any point in the simplex. The strict model certifies
the whole node when

\[
\bar v^TA\bar v>\lVert A\rVert_1(\delta^2+2\delta).
\]

If $\bar v^TA\bar v\leq0$, the centre itself disproves strict copositivity. Otherwise the node must be sliced.

## Maximal SNC Slice

The paper allows a choice of slicing vertex. This implementation takes the first endpoint $v_\ell$ of the first negative Gram
entry in index order. Its diagonal value is positive because of the vertex check.

For every other vertex $v_j$, define a point on the edge from $v_\ell$ to $v_j$:

\[
\widehat v_j=(1-\lambda_j)v_\ell+\lambda_jv_j.
\]

Choose the largest $\lambda_j\in(0,1]$ for which

\[
\widehat v_j^TAv_\ell\geq0.
\]

The value has the simple formula

\[
\lambda_j=
\begin{cases}
1, & v_j^TAv_\ell\geq0,\\[2mm]
\dfrac{v_\ell^TAv_\ell}{v_\ell^TAv_\ell-v_j^TAv_\ell}, & v_j^TAv_\ell<0.
\end{cases}
\]

Thus a nonnegative edge is left unchanged, while a negative edge is cut exactly where its interaction with $v_\ell$ becomes
zero.

The implementation evaluates the same point without rational matrix arithmetic. If
$v_\ell=z_\ell/d_\ell$, $v_j=z_j/d_j$, $D=z_\ell^TAz_\ell>0$, and
$C=z_j^TAz_\ell<0$, then

\[
\widehat v_j=\frac{-Cz_\ell+Dz_j}{Dd_j-Cd_\ell}.
\]

The denominator is positive. Dividing the two numerator coefficients and the denominator by
$\gcd(D,|C|)$ makes the stored representation smaller without changing the point.

The sliced simplex is

\[
\Delta_1=\operatorname{conv}\bigl(v_\ell,\{\widehat v_j:j\neq\ell\}\bigr).
\]

The remaining polyhedron is

\[
\Omega_1=
\operatorname{conv}\bigl((V_\Delta\cup V_{\Delta_1})\setminus\{v_\ell\}\bigr).
\]

The original vertex $v_\ell$ is absent from Ω₁.

## Why The Removed Simplex Needs No Search

Every point $x\in\Delta_1$ can be written as

\[
x=t v_\ell+(1-t)y,
\]

where $0\leq t\leq1$ and $y$ lies on the opposite face generated by the points $\widehat v_j$. That face lies in Ω₁. The
construction gives

\[
v_\ell^TAv_\ell>0,
\qquad
v_\ell^TAy\geq0.
\]

If the recursive search proves $y^TAy>0$ on Ω₁, then

\[
x^TAx=t^2v_\ell^TAv_\ell+2t(1-t)v_\ell^TAy+(1-t)^2y^TAy>0.
\]

This is the strict version of the paper's Theorem 3. It is a conditional removal: Δ₁ is safe once Ω₁ is safe. The algorithm
may omit Δ₁ from its work list, but it must still prove every child covering Ω₁.

## Triangulating The Remainder

Let $c_0,\ldots,c_{m-1}$ be the vertices whose edges were actually cut, and let $U$ be the unchanged vertices. SNC divides Ω₁
into $m\leq n-1$ simplices. Child $k$ has the vertices

\[
U, v_{c_0},\ldots,v_{c_k},\widehat v_{c_k},\ldots,\widehat v_{c_{m-1}}.
\]

Every child has exactly $n$ vertices. The implementation visits them in ascending $k$ order depth-first. A path frame retains the
parent simplex, its cut/intersection description, and the index of the next child. Only that next child is materialized; its complete
subtree is processed before the following sibling is constructed. This is the same order as pushing all children in reverse order
onto a LIFO work list. It returns `false` at the first rejected child and `true` only after every generated child is certified.

## Exact Representation

Each rational vertex is stored as an integer numerator vector and a positive integer denominator. A stored Gram entry is the
integer numerator $z_i^TAz_j$; positive denominators mean its sign is the sign of the true rational entry. Intersection points,
child Gram matrices, the centre, radius, and radius bound are all evaluated after clearing positive denominators. No floating-point
tolerance or rational matrix storage is used.

The lazy path frame stores only sparse descriptions of the unmaterialized siblings. It does not approximate or defer any geometric
choice: every child receives exactly the same vertices and Gram matrix when its turn is reached. The separate logical open-node
counter includes those siblings before they are materialized, preserving the resource-limit decision of the former explicit work
list.

For the centre-radius check, write $v_i=z_i/d_i$, let $L=\operatorname{lcm}(d_1,\ldots,d_n)$,
$f_i=L/d_i$, and $y=\sum_i f_i z_i$. Then $\bar v=y/(nL)$. With

\[
R=\max_i\left\lVert n f_i z_i-y\right\rVert_1,
\]

the true radius is $\delta=R/(nL)$. The implemented all-integer certificate is exactly

\[
\lVert A\rVert_1\left(R^2+2nLR\right)<y^TAy,
\]

which is the displayed strict centre-radius inequality after multiplication by the positive denominator $(nL)^2$.

## Known Difficult Inputs

SNC struggles when the selected slicing vertex has negative interactions with many other vertices. If $m$ incident edges must be
cut, the remainder produces $m$ same-dimensional child simplices. A dense negative Gram pattern can therefore create almost
$n-1$ children at one node, and the same pattern may recur in their descendants.

The method also has no dimension reduction. Repeated edge intersections introduce new exact rational vertices; their numerators and
denominators can grow while the search tree expands. The centre-radius test may remain inconclusive on wide simplices or when the
minimum of the form is concentrated near a face rather than near the centre.

Boundary matrices are the fundamental termination problem. If an exact nonnegative zero is never generated as a simplex vertex or
centre, SNC can produce progressively smaller simplices approaching that zero without exposing it in a finite step. Such a run is
unresolved rather than a negative classification.

## Non-strict Source Rules, Strict Adaptation, Choices, And Limits

The published Algorithm 1 supplies non-strict mode. coposit keeps its maximal slicing parameters and child construction in both modes
and changes only the terminal inequalities when strict mode is requested:

- a nonpositive vertex or centre rejects;
- an entrywise nonnegative Gram matrix certifies only after every diagonal has proved positive;
- the centre-radius inequality must be strict.

The paper leaves the positive slicing vertex and cut order open. This model preserves the verified experiment's deterministic
choices: first endpoint of the first negative Gram entry, ascending cut indices, and depth-first traversal.

As with other simplex refinements, a boundary matrix may generate an unending refinement without exposing its zero direction.
coposit allows at most 50,000 open nodes. Before a slice creates its unfinished children, it returns the distinct unresolved
`node_limit` outcome if the logical unfinished count, including unmaterialized siblings, would exceed that count. A timed native-module run may also
observe the shared signal flag at simplex and matrix-row boundaries and return a distinct timeout outcome. Neither resource result
is `false`.

Standalone model and test builds compile the timeout checkpoints to no-ops, so the strict adaptation has no timer thread, clock
read, or signal handler. The fixed 50,000-open-node guard applies to every build and supplies no mathematical decision.
