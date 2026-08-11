# Adaptive Sponsel–COPOMATRIX

Classification: Coposit-created maintained ordinary- and strict-copositivity model.

Public mode boundary: `solve(A, copositivity_mode::copositive)` decides ordinary copositivity and
`solve(A, copositivity_mode::strictly_copositive)` decides strict copositivity. Strict remains the default. The Python `both` mode is
not advertised for this model because the two answers require separate traversals.

## Idea In Plain Language

This model combines two exact algorithms:

- **Sponsel subdivision** keeps the current order. It first tries a mode-dependent whole-simplex certificate and, if that fails,
  splits one troublesome edge into two child simplices.
- **COPOMATRIX projection** removes one coordinate and creates one or more lower-order problems.

COPOMATRIX guarantees progress in dimension, but a pivot row with many positive and negative entries can create a combinatorial
number of children. Sponsel normally creates only two children and may certify a complete simplex without splitting, but repeated
same-order subdivision has no useful finite bound on boundary inputs. The hybrid therefore counts, without constructing, the exact
number of immediate COPOMATRIX children for every current pivot.

The adaptive rule is:

1. Choose the COPOMATRIX pivot with the fewest immediate children, retaining the first local pivot on a tie.
2. If that minimum is at most two, apply COPOMATRIX immediately.
3. Otherwise apply Sponsel's ordinary or strict `H` certificate and exact Bundfuss edge split.
4. After 1,000 consecutive Sponsel splits on one branch, force COPOMATRIX at the same minimum-child pivot.

Every COPOMATRIX child has lower order and resets the branch counter to zero.

## Name And Sources

**Adaptive** means that every child independently chooses between the two operations. **Sponsel** identifies Julia Sponsel, Stefan
Bundfuss, and Mirjam Dür's strengthened partition framework. **COPOMATRIX** is Jia Xu and Yong Yao's published algorithm name.

The Sponsel mathematics comes from:

> Julia Sponsel, Stefan Bundfuss, and Mirjam Dür, “An Improved Algorithm to Test Copositivity,” *Journal of Global Optimization*
> 52(3), 537–551 (2012), [DOI 10.1007/s10898-011-9766-2](https://doi.org/10.1007/s10898-011-9766-2).

Its inherited subdivision comes from:

> Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
> Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035).

The maintained exact ordinary reconstruction and strict `H` adaptation are documented in
[`../baselines/sponsel_2012/ALGORITHM.md`](../baselines/sponsel_2012/ALGORITHM.md).

The COPOMATRIX mathematics comes from:

> Jia Xu and Yong Yao, “An Algorithm for Determining Copositive Matrices,” *Linear Algebra and its Applications* 435(11),
> 2784–2792 (2011), [DOI 10.1016/j.laa.2011.04.038](https://doi.org/10.1016/j.laa.2011.04.038), retained as
> [`xu_yao_2011_copomatrix.pdf`](../../research/papers/xu_yao_2011_copomatrix.pdf).

Its maintained exact ordinary implementation and strict adaptation are documented in
[`../baselines/copomatrix_2011/ALGORITHM.md`](../baselines/copomatrix_2011/ALGORITHM.md). The adaptive routing and cutoff follow the
structure of [`../adaptive_dutour_copomatrix/`](../adaptive_dutour_copomatrix/), with Sponsel replacing Dutour as the same-order
operation.

No paper describes this hybrid, its minimum-child pivot rule, or its 1,000-split cutoff.

## Public Decision Problem

For a nonempty square symmetric integer matrix $A$, ordinary mode decides

\[
x^TAx\geq0
\qquad\text{for every nonzero }x\geq0,
\]

while strict mode decides

\[
x^TAx>0
\qquad\text{for every nonzero }x\geq0.
\]

Equality is accepted in ordinary mode and rejected in strict mode. The public boundary checks nonemptiness, squareness, and exact
symmetry and throws `std::invalid_argument` for an invalid matrix. A cooperative timeout remains unresolved and is never converted
to `false`.

## Node State

A recursive node stores:

- an exact symmetric integer Gram matrix $G$;
- a nonnegative integer `sponsel_streak`.

The checker also retains the selected public mode for the entire traversal. Children never change from ordinary to strict rules or
vice versa.

The matrix represents the original quadratic form on a current simplicial region. For a ray or vertex matrix $V$,

\[
G=V^TAV,
\qquad
(Vy)^TA(Vy)=y^TGy.
\]

The counter records consecutive Sponsel **splits** on the current branch since the last COPOMATRIX reduction. It starts at zero,
increases by one in both Sponsel children, and resets to zero in every lower-order COPOMATRIX child. A successful Sponsel certificate
or a rejection ends the branch and therefore does not increment the counter.

Every parent decision is the conjunction of its exact child decisions. The solver rejects on the first failed child and accepts only
after every required child passes.

## Complete Routing

For a node $G$ of order $n$:

1. Apply exact direct criteria when $n\leq3$.
2. Reject a negative diagonal in ordinary mode or a nonpositive diagonal in strict mode.
3. Count the exact immediate COPOMATRIX children for every pivot and choose the smallest count, retaining the first local pivot on a
   tie.
4. If the minimum is at most two, apply COPOMATRIX and reset all child counters.
5. If the minimum exceeds two and `sponsel_streak >= 1000`, force COPOMATRIX at the same minimum-child pivot and reset all child
   counters.
6. Otherwise apply the mode-dependent Sponsel edge test and `H` certificate and, if still unresolved, one Sponsel/Bundfuss split.

```text
check(G, streak, mode):
    checkpoint

    if order(G) <= 3:
        return direct_test(G, mode)

    if ordinary mode and any diagonal entry is < 0:
        return false
    if strict mode and any diagonal entry is <= 0:
        return false

    pivot := first pivot attaining the minimum exact COPOMATRIX child count
    if child_count(pivot) <= 2:
        return copomatrix(G, pivot, mode)       # lower-order children restart at zero

    if streak >= 1000:
        return copomatrix(G, pivot, mode)       # force the least-branched projection

    if Sponsel rejects G in mode:
        return false
    if Sponsel certifies G in mode:
        return true

    (G_left, G_right) := exact Sponsel split
    if check(G_left, streak + 1, mode) is false:
        return false
    return check(G_right, streak + 1, mode)
```

Thus a branch can contain exactly 1,000 consecutive Sponsel splits. The next node must reduce its order before another Sponsel step.

## Direct Criteria Through Order Three

Order zero is internally true, although public input cannot be empty. Order one passes when its sole diagonal is nonnegative in
ordinary mode and positive in strict mode.

For

\[
G=\begin{pmatrix}a&b\\b&c\end{pmatrix},
\]

ordinary order two passes exactly when $a,c\geq0$ and either $b\geq0$ or $ac-b^2\geq0$. Strict order two replaces the three
non-strict diagonal and determinant inequalities by strict ones.

Order three applies that rule to every principal pair and then uses the exact determinant/adjugate form of Hadeler's order-three
criterion, with equality accepted only in ordinary mode. The implementation calls the shared integer-only
`small_copositivity::check` routine, so both modes use the same established order-one-through-three boundary rules as the literature
baselines.

## The Minimum-Child COPOMATRIX Operation

For a selected pivot coordinate, write

\[
G=\begin{pmatrix}
a&p^T\\
p&B
\end{pmatrix},
\qquad a\geq0\text{ in ordinary mode},
\qquad a>0\text{ in strict mode}.
\]

For a nonnegative vector $(t,y)$,

\[
q(t,y)=at^2+2t\,p^Ty+y^TBy.
\]

The principal child $B$ is always required. When $a=0$ in ordinary mode, no Schur matrix is formed. If any component of $p$ is
negative, increasing $t$ along that coordinate gives a negative value, so the node rejects. If $p\geq0$, all terms involving $t$
are nonnegative and the already-checked principal child $B$ decides the node.

For $a>0$, completing the square shows that the additional problem uses the division-free Schur
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
\operatorname{property}_{m}(G)
\iff
\operatorname{property}_{m}(B)
\land
\left[y^TSy\mathrel{\triangleright_m}0\text{ for every nonzero }y\geq0\text{ with }p^Ty\leq0\right],
\]

where $m$ is the selected mode, $\triangleright_m$ is $\geq$ in ordinary mode and $>$ in strict mode, and
$\operatorname{property}_m$ denotes the corresponding copositivity predicate.

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

For an ordinary-mode zero diagonal, the actual operation creates only the principal child and then either rejects on a negative
pivot-row entry or accepts that child's result; its predicted child count is therefore one.

