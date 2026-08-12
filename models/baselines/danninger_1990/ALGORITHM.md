# Danninger 1990

Classification: exact Coposit reconstruction of Danninger's published reduction; the concrete triangulation and traversal are
pinned to the retained experiment rather than a recoverable original program.

## Decision Modes

`solve(A, mode)` runs the same finite dimension-reducing recursion for non-strict or strict copositivity. Strict mode requires every
selected diagonal pivot $a$ and every recursive child value to be positive. Non-strict mode permits equality. Its only additional
algebraic case is a zero pivot:

\[
A=\begin{pmatrix}0&p^T\\p&B\end{pmatrix}.
\]

If some $p_i<0$, then $te_1+e_{i+1}$ has negative quadratic value for sufficiently large $t$, so non-strict copositivity fails. If
$p\geq0$, the cross term is harmless and $A$ is copositive exactly when $B$ is copositive; no Schur matrix is formed. For $a>0$,
both modes use the same $B$ and $aB-pp^T$ children, primitive rays, staircase triangulation, and depth-first order. The exact
order-one through order-three base criteria use non-strict or strict comparisons according to the selected mode.

The default remains `strictly_copositive`. `solve(A, mode)` answers one selected predicate. `classify(A)` follows the non-strict
recursion once and returns both `is_copositive` and `is_strictly_copositive`; it does not run two recursion trees. The only possible
pairs are `{false, false}`, `{true, false}`, and `{true, true}` because strict copositivity implies non-strict copositivity.

## What The Algorithm Does

Danninger's algorithm removes one matrix coordinate at every recursive step. It does this by minimizing the quadratic form over
that coordinate exactly. The remaining nonnegative orthant is divided into two regions according to the sign of one linear
expression. Each region becomes one or more smaller strict-copositivity problems of order $n-1$.

This guaranteed dimension reduction is the main attraction: every branch is finite. Its weakness is branching. When the eliminated
row contains many positive and many negative entries, the two regions may need a very large number of simplicial cones.

## Name And Sources

The identifier follows Coposit's `<first-author>_<year>` rule. It names Gabriele Danninger and the 1990 publication year of:

> Gabriele Danninger, “A Recursive Algorithm for Determining (Strict) Copositivity of a Symmetric Matrix,” in *XIV Symposium on
> Operations Research (Ulm, 1989)*, *Methods of Operations Research*, volume 62, Hain, 1990, pages 45–52.

