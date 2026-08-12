# Adaptive Dutour-Danninger

Classification: Coposit-created experimental strict-copositivity model.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to a non-strict query.

## What The Algorithm Does

Adaptive Dutour-Danninger combines two exact algorithms whose weaknesses are complementary:

- Danninger removes one coordinate and therefore reduces the matrix order, but a mixed-sign pivot row can create many children.
- Dutour keeps the same matrix order, but its two-child cone split can change the geometry until a cheap Danninger reduction becomes
  available.

At every recursive node, the algorithm first asks whether any Danninger pivot would create at most two children. It uses the first
such pivot. If none exists, it performs exactly one Dutour split. Every child begins the same decision again from its first possible
Danninger pivot.

This is not fixed alternation and it does not run one complete algorithm after the other. The choice is reconsidered from the
current matrix at every node.

## Name And Sources

The name describes the combined algorithm:

- **Adaptive** means the choice between the two operations is reconsidered at every node.
- **Dutour** identifies Mathieu Dutour Sikirić's maximum-ratio two-cone split rather than a generic cone operation.
- **Danninger** identifies Gabriele Danninger's one-coordinate dimension reduction.

The Danninger part comes from:

- Gabriele Danninger, “A Recursive Algorithm for Determining (Strict) Copositivity of a Symmetric Matrix,” in *XIV Symposium on
  Operations Research (Ulm, 1989)*, *Methods of Operations Research*, volume 62, Hain, 1990, pages 45–52;
- Coposit's maintained [`danninger_1990` algorithm document](../../baselines/danninger_1990/ALGORITHM.md).

The Dutour part comes from Mathieu Dutour Sikirić's Polyhedral Common implementation:

- [`PairDecomposition` introduction, commit `33ce96e4d0589f340a0fbfd7824ff70f9a2ce093`](https://github.com/MathieuDutSik/polyhedral_common/commit/33ce96e4d0589f340a0fbfd7824ff70f9a2ce093);
- pinned strict-copositivity source commit
  [`d2252bc89d991fa6df9750ac9647e19b6a9aca02`](https://github.com/MathieuDutSik/polyhedral_common/commit/d2252bc89d991fa6df9750ac9647e19b6a9aca02);
- Coposit's maintained [`dutour_2018` algorithm document](../../baselines/dutour_2018/ALGORITHM.md).

No paper describes the adaptive choice rule. It is a Coposit-created combination implemented completely in this model's
[`solver.cpp`](solver.cpp).

## What A Recursive Node Represents

A node stores a symmetric exact integer matrix $C$. Initially $C=A$, the input matrix. At deeper levels, $C$ is the Gram matrix of
the original quadratic form after restriction to a current simplicial cone or after a Danninger coordinate elimination.

Every node decides

\[
z^TCz>0\qquad\text{for every nonzero }z\geq0.
\]

Both component operations replace the current node by equivalent child decisions:

- a Dutour step covers the current cone with two child cones of the same dimension;
- a Danninger step minimizes over one coordinate and produces child problems of dimension one less.

Only the transformed Gram matrices are needed. The solver never reconstructs the complete chain of cone generators.

### Public decision problem

The input parser supplies a nonempty square symmetric integer matrix $A$. The solver assumes that contract and does not repeat the
shape or symmetry scan.

For a valid input it decides strict copositivity:

\[
A\in\operatorname{int}(\mathcal C_n)
\quad\Longleftrightarrow\quad
x^TAx>0\quad\text{for every }x\in\mathbb R_+^n\setminus\{0\}.
\]

Homogeneity is essential. If $x^TAx$ has one nonpositive value on a nonzero nonnegative vector, every positive multiple of that
vector has the same sign. Conversely, the orthant may be studied through any collection of cones whose union is the orthant.

The maintained interface returns only a Boolean classification. Internally derived vectors prove rejection, but the current model
does not return a witness. A cooperative timeout raises a distinct interruption outcome through the caller; it is never converted
to `false`.

### Node invariant

Every call `check(C)` has the following invariant:

1. $C$ is symmetric and has exact integer entries.
2. Its nonnegative coordinate orthant parametrizes a current cone or a reduced minimization region.
3. `check(C)` is true exactly when the represented part of the parent problem has strictly positive quadratic value.
4. If a step creates children $C_1,\ldots,C_k$, the parent is true exactly when every child is true.

For a cone restriction with ray matrix $R$, this invariant is the congruence identity

\[
(R\lambda)^TC(R\lambda)=\lambda^T(R^TCR)\lambda,
\qquad \lambda\geq0.
\]

For a Danninger reduction, the invariant instead follows from exact minimization over the eliminated coordinate. These are the two
ways a child matrix is produced; neither relies on sampling or an approximation.

The matrix stored at a node is sufficient because every later decision uses only its entries: diagonal signs, off-diagonal signs,
two-generator determinants, pivot-row signs, Schur products, or congruence transforms. Generator history would not change any of
those decisions.

## Complete Decision Flow

For a current matrix $C$ of order $n$:

1. Use direct exact strict-copositivity tests when $n\leq3$.
2. Reject if any diagonal entry is nonpositive.
3. Scan pivot rows from index zero upward and choose the first narrow Danninger pivot.
4. If a narrow pivot exists, perform its one- or two-child Danninger reduction.
5. If no narrow pivot exists, perform one Dutour cone split.
6. Restart this complete flow, beginning with pivot zero, in every child.
7. Return `false` when any child fails. Return `true` only when every required child succeeds.

The following sections define the terminal tests, narrow pivots, and both transformations exactly.

## Exact Pseudocode

The recursive control flow implemented in `solver.cpp` is:

```text
check(C):
    checkpoint for cooperative timeout

    if order(C) <= 3:
        return the exact direct criterion for that order

    if any diagonal entry of C is <= 0:
        return false

    for pivot i = 0, 1, ..., order(C)-1:
        count positive and negative off-diagonal entries in row i
        stop counting row i once both signs occur and their total exceeds 2

        if row i has no positive entries:
            return check(the Danninger Schur child for i)

        if row i has no negative entries:
            return check(the Danninger principal child for i)

        if row i has exactly one positive and one negative entry:
            if check(the Danninger plus child for i) is false:
                return false
            return check(the Danninger minus child for i)

    inspect every negative pair (i,j) in lexicographic order:
        if the two-generator restriction is not strict:
            return false
        remember the first pair maximizing c_ij^2 / (c_ii c_jj)

    if no negative pair exists:
        return true

    form the Dutour child replacing generator i by v_i + v_j
    form the Dutour child replacing generator j by v_i + v_j
    if check(first child) is false:
        return false
    return check(second child)
```

There is no persistent global queue. Recursion gives a depth-first traversal. A failing child immediately short-circuits every
remaining sibling on the current call stack.

## Direct Terminal Cases Through Order Three

Danninger recursion and Dutour subdivision are unnecessary once a child reaches order three. This model owns the same exact
terminal criteria as the maintained Danninger model.

### Order zero

Order zero succeeds internally. Public input can never have order zero, but recursive code treats the empty residual problem as
vacuously complete.

### Order one

For $C=(c_{11})$,

\[
x^TCx=c_{11}x_1^2.
\]

Strict copositivity is therefore exactly $c_{11}>0$.

### Order two

Write

\[
C=\begin{pmatrix}a&b\\b&d\end{pmatrix}.
\]

Both $a$ and $d$ must be positive because the coordinate vectors are nonnegative test directions. If $b\geq0$, every term in

\[
a x_1^2+2b x_1x_2+d x_2^2
\]

is nonnegative and at least one positive diagonal term is present for every nonzero $x\geq0$. The matrix is then strictly
copositive.

If $b<0$, strict copositivity is equivalent to

\[
ad-b^2>0.
\]

Necessity follows from the nonnegative coefficient vector $(d,-b)$:

\[
\begin{pmatrix}d&-b\end{pmatrix}
C
\begin{pmatrix}d\\-b\end{pmatrix}
=d(ad-b^2).
\]

If the determinant is zero, this is a nonnegative zero direction; if it is negative, it is a negative direction. Sufficiency follows
because positive diagonal and positive determinant make the two-dimensional form positive definite.

### Order three

Write

\[
C=\begin{pmatrix}
a&b&c\\
b&d&e\\
c&e&f
\end{pmatrix}.
\]

The implementation first applies the order-two criterion to the three principal pairs $(1,2)$, $(1,3)$, and $(2,3)$. If any pair
fails, its two-coordinate witness also rejects $C$.

After all proper principal faces pass, it calculates

\[
\det C=adf+2bce-ae^2-dc^2-fb^2.
\]

If $\det C>0$, the matrix passes the order-three criterion. If $\det C\leq0$, Hadeler's inductive theorem says that $C$ fails
strict copositivity exactly when its adjugate is strictly positive entrywise. The six distinct entries tested by the code are

\[
\begin{aligned}
(\operatorname{adj}C)_{11}&=df-e^2,
& (\operatorname{adj}C)_{22}&=af-c^2,
& (\operatorname{adj}C)_{33}&=ad-b^2,\\
(\operatorname{adj}C)_{12}&=ce-bf,
& (\operatorname{adj}C)_{13}&=be-cd,
& (\operatorname{adj}C)_{23}&=bc-ae.
\end{aligned}
\]

The matrix is rejected only when all six are positive. Encountering any nonpositive adjugate entry proves that Hadeler's failure
condition is absent, so the direct test accepts.

Every product, sum, determinant, and sign test is a FLINT integer operation. A determinant or cofactor equal to zero is distinguished
exactly from a positive or negative value.

## How A Narrow Danninger Pivot Is Found

For a possible pivot row $i$, ignore the diagonal and count

\[
r_i=\#\{j\neq i:c_{ij}>0\},
\qquad
s_i=\#\{j\neq i:c_{ij}<0\}.
\]

Zero entries belong to neither group. Danninger's two staircase triangulations for this pivot contain

\[
\binom{r_i+s_i}{r_i}
\]

children in total. The adaptive algorithm calls a pivot **narrow** when this number is at most two. There are exactly three cases:

1. $r_i=0$: no positive off-diagonal entries, so Danninger creates one Schur child.
2. $s_i=0$: no negative off-diagonal entries, so Danninger creates one principal child.
3. $r_i=s_i=1$: exactly one positive and one negative entry, plus any number of zeros, so Danninger creates two children.

If one sign count is zero, the binomial coefficient is one. If both are nonzero, its smallest value is
$\binom21=2$; one more nonzero entry makes it at least three. The implementation can therefore recognize a narrow row using only

```text
r == 0  or  s == 0  or  (r == 1 and s == 1)
```

While scanning a row, it stops as soon as both signs have appeared and $r_i+s_i>2$, because later entries cannot make that row
narrow again. Rows are tried in ascending index order and the first narrow pivot is used immediately. There is no scoring,
lookahead, or comparison between different narrow pivots.

The binomial count is the sum of the two Danninger staircase counts. With $r$ positive and $s$ negative entries, the plus half-cone
has

\[
\binom{r+s-1}{r-1}
\]

simplicial children and the minus half-cone has

\[
\binom{r+s-1}{r}
\]

children. Pascal's identity gives

\[
\binom{r+s-1}{r-1}+\binom{r+s-1}{r}=\binom{r+s}{r}.
\]

The adaptive model never constructs those general staircases. It uses the count only to decide whether the special cases with one
or two children apply. A row with $(r,s)=(1,2)$ already predicts three children and is rejected as a Danninger pivot. The scan then
continues, so a later narrow row can still be selected.

### Exact pivot scan order

For each candidate row $i$:

1. columns are inspected from zero upward, skipping column $i$;
2. positive and negative counters are incremented from the exact sign of $c_{ij}$;
3. zeros do not affect either counter;
4. the row scan terminates early only after both counters are nonzero and their sum exceeds two;
5. the first row that remains narrow is selected, and no later row is inspected.

The early exit cannot turn a wide row into a narrow one: sign counts only increase. It also does not affect which narrow row is
selected, because a row is abandoned only after it has irreversibly failed the criterion.

## The Danninger Step

Let $i$ be the selected pivot. Keep all other indices in their original order and write the matrix, after conceptually moving the
pivot first, as

\[
C=\begin{pmatrix}a&p^T\\p&B\end{pmatrix},
\qquad a=c_{ii}>0.
\]

For a nonnegative vector split as $(t,y)$,

\[
\begin{pmatrix}t\\y\end{pmatrix}^{T}
C
\begin{pmatrix}t\\y\end{pmatrix}
=a t^2+2t\,p^Ty+y^TBy.
\]

For fixed $y$, minimizing over $t\geq0$ gives two reduced forms:

- on $p^Ty\geq0$, the minimum is $y^TBy$;
- on $p^Ty\leq0$, the minimum has the sign of $y^T(aB-pp^T)y$.

To see this, complete the square:

\[
a t^2+2t\,p^Ty+y^TBy
=a\left(t+\frac{p^Ty}{a}\right)^2
+y^T\left(B-\frac{pp^T}{a}\right)y.
\]

The unconstrained minimizer is

\[
t_*=-\frac{p^Ty}{a}.
\]

When $p^Ty\geq0$, this minimizer is nonpositive, so the constrained minimum over $t\geq0$ occurs at $t=0$ and equals $y^TBy$.
When $p^Ty\leq0$, the unconstrained minimizer is feasible and the minimum equals

\[
y^T\left(B-\frac{pp^T}{a}\right)y
=\frac1a y^T(aB-pp^T)y.
\]

Because $a>0$, multiplication by $a$ preserves every sign. This is why the implementation uses the integer Schur matrix rather
than storing fractions.

Define the division-free Schur matrix

\[
S=aB-pp^T.
\]

### No negative pivot entries

If $s_i=0$, then $p\geq0$ and $p^Ty\geq0$ throughout the nonnegative orthant. The algorithm creates only the principal child $B$
and recursively checks it.

No second child is missing: the minus region $p^Ty\leq0$ is contained in the face $p^Ty=0$, and on that face the two minimized
forms agree. Checking $B$ on the complete orthant already checks this boundary.

### No positive pivot entries

If $r_i=0$, then $p\leq0$ and $p^Ty\leq0$ throughout the nonnegative orthant. The algorithm creates only the Schur child $S$ and
recursively checks it. When every entry of $p$ is zero, both descriptions apply; the implementation takes the no-negative branch
and checks $B$.

When $p=0$, $S=aB$ is only a positive scaling of $B$, so either choice would give the same strict decision.

### One positive and one negative pivot entry

Let $p_u>0$ be the unique positive entry and $p_v<0$ the unique negative entry. Define

\[
g=\gcd(p_u,|p_v|)
\]

and the primitive integer boundary ray

\[
w=\frac{|p_v|}{g}e_u+\frac{p_u}{g}e_v.
\]

It lies on the separating hyperplane because $p^Tw=0$. Let $e_z$ denote the coordinate rays belonging to zero entries of $p$,
kept in ascending reduced-coordinate order. Construct

\[
R_+=\bigl[e_z\text{ for all zero indices},\ e_u,\ w\bigr],
\qquad
R_-=\bigl[e_z\text{ for all zero indices},\ e_v,\ w\bigr].
\]

The plus-side child is

\[
C_+=R_+^TBR_+,
\]

and the minus-side child is

\[
C_-=R_-^TSR_-.
\]

The algorithm checks $C_+$ first. If it succeeds, it checks $C_-$. Both children have order $n-1$.

### Why these rays cover the two half-cones

Only coordinates $u$ and $v$ occur in $p^Ty$; every zero coordinate is unconstrained and contributes its coordinate ray to both
children. On the plus side,

\[
p_u y_u-|p_v|y_v\geq0.
\]

For any such $y$, choose

\[
\beta=\frac{g y_v}{p_u}\geq0.
\]

Then $\beta w$ has $v$-coordinate $y_v$, and its $u$-coordinate is $|p_v|y_v/p_u$. The remaining $u$-coordinate is

\[
y_u-\frac{|p_v|}{p_u}y_v\geq0.
\]

Thus every plus-side vector is a nonnegative combination of $e_u$, $w$, and the zero-coordinate rays. Conversely, every one of
those generators satisfies $p^Ty\geq0$, so they generate exactly the plus half-cone.

On the minus side,

\[
p_u y_u-|p_v|y_v\leq0.
\]

Choosing

\[
\alpha=\frac{g y_u}{|p_v|}\geq0
\]

matches the $u$-coordinate with $\alpha w$ and leaves the nonnegative residual

\[
y_v-\frac{p_u}{|p_v|}y_u
\]

on $e_v$. Therefore $e_v$, $w$, and the zero-coordinate rays generate exactly the minus half-cone.

The common boundary ray $w$ occurs in both children. This overlap is intentional and harmless: both child decisions must succeed.

### Exact child construction

Suppose the ordered child rays are $r_1,\ldots,r_{n-1}$ and $R$ is the matrix with those rays as columns. The child Gram entry is

\[
(R^TMR)_{k\ell}=r_k^TMr_\ell,
\]

where $M=B$ on the plus side and $M=S$ on the minus side. Every ray has at most two nonzero integer coefficients, so each child
entry is a sum of at most four products

\[
(r_k)_a(r_\ell)_b M_{ab}.
\]

The implementation stores a sparse ray as two coordinate indices, two coefficients, and a count of one or two. It evaluates only
the upper triangle and mirrors it to retain exact symmetry.

The retained index order is also exact:

1. all zero-coordinate rays in ascending reduced-coordinate order;
2. $e_u$ for the plus child or $e_v$ for the minus child;
3. the boundary ray $w$.

Changing this order would only apply a simultaneous row-and-column permutation to a child, but preserving it makes traversal and
subsequent first-pivot choices deterministic.

## The Dutour Fallback

The fallback is used only after every pivot row has failed the narrow test.

### Immediate two-generator rejection

For every negative off-diagonal entry $c_{ij}<0$, compare

\[
c_{ij}^2\quad\text{with}\quad c_{ii}c_{jj}.
\]

If $c_{ij}^2\geq c_{ii}c_{jj}$, the restriction to those two generators has a nonpositive direction. Equality gives a
nonnegative zero and a larger left side gives a negative direction. Either rejects strict copositivity immediately.

The rejection is constructive. Put

\[
a=c_{ii}>0,\qquad b=c_{jj}>0,\qquad c=c_{ij}<0.
\]

The nonnegative coefficient vector $(b,-c)$ has value

\[
\begin{pmatrix}b&-c\end{pmatrix}
\begin{pmatrix}a&c\\c&b\end{pmatrix}
\begin{pmatrix}b\\-c\end{pmatrix}
=b(ab-c^2)\leq0.
\]

These coefficients select an actual nonnegative combination of two current cone generators, so failure of the two-dimensional
restriction is failure of the complete node.

### Pair selection

If every negative pair passes, select the pair maximizing

\[
\rho_{ij}=\frac{c_{ij}^2}{c_{ii}c_{jj}}.
\]

Ratios are compared by exact cross multiplication. The first pair in index order wins an exact tie. If there is no negative
off-diagonal entry, the matrix is entrywise nonnegative; its already positive diagonal certifies the node.

For two candidate pairs with positive denominators, the implementation compares

\[
\frac{n_1}{d_1}>\frac{n_2}{d_2}
\quad\Longleftrightarrow\quad
n_1d_2>n_2d_1.
\]

It never constructs a rational number. Pairs are scanned as $(0,1),(0,2),\ldots,(n-2,n-1)$. The stored pair changes only on a
strictly larger ratio, so an exact tie leaves the earlier pair selected.

The ratio is a normalized measure of how close the negative two-generator restriction is to the rejection boundary
$c_{ij}^2=c_{ii}c_{jj}$. It is a routing heuristic only. Correctness does not depend on choosing the maximum; correctness follows
from the exact cone cover after the pair is chosen.

If no negative entry exists, then for every nonzero $\lambda\geq0$,

\[
\lambda^TC\lambda
=\sum_i c_{ii}\lambda_i^2+2\sum_{i<j}c_{ij}\lambda_i\lambda_j>0,
\]

because every term is nonnegative and some positive diagonal term occurs. This is a complete certificate for the node.

### Two cone children

For the selected pair introduce

\[
w=v_i+v_j.
\]

The first child replaces $v_i$ with $w$ and the second replaces $v_j$ with $w$. The new Gram row and column satisfy

\[
w^TCw=c_{ii}+2c_{ij}+c_{jj},
\qquad
w^TCv_k=c_{ik}+c_{jk}.
\]

The first child is checked before the second. Both keep order $n$, and each restarts the narrow-pivot scan at row zero.

### Why the two children cover the parent cone

Consider only the coefficients of the selected rays. A parent point contains

\[
\alpha v_i+\beta v_j,
\qquad \alpha,\beta\geq0.
\]

If $\alpha\geq\beta$, then

\[
\alpha v_i+\beta v_j=(\alpha-\beta)v_i+\beta(v_i+v_j),
\]

which lies in the child retaining $v_i$ and replacing $v_j$ by $w$. If $\beta\geq\alpha$, then

\[
\alpha v_i+\beta v_j=\alpha(v_i+v_j)+(\beta-\alpha)v_j,
\]

which lies in the child replacing $v_i$ by $w$ and retaining $v_j$. All unchanged generator coefficients carry over. Hence the
union of the two child cones is exactly the parent cone.

Their intersection is the face on which $\alpha=\beta$ in the original two-ray coordinates. Overlap causes no loss or false
acceptance because both children are required to pass.

### Exact Gram update

The solver does not store $v_i$ or $v_j$. Replacing generator $v_r$ by $v_r+v_o$ changes only row and column $r$:

\[
\begin{aligned}
c'_{rr}&=c_{rr}+2c_{ro}+c_{oo},\\
c'_{rk}&=c_{rk}+c_{ok} && (k\neq r),\\
c'_{k\ell}&=c_{k\ell} && (k,\ell\neq r).
\end{aligned}
\]

The implementation copies the parent matrix twice, applies this update with $r=i,o=j$ to the first child and $r=j,o=i$ to the
second, recursively checks the first, and constructs no further decision for the second if the first has already failed.

## Exact Arithmetic And Maintained Representation

Every matrix entry and decision uses FLINT arbitrary-precision integers.

- The direct terminal criteria are implemented locally.
- Narrow-pivot selection uses exact signs and counts.
- The Schur child uses $aB-pp^T$, so no division is introduced.
- A mixed boundary ray is made primitive by one greatest-common-divisor reduction.
- Congruence transforms use sparse rays containing at most two nonzero integer coefficients.
- Dutour ratios use cross multiplication, and a sum-ray split updates one row and column.

Positive scaling of a ray or a complete Gram matrix does not change strict copositivity. No floating-point tolerance, rational
matrix storage, fixed-width mask, or small-dimension cap is used. The public `solve` boundary requires a nonempty square symmetric
matrix. Cooperative timeout checkpoints report interruption as unresolved rather than `false`.

### Where integer coefficients grow

The model is exact, but exactness does not mean coefficients remain small.

- A Schur entry $a b_{k\ell}-p_kp_\ell$ multiplies current entries before subtracting them.
- A mixed Danninger ray contains $|p_v|/g$ and $p_u/g$, so a congruence child multiplies matrix entries by products of these values.
- A Dutour split repeatedly adds rows and columns; after many same-order splits, a stored generator represents the sum of many older
  rays.

Only the mixed boundary ray is normalized, by its greatest common divisor. The solver does not divide an entire child matrix by its
content. Such a division would preserve the decision but would be an additional arithmetic policy not present in this model.

### Work performed at one node

For a matrix of order $n$:

- direct tests through order three take constant time;
- the diagonal test is $O(n)$ exact sign operations;
- scanning all candidate pivot rows is at most $O(n^2)$ sign operations;
- building a principal or Schur child is $O(n^2)$ integer work;
- each narrow mixed congruence has $O(n^2)$ entries and at most four multiply-add terms per entry;
- a Dutour inspection considers $n(n-1)/2$ pairs, and each child copies an $n\times n$ matrix before an $O(n)$ row-column update.

These polynomial per-node costs are not the main worst-case issue. The number of recursive nodes can be very large, and same-order
Dutour refinement is not known to be finite. Recursion also retains sibling matrices on the call stack, so memory depends on both
matrix order and search depth.

## Why The Complete Algorithm Is Correct

The correctness statement is conditional only on completion: whenever the solver returns, its Boolean value is the exact strict
copositivity classification.

### Rejection steps are sound

- A nonpositive diagonal uses a coordinate vector with nonpositive quadratic value.
- A failed order-two restriction supplies the explicit two-coordinate vector shown above.
- The order-three branch is Hadeler's exact criterion after all proper principal faces pass.
- A Danninger child failure occurs in a region obtained by exact minimization of the parent form.
- A Dutour two-generator failure occurs inside the current cone.

Every `false` therefore has a real nonnegative direction in the represented parent problem, even though the public API does not
return that direction.

### Acceptance steps are sound

- A direct terminal test is a complete low-dimensional criterion.
- An entrywise nonnegative Gram matrix with positive diagonal is positive on every nonzero nonnegative coefficient vector.
- A one-child Danninger case covers the complete reduced orthant because the pivot row has only one sign.
- The two narrow Danninger children exactly cover the two sign half-cones.
- The two Dutour children exactly cover the parent cone.

Thus a node returns `true` only after every part of its represented region has been certified.

### Recursive composition

Assume recursively that completed child calls return exact decisions. In a Danninger step, strict positivity of the parent is
equivalent to strict positivity of every reduced half-cone child. In a Dutour step, it is equivalent to strict positivity on both
covering cones. The conjunction of exact child decisions is therefore the exact parent decision. Starting from $C=A$ proves the
result for the input matrix.

The special case $y=0$ in Danninger's minimization needs no child direction: then the parent value is $at^2>0$ for every $t>0$
because the pivot diagonal was checked positive.

## Worked Routing Examples

These examples illustrate routing and child construction, not performance measurements.

### A narrow two-child Danninger root

Consider

\[
C=\begin{pmatrix}
4&1&-1&0\\
1&4&0&0\\
-1&0&4&0\\
0&0&0&4
\end{pmatrix}.
\]

Row zero has one positive entry, one negative entry, and one zero, so it is the first narrow pivot. With the remaining coordinates
kept in order,

\[
a=4,
\qquad
p=\begin{pmatrix}1\\-1\\0\end{pmatrix},
\qquad
B=4I_3.
\]

The Schur matrix is

\[
S=4B-pp^T
=\begin{pmatrix}
15&1&0\\
1&15&0\\
0&0&16
\end{pmatrix}.
\]

Here $g=1$ and the boundary ray is $w=e_1+e_2$ in reduced coordinates. The zero-coordinate ray is $e_3$. The exact ordered ray
matrices are

\[
R_+=\bigl[e_3,e_1,w\bigr],
\qquad
R_-=\bigl[e_3,e_2,w\bigr].
\]

The solver checks $R_+^TBR_+$ first and $R_-^TSR_-$ second. Both children have order three and are decided immediately by the direct
terminal criterion; the adaptive router is not invoked again below them.

The actual child matrices are

\[
C_+=R_+^TBR_+
=\begin{pmatrix}
4&0&0\\
0&4&4\\
0&4&8
\end{pmatrix},
\qquad
C_-=R_-^TSR_-
=\begin{pmatrix}
16&0&0\\
0&15&16\\
0&16&32
\end{pmatrix}.
\]

For $C_+$, the only negative-pair condition is vacuous and its determinant is positive. The same is true for $C_-$. Both direct
calls return `true`, so the root returns `true`.

### A root forced into the Dutour fallback

Consider the sign pattern

\[
C=\begin{pmatrix}
1&1&1&-2\\
1&1&-1&1\\
1&-1&1&1\\
-2&1&1&1
\end{pmatrix}.
\]

Every row has three nonzero off-diagonal entries and contains both signs. Hence every row has either $(r,s)=(2,1)$ or $(1,2)$,
which predicts three Danninger children. No row is narrow.

The Dutour inspection reaches pair $(0,3)$ with

\[
c_{03}^2=4\geq c_{00}c_{33}=1.
\]

It rejects immediately from that two-generator restriction. No cone children are constructed.

### A Dutour split that exposes Danninger pivots

Consider

\[
C=\begin{pmatrix}
4&-1&1&-1\\
-1&4&-1&1\\
1&-1&4&-1\\
-1&1&-1&4
\end{pmatrix}.
\]

Every row has one positive and two negative off-diagonal entries, so no row is narrow. Every negative pair has

\[
\rho_{ij}=\frac{1}{16}<1.
\]

All ratios tie, so the lexicographically first negative pair $(0,1)$ is selected. Replacing generator zero by $v_0+v_1$ gives

\[
C^{(0)}=\begin{pmatrix}
6&3&0&0\\
3&4&-1&1\\
0&-1&4&-1\\
0&1&-1&4
\end{pmatrix}.
\]

Row zero now has no negative entry. The child immediately takes a one-child Danninger step, deletes row and column zero, and reaches

\[
\begin{pmatrix}
4&-1&1\\
-1&4&-1\\
1&-1&4
\end{pmatrix}.
\]

Its order-three determinant is $54>0$, so the first Dutour child succeeds. Replacing generator one instead gives

\[
C^{(1)}=\begin{pmatrix}
4&3&1&-1\\
3&6&0&0\\
1&0&4&-1\\
-1&0&-1&4
\end{pmatrix}.
\]

Row zero remains wide, but row one has no negative entry and is the first narrow pivot. Its principal Danninger child is

\[
\begin{pmatrix}
4&1&-1\\
1&4&-1\\
-1&-1&4
\end{pmatrix},
\]

again with determinant $54>0$. The second child succeeds and therefore so does the root. This trace shows the adaptive behavior
precisely: one Dutour split does not schedule another Dutour split; each child restarts the row scan and immediately uses the newly
available Danninger pivot.

## Known Difficult Inputs

The adaptive rule avoids the most obvious combinatorial Danninger steps, but it does not make every input easy. Its difficult cases
come from the interaction between a local sign-count rule, same-order cone refinement, exact coefficient growth, and depth-first
search.

### No narrow row

The gate helps only when at least one current row is narrow. If every row contains both signs and at least three nonzero
off-diagonal entries, no Danninger reduction is permitted. The node must use Dutour, even when its order is large and dimension
reduction would otherwise be desirable.

A Dutour split can change several sign counts because the new Gram interactions are sums

\[
c'_{rk}=c_{rk}+c_{ok}.
\]

Cancellation may produce zeros or remove one sign class, which can expose a narrow row. The opposite can also happen: two entries
of the same sign reinforce one another, or cancellation in one column is accompanied by a new mixed pattern elsewhere. The
adaptive rule has no theorem saying that a split must expose a narrow row after finitely many steps.

Corpus matrix **9639**, an order-171 matrix derived from the DIMACS `keller4` clique instance, is a representative structural
example. At the root it has no narrow pivot; its first row has 46 positive and 124 negative off-diagonal entries. The model must
therefore begin with same-order Dutour refinement despite the high dimension. This matrix identifier is included to make the
failure shape reproducible, not as a benchmark claim.

### The first narrow row need not be the best narrow row

The pivot test knows only the signs in the current row. It does not inspect:

- the magnitudes of the pivot entries;
- the expected sizes of the Schur entries;
- the signs that will appear in either transformed child;
- whether another narrow row would lead to easier descendants;
- how much exact-integer coefficient growth a mixed boundary ray will cause.

Consequently, a row with one immediate child can produce a difficult Schur descendant, while a later row with two immediate
children might have produced two easy descendants. The algorithm still takes the first row. This is not a correctness problem:
every allowed pivot gives an equivalent decomposition. It is a search-tree-size problem.

This also explains why "at most two children" is only an immediate-width rule. It limits the branching factor of the current
Danninger step; it says nothing about the number or difficulty of nodes below those children.

### Negative interactions close to the two-generator boundary

The Dutour fallback rejects a negative pair only when

\[
c_{ij}^2\geq c_{ii}c_{jj}.
\]

If many ratios are just below one, every pair passes the local strict test while the Gram matrix still has negative entries. The
algorithm must subdivide. The maximum-ratio rule chooses the locally closest pair, but the resulting sum ray need not eliminate
that negative geometry globally. Closely related negative pairs may remain in one or both children.

A boundary or non-copositive matrix is therefore not necessarily rejected quickly. It is quick only when a nonpositive direction
appears as a current generator or inside a current two-generator face. A counterexample using three or more current generators may
require many subdivisions before it becomes visible to either local rejection test.

### Exact coefficient growth

All decisions remain exact, but large values make each decision more expensive. A Schur product can roughly double the bit length
of current entries before cancellation. Repeated Schur reductions can compound that growth. Mixed boundary rays multiply Gram
entries by products of primitive ray coefficients. Repeated Dutour sums encode increasingly long combinations of older generators.

The implementation performs no whole-matrix content normalization after a child is built. Therefore two mathematically equivalent
positive scalings can have very different arithmetic costs. This is a deliberate fidelity and simplicity choice, not a claim that
the stored representation is coefficient-optimal.

### Depth-first traversal can commit to a difficult child

The solver checks one child to completion before visiting its sibling. This gives fast short-circuit rejection and uses less
bookkeeping than a global queue, but a difficult first child can delay an easy rejection in the second child. The exact first-child
orders are:

- Danninger plus child before minus child;
- Dutour child replacing generator $i$ before the child replacing generator $j$, for the selected $i<j$.

Changing the traversal order would preserve the mathematical answer on terminating runs but could change whether a resource-limited
run finishes.

## Termination And Mathematical Limit

Every Danninger child has order $n-1$. Along a path containing only Danninger steps, matrix order is therefore a strictly decreasing
nonnegative integer measure. Such a path reaches a direct case after finitely many reductions.

A Dutour child still has order $n$. The new cone is geometrically smaller than the parent cone, but this implementation does not
store or test a discrete cone-size measure that is proved to decrease. Generator coefficients can keep changing through sums while
the matrix order and number of generators remain constant. The adaptive path can consequently have the form

\[
n,n,n,n,\ldots
\]

for an unbounded number of Dutour nodes before it finds a narrow row, a nonpositive two-generator restriction, or an
entrywise-nonnegative certificate.

There is no established proof that this adaptive routing terminates for every exact input. The maintained model deliberately has no
fallback to the full Danninger staircase, because adding that fallback would be a different algorithm and could introduce the very
combinatorial expansion the narrow gate was designed to avoid.

This limit separates two claims:

1. **Decision soundness:** if the solver returns `true` or `false`, that result follows from exact transformations and certificates.
2. **Decision completeness as a terminating program:** it is not established that the solver returns for every valid input.

The first claim is proved by the cone covers and minimization identities above. It does not imply the second.

### Cooperative interruption

Timed native-module builds observe a shared signal flag at explicit checkpoints. A set flag throws `timeout_requested`, which the
native boundary reports as an unresolved timeout. Checkpoints occur:

- when the model starts;
- at entry to every recursive node;
- once per candidate pivot row;
- once per outer Dutour pair row;
- once per produced matrix row during principal, Schur, and congruence construction, and once per updated sum-ray index.

The flag is not a mathematical test. An interruption supplies neither a witness nor a certificate, so it must never become
`false`. In standalone model and test builds, these checkpoints compile to no-ops and do not alter traversal.

Memory exhaustion, process termination, or any other failure before a Boolean return has the same logical status: no classification
was obtained.

## Source Boundary And Coposit Choices

The model has three provenance layers. They must not be conflated.

| Component | Provenance | Exact maintained choice |
|---|---|---|
| One-coordinate minimization | Danninger, 1990 | Minimize over one nonnegative coordinate and separate by the sign of $p^Ty$. |
| Integer reduced form | Danninger reconstruction | Use $S=aB-pp^T$ rather than the rational Schur complement. |
| Sign-defined half-cones | Danninger reconstruction | Use coordinate rays and primitive positive-negative boundary rays. |
| General mixed-sign branching count | Danninger reconstruction | Count the two staircase families by $\binom{r+s}{r}$. |
| Direct criteria through order three | Maintained Coposit/Danninger reconstruction | Use exact order-one and order-two tests and Hadeler's order-three criterion. |
| Negative two-ray rejection | Pinned Dutour implementation | Reject when $c_{ij}<0$ and $c_{ij}^2\geq c_{ii}c_{jj}$. |
| Dutour pair choice | Pinned Dutour implementation | Maximize $c_{ij}^2/(c_{ii}c_{jj})$ by exact cross multiplication; keep the first tie. |
| Dutour subdivision | Pinned Dutour implementation | Split through the unscaled sum ray $v_i+v_j$ into two covering cones. |
| Narrow-pivot gate | Coposit-created | Permit Danninger only when $r=0$, $s=0$, or $r=s=1$. |
| Pivot selection | Coposit-created | Scan current rows in index order and use the first narrow row. |
| Adaptive routing | Coposit-created | Use one Dutour split only when no row is narrow, then restart the decision in every child. |
| Matrix-only node storage | Coposit representation choice | Store exact Gram matrices, not complete generator histories. |
| Cooperative interruption | Shared Coposit infrastructure | Check a signal flag without changing any mathematical branch. |

The Danninger citation supports the published dimension-reduction idea, but no original source program and no complete publicly
accessible copy of the short proceedings article were available for a line-by-line audit. The concrete integer rays, terminal
shortcut, and deterministic traversal are therefore accurately described as the maintained reconstruction, not as recovered
original Danninger source code. The detailed reconstruction boundary is recorded in the
[`danninger_1990` algorithm document](../../baselines/danninger_1990/ALGORITHM.md).

The Dutour fallback was checked against the pinned Polyhedral Common implementation. Its strict pair test, maximum-ratio heuristic,
first-tie behavior, unscaled sum ray, two children, and child order are source-derived. Coposit's Gram-only storage removes unused
generator-coordinate state without changing those mathematical decisions. The detailed fidelity boundary is recorded in the
[`dutour_2018` algorithm document](../../baselines/dutour_2018/ALGORITHM.md).

No external source describes the hybrid as a whole. These decisions define Coposit's Adaptive Dutour-Danninger model:

1. apply direct exact criteria at orders zero through three;
2. reject a nonpositive diagonal before any order-four-or-larger transformation;
3. use Danninger only when the current pivot sign pattern creates one or two children;
4. scan pivots in index order and take the first usable one;
5. use exactly one maximum-ratio Dutour split only when no usable Danninger pivot exists;
6. check the defined first child before its sibling and stop on the first failure;
7. restart the complete adaptive decision, including the direct case and pivot-zero scan, in every child.

Changing the gate, pivot choice, fallback, child construction, or recursive restart rule creates a different hybrid model.

## Implementation-To-Algorithm Correspondence

The complete maintained implementation is local to [`solver.cpp`](solver.cpp). The following map is intended for a human checking
that the prose and source still agree.

| Source element | Mathematical responsibility |
|---|---|
| `solve` | Validate nonempty square symmetric public input, then create one checker and start at the input Gram matrix. |
| `adaptive_dutour_danninger_checker::check` | Apply a timeout checkpoint, direct terminal cases, diagonal rejection, adaptive pivot routing, and Dutour fallback in that order. |
| `decide_small` | Dispatch orders zero, one, two, and three to the exact terminal criteria. |
| `is_strictly_copositive_1x1` | Test the sole diagonal entry. |
| `is_strictly_copositive_2x2` | Test positive diagonal and, only for a negative off-diagonal, positive determinant. |
| `is_strictly_copositive_3x3` | Test the three principal pairs, determinant, and the six independent adjugate entries. |
| `first_narrow_danninger_pivot` | Count exact positive and negative row entries and return the first row satisfying the narrow cases. |
| `check_danninger` | Partition the pivot row, choose the one-child or two-child case, build the required matrices and rays, and recurse. |
| `make_principal_block` | Build $B$ by deleting the pivot row and column while retaining index order. |
| `make_schur_block` | Build $S=aB-pp^T$ exactly and symmetrically. |
| `coordinate_ray` | Represent one standard basis ray. |
| `pair_ray` | Construct and gcd-reduce the unique positive-negative boundary ray. |
| `transform` | Calculate $R^TMR$ from one- or two-entry sparse rays, upper triangle first, then mirror. |
| `check_dutour` | Test all negative pairs, choose the exact maximum ratio, certify an entrywise-nonnegative node, or recurse into two sum-ray children. |
| `replace_generator_with_sum` | Apply the Gram row-and-column update for replacing one generator by its sum with another. |

### Data and index conventions

The public matrix and every child use zero-based C++ indices. Mathematical formulas use symbolic indices and the worked example
uses one-based names $e_1,e_2,e_3$ for readability. When a Danninger pivot is removed, `remaining` lists every old index except the
pivot in ascending order. The reduced position $k$ refers to original index `remaining[k]`.

The vectors `positive`, `zero`, and `negative` store reduced positions, not original matrix indices. This is why a sparse boundary
ray can be applied directly to $B$ or $S$: both reduced matrices use exactly the `remaining` order.

A `sparse_ray` has storage for two index-coefficient pairs. A coordinate ray leaves `count` equal to one. A mixed boundary ray sets
`count` to two. No other ray shape is possible in the narrow Danninger implementation.

### Allocation and short-circuit order

In a one-sign Danninger case, only the required child matrix is built. In the mixed case, both $B$ and $S$ are built before the plus
child is tested, because each is required if both recursive calls are reached. The plus congruence child is temporary and is
destroyed after its recursive call returns. The minus child is not recursively entered if the plus child fails.

In a Dutour fallback, the source copies and updates both child matrices before recursively testing the first. It does not enter the
second recursive call after a first-child failure. Thus "short circuit" refers to decisions and descendant traversal, not to
avoiding allocation of the already prepared sibling.

Every child call starts at `check`; there is no special continuation state saying that it came from Danninger or Dutour. This is the
mechanism that makes the routing genuinely adaptive rather than alternating.

### Strict inequalities used by the implementation

Strict copositivity makes equality behavior important. The implementation uses the following exact boundaries:

| Test | Accepting side | Equality means |
|---|---:|---|
| Diagonal $c_{ii}$ | $c_{ii}>0$ | Immediate nonzero zero direction $e_i$. |
| Negative order-two determinant | $c_{ii}c_{jj}-c_{ij}^2>0$ | Nonnegative zero in that coordinate face. |
| Order-three determinant fast path | $\det C>0$ | Continue to the adjugate branch; equality is not accepted by this fast path. |
| Order-three Hadeler failure condition | Every independent adjugate entry is $>0$ | Any zero adjugate entry prevents that failure condition. |
| Dutour two-ray test | $c_{ij}^2<c_{ii}c_{jj}$ | Reject; the selected two-ray cone contains a zero. |
| Dutour ratio replacement | New ratio strictly greater | Equal ratios retain the pair encountered first. |

There is no epsilon and no ambiguous zero. FLINT sign and comparison operations distinguish equality exactly.

## What This Model Deliberately Does Not Do

Understanding the omissions is necessary to distinguish this algorithm from neighboring models:

- It does not test non-strict, non-strict copositivity. Every accepted result is about strict copositivity.
- It does not run the complete Danninger staircase when all rows are wide.
- It does not alternate Danninger and Dutour according to recursion depth.
- It does not score all narrow pivots or perform descendant lookahead.
- It does not use SNC slicing, simplex-center bounds, Hadeler principal-submatrix enumeration, Dickinson support enumeration, or
  connected-component decomposition.
- It does not reconstruct null spaces, solve a linear system, or enumerate arbitrary principal or non-principal submatrices.
- It does not normalize a complete child matrix by its integer content.
- It does not use floating point, tolerances, rational matrix storage, fixed-width sign masks, or a fixed maximum dimension.
- It does not retain an explicit counterexample vector through the recursion, even though every rejection has a mathematical
  witness.
- It does not share its algorithm implementation with another model directory. Similar code is intentionally local so this model's
  routing can change without changing a baseline.

Adding any omitted mathematical shortcut or fallback may be useful, but it would need to be documented and evaluated as a changed
model rather than silently attributed to Adaptive Dutour-Danninger.