For every other pivot, the count is at most two exactly when $s=0$ or $t\leq1$. The model computes these counts exactly with FLINT
integers, chooses the smallest count over all rows, and retains the first local row on a tie. It does not construct any rejected
pivot's children. A minimum of one or two triggers COPOMATRIX immediately. A larger minimum sends the node to Sponsel unless the
1,000-split cutoff has been reached, in which case the same minimum-child pivot is forced.

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

For each final sparse ray matrix $R$, the Schur child is $R^TSR$. Every principal and transformed Schur child has order $n-1$
and re-enters the complete adaptive algorithm with counter zero and the unchanged mode. An entrywise-nonnegative child passes
immediately after its diagonal is known nonnegative in ordinary mode or positive in strict mode.

Selecting a pivot other than zero is an exact coordinate permutation before applying the COPOMATRIX projection theorem. The source
baseline's fixed pivot is changed only by this explicitly Coposit-created adaptive model.

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

If no negative entry exists, the mode-appropriate diagonal test and entrywise nonnegativity certify the node.

The selected edge rejects ordinary copositivity exactly when

\[
\gamma^2>\alpha\beta.
\]

Strict mode rejects when

\[
\gamma^2\geq\alpha\beta.
\]

Thus equality is retained as a valid zero in ordinary mode and rejected in strict mode.

### Mode-dependent `H` certificate

If the selected edge passes, construct $S_H(G)$ by retaining the diagonal and every negative off-diagonal entry while replacing
every positive off-diagonal entry with zero. Then

\[
G=S_H(G)+N^+(G),
\]

where $N^+(G)$ is entrywise nonnegative.

In ordinary mode, the node is accepted if

\[
S_H(G)\succeq0.
\]

In strict mode, it is accepted only if

\[
S_H(G)\succ0,
\]

then for every nonzero $y\geq0$,

\[
y^TGy=y^TS_H(G)y+y^TN^+(G)y\mathrel{\triangleright_m}0.
\]

The complete simplex is accepted. Positive semidefiniteness or positive definiteness is checked by the same exact fraction-free LDLT
factorization according to the selected mode. Certificate failure is not rejection; it only means subdivision is required.

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

- a diagonal entry is negative in ordinary mode or nonpositive in strict mode;
- a direct low-order criterion fails;
- a zero ordinary COPOMATRIX pivot has a negative off-diagonal component;
- a selected Sponsel edge satisfies $\gamma^2>\alpha\beta$, or satisfies equality in strict mode;
- any principal or transformed Schur child rejects.

It accepts when:

- a direct criterion passes;
- a node is entrywise nonnegative with mode-valid diagonal;
- the ordinary positive-semidefinite or strict positive-definite Sponsel `H` certificate passes;
- or every child in an exact Sponsel cover or COPOMATRIX projection passes.

The maintained API returns only the Boolean classification and does not reconstruct a witness in the original coordinates.

## Exact Arithmetic

All matrix entries, determinant and edge products, rational comparisons, Schur products, greatest common divisors, sparse rays,
child updates, and LDLT decisions use FLINT arbitrary-precision integers.

The model uses no floating point, numerical tolerance, rational matrix storage, fixed-width support mask, or dimension cap. One
fraction-free LDLT object sized to the input order is reused by every Sponsel certificate. COPOMATRIX children may be smaller, so the
same storage remains sufficient.

## Traversal And Termination

Traversal is recursive and depth-first:

- a Sponsel split checks the child replacing the first selected endpoint before its sibling;
- a COPOMATRIX node checks the principal child before its Schur children;
- the Xu–Yao staircase consumes a positive label before the alternative consuming a negative label;
- any rejection short-circuits all remaining siblings.

Along every branch, at most 1,000 consecutive Sponsel splits retain the current order. The next nonterminal node must use COPOMATRIX,
whose children all have order one smaller. Each Sponsel node has two children and each COPOMATRIX decomposition has finitely many
children. Therefore the complete mathematical recursion tree is finite for every finite input.

This model deliberately does not inherit Sponsel's 50,000 simultaneously-open-node guard. Recursive depth-first traversal keeps
only the current call stack and pending siblings, while the forced projection supplies the hybrid's termination mechanism. A finite
tree may nevertheless be far too large for practical time or memory. Timeout or process failure remains unresolved.

## Source Behavior And Coposit Choices

Source-derived Sponsel behavior:

- ordinary nonnegative diagonals and strict positive diagonals;
- minimum negative-entry edge selection and first-tie rule;
- ordinary $\gamma^2>\alpha\beta$ and strict $\gamma^2\geq\alpha\beta$ edge rejection;
- the stripped `H` matrix;
- ordinary positive-semidefinite and strict positive-definite `H` certification;
- exact Bundfuss $\lambda$ rule and two child simplices.

Source-derived COPOMATRIX behavior:

- the principal child and division-free Schur matrix;
- the ordinary zero-pivot rule;
- pivot-sign normalization and primitive positive-negative boundary geometry;
- Xu–Yao negative-half-simplex decomposition.

Coposit-created choices:

- shared exact ordinary and strict direct criteria through order three;
- exact minimum-child COPOMATRIX pivot across all rows, with first-local-index tie breaking;
- Sponsel only when the minimum COPOMATRIX child count exceeds two;
- the branch-local 1,000-Sponsel-split cutoff;
- forcing the same minimum-child pivot at the cutoff;
- counter reset after every order reduction;
- recursive adaptive restart in every child;
- checking the child replacing the first selected endpoint to completion before its sibling;
- omission of the standalone Sponsel open-node limit.

Representation-only choices include FLINT integers, exact binomial child counts, denominator clearing, primitive sparse rays,
matrix-only node state, one mode-selected reusable LDLT object, and cooperative timeout checkpoints.

Changing the minimum-child gate, pivot selection, cutoff, forced pivot, Sponsel certificate, edge choice, split formula, Vmatrix
traversal, child order, or counter semantics changes the maintained model's mathematical control flow and therefore requires a
distinct result identity. Historical binary hashes remain separate even when the model identifier is intentionally retained.

## Known Difficult Inputs

### Forced COPOMATRIX breadth

After 1,000 Sponsel splits, the least-branched pivot is forced even when every mixed-sign row creates more than two children. Its
child count can still equal

\[
1+\binom{s+t-1}{s}
\].

The cutoff guarantees dimensional progress but may replace a deep binary refinement with a very broad projection.

### Minimum immediate breadth is not minimum total work

The pivot rule minimizes only the number of children created at the current node. It has no information about the sizes of the
descendant trees under those children. A one-child projection can therefore lead to substantially more later work than a two-child
projection. Corpus matrix 9648 is a reproducible example on which the minimum-child choice enters a harder descendant search than the
preceding first-narrow-pivot rule.

### Strict matrices outside the `H` certificate

When $S_H(G)$ is not positive definite, every wide same-order node pays for an exact factorization and then still performs a split.
Large strict trees must exhaust every child because no rejection can short-circuit them.

### Boundary zeros not generated exactly

In ordinary mode a boundary zero is valid and must eventually be covered rather than treated as rejection. Before the cutoff, the
Bundfuss edge points may approach a high-support zero without producing it as a vertex or two-vertex equality. The forced COPOMATRIX
step terminates that same-order streak, but its negative-half-simplex can itself be combinatorially large.

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
| `solve` | Validate the public matrix, retain the selected mode, allocate reusable LDLT storage, and start at streak zero. |
| `adaptive_sponsel_copomatrix_checker::check` | Apply mode-dependent direct tests, minimum-child routing, forced cutoff, and Sponsel fallback. |
| `diagonal_fails` | Apply nonnegative ordinary or positive strict diagonal semantics consistently. |
| `minimum_child_copomatrix_pivot` | Count every pivot exactly and return the first row attaining the minimum immediate-child count. |
| `check_copomatrix` | Build and visit the principal and negative-side Schur problems. |
| `check_projection` | Reject a bad diagonal, accept an easy nonnegative child, or restart adaptively at streak zero. |
| `check_negative_staircase` | Generate Xu–Yao Schur simplices depth-first. |
| `make_principal_block`, `make_schur_block` | Construct the exact lower-order projection matrices. |
| `coordinate_ray`, `pair_ray`, `transform` | Build sparse integer rays and form $R^TMR$. |
| `check_sponsel` | Select/test the minimum edge in the selected mode, apply `H`, and recurse into two exact split children. |
| `passes_h_certificate` | Strip positive off-diagonal entries and test exact semidefiniteness or definiteness by mode. |
| `calculate_lambda`, `sponsel_split` | Select the exact Bundfuss edge point and construct denominator-cleared children. |
