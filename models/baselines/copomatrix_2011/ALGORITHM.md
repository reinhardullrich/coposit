# COPOMATRIX 2011

Classification: exact ordinary implementation and strict adaptation of Xu and Yao's COPOMATRIX mathematics, with a non-literal
depth-first scheduler. The projection, negative-half-simplex geometry, and Vmatrix decomposition are source-derived. Strict
equality handling, early all-diagonal checks, deterministic traversal, and fraction-free integer representation are explicit
Coposit choices described below.

## Decision Modes

`copositive` mode preserves Algorithm 2's non-strict boundary rules: reject $k_{ii}<0$, accept an entrywise-nonnegative projection
matrix with nonnegative diagonal, and require ordinary copositivity of every principal and Schur child. When the fixed pivot is
zero, a negative entry in its remaining row gives an immediate negative two-coordinate direction; otherwise that row contributes
only nonnegative cross terms and the decision reduces exactly to the complete principal block.

`strictly_copositive` mode rejects $k_{ii}\leq0$, requires positive diagonal before the entrywise-nonnegative certificate, and
requires strict copositivity recursively. With positive pivot, both modes construct the same $C$ and $aC-pp^T$, use Xu and Yao's
same Vmatrix decomposition, represent the same boundary rays by primitive integers, and traverse the same staircase. The public
`solve(A, mode)` defaults to strict mode and answers only the requested predicate.

## What The Algorithm Does

COPOMATRIX removes the first coordinate of the current matrix. It always checks the principal matrix left after that removal. If
the removed row contains a negative off-diagonal entry, it also minimizes the quadratic form over the removed coordinate and checks
a Schur matrix on the part of the remaining simplex where that minimum is attained.

That part is generally not a simplex. Xu and Yao positively rescale the variables so every pivot coefficient is only $-1$, $0$, or
$1$. The separating hyperplane then meets every positive-negative simplex edge at its midpoint. Their Vmatrix decomposition
recursively subdivides the resulting negative half-simplex into ordinary simplices.

Every final child has order one less than its parent. The search can branch combinatorially, but every branch has finite depth.

## Baseline Identity

The model keeps the name `copomatrix_2011` because its projection and every generated mathematical child are Xu and Yao's
COPOMATRIX construction. Its scheduler is deliberately not a literal transcription of Algorithm 2. The paper maintains a set
$F$, checks the $(1,1)$ entry of every member, forms the union of `Proj(K)` for every $K\in F$, removes nonnegative matrices, and
only then replaces $F$. Thus the paper advances complete projection frontiers.

The maintained code instead checks every diagonal of one node, processes its principal child before its Schur children, and
finishes descendants depth-first with first-failure termination. This visits the same required projection problems but can expose
a witness in a different order, retain fewer live matrices, and perform a different amount of work before stopping. It must
therefore be understood as the maintained depth-first COPOMATRIX baseline, not as a reproduction of the unavailable Maple
program's scheduling or timings. The identifier is retained; this difference is documented rather than hidden by a rename.

## Name And Primary Source

The user-selected identifier is an explicit exception to Coposit's usual `<first-author>_<year>` rule: `copomatrix` is the authors'
uppercase algorithm name, not an author name and not “COPOMATRIX 2.0.” `2011` is the journal publication year.

Primary source:

> Jia Xu and Yong Yao, “An Algorithm for Determining Copositive Matrices,” *Linear Algebra and its Applications* 435(11),
> 2784–2792 (2011), [DOI 10.1016/j.laa.2011.04.038](https://doi.org/10.1016/j.laa.2011.04.038),
> [retained arXiv:1011.2039v2 PDF](../../../research/papers/xu_yao_2011_copomatrix.pdf),
> [arXiv record](https://arxiv.org/abs/1011.2039).

The preprint was first submitted in 2010 and revised in 2011. The maintained identifier uses the journal year.

The paper contains:

- Lemma 1.1, giving the ordinary and strict one-coordinate projection equivalences;
- Theorem 2.1, giving the two-part decomposition of the normalized negative half-simplex;
- Algorithm 1, named `Vmatrix`, which repeats that decomposition until every part is simplicial;
- Algorithm 2, named `COPOMATRIX`, which gives the complete ordinary-copositivity work-set algorithm.

The authors state that a similar strict algorithm can be formulated, but Algorithm 2 itself is written with ordinary inequalities.
No original Maple implementation was recovered. This model is therefore an independent implementation of the published mathematics,
including the paper's ordinary rules and its stated strict analogue, not a transcription of author source code.

## Public Decision Problem

For a nonempty square symmetric integer matrix $A$, the selected mode decides either

\[
x^TAx\geq0
\quad\text{or}\quad
x^TAx>0
\qquad\text{for every }x\in\mathbb R_+^n\setminus\{0\}.
\]

The public boundary rejects empty, nonsquare, and asymmetric matrices with `std::invalid_argument`. Symmetry is checked exactly; the
input is never silently replaced by its symmetric part.

The result is Boolean only when the recursive decision completes. A cooperative timeout is an unresolved resource outcome and is
not converted to `false`.

Homogeneity permits restriction to the standard simplex

\[
\Delta_n=\left\{x\in\mathbb R_+^n:\sum_{i=1}^n x_i=1\right\}.
\]

Every nonzero nonnegative vector is a positive multiple of a point in $\Delta_n$, and positive scaling does not change the sign of
a quadratic form.

## The One-Coordinate Projection

At every nontrivial node, COPOMATRIX uses the current first coordinate. Write

\[
A=\begin{pmatrix}
a&p^T\\
p&C
\end{pmatrix},
\]

where $a=a_{11}$, $p$ is the rest of the first column, and $C$ is the principal matrix obtained by deleting the first row and
column. Split a nonnegative vector as $(t,y)$:

\[
q(t,y)=
\begin{pmatrix}t\\y\end{pmatrix}^T
A
\begin{pmatrix}t\\y\end{pmatrix}
=at^2+2t\,p^Ty+y^TCy.
\]

Strict copositivity requires $a>0$, since the first coordinate vector has value $a$. For fixed $y$, $q(t,y)$ is then a convex
quadratic in $t\geq0$.

### The principal child is always required

Setting $t=0$ gives

\[
q(0,y)=y^TCy.
\]

Therefore $A$ can be strictly copositive only if $C$ is strictly copositive on the complete remaining orthant. COPOMATRIX always
creates this principal child. This is the important asymmetry between COPOMATRIX and the maintained Danninger reconstruction:
COPOMATRIX does not triangulate a positive half-region for $C$; it checks $C$ globally once.

### The negative half-region

Complete the square:

\[
q(t,y)
=a\left(t+\frac{p^Ty}{a}\right)^2
+y^T\left(C-\frac{pp^T}{a}\right)y.
\]

If $p^Ty\geq0$, the unconstrained minimizer is nonpositive, so the minimum over $t\geq0$ occurs at $t=0$. The already-required
principal child $C$ covers this case.

If $p^Ty\leq0$, the minimizer

\[
t_*=-\frac{p^Ty}{a}
\]

is feasible. The minimized value has the sign of

\[
y^T\left(aC-pp^T\right)y.
\]

Define the division-free Schur matrix

\[
S=aC-pp^T.
\]

The additional problem is strict positivity of $S$ on

\[
T^-_p=\{y\geq0:p^Ty\leq0\}.
\]

By homogeneity, this cone may be intersected with $\Delta_{n-1}$.

### Exact projection equivalence

For $a>0$,

\[
A\text{ is strictly copositive}
\iff
\begin{cases}
C\text{ is strictly copositive},\\
y^TSy>0\text{ for every nonzero }y\geq0\text{ with }p^Ty\leq0.
\end{cases}
\]

Necessity follows by using $t=0$ for $C$ and $t=t_*$ for $S$. For sufficiency:

- if $y=0$ and $t>0$, then $q(t,0)=at^2>0$;
- if $y\neq0$ and $p^Ty\geq0$, then $q(t,y)\geq y^TCy>0$;
- if $y\neq0$ and $p^Ty\leq0$, then $q(t,y)$ is at least its strictly positive minimum $y^TSy/a$.

This is the mathematical reason every recursive child is necessary and sufficient rather than a heuristic test.

## Xu And Yao's Sign Normalization

The source paper normalizes the pivot magnitudes before triangulating $T^-_p$. Define

\[
d_i=
\begin{cases}
1,&p_i=0,\\
1/|p_i|,&p_i\neq0,
\end{cases}
\qquad
D=\operatorname{diag}(d_1,\ldots,d_{n-1}).
\]

Under the positive diagonal congruence

\[
\bar A=
\begin{pmatrix}1&0\\0&D\end{pmatrix}
A
\begin{pmatrix}1&0\\0&D\end{pmatrix},
\]

the pivot vector becomes

\[
\beta=Dp,
\qquad
\beta_i=\operatorname{sign}(p_i)\in\{-1,0,1\}.
\]

Positive diagonal congruence is a bijection of the nonnegative orthant, so it preserves ordinary and strict copositivity.

The normalized principal and Schur matrices are

\[
\bar C=DCD,
\qquad
\bar S=aDCD-\beta\beta^T=D(aC-pp^T)D=DSD.
\]

The source negative polytope is therefore

\[
\widehat T^-
=\{y\in\Delta_{n-1}:\beta^Ty\leq0\}.
\]

The normalization removes all pivot magnitudes from its geometry. Only the three sign classes remain.

## Geometry Of The Normalized Negative Half-Simplex

Let the nonzero sign positions, in their current index order, be

\[
P_0,\ldots,P_{s-1}
\quad\text{for }\beta_i=1,
\qquad
N_0,\ldots,N_{t-1}
\quad\text{for }\beta_i=-1.
\]

Let $Z$ contain the zero positions. The inequality is

\[
\sum_{i=0}^{s-1}y_{P_i}
-\sum_{j=0}^{t-1}y_{N_j}
\leq0.
\]

When $t=0$, the inequality holds only on the zero-coordinate face, and no Schur child is required because the principal child has
already checked that face. This is the source projection rule.

Assume now that $t\geq1$. The vertices of the nonzero-sign part of $\widehat T^-$ are:

- every negative coordinate vertex $e_{N_j}$;
- the midpoint

  \[
  M_{ij}=\frac{e_{P_i}+e_{N_j}}2
  \]

  of every positive-negative edge.

Every zero coordinate vertex $e_z$ is coned into every final simplex. A zero coordinate does not change the number of simplices.

The midpoint formula follows immediately from the normalized signs. On an edge

\[
y=\lambda e_{P_i}+(1-\lambda)e_{N_j},
\]

the boundary equation is

\[
\lambda-(1-\lambda)=0,
\]

so $\lambda=1/2$.

## The Vmatrix Decomposition

Xu and Yao denote the nonzero-sign polytope by two ordered lists

\[
[[P_0,\ldots,P_{s-1}],[N_0,\ldots,N_{t-1}]].
\]

It is already simplicial exactly when either:

- the positive list is empty; or
- the negative list contains one element.

Otherwise Theorem 2.1 selects the first positive-negative midpoint $M_{P_0N_0}$ and decomposes the polytope into

\[
\operatorname{conv}\left(M_{P_0N_0},[[P_1,\ldots,P_{s-1}],[N_0,\ldots,N_{t-1}]]\right)
\]

and

\[
\operatorname{conv}\left(M_{P_0N_0},[[P_0,\ldots,P_{s-1}],[N_1,\ldots,N_{t-1}]]\right).
\]

Thus both children retain the midpoint. The first deletes the first positive label; the second deletes the first negative label.
Repeating this rule reaches one of the two simplicial boundary cases.

### Coposit's deterministic staircase traversal

Algorithm 1 says to choose a pending nonsimplicial polytope but does not specify that choice or a final traversal order. Coposit
resolves this in the same deterministic spirit as the maintained Danninger staircase:

1. positive and negative indices retain ascending current-coordinate order;
2. zero coordinate rays are placed first in every child;
3. at a nonsimplicial state, append the first positive-negative boundary ray;
4. visit the child deleting the first positive label;
5. then visit the child deleting the first negative label;
6. finish one child recursively before constructing later descendants;
7. stop immediately on the first failed child.

The recursion can be viewed as a staircase through a grid. A state $(i,j)$ means that positive labels before $i$ and negative
labels before $j$ have already been removed, while their selected midpoint rays remain in the current ray list.

The exact maintained recursion is:

```text
negative_staircase(i, j, retained_rays):
    if i == number_of_positive_labels:
        append e_Nj, ..., e_N(t-1)
        check the resulting Schur child

    else if j + 1 == number_of_negative_labels:
        append e_Nj
        append every midpoint ray between P_i, ..., P_(s-1) and N_j
        check the resulting Schur child

    else:
        append the midpoint ray between P_i and N_j
        check negative_staircase(i + 1, j, retained_rays) first
        check negative_staircase(i, j + 1, retained_rays) second
```

Every base case has exactly $s+t$ nonzero-sign rays when retained midpoint rays are included. Adding all zero rays gives exactly
$n-1$ rays, so every transformed child is square and has order $n-1$.

### Example with two positive and three negative labels

For positive labels $P_0,P_1$ and negative labels $N_0,N_1,N_2$, the traversal produces six simplices in this order:

\[
\begin{aligned}
&[M_{P_0N_0},M_{P_1N_0},e_{N_0},e_{N_1},e_{N_2}],\\
&[M_{P_0N_0},M_{P_1N_0},M_{P_1N_1},e_{N_1},e_{N_2}],\\
&[M_{P_0N_0},M_{P_1N_0},M_{P_1N_1},e_{N_2},M_{P_1N_2}],\\
&[M_{P_0N_0},M_{P_0N_1},M_{P_1N_1},e_{N_1},e_{N_2}],\\
&[M_{P_0N_0},M_{P_0N_1},M_{P_1N_1},e_{N_2},M_{P_1N_2}],\\
&[M_{P_0N_0},M_{P_0N_1},e_{N_2},M_{P_0N_2},M_{P_1N_2}].
\end{aligned}
\]

Column order inside a simplex is the maintained recursion's ray order. A different column order would only permute the child
matrix, but preserving it makes every later first-coordinate projection deterministic.

### Number of Schur children

With $s$ positive and $t\geq1$ negative pivot entries, Vmatrix produces

\[
\binom{s+t-1}{s}
\]

negative-side simplices. COPOMATRIX also has the one principal child, so the immediate total is

\[
1+\binom{s+t-1}{s}.
\]

When $t=0$, there is only the principal child. Zero pivot entries do not appear in the binomial count.

## Fraction-Free Integer Representation

The paper's $D$, normalized midpoint rays, and normalized matrices are rational. Coposit keeps the maintained core integer-only.
It represents matrices that are positively diagonally congruent to the paper's matrices, so every sign decision is unchanged.

### Principal child

The source principal child is $DCD$. Coposit recursively checks $C$ itself. Since $D$ is symmetric,

\[
DCD=D^TCD
\]

is a positive diagonal congruence of $C$, the strict decision and every entry sign are identical. Applying COPOMATRIX recursively to
positively diagonally congruent matrices also preserves the normalized pivot signs and projection decisions.

### Schur child

Coposit forms the exact integer matrix

\[
S=aC-pp^T.
\]

The source normalized Schur matrix is $DSD$.

For a source simplex with ray matrix $W$, its child is

\[
W^TDSDW.
\]

Each column of $DW$ may be multiplied independently by a positive number. If $H$ contains those column scalings and

\[
R=DWH,
\]

then Coposit checks

\[
R^TSR=H(W^TDSDW)H.
\]

This is again positive diagonal congruence, so it preserves strict copositivity.

### Primitive integer form of a midpoint

For $p_u>0$ and $p_v<0$, the normalized midpoint is

\[
M_{uv}=\frac{e_u+e_v}{2}.
\]

After applying $D$,

\[
DM_{uv}=\frac{1}{2p_u}e_u+\frac{1}{2|p_v|}e_v.
\]

Let

\[
g=\gcd(p_u,|p_v|).
\]

Multiplication by the positive value $2p_u|p_v|/g$ gives the primitive integer ray

\[
r_{uv}=\frac{|p_v|}{g}e_u+\frac{p_u}{g}e_v.
\]

It lies on the original boundary because $p^Tr_{uv}=0$. This is the same primitive weighted pair ray used by the maintained
Danninger reconstruction, but here its occurrence and triangulation come from Xu and Yao's normalized midpoint decomposition.

Negative and zero coordinate vertices are represented by ordinary coordinate rays. Their omitted positive scale factors do not
change the child decision.

### Sparse congruence evaluation

A coordinate ray has one nonzero coefficient and a boundary ray has two. For rays $r_k,r_l$, the child entry is

\[
(R^TSR)_{kl}=r_k^TSr_l.
\]

At most four coefficient-matrix products contribute to one entry. The implementation computes only the upper triangle and mirrors
it, preserving exact symmetry.

No floating-point number, tolerance, rational matrix, fixed-width sign mask, or dimension cap is used.

## Complete Maintained Decision Flow

For a current exact symmetric integer matrix $K$:

1. Check every diagonal entry. Reject a negative value in ordinary mode or a nonpositive value in strict mode.
2. If $K$ has order one, accept after that mode-dependent diagonal test.
3. Fix the first coordinate as the pivot.
4. Partition the remaining pivot entries into positive, zero, and negative lists in index order.
5. Build the principal block $C$.
6. Apply the projection certificate for the selected mode to $C$:
   - reject a negative diagonal in ordinary mode or a nonpositive one in strict mode;
   - accept this child immediately if every off-diagonal entry is nonnegative;
   - otherwise recursively run COPOMATRIX on $C$.
7. If the pivot is zero, reject when $p$ has a negative entry; otherwise return the principal child's result.
8. If the positive pivot has no negative entry, accept the current node after the principal child succeeds.
9. Build $S=aC-pp^T$.
10. If the pivot has no positive entry, apply the projection certificate recursively to the single Schur child $S$.
11. Otherwise generate the Vmatrix negative staircase lazily, transform $S$ by every final ray matrix, and apply the projection
    certificate to every child in the defined order.
12. Return `false` on the first failed child. Return `true` only after the principal child and every required Schur child pass.

The complete pseudocode is:

```text
check(K, mode):
    checkpoint
    reject if any diagonal of K fails mode
    accept if order(K) == 1

    p := first row of K without its diagonal
    C := K with first row and column removed

    if check_projection(C) is false:
        return false

    if k_00 == 0:
        return p has no negative entry

    if p has no negative entry:
        return true

    S := k_00 C - p p^T

    if p has no positive entry:
        return check_projection(S)

    prepend every zero-coordinate ray
    return negative_staircase(S, positive_indices, negative_indices)

check_projection(M):
    reject if any diagonal of M is nonpositive
    if every off-diagonal entry of M is nonnegative:
        return true
    return check(M)
```

There are no special order-two or order-three formulas. Apart from the necessary order-one stop, the published projection mechanism
continues uniformly to the bottom.

## Ordinary Source Rules And Strict Adaptation

Published Algorithm 2 decides ordinary copositivity. The strict model makes the following exact changes.

### Nonpositive diagonal rejection

Ordinary COPOMATRIX rejects a negative pivot entry. Strict copositivity must also reject equality:

\[
k_{ii}\leq0
\quad\Longrightarrow\quad
e_i^TKe_i\leq0.
\]

Coposit checks every diagonal at node entry, following the exact early rule used in the maintained Danninger model. This can expose a
witness earlier than the source work-set order, but it changes no classification or generated child when all diagonals are positive.

### Entrywise-nonnegative certificate

For ordinary copositivity, any entrywise-nonnegative matrix can be removed from the work set. For strict copositivity, its diagonal
must also be positive. Then every nonzero $x\geq0$ satisfies

\[
x^TKx
=\sum_i k_{ii}x_i^2+2\sum_{i<j}k_{ij}x_ix_j>0.
\]

If an entrywise-nonnegative child has a zero diagonal, the associated coordinate vector is a strict counterexample and the child is
rejected rather than removed as safe.

### Strict recursive conjunction

Lemma 1.1(b) in the source already states the strict projection equivalence. Consequently no tolerance, perturbation, or ordinary
certificate conversion is needed: every principal and Schur child is simply required to be strictly copositive.

## Traversal And Source Boundary

The following parts are source behavior:

- fixed first-coordinate projection;
- positive diagonal sign normalization;
- one complete principal child;
- Schur testing only on the normalized negative half-simplex;
- coordinate and positive-negative midpoint vertices;
- Theorem 2.1's two-part decomposition;
- recursive projection until order one;
- removal of easy entrywise-nonnegative projection matrices.

The following are strict or deterministic Coposit choices:

- change every required positive diagonal comparison from nonnegative to strictly positive;
- require positive diagonal before using the entrywise-nonnegative strict certificate;
- inspect all node diagonals immediately;
- process the principal child before Schur children;
- retain sign indices in ascending order;
- traverse Theorem 2.1's positive-deletion child before its negative-deletion child;
- use depth-first recursion and first-failure short circuit rather than an unspecified work-set selection;
- include zero rays first in every transformed child.

The following are representation-only optimizations:

- recurse on $C$ instead of materializing the rationally normalized $DCD$;
- store $S=aC-pp^T$ rather than $DSD$;
- replace rational normalized midpoint columns by primitive integer boundary rays;
- store only current Gram matrices and sparse one- or two-entry rays;
- generate final simplices lazily instead of storing the complete Vmatrix family.

The paper does not specify equality-based matrix deduplication for its mathematical set $F$. The maintained depth-first traversal
does not compare or merge equal children.

## Correctness Of The Complete Strict Model

Correctness follows by induction on matrix order.

### Base case

An order-one matrix $(a)$ is strictly copositive exactly when $a>0$.

### Inductive step

Assume every completed child of order $n-1$ is classified exactly. For a parent of order $n$:

1. A nonpositive diagonal is an explicit coordinate-vector counterexample.
2. With positive pivot $a$, the one-coordinate projection equivalence reduces the parent exactly to $C$ and $S$ on $T^-_p$.
3. Positive diagonal normalization maps $T^-_p$ bijectively to $\widehat T^-$.
4. Vmatrix simplices cover $\widehat T^-$; their interiors do not overlap, though shared boundary faces may overlap.
5. Every integer ray matrix is positively diagonally congruent to its source normalized simplex matrix.
6. By the induction hypothesis, every recursively completed child result is exact.

The parent returns `true` only if $C$ and every part of the Schur region pass. It returns `false` only from an actual nonpositive
direction in one of those necessary regions. Therefore every completed result is the exact strict-copositivity decision.

## Termination And Cost

Every principal and Schur projection child has order exactly one less than its parent. Matrix order is therefore a strictly
decreasing nonnegative integer along every branch. The recursion reaches order one after at most $n-1$ projection levels.

This finite depth does not imply a small tree. A node with $s$ positive and $t$ negative pivot entries can create

\[
1+\binom{s+t-1}{s}
\]

children. Balanced sign classes maximize the binomial term. The source paper derives a global worst-case operation bound of

\[
2^{(n-2)(n-3)/2+1}.
\]

The per-node matrix work is polynomial:

- sign partition and diagonal checks are $O(n)$;
- principal and Schur construction are $O(n^2)$ exact integer operations;
- one transformed child has $O(n^2)$ entries with at most four coefficient products per entry;
- Vmatrix recursion uses $O(n)$ path storage but may visit a combinatorial number of leaves.

Depth-first generation avoids holding all sibling matrices simultaneously. It does not reduce the number of children that must pass
for a strictly copositive input.

## Known Difficult Inputs

### Balanced mixed signs in the first pivot row

The primary weakness is a pivot with many positive and many negative off-diagonal entries. The negative half-simplex then has a
large Vmatrix triangulation before the order can decrease.

Corpus matrix **811**, a reduced-B matrix derived from QAPLIB `nug24:A`, is a reproducible structure. Its first row has 10 positive
and 11 negative off-diagonal entries. COPOMATRIX therefore has

\[
1+\binom{20}{10}=184757
\]

immediate projection children at the root. This number follows directly from the sign pattern and child-count formula.

### Fixed first-coordinate projection

The source algorithm always projects the first coordinate of the current child. It does not search for a row with a smaller
negative staircase. A poor first row can therefore dominate the entire tree even when another row has a simple sign pattern.

The difficulty can also arise below the root. Congruence transforms change the descendant entries, so an easy root projection can
produce children whose first rows have balanced mixed signs.

### Exact coefficient growth

The Schur formula multiplies current entries before subtracting them. Primitive boundary rays can contain large coefficients when
positive and negative pivot magnitudes have a small greatest common divisor. Repeated congruence transforms can therefore increase
integer bit lengths substantially even when the number of children is modest.

### Strictly copositive matrices require every child

A rejection can short-circuit at its first witness. A strictly copositive matrix has no failing child, so every simplex in every
required Vmatrix family must be checked. The finite guarantee controls depth, not the total amount of work.

## Relationship To Other Maintained Models

COPOMATRIX is closest to Danninger because both:

- eliminate one coordinate;
- use the division-free Schur matrix $aC-pp^T$;
- split a pivot-defined half-region by positive-negative boundary rays;
- produce only order-$(n-1)$ recursive children;
- can branch combinatorially but terminate in finite depth.

The important difference is child geometry. Danninger triangulates a positive region for $C$ and a negative region for $S$.
COPOMATRIX checks $C$ once on the complete orthant and triangulates only the negative region for $S$. Its immediate mixed-sign child
count is consequently

\[
1+\binom{s+t-1}{s}
\]

instead of Danninger's

\[
\binom{s+t}{s}.
\]

COPOMATRIX is not a same-dimension cone or simplex partition like Dutour, Bundfuss, or Safi. Its Vmatrix subdivision is an auxiliary
construction used to create lower-dimensional projection children. It does not enumerate principal subsets like Hadeler or
Dickinson and does not solve linear systems or construct null spaces.

## Implementation-To-Algorithm Correspondence

The complete maintained implementation is local to [`solver.cpp`](solver.cpp).

| Source element | Responsibility |
|---|---|
| `solve` | Validate public shape and symmetry, then start the checker. |
| `copomatrix_checker::check` | Apply mode-dependent diagonal tests, fixed first-coordinate projection, zero-pivot rule, principal-first traversal, and sign-case routing. |
| `check_projection` | Remove an easy entrywise-nonnegative projection matrix under the selected mode; otherwise recurse. |
| `check_negative_staircase` | Implement Theorem 2.1 and Vmatrix lazily with positive-deletion before negative-deletion. |
| `make_principal_block` | Construct the integer representative $C$ of the source child $DCD$. |
| `make_schur_block` | Construct $S=aC-pp^T$. |
| `coordinate_ray` | Represent a negative or zero simplex vertex after harmless positive scaling. |
| `pair_ray` | Convert one normalized midpoint to its primitive integer positive-negative boundary ray. |
| `transform` | Compute $R^TSR$ exactly from sparse rays. |

## What This Model Deliberately Does Not Do

- It does not use direct order-two or order-three shortcuts.
- It does not choose a pivot other than the current first coordinate.
- It does not triangulate COPOMATRIX's negative region with Danninger's different path simplices; it preserves Xu and Yao's
  midpoint decomposition and uses Danninger only to pin branch order and depth-first traversal.
- It does not add Dutour splitting, adaptive routing, SNC slicing, Bundfuss bounds, Hadeler or Dickinson certificates, connected
  components, or duplicate-row preprocessing.
- It does not normalize a complete child matrix by its integer content.
- It does not store the source work set, the complete family of Vmatrix simplices, or explicit original-space generator histories.
- It does not use the unrecovered Maple implementation or claim line-for-line program fidelity.

Any change to the pivot, Vmatrix geometry, projection children, or recursive acceptance rules creates a different mathematical
model rather than an implementation optimization of `copomatrix_2011`.
