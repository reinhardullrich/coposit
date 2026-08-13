# Sponsel 2012

Classification: exact non-strict implementation and strict adaptation of Sponsel, Bundfuss, and Dür's 2012 strengthened
simplicial-partition framework. It uses the paper's positive-semidefinite `H` certificate in non-strict mode, its strict-safe
positive-definite form in strict mode, and retains coposit's exact Bundfuss 2008 split and traversal whenever that certificate does
not decide a simplex.

## Decision Modes

The two modes share the stripped matrix $S(G)$, minimum negative edge, three-$\lambda$ split, integer child construction, evaluation
order, and LIFO traversal. Non-strict mode rejects $g_{ii}<0$ and $g_{ij}^2>g_{ii}g_{jj}$ and accepts the published certificate
$S(G)\succeq0$. Strict mode rejects $g_{ii}\leq0$ and $g_{ij}^2\geq g_{ii}g_{jj}$ and accepts only $S(G)\succ0$. The existing
fraction-free LDLT factorization obtains both decisions exactly from its rank and positive inertia count; no numerical eigensolver
or SDP solver is used.

`solve(A, mode)` defaults to `strictly_copositive` and returns only the requested Boolean. A non-strict equality is retained rather
than treated as a strict counterexample, while the published semidefinite `H` certificate can often close such a node immediately.

## What The Algorithm Does

The algorithm studies the quadratic form on the standard simplex. It maintains a partition of that simplex into smaller simplices.
For each simplex it stores the Gram matrix of the quadratic form at the simplex vertices.

The maintained Bundfuss 2008 model can finish a simplex cheaply only when this Gram matrix is entrywise nonnegative. Sponsel,
Bundfuss, and Dür observed that a larger class of Gram matrices can be certified without subdivision. Their middle-cost class,
called `H` in the paper, removes every positive off-diagonal entry and asks whether the remaining symmetric matrix is positive
semidefinite.

The published `H` test certifies non-strict copositivity. coposit decides strict copositivity, so this model uses the exact stronger
condition that the remaining matrix is positive definite. Positive definiteness is checked by the shared fraction-free LDLT
factorization; no floating-point eigenvalue, tolerance, linear program, semidefinite program, or external solver is used.

If this new certificate fails, the model does exactly what `bundfuss_2008` does: select the most negative edge interaction, reject an
exact nonpositive two-vertex restriction, or insert the Bundfuss rational edge point and visit both child simplices depth-first.

## Name And Sources

The identifier follows coposit's `<first-author>_<year>` rule. `sponsel` names Julia Sponsel, the paper's first author, and `2012` is
the journal publication year.

Primary source:

