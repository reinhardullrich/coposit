# Adaptive Zischg–Sponsel–COPOMATRIX

Classification: legacy Coposit-created strict-copositivity comparison retained for reproducibility. It combines an adaptive
Sponsel–COPOMATRIX traversal with Zischg–Bomze negative-graph decomposition after projection.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to an ordinary query.

## Idea In Plain Language

This model combines three exact ideas:

- **Sponsel subdivision** keeps the current order. It first tries a strong whole-simplex certificate and, if that fails, splits one
  troublesome edge into two child simplices.
- **COPOMATRIX projection** removes one coordinate and creates one or more lower-order problems.
- **Zischg–Bomze decomposition** splits a projected matrix into the connected components of its negative-entry graph. Cross-component
  entries are nonnegative, so strict copositivity is exactly the conjunction of the smaller component problems.

COPOMATRIX guarantees progress in dimension, but a pivot row with many positive and negative entries can create a combinatorial
number of children. Sponsel normally creates only two children and may certify a complete simplex without splitting, but repeated
same-order subdivision has no useful finite bound on boundary inputs.

The adaptive rule is:

1. Use the first COPOMATRIX pivot producing at most two immediate children.
2. If no such pivot exists, apply Sponsel's strict `H` certificate and exact Bundfuss edge split.
3. After 10,000 consecutive Sponsel splits on one branch, force COPOMATRIX at pivot zero even when it creates more children.
4. Before adaptively restarting on any matrix created by COPOMATRIX, split its negative-entry graph into components and solve those
   components independently.

Every COPOMATRIX child has lower order and resets the branch counter to zero.

## Name And Sources

**Adaptive** means that every child independently chooses between the two main operations. **Zischg** identifies Johannes Zischg's
negative-entry graph decomposition with Immanuel M. Bomze. **Sponsel** identifies Julia Sponsel, Stefan Bundfuss, and Mirjam Dür's
strengthened partition framework. **COPOMATRIX** is Jia Xu and Yong Yao's published algorithm name.

The Sponsel mathematics comes from:

> Julia Sponsel, Stefan Bundfuss, and Mirjam Dür, “An Improved Algorithm to Test Copositivity,” *Journal of Global Optimization*
> 52(3), 537–551 (2012), [DOI 10.1007/s10898-011-9766-2](https://doi.org/10.1007/s10898-011-9766-2).

Its inherited subdivision comes from:

> Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
> Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035).

The maintained exact reconstruction and strict `H` adaptation are documented in
[`../../baselines/sponsel_2012/ALGORITHM.md`](../../baselines/sponsel_2012/ALGORITHM.md).

The COPOMATRIX mathematics comes from:

> Jia Xu and Yong Yao, “An Algorithm for Determining Copositive Matrices,” *Linear Algebra and its Applications* 435(11),
> 2784–2792 (2011), [DOI 10.1016/j.laa.2011.04.038](https://doi.org/10.1016/j.laa.2011.04.038), retained as
> [`xu_yao_2011_copomatrix.pdf`](../../../research/papers/xu_yao_2011_copomatrix.pdf).

Its maintained exact strict adaptation is documented in
[`../../baselines/copomatrix_2011/ALGORITHM.md`](../../baselines/copomatrix_2011/ALGORITHM.md). The adaptive routing and cutoff follow
the structure of [`../../adaptive_dutour_copomatrix/`](../../adaptive_dutour_copomatrix/), with Sponsel replacing Dutour as the
same-order operation.

The component theorem comes from:

> Johannes Zischg and Immanuel M. Bomze, “Novel shortcut strategies in copositivity detection: Decomposition for quicker positive
> certificates,” *Operations Research Perspectives* 14 (2025), 100324,
> [DOI 10.1016/j.orp.2024.100324](https://doi.org/10.1016/j.orp.2024.100324).

Zischg and Bomze state the ordinary copositivity decomposition. Coposit's
[`STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md`](../../../research/STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md)
records the strict extension used here. No paper describes this complete hybrid, its projection-local placement of the component
rule, its narrow-pivot routing, or its 10,000-split cutoff.

## Public Decision Problem

For a nonempty square symmetric integer matrix $A$, the model decides

\[
x^TAx>0
\qquad\text{for every nonzero }x\geq0.
\]

Equality rejects. The public boundary checks nonemptiness, squareness, and exact symmetry and throws `std::invalid_argument` for an
invalid matrix. A cooperative timeout remains unresolved and is never converted to `false`.

## Node State

A recursive node stores:

- an exact symmetric integer Gram matrix $G$;
- a nonnegative integer `sponsel_streak`.

The matrix represents the original quadratic form on a current simplicial region. For a ray or vertex matrix $V$,

\[
G=V^TAV,
\qquad
(Vy)^TA(Vy)=y^TGy.
\]

The counter records consecutive Sponsel **splits** on the current branch since the last COPOMATRIX reduction. It starts at zero,
increases by one in both Sponsel children, and resets to zero in every lower-order COPOMATRIX child. A successful Sponsel certificate
or a rejection ends the branch and therefore does not increment the counter.

Negative-graph components are computed transiently from each COPOMATRIX child and are not retained as node state. Component blocks
restart the adaptive solver with streak zero because they are lower-order descendants of a projection.

Every parent decision is the conjunction of its exact child decisions. The solver rejects on the first failed child and accepts only
after every required child passes.

## Complete Routing

For a node $G$ of order $n$:

1. Apply exact direct criteria when $n\leq3$.
2. Reject if any diagonal entry is nonpositive.
3. Scan rows in index order and choose the first COPOMATRIX pivot producing at most two immediate children.
4. If such a pivot exists, apply COPOMATRIX, component-decompose each projected child, and reset all child counters.
5. If no pivot is narrow and `sponsel_streak >= 10000`, force COPOMATRIX at pivot zero, component-decompose each projected child,
   and reset all child counters.
6. Otherwise apply the Sponsel certificate and, if still unresolved, one Sponsel/Bundfuss split.

```text
check(G, streak):
    checkpoint

    if order(G) <= 3:
        return direct_strict_test(G)

    if any diagonal entry is <= 0:
        return false

    pivot := first COPOMATRIX pivot with at most two children
    if pivot exists:
        return copomatrix(G, pivot)       # decompose each projection child; restart at zero

    if streak >= 10000:
        return copomatrix(G, 0)           # decompose each projection child; forced before Sponsel

    if Sponsel rejects G:
        return false
    if Sponsel certifies G:
        return true

    (G_left, G_right) := exact Sponsel split
    if check(G_left, streak + 1) is false:
        return false
    return check(G_right, streak + 1)
```

Thus a branch can contain exactly 10,000 consecutive Sponsel splits. The next node must reduce its order before another Sponsel step.

## Direct Criteria Through Order Three

Order zero is internally true, although public input cannot be empty. Order one passes exactly when its sole diagonal is positive.

For

\[
G=\begin{pmatrix}a&b\\b&c\end{pmatrix},
\]

order two passes exactly when $a,c>0$ and either $b\geq0$ or $ac-b^2>0$.

Order three applies that rule to every principal pair and then uses the exact determinant/adjugate form of Hadeler's order-three
criterion. These are the same integer-only direct rules owned by the existing adaptive models.

## The Narrow COPOMATRIX Operation

For a selected pivot coordinate, write

\[
G=\begin{pmatrix}
a&p^T\\
p&B
\end{pmatrix},
\qquad a>0.
\]

For a nonnegative vector $(t,y)$,

\[
q(t,y)=at^2+2t\,p^Ty+y^TBy.
\]

The principal child $B$ is always required. Completing the square shows that the additional problem uses the division-free Schur
matrix

\[
S=aB-pp^T
\]

on the half-region

\[
y\geq0,
\qquad p^Ty\leq0.
\]

Therefore

\[
\operatorname{strict}(G)
\iff
\operatorname{strict}(B)
\land
\left[y^TSy>0\text{ for every nonzero }y\geq0\text{ with }p^Ty\leq0\right].
\]

### Sign classes and child count

Let $P,N,Z$ contain the positive, negative, and zero positions of $p$, and let $s=|P|$, $t=|N|$. After positive diagonal
normalization, the negative half-simplex has negative coordinate vertices and one midpoint on every positive-negative edge.

The implementation uses the primitive integer boundary ray

\[
r_{ij}=\frac{|p_j|}{g}e_i+\frac{p_i}{g}e_j,
\qquad
g=\gcd(p_i,|p_j|),
\]

for $i\in P$, $j\in N$. It lies exactly on $p^Tr_{ij}=0$.

The total immediate child count is:

- one if $t=0$;
- two if $s=0$ and $t>0$;
- otherwise

  \[
  1+\binom{s+t-1}{s}.
  \]

This count is at most two exactly when $s=0$ or $t\leq1$. The adaptive gate chooses the first row meeting that condition. At the
10,000-split cutoff, pivot zero is used regardless of its child count.

### Negative-half-simplex traversal

Zero coordinate rays are included in every child. For ordered positive and negative lists, the Xu–Yao staircase proceeds as follows:

```text
staircase(P[i:], N[j:], rays):
    if all positive labels are consumed:
        append all remaining negative coordinate rays and check the child
    else if only N[j] remains:
        append e_N[j] and all boundary rays r_(P[k],N[j]) for k >= i; check the child
    else:
        append r_(P[i],N[j])
        recurse first after consuming P[i]
        recurse second after consuming N[j]
```

For each final sparse ray matrix $R$, the Schur child is $R^TSR$. Every principal and transformed Schur child has order $n-1$ and
passes through the projection-local component rule described next. Each connected non-singleton component re-enters the complete
adaptive algorithm with counter zero.

Selecting a pivot other than zero is an exact coordinate permutation before applying the COPOMATRIX projection theorem. The source
baseline's fixed pivot is changed only by this explicitly Coposit-created adaptive model.

## Negative-Graph Components After Projection

For an exact symmetric projected Gram matrix $M$, define its negative-entry graph

\[
G^-(M)=(\{0,\ldots,m-1\},E),
\qquad
i-j\in E \iff m_{ij}<0.
\]

Let $C_1,\ldots,C_r$ be the connected components. By definition, every entry between two different components is nonnegative. For
any nonnegative vector $x$, partitioned along those components,

\[
x^TMx
=
\sum_{k=1}^r x_{C_k}^TM[C_k,C_k]x_{C_k}
+2\sum_{1\leq k<\ell\leq r}x_{C_k}^TM[C_k,C_\ell]x_{C_\ell}.
\]

Every cross term in the second sum is nonnegative. Therefore:

\[
M\text{ is strictly copositive}
\iff
M[C_k,C_k]\text{ is strictly copositive for every }k.
\]

The forward implication follows by padding a component vector with zeros. For the reverse implication, every nonzero $x\geq0$ has
at least one nonzero component restriction; its component quadratic term is strictly positive, all other component terms are
nonnegative, and all cross terms are nonnegative. This proof applies unchanged to principal blocks, division-free Schur matrices,
and transformed Schur Gram matrices because it uses only the exact entries of the child being tested.

The implementation first rejects a nonpositive diagonal. It then discovers components in increasing order of their smallest
unvisited coordinate using a depth-first scan of exact negative signs:

\[
\texttt{projected\_check}(M):
\begin{cases}
\text{reject}, & \exists i:m_{ii}\leq0,\\
\texttt{check}(M,0), & G^-(M)\text{ is connected},\\
\bigwedge_k\texttt{check}(M[C_k,C_k],0), & G^-(M)\text{ is disconnected}.
\end{cases}
\]

Singleton components need no recursive call after the positive-diagonal check. If there are no negative edges, all components are
singletons and the child passes immediately; this is exactly the former positive-diagonal, entrywise-nonnegative shortcut.

The rule is deliberately applied only to matrices produced by a COPOMATRIX projection. It does not decompose the original matrix or
ordinary Sponsel children. Thus the experiment measures whether dimension reduction exposes useful disconnected structure without
changing the base model's same-order Sponsel routing.

## The Sponsel Operation

Sponsel studies the quadratic form on the standard simplex. The current Gram matrix $G$ has entries

\[
g_{ij}=v_i^TAv_j
\]

for the current simplex vertices $v_i$.

### Select and test the edge

The model selects the numerically smallest negative off-diagonal entry $g_{ij}$, retaining the first pair on ties. Write

\[
\alpha=g_{ii},
\qquad
\beta=g_{jj},
\qquad
\gamma=g_{ij}<0.
\]

If no negative entry exists, positive diagonal and entrywise nonnegativity certify the node.

If

\[
\gamma^2\geq\alpha\beta,
\]

the two-vertex edge contains a nonpositive direction and the model rejects. Equality rejects because the decision is strict.

### Strict `H` certificate

If the selected edge passes, construct $S_H(G)$ by retaining the diagonal and every negative off-diagonal entry while replacing
every positive off-diagonal entry with zero. Then

\[
G=S_H(G)+N^+(G),
\]

where $N^+(G)$ is entrywise nonnegative.

If

\[
S_H(G)\succ0,
\]

then for every nonzero $y\geq0$,

\[
y^TGy=y^TS_H(G)y+y^TN^+(G)y>0.
\]

The complete simplex is accepted. Positive definiteness is checked by exact fraction-free LDLT. Failure is not rejection; it only
means subdivision is required.

### Exact edge point and children

When the node remains unresolved, Sponsel retains the Bundfuss split. Define

\[
\lambda_1=\frac{-\gamma}{\alpha-\gamma},
\qquad
\lambda_2=\frac{\beta-\gamma}{\alpha-2\gamma+\beta},
\qquad
\lambda_3=\frac{\beta}{\beta-\gamma},
\]

and choose

\[
\lambda=\max\left(\lambda_1,\min(\lambda_2,\lambda_3)\right).
\]

The new edge point is

\[
w=\lambda v_i+(1-\lambda)v_j.
\]

Two child simplices cover the parent:

- replace $v_i$ by $w$;
- replace $v_j$ by $w$.

Writing $\lambda=p/d$ in lowest terms and $q=d-p$, child Gram matrices are stored after multiplication by $d^2$. Entries
involving the new point use

\[
d^2w^TAv_k=d\left(pg_{ik}+qg_{jk}\right),
\]

and

\[
d^2w^TAw=p^2\alpha+2pq\gamma+q^2\beta.
\]

The common integer content is removed afterward. This changes only positive scale. The first child is checked to completion before
the second, and both inherit `sponsel_streak + 1`.

## Acceptance, Rejection, And Witnesses

The model rejects when:

- a diagonal entry is nonpositive;
- a direct low-order criterion fails;
- a selected Sponsel edge satisfies $\gamma^2\geq\alpha\beta$;
- any connected component of a principal or transformed Schur child rejects.

It accepts when:

- a direct criterion passes;
- a node is entrywise nonnegative with positive diagonal;
- the strict Sponsel `H` certificate passes;
- every connected component of a projected child passes;
- or every child in an exact Sponsel cover or COPOMATRIX projection passes.

The maintained API returns only the Boolean classification and does not reconstruct a witness in the original coordinates.

## Exact Arithmetic

All matrix entries, determinant and edge products, rational comparisons, Schur products, greatest common divisors, sparse rays,
child updates, and LDLT decisions use FLINT arbitrary-precision integers. Component construction depends only on exact
`integer::sign() < 0` tests; it introduces no numerical threshold.

The model uses no floating point, numerical tolerance, rational matrix storage, fixed-width support mask, or dimension cap. One
fraction-free LDLT object sized to the input order is reused by every Sponsel certificate. COPOMATRIX children may be smaller, so the
same storage remains sufficient.

## Traversal And Termination

Traversal is recursive and depth-first:

- a Sponsel split checks the child replacing the first selected endpoint before its sibling;
- a COPOMATRIX node checks the principal child before its Schur children;
- each projected child checks negative-graph components in increasing order of their first undiscovered coordinate;
- the Xu–Yao staircase consumes a positive label before the alternative consuming a negative label;
- any rejection short-circuits all remaining siblings.

Along every branch, at most 10,000 consecutive Sponsel splits retain the current order. The next nonterminal node must use COPOMATRIX,
whose children all have order one smaller. Each Sponsel node has two children and each COPOMATRIX decomposition has finitely many
children. A disconnected projected child is replaced by proper principal blocks; a connected child is restarted once without
another component call until its next COPOMATRIX projection. Therefore the component rule cannot create a same-matrix recursion
cycle, and the complete mathematical recursion tree is finite for every finite input.

This model deliberately does not inherit Sponsel's 50,000 simultaneously-open-node guard. Recursive depth-first traversal keeps
only the current call stack and pending siblings, while the forced projection supplies the hybrid's termination mechanism. A finite
tree may nevertheless be far too large for practical time or memory. Timeout or process failure remains unresolved.

## Source Behavior And Coposit Choices

Source-derived Sponsel behavior:

- minimum negative-entry edge selection and first-tie rule;
- exact two-vertex rejection;
- the stripped `H` matrix;
- exact Bundfuss $\lambda$ rule and two child simplices.

Source-derived COPOMATRIX behavior:

- the principal child and division-free Schur matrix;
- pivot-sign normalization and primitive positive-negative boundary geometry;
- Xu–Yao negative-half-simplex decomposition.

Source-derived Zischg–Bomze behavior:

- the graph edge rule $m_{ij}<0$;
- exact decomposition into negative-graph components;
- conjunction of component copositivity decisions, extended here to the strict decision by the same quadratic-form identity.

Coposit-created choices:

- strict positive definiteness instead of the paper's ordinary positive-semidefinite `H` certificate;
- direct criteria through order three;
- first narrow COPOMATRIX pivot across all rows;
- Sponsel only when no narrow pivot exists;
- the branch-local 10,000-Sponsel-split cutoff;
- forced pivot zero;
- counter reset after every order reduction;
- recursive adaptive restart in every child;
- applying negative-graph decomposition after every COPOMATRIX projection but not at the public root or ordinary Sponsel nodes;
- skipping singleton component calls after their positive diagonal has been verified;
- checking the child replacing the first selected endpoint to completion before its sibling;
- omission of the standalone Sponsel open-node limit.

Representation-only choices include FLINT integers, denominator clearing, primitive sparse rays, matrix-only node state, reusable
LDLT storage, an $O(m^2)$ depth-first component scan without a separately stored adjacency matrix, and cooperative timeout
checkpoints.

Changing the narrow gate, pivot order, cutoff, forced pivot, component placement, component sign rule, Sponsel certificate, edge
choice, split formula, Vmatrix traversal, child order, or counter semantics changes the maintained model's mathematical control
flow and therefore requires a distinct result identity.

## Known Difficult Inputs

### Connected projected negative graphs

If every COPOMATRIX child has a connected or nearly connected negative-entry graph, the model pays an $O(m^2)$ sign scan but obtains
no decomposition. Dense negative patterns are the clearest case. A disconnected graph with one dominant component gives only small
side blocks and leaves almost all downstream work unchanged.

### Decomposition appears only after projection

The model intentionally does not split the input or ordinary Sponsel children. A matrix whose useful negative components disappear
under the chosen Schur or ray congruence receives no benefit, and a matrix already decomposable at the root must first reach a
COPOMATRIX operation before the rule can act.

### Forced COPOMATRIX breadth

After 10,000 Sponsel splits, pivot zero is forced even when its mixed-sign row creates

\[
1+\binom{s+t-1}{s}
\]

children. The cutoff guarantees dimensional progress but may replace a deep binary refinement with a very broad projection.

### Strict matrices outside the `H` certificate

When $S_H(G)$ is not positive definite, every wide same-order node pays for an exact factorization and then still performs a split.
Large strict trees must exhaust every child because no rejection can short-circuit them.

### Boundary zeros not generated exactly

Before the cutoff, the Bundfuss edge points may approach a high-support zero without producing it as a vertex or two-vertex witness.
The forced COPOMATRIX step terminates that same-order streak, but its negative-half-simplex can itself be combinatorially large.

### Minimum-entry scaling sensitivity

Sponsel selects the raw most negative entry. Positive scaling of current rays can change that selection even though it preserves the
underlying copositivity question. The chosen edge need not be the pair closest to its normalized two-generator boundary.

### Exact coefficient growth

Sponsel denominator clearing and COPOMATRIX Schur products both enlarge integer coefficients. Repeated alternation can therefore make
later LDLT factorizations and congruence transforms expensive even when the number of nodes is moderate.

## Implementation Map

The complete implementation is local to [`solver.cpp`](solver.cpp).

| Function | Responsibility |
|---|---|
| `solve` | Validate the public matrix, allocate reusable LDLT storage, and start at streak zero. |
| `adaptive_zischg_sponsel_copomatrix_checker::check` | Apply direct tests, narrow routing, forced cutoff, and Sponsel fallback. |
| `first_narrow_copomatrix_pivot` | Return the first row whose COPOMATRIX projection has at most two children. |
| `check_copomatrix` | Build and visit the principal and negative-side Schur problems. |
| `check_projection` | Reject a bad diagonal, split a projected negative graph, and restart connected blocks at streak zero. |
| `negative_components` | Discover exact negative-entry connected components without storing an adjacency matrix. |
| `check_negative_staircase` | Generate Xu–Yao Schur simplices depth-first. |
| `make_principal_block`, `make_schur_block` | Construct the exact lower-order projection matrices. |
| `coordinate_ray`, `pair_ray`, `transform` | Build sparse integer rays and form $R^TMR$. |
| `check_sponsel` | Select/test the minimum edge, apply `H`, and recurse into two exact split children. |
| `passes_strict_h_certificate` | Strip positive off-diagonal entries and test exact positive definiteness. |
| `calculate_lambda`, `sponsel_split` | Select the exact Bundfuss edge point and construct denominator-cleared children. |