The [German National Library contents](https://d-nb.info/901006696/04) confirms the title, author, volume, year, and starting page.
No original source code and no complete publicly accessible copy of the eight-page article were found during this audit. The
[local note](../../../research/papers/Danninger/danninger_1990_recursive_algorithm_strict_copositivity.md) is explicitly a
mathematical reconstruction, not a transcription. The implementation follows that reconstruction and the independently written
[FracESSA experiment](../../../experiments/copositivity_danninger_2026-08-07/danninger.cpp).
The retained experiment has SHA-256 `6f043369e2f5b752bd83ae9569ecc0cc4a779b7948e18ba03c7de6deb018a07e`.

## The One-Coordinate Reduction

The implementation always uses the first coordinate of the current matrix as its pivot. Write the matrix as

\[
A=\begin{pmatrix}a&p^T\\p&B\end{pmatrix},
\]

where $a$ is the first diagonal entry, $p$ is the rest of the first row, and $B$ is the principal matrix left after removing
the first row and column. Split a nonnegative vector as $(t,y)$, where $t\geq0$ and $y\geq0$. Then

\[
\begin{pmatrix}t\\y\end{pmatrix}^{T}
A
\begin{pmatrix}t\\y\end{pmatrix}
=a t^2+2t\,p^Ty+y^TBy.
\]

A nonpositive diagonal entry is already a strict-copositivity counterexample, so the recursive step may assume $a>0$. For a
fixed $y$, the expression is a convex quadratic in $t$. Its minimum over $t\geq0$ has two cases.

### When $p^Ty\geq0$

The minimum occurs at $t=0$. The remaining form is simply

\[
y^TBy.
\]

Therefore $B$ must be strictly copositive on

\[
\Gamma_+=\{y\geq0:p^Ty\geq0\}.
\]

### When $p^Ty\leq0$

The minimum occurs at $t=-p^Ty/a$. Substitution gives the Schur-complement form

\[
y^T\left(B-\frac{pp^T}{a}\right)y.
\]

The implementation avoids division by multiplying by the positive value $a$. It tests the exact integer matrix

\[
S=aB-pp^T
\]

on

\[
\Gamma_-=\{y\geq0:p^Ty\leq0\}.
\]

Thus $A$ is strictly copositive exactly when $B$ succeeds on Γ₊ and $S$ succeeds on Γ₋.

## Easy Sign Cases

The signs of the entries of $p$ determine the geometry.

- If every entry of $p$ is nonnegative, then Γ₊ is the complete orthant. Testing $B$ already includes the boundary of Γ₋,
  so only $B$ is needed. The implementation does not construct $S$ in this case.
- If every entry of $p$ is nonpositive, then Γ₋ is the complete orthant, so only $S$ is needed.
- If every entry is zero, the first case applies and the algorithm tests only $B$.

Each easy case creates one child of order $n-1$.

## Mixed Signs And Staircase Triangulation

Suppose $p_i>0$ and $p_j<0$. With $g_{ij}=\gcd(p_i,|p_j|)$, the primitive integer ray

\[
r_{ij}=\frac{|p_j|}{g_{ij}}e_i+\frac{p_i}{g_{ij}}e_j
\]

lies on the separating boundary because $p^Tr_{ij}=0$. Here $e_i$ and $e_j$ are coordinate rays. The two half-cones are
generated by:

- zero coordinate rays, positive coordinate rays, and the boundary rays $r_{ij}$ for Γ₊;
- zero coordinate rays, negative coordinate rays, and the same boundary rays for Γ₋.

These cones can have more generators than their dimension. The retained reconstruction divides each into simplicial cones by a
standard staircase triangulation. If $r$ entries of $p$ are positive and $s$ are negative, the two staircases together contain

\[
\binom{r+s}{r}
\]

children. The implementation generates one staircase path at a time instead of first storing the complete family. This allows a
failed child to stop the search before unused siblings are constructed.

The traversal is depth first. At every grid point it follows the down branch first, while the right branch remains unfinished; at
the end of a path it immediately checks the resulting order-$(n-1)$ matrix before returning to any sibling. The implementation
stores this control state in an explicit LIFO vector whose frames contain the grid row, grid column, and next branch. It does not use
the process call stack for the staircase walk. This representation preserves the recursive order while making its active depth
measurable and enforceable.

The construction is completely deterministic. Let $P_0,\ldots,P_{r-1}$ be the positive indices and
$N_0,\ldots,N_{s-1}$ the negative indices, both in their current index order.

- A plus-side path runs in the integer grid from $(0,0)$ to $(r-1,s)$ using down and right steps. At a grid vertex $(a,b)$ it
  contributes $e_{P_a}$ when $b=0$, and $r_{P_aN_{b-1}}$ when $b>0$.
- A minus-side path runs from $(0,0)$ to $(r,s-1)$. At $(a,b)$ it contributes $e_{N_b}$ when $a=0$, and
  $r_{P_{a-1}N_b}$ when $a>0$.
- Every zero coordinate ray is included in every child.

Each path therefore supplies exactly $r+s$ nonzero-sign rays; together with the zero rays it forms a square ray matrix of order
$n-1$. The generator visits down continuations before right continuations. There are

\[
\binom{r+s-1}{r-1}
\]

plus paths and

\[
\binom{r+s-1}{r}
\]

minus paths. Pascal's identity gives the displayed total $\binom{r+s}{r}$.

The same boundary ray $r_{ij}$ occurs in many staircase paths. At each recursive node, the implementation computes and primitively
reduces each of the $rs$ distinct positive-negative pair rays once, then reuses those exact rays throughout both staircases. This
cache changes only repeated integer construction and `gcd` work; it does not change a generator, a child, or their order.

For a child with ray matrix $R$, the next copositivity problem in the selected mode is

\[
R^TBR
\]

on the plus side or

\[
R^TSR
\]

on the minus side. Primitive integer rays keep these transformed matrices integral. The implementation processes all plus-side
paths before the minus-side paths and preserves the retained experiment's path order.

Because every plus-side child is tested before any minus-side child, the implementation delays constructing $S$ until the minus
side is reached. A nonnegative pivot row needs only $B$, and a mixed-sign node whose plus subtree rejects never allocates or fills
$S$. A nonpositive pivot row still constructs $S$ immediately because it is the only child. This scheduling changes no
mathematical test or traversal decision.

## Complete Control Flow

1. Orders zero through three use the direct exact criterion for the selected mode.
2. For larger matrices, reject a negative diagonal in non-strict mode or a nonpositive diagonal in strict mode.
3. Pivot on the current first coordinate. In non-strict mode, handle $a=0$ by rejecting $p\not\geq0$ or recurring only on $B$.
4. For $a>0$, construct $p$ and $B$, then classify the signs in $p$.
5. Use the one-child rule when $p$ has only one sign, constructing $S=aB-pp^T$ only for the nonpositive case.
6. Otherwise compute each primitive pair ray once, generate the plus staircase lazily, and recursively test every transformed $B$
   child.
7. If all plus children pass, construct $S$ and generate and test every transformed $S$ child.
8. Return `true` only when every recursive child passes.

The order-one test is $a_{11}>0$. The order-two test requires positive diagonal and, when the off-diagonal entry is negative, a
positive determinant. Order three uses Hadeler's exact closed criterion, implemented without square roots.

## Combined Classification

The combined operation must follow the non-strict tree. A zero found on one branch proves that the matrix is not strictly
copositive, but it does not rule out a negative value on another branch. The traversal therefore remembers strict failure and keeps
checking non-strict copositivity.

At order at most three, the direct helper evaluates both exact predicates. A negative face returns `{false, false}`; a zero face
sets the remembered strict field to `false` and passes for non-strict recursion. At a larger node, a negative pivot direction also
returns `{false, false}`. A zero diagonal records strict failure. If its coupling vector $p$ has a negative component, non-strict
copositivity fails too; otherwise the algorithm recurses only on $B$, exactly as in non-strict mode. A positive pivot uses the same
$B$, $aB-pp^T$, cached primitive rays, and staircase children as non-strict mode, with the Schur form still delayed until its branch
is reached. Every child receives the same remembered strict field, so any nonpositive child records the boundary while any negative
child ends the entire traversal.

If the complete non-strict recursion passes, `classify` returns `{true, remembered_strict_result}`. A selected strict solve retains
its earlier termination and does not pay for siblings after the first nonpositive witness. Both operations assume the parser supplied
a nonempty square symmetric matrix and do not repeat that validation.

## Known Difficult Inputs

Danninger's weakness is a pivot row containing many positive and many negative off-diagonal entries. If the row has $r$ positive
and $s$ negative entries, the step produces

\[
\binom{r+s}{r}
\]

children before the dimension reduction can continue. This number is largest when the two sign classes have similar sizes.

The baseline always pivots on the first coordinate of the current transformed matrix. It cannot avoid a wide first row by selecting
a different coordinate. Corpus matrix **811**, a reduced-B matrix derived from QAPLIB `nug24:A`, is an exact example. Its first row
has 10 positive and 11 negative off-diagonal entries, so the first Danninger reduction has

\[
\binom{21}{10}=352716
\]

immediate staircase children.

The same difficulty can reappear below the root: a harmless-looking pivot can transform the descendants into matrices whose first
rows have wider mixed-sign patterns. A rule based only on the current row therefore cannot reliably predict the size of the full
descendant tree.

Corpus matrix **10244**, the order-999 Johnson-Reams generalized Horn matrix, is a reproducible depth case. The former native
staircase recursion exhausted the process stack while following its first-child chain. The explicit LIFO representation now stops
that traversal at Coposit's shared 50,000-open-node limit and reports the unresolved `node_limit` outcome instead of crashing.

## Termination, Cost, And Fidelity

Every matrix child has order exactly one less than its parent, so a matrix-child chain has at most $n$ levels. The staircase walk
inside one level can add up to $n-1$ active grid frames, so nested first-child traversal can retain $O(n^2)$ logical frames even
though every individual branch is finite. The number of completed children can also be combinatorially large.

Coposit counts active matrix checks and active staircase frames together. Before opening another one, it enforces the shared limit
of 50,000. Exceeding it throws the standard resource exception, which the native wrapper reports as unresolved `node_limit`; it is
never converted to `false`. Completed children no longer count, so the limit bounds simultaneous unfinished work rather than the
total number of children processed during a run.

The maintained code was checked against the retained experiment for strict mode. Non-strict mode uses Danninger's published zero-pivot
and non-strict boundary rules; fixed first-coordinate pivot, direct small-order tests, division-free Schur matrix, sign partition,
primitive pair rays, lazy staircase triangulation, plus-before-minus traversal, and recursive child order otherwise agree. FLINT
integer storage, per-node reuse of primitive pair rays, delayed Schur construction, and the explicit bounded LIFO representation
change no mathematical decision.

The one-coordinate minimization, sign-defined half-cones, boundary rays, and dimension-reducing recursion are the part supported by
Danninger's publication and later descriptions. The fixed first pivot, this exact standard staircase, down-before-right path order,
plus-before-minus traversal, and the Hadeler-based order-three shortcut are the retained reconstruction's concrete choices. Without
the complete article or original program they must not be presented as line-for-line source fidelity. The model adds no pivot
heuristic, Dutour split, SNC slice, graph reduction, or other mathematical solver rule.

Timed native-module builds observe a shared signal flag at traversal and matrix-row boundaries and return a distinct timeout
outcome. Standalone model and test builds compile those checkpoints to no-ops, so the reconstruction has no timer thread, clock read,
signal handler, or changed untimed traversal.