- Julia Sponsel, Stefan Bundfuss, and Mirjam Dür, “An Improved Algorithm to Test Copositivity,” *Journal of Global Optimization*
  52(3), 537–551 (2012), [DOI 10.1007/s10898-011-9766-2](https://doi.org/10.1007/s10898-011-9766-2),
  [publisher-version PDF](https://pure.rug.nl/ws/files/2475995/2012JGlobOptimSponsel.pdf).

The paper was published online in 2011 and appeared in the 2012 journal issue. The maintained name uses the journal year.

The inherited subdivision mathematics comes from:

- Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
  Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035).

The concrete minimum-edge choice, three candidate fractions, two-child construction, child inspection order, and LIFO traversal are
copied from coposit's independent [`bundfuss_2008`](../bundfuss_2008/ALGORITHM.md) model. That model pins those choices to J. M. G.
Salmerón's public `bundfuss` implementation at
[commit `5537fd94768efbce85b3225b05bf39db8d81a332`](https://github.com/josmangarsal/copositivity-detection-bundfuss-faces/commit/5537fd94768efbce85b3225b05bf39db8d81a332).
No source from that repository is copied into this model.

The 2012 paper defines the non-strict-copositivity framework and its positive-semidefinite `H` certificate. The strict
positive-definite analogue and the decision to preserve the maintained Bundfuss split in both modes are explicit coposit choices.

## Public Decision Problem

For a nonempty square symmetric integer matrix $A$, the selected mode decides either

\[
x^TAx\geq0
\quad\text{or}\quad
x^TAx>0
\qquad\text{for every }x\in\mathbb R_+^n\setminus\{0\}.
\]

The input parser supplies a nonempty square exactly symmetric matrix. The model assumes that contract without rescanning the matrix.

A completed return value is Boolean. A cooperative timeout or the maintained open-node resource limit is unresolved and is never
converted to `false`.

## Why A Simplex Partition Is Sufficient

The quadratic form is homogeneous:

\[
(c x)^TA(c x)=c^2x^TAx \qquad(c>0).
\]

Every nonzero $x\geq0$ is a positive multiple of one point in the standard simplex

\[
\Delta^S=\left\{x\in\mathbb R_+^n:e^Tx=1\right\}.
\]

It is therefore enough to prove positivity on $\Delta^S$.

A node is an $(n-1)$-dimensional simplex

\[
\Delta=\operatorname{conv}\{v_1,\ldots,v_n\}\subseteq\Delta^S.
\]

Let

\[
V=\begin{pmatrix}v_1&\cdots&v_n\end{pmatrix}
\]

and define the node Gram matrix

\[
G=V^TAV,\qquad g_{ij}=v_i^TAv_j.
\]

Every point $x\in\Delta$ has barycentric coordinates $y\geq0$ with $e^Ty=1$ and $x=Vy$. Hence

\[
x^TAx=y^TGy.
\]

Testing the original quadratic form on one simplex is exactly the same as testing its Gram matrix on nonnegative barycentric
coordinates.

The initial simplex has $V=I$, so its Gram matrix is the input matrix $A$.

## The 2012 Certificate Framework

The paper writes the node certificate abstractly as membership in a tractable set

\[
\mathcal M\subseteq\operatorname{COP}_n.
\]

If every simplex Gram matrix in a partition belongs to $\mathcal M$, then $A$ is copositive. The original Bundfuss certificate uses
the entrywise-nonnegative cone

\[
\mathcal N=\{G:g_{ij}\geq0\text{ for every }i,j\}.
\]

The 2012 paper compares three choices:

1. $\mathcal N$, which is very cheap but often requires many subdivisions;
2. the new cone $\mathcal H$, which adds one positive-semidefinite test;
3. $\mathcal S_++\mathcal N$, which is stronger but requires a semidefinite feasibility problem.

This model implements only the middle `H` idea. It deliberately does not implement the semidefinite-programming choice.

## Constructing The `H` Matrix

For a symmetric matrix $G$, define $N^+(G)$ by retaining only positive off-diagonal entries:

\[
N^+(G)_{ij}=
\begin{cases}
g_{ij},&i\neq j\text{ and }g_{ij}>0,\\
0,&\text{otherwise}.
\end{cases}
\]

The diagonal of $N^+(G)$ is zero. Define the stripped matrix

\[
S(G)=G-N^+(G).
\]

Thus $S(G)$ keeps the complete diagonal and every nonpositive off-diagonal entry, while every positive off-diagonal entry becomes
zero. It is a symmetric Z-matrix: all of its off-diagonal entries are nonpositive.

The decomposition

\[
G=S(G)+N^+(G)
\]

is exact, and $N^+(G)$ is entrywise nonnegative.

Sponsel, Bundfuss, and Dür define

\[
\mathcal H=\{G:S(G)\succeq0\}.
\]

Because a positive-semidefinite matrix and an entrywise-nonnegative matrix are both copositive, every $G\in\mathcal H$ is
copositive. The paper proves the cone inclusions

\[
\mathcal N\subsetneq\mathcal H\subsetneq\mathcal S_++\mathcal N
\]

for orders at least three.

## Why Positive Semidefiniteness Is Not Strict Enough

The published `H` certificate permits equality. For example,

\[
G=\begin{pmatrix}1&-1\\-1&1\end{pmatrix}
\]

satisfies $S(G)=G\succeq0$, but

\[
(1,1)G(1,1)^T=0.
\]

Accepting every $S(G)\succeq0$ node would therefore misclassify a non-strict-copositive boundary matrix as strictly copositive.

The maintained strict certificate is

\[
S(G)\succ0.
\]

For every nonzero $y\geq0$,

\[
y^TGy
=y^TS(G)y+y^TN^+(G)y.
\]

The first term is strictly positive because $S(G)\succ0$. The second is nonnegative because both $y$ and $N^+(G)$ are
entrywise nonnegative. Consequently

\[
y^TGy>0,
\]

which certifies the complete simplex.

This condition is sufficient, not necessary. A node can be strictly copositive even when $S(G)$ is singular or indefinite. Failure
of the certificate is therefore never a rejection; it only means that subdivision must continue.

## Exact Positive-Definiteness Test

The model constructs $S(G)$ as an integer matrix and factors it with coposit's reusable fraction-free LDLT implementation. Symmetric
exact congruence operations expose the inertia, meaning the numbers of positive, negative, and zero quadratic directions.

The certificate passes exactly when the factorization is nonsingular and all $n$ inertia entries are positive. There is no computed
eigenvalue and no comparison with a numerical tolerance.

Every node has the same order as the input, so one factorization object sized to that order is reused for all stripped matrices. The
factored copy is discarded after inspection; the node Gram matrix remains unchanged for a possible split.

## Complete Node Decision Flow

For a node Gram matrix $G$, the maintained implementation follows this order.

### 1. Reject a nonpositive vertex

If some diagonal entry satisfies

\[
g_{ii}\leq0,
\]

then the simplex vertex $v_i$ is a real nonzero nonnegative direction with

\[
v_i^TAv_i=g_{ii}\leq0.
\]

The input is not strictly copositive, so the complete search returns `false`.

### 2. Find the first minimum negative edge

Scan off-diagonal entries in lexicographic order

\[
(0,1),(0,2),\ldots,(0,n-1),(1,2),\ldots,(n-2,n-1).
\]

Among negative entries, retain the numerically smallest value. Equal values preserve the first pair encountered. Denote the selected
indices by $i,j$ and write

\[
\alpha=g_{ii},\qquad \beta=g_{jj},\qquad \gamma=g_{ij}<0.
\]

If there is no negative off-diagonal entry, then $G$ is entrywise nonnegative. The already-verified positive diagonal gives the cheap
strict certificate

\[
y^TGy>0\qquad(y\geq0,\ y\neq0),
\]

so the node is accepted without an LDLT factorization.

### 3. Reject a nonpositive selected edge

The two-vertex restriction is

\[
\begin{pmatrix}u&v\end{pmatrix}
\begin{pmatrix}\alpha&\gamma\\\gamma&\beta\end{pmatrix}
\begin{pmatrix}u\\v\end{pmatrix},
\qquad u,v\geq0.
\]

With $\alpha,\beta>0$ and $\gamma<0$, this restriction has a nonpositive nonzero direction exactly when

\[
\gamma^2\geq\alpha\beta.
\]

The comparison is performed by exact integer multiplication. Equality rejects because the model decides strict copositivity.

### 4. Apply the strict `H` certificate

Construct $S(G)$ by zeroing positive off-diagonal entries and preserving all other entries. If

\[
S(G)\succ0,
\]

accept the node without subdivision.

The selected edge test is performed before LDLT because it is cheaper and can already supply an exact strict counterexample. This
ordering does not change classifications: a nonpositive principal two-by-two restriction would also prevent $S(G)$ from being
positive definite.

### 5. Split an unresolved node

If neither certificate applies and no witness exists, calculate the inherited Bundfuss edge point and replace the current simplex by
its two exact children.

## The Inherited Bundfuss Edge Point

For the selected edge define

\[
\lambda_1=\frac{-\gamma}{\alpha-\gamma},\qquad
\lambda_2=\frac{\beta-\gamma}{\alpha-2\gamma+\beta},\qquad
\lambda_3=\frac{\beta}{\beta-\gamma}.
\]

All numerators and denominators are positive because $\alpha,\beta>0$ and $\gamma<0$. Select

\[
\lambda=\max\left(\lambda_1,\min(\lambda_2,\lambda_3)\right).
\]

The new edge point is

\[
w=\lambda v_i+(1-\lambda)v_j.
\]

The three fractions have distinct roles:

- $\lambda_1$ is where $w^TAv_i=0$;
- $\lambda_3$ is where $w^TAv_j=0$;
- $\lambda_2$ is the stationary point of $w^TAw$ on the selected edge.

The maximum/minimum expression places the stationary candidate inside the interval bounded by the two endpoint-interaction zeros.
Fraction comparison uses cross multiplication, so the exact selected value never passes through floating point.

## Child Construction

Write the selected fraction in lowest terms as

\[
\lambda=\frac{p}{d},\qquad q=d-p.
\]

Then

\[
w=\frac{p}{d}v_i+\frac{q}{d}v_j.
\]

The parent simplex is the union of two children:

1. replace $v_i$ with $w$ and retain $v_j$;
2. retain $v_i$ and replace $v_j$ with $w$.

Every other vertex is unchanged. The children meet on their common face and their interiors do not overlap.

The rational child Gram entries involving $w$ are

\[
w^TAv_k=\frac{p g_{ik}+q g_{jk}}{d}
\]

and

\[
w^TAw=\frac{p^2\alpha+2pq\gamma+q^2\beta}{d^2}.
\]

coposit stores $d^2$ times the complete child Gram matrix. Thus

\[
d^2w^TAv_k=d\left(p g_{ik}+q g_{jk}\right)
\]

and

\[
d^2w^TAw=p^2\alpha+2pq\gamma+q^2\beta
\]

are integers. Every unchanged entry is multiplied by $d^2$.

After construction, the greatest common divisor of the upper-triangle entries is divided out. This removes only a positive common
scale and changes neither the represented quadratic form nor any subsequent sign, ratio, positive-definiteness, or split decision.

## Traversal

Both children are inspected immediately:

1. inspect the child replacing $v_i$;
2. inspect the child replacing $v_j$.

A rejected child ends the complete search. An accepted child is discarded. An unresolved child is appended to a LIFO work list.
Because the first unresolved child is appended before the second, the second child is the next node expanded when both remain.

The common exact split coefficients and new Gram row are prepared once. coposit constructs and inspects the first child before
constructing the second. If the first child rejects, the unreachable sibling is never allocated. Otherwise the second child is
constructed exactly as before. This changes no inspection or LIFO traversal order.

The strict `H` certificate changes only whether a child is accepted during inspection. It does not change the selected edge, split
point, child matrices, child inspection order, work-list order, or first-failure behavior of any node that remains unresolved.

## Complete Pseudocode

```text
inspect(G):
    if some g_ii <= 0:
        return reject

    choose the first numerically minimum negative g_ij
    if no negative off-diagonal exists:
        return accept

    if g_ij^2 >= g_ii * g_jj:
        return reject

    form S(G) by replacing positive off-diagonals with zero
    if S(G) is positive definite by exact LDLT:
        return accept

    return split at (i,j)

solve(A):
    receive parser-guaranteed nonempty, square, symmetric input
    inspect A
    reject or accept immediately when decided
    otherwise push A and its selected edge

    while unresolved nodes remain:
        pop the most recently pushed node
        prepare the exact Bundfuss split data
        construct and inspect the first child; retain it only if unresolved
        if the first child did not reject, construct and inspect the second child; retain it only if unresolved

    return true
```

## Correctness

Correctness has three independent parts.

### Rejection is sound

A nonpositive diagonal is an explicit simplex vertex witness. A selected edge with $\gamma^2\geq\alpha\beta$ has an explicit
nonpositive nonnegative two-coordinate direction. Either direction maps through the current vertex matrix $V$ to a nonzero point of
the original nonnegative simplex. Therefore every `false` result has a real strict-copositivity counterexample.

### Acceptance is sound

An entrywise-nonnegative Gram matrix with positive diagonal is strictly copositive. For the new certificate,
$G=S(G)+N^+(G)$ with $S(G)\succ0$ and $N^+(G)\geq0$, so every nonzero nonnegative barycentric vector has strictly positive quadratic
value. Accepted nodes therefore cover only regions on which the original form is strictly positive.

### Subdivision preserves coverage

The two children exactly cover their parent simplex. Returning `true` only after every descendant is accepted proves strict
positivity on the initial standard simplex and hence on the complete nonnegative orthant.

The `H` test is only an additional sound acceptance rule. Removing it yields the maintained Bundfuss traversal. Adding it cannot
hide a negative or zero direction because positive definiteness proves strict positivity on the entire accepted simplex.

## Termination And Resource Outcomes

Subdivision keeps the same matrix order. The tree can therefore grow without a dimension-decreasing bound. The 2012 theory proves
finite detection of a genuinely negative region when the partition diameter tends to zero, and finite certification of strictly
copositive inputs for certificate families containing the nonnegative cone under the same refinement assumption. coposit does not
claim a stronger termination theorem for the concrete inherited lambda rule than its sources establish.

Non-strict-copositive boundary matrices remain the important failure mode for a strict partition search. A zero can lie inside a
sequence of nested simplices without ever becoming one of the generated vertices or selected two-vertex zeros.

The maintained model bounds simultaneously unfinished nodes at 50,000. If a split would exceed that bound, the result is the
distinct unresolved node-limit outcome. Timed native calls can likewise return a cooperative timeout at safe checkpoints. Neither
outcome is `false`.

The `H` certificate may greatly reduce the tree when it succeeds, but it does not alter the unresolved tree and supplies no new
guarantee for a node on which $S(G)$ is not positive definite.

## Known Difficult Inputs

### Strict matrices outside the positive-definite `H` certificate

The strict-safe condition is deliberately narrower than the paper's non-strict `H` cone. Positive off-diagonal terms can make $G$
strictly copositive even when $S(G)$ is singular. For example, let

\[
S=\begin{pmatrix}
2&-1&-1\\
-1&1&0\\
-1&0&1
\end{pmatrix},
\qquad
N=\begin{pmatrix}
0&0&0\\
0&0&1\\
0&1&0
\end{pmatrix},
\qquad G=S+N.
\]

The matrix $S$ is positive semidefinite with kernel spanned by $(1,1,1)^T$. The added nonnegative interaction is positive on that
nonnegative kernel direction, so $G$ is strictly copositive, but $S(G)=S$ is not positive definite. The model must subdivide this
node even though a more elaborate exact strict analysis of the semidefinite kernel could certify it.

### Boundary zeros not generated by the split

Corpus matrix **9161**, the Brás-Eichfelder-Júdice matrix M5, is the retained structural example for the inherited Bundfuss family.
Its nonnegative zero is not forced to occur at a generated vertex under the minimum-edge lambda refinement. The strict `H`
certificate cannot accept a region containing that zero, so the search can continue through increasingly small simplices without a
Boolean decision.

### Cost when the certificate rarely succeeds

Every unresolved node with a negative edge now performs an exact $n\times n$ fraction-free factorization before it splits. When most
stripped matrices are not positive definite, this is additional exact arithmetic without corresponding pruning. The intended gain
therefore depends on the generated Gram matrices entering the strict interior of `H` often enough.

### Integer coefficient growth

Each rational split clears a squared denominator and later removes common content. Repeated splits can still increase integer bit
lengths substantially. The LDLT certificate then factors those larger integers exactly.

### Raw minimum-edge selection

The inherited split chooses the numerically smallest negative Gram entry. This selection is sensitive to the positive scaling of
individual simplex rays and does not normalize by the two diagonal entries. A strongly negative raw entry can be a poor refinement
choice even when another edge is closer to the strict two-generator boundary.

## Relationship To Bundfuss 2008

The two maintained models have identical behavior until a node contains a negative off-diagonal entry that does not already supply
a two-vertex witness.

`bundfuss_2008` then splits immediately. `sponsel_2012` first asks whether $S(G)\succ0$. If yes, the complete node is accepted. If
not, both models use the same selected pair, lambda value, child matrices, and traversal.

Thus Sponsel 2012 is not a dimension-reducing algorithm and does not replace the Bundfuss geometry. It is the Bundfuss partition
equipped with one stronger exact node certificate.

## Source Behavior And coposit Choices

Source-derived from Sponsel, Bundfuss, and Dür 2012:

- represent a node by $V^TAV$;
- accept nodes using membership in a tractable inner approximation of the copositive cone;
- define $N^+(G)$ from positive off-diagonal entries;
- define $S(G)=G-N^+(G)$;
- use $S(G)\succeq0$ as the paper's non-strict `H` certificate;
- retain simplex subdivision when the selected certificate does not apply.

Inherited unchanged from `bundfuss_2008`:

- strict vertex and two-generator rejection;
- first minimum negative edge selection;
- the three lambda candidates and their maximum/minimum rule;
- exact two-child construction;
- child inspection order and LIFO traversal;
- common-content reduction;
- timeout checkpoints and the open-node resource boundary.

Explicit coposit strict adaptation:

- require $S(G)\succ0$ instead of the paper's $S(G)\succeq0$;
- use exact fraction-free LDLT rather than numerical eigenvalues;
- retain the Bundfuss edge choice instead of the paper's suggested smallest-eigenvector heuristic for failed `H` tests.

The paper presents its eigenvector rule as a promising refinement strategy, not as part of the correctness theorem. The user-selected
model deliberately keeps the already exact Bundfuss traversal so the only mathematical addition is the strict-safe `H` certificate.

## Implementation-To-Algorithm Correspondence

The complete maintained implementation is local to [`solver.cpp`](solver.cpp).

| Implementation element | Responsibility |
|---|---|
| `solve` | Validate public shape and symmetry, then start the selected mode. |
| `test_copositivity` | Own the mode, reusable LDLT object, LIFO work list, traversal, and first-failure behavior. |
| `inspect` | Apply mode-dependent vertex and edge rejection, cheap nonnegative acceptance, and the selected `H` certificate. |
| `passes_h_certificate` | Construct $S(G)$ and prove exact positive semidefiniteness or definiteness according to the mode. |
| `calculate_lambda` | Evaluate and compare the three inherited rational split candidates exactly. |
| `prepare_split` | Calculate the common denominator, new Gram row, and new diagonal once. |
| `make_child` | Construct and content-reduce one reached child; the second call is delayed until the first inspection passes. |
| `divide_by_content` | Remove a harmless positive common integer scale. |

The exact inertia implementation used by the certificate is shared model-independent infrastructure in
[`fraction_free_ldlt.hpp`](../../../cpp/include/coposit/fraction_free_ldlt.hpp).

## What This Model Deliberately Does Not Do

- Strict mode does not accept the paper's complete positive-semidefinite `H` cone, because that would be unsound for strict
  copositivity; non-strict mode does accept it.
- It does not solve the paper's stronger $\mathcal S_++\mathcal N$ semidefinite feasibility problem.
- It does not use an SDP solver, LP solver, eigenvalue routine, tolerance, or floating-point split decision.
- It does not use the paper's suggested smallest-eigenvector edge heuristic.
- It does not add Salmerón's separate monotonicity route.
- It does not add Dutour splitting, Danninger projection, SNC slicing, principal-submatrix certificates, connected components, or
  duplicate-row preprocessing.
- It does not claim that the positive-definite `H` condition recognizes every strictly copositive matrix in `H`.

Changing the strict `H` condition, edge selection, lambda formula, child construction, or traversal creates a different model and
requires a new model directory or an explicit reclassification of `sponsel_2012`.
