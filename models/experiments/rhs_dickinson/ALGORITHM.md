# RHS Dickinson

Classification: Coposit-created exact strict-copositivity variant of the maintained `dickinson_2019` adaptation.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to a non-strict query.

## Idea In Plain Language

Dickinson's published construction normally obtains a certificate vector for a nonsingular principal matrix $A_I$ by solving

\[
A_Iu=\mathbf1.
\]

The paper observes that the all-ones right-hand side is not essential: any strictly positive right-hand side is valid. RHS Dickinson
uses that freedom to search the one-parameter families

\[
b_k(t)=\mathbf1+t e_k,\qquad t\ge0,\quad k\in I.
\]

For each coordinate direction, the solution $u_k(t)=A_I^{-1}b_k(t)$ and the full product $Au_k(t)$ depend affinely on $t$. Their
supports and sign sets can change only at finitely many exact rational values. The algorithm evaluates every such value and every
interval between consecutive values, then retains the single vector that gives the widest Dickinson coverage interval.

No approximation, numerical tolerance, rational matrix, or general linear-programming solver is used. A rational parameter $t=p/q$
is represented by the positively scaled integer vector $q u_0+p d_k$.

## Name, Source, And Classification

“RHS” means **right-hand side**. The name distinguishes the model from the historical baseline because choosing a right-hand side to
maximize later coverage is a Coposit traversal heuristic, not an algorithm specified in the paper.

The underlying certificate theorem, non-strict algorithms, and permission to replace $\mathbf1$ by any strictly positive vector come
from:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

The note immediately following Dickinson's Algorithms 1 and 2 states that the equation $Xw=\mathbf1$ may use any right-hand side in
the strictly positive orthant and still yields a valid construction.

This model was copied from Coposit's [`dickinson_2019`](../../baselines/dickinson_2019/ALGORITHM.md) implementation. The local authoritative source
is [`solver.cpp`](solver.cpp). The fixed subset traversal, non-strict coverage theorem, singular-vector rule, negative witness, and
strict final interpretation remain those of the baseline. The exact one-dimensional right-hand-side search is the Coposit change.

## Problem And Notation

The input is a nonempty symmetric integer matrix $A\in\mathbb Z^{n\times n}$. The model returns `true` exactly when

\[
x^TAx>0\qquad\text{for every }x\in\mathbb R_+^n\setminus\{0\}.
\]

For a vector $u\in\mathbb R^n$ define

\[
\operatorname{supp}(u)=\{i:u_i\ne0\}
\]

and

\[
N_A(u)=\{i:(Au)_i\ge0\}.
\]

For an index set $I$, $A_I$ is the principal matrix on $I$. A vector calculated in the coordinates of $I$ is embedded into the full
space by inserting zeros outside $I$.

An admissible Dickinson vector must lie outside the nonpositive orthant, meaning that at least one component is positive.

## Dickinson Coverage

An admissible vector $u$ covers a support $J$ precisely when

\[
\operatorname{supp}(u)\subseteq J\subseteq N_A(u).
\]

It therefore covers the Boolean interval

\[
[\operatorname{supp}(u),N_A(u)]
=\{J:\operatorname{supp}(u)\subseteq J\subseteq N_A(u)\}.
\]

The number of supports in this interval is

\[
2^{|N_A(u)|-|\operatorname{supp}(u)|}.
\]

RHS Dickinson calls the exponent

\[
|N_A(u)|-|\operatorname{supp}(u)|
\]

the **coverage width**. It chooses the candidate with the greatest width. If two candidates have equal width, it chooses the one with
larger $|N_A(u)|$, favoring coverage of more outside indices. An exact tie leaves the earlier candidate unchanged. The non-strict
$t=0$ solution is the initial candidate, so a nonconstant right-hand side must strictly improve one of these two measures before it
is selected.

Coverage width is a deterministic heuristic, not a theorem that the chosen vector minimizes runtime. Every selected vector is valid
independently of the score.

## Processing A Nonsingular Principal Matrix

Let $I$ be the current uncovered support and let $m=|I|$. After one exact factorization of $A_I$, first solve

\[
A_Iu_0=\mathbf1.
\]

The fraction-free solver returns an integer numerator $z_0$ and a positive common denominator $D$ such that

\[
A_Iz_0=D\mathbf1.
\]

### Negative witness

If $z_0\le0$, then $-z_0\ge0$ and

\[
(-z_0)^TA_I(-z_0)=z_0^TA_Iz_0=Dz_0^T\mathbf1<0.
\]

The embedded vector is an exact negative witness, so the complete matrix is not copositive and the algorithm immediately returns
`false`. No right-hand-side search is necessary or permitted to override this proof.

### When the search is skipped

The search is skipped only for $m=1$. In one dimension, changing a positive scalar right-hand side only positively rescales the same
solution and cannot change its coverage signature. Even when the baseline vector already has a global upper set, higher-dimensional
supports are searched because a breakpoint can zero a vector component, shrink the lower support, and widen the complete Boolean
interval.

### Coordinate directions

Otherwise, solve all $m$ systems

\[
A_Id_k=e_k,\qquad k=1,\ldots,m,
\]

from the retained factorization. The shared fraction-free solve accepts multiple right-hand sides and returns integer numerators
$z_k$ with the same positive denominator $D$:

\[
A_Iz_k=De_k.
\]

For $t\ge0$,

\[
u_k(t)=u_0+t d_k
\]

satisfies

\[
A_Iu_k(t)=\mathbf1+t e_k>0.
\]

Thus its product is strictly positive on every coordinate of $I$. Its embedded support is contained in $I$, and any candidate with a
positive component is an admissible Dickinson vector.

## Why Only Finitely Many Parameters Matter

For every local vector coordinate $i\in I$,

\[
(u_k(t))_i=(u_0)_i+t(d_k)_i.
\]

For every full-matrix product coordinate $j$, including indices outside $I$,

\[
(Au_k(t))_j=(Au_0)_j+t(Ad_k)_j.
\]

Each expression is affine in $t$. A nonconstant affine expression changes its zero/nonzero status or sign only at its root

\[
t=-\frac{a}{c},
\]

where $a$ is its value at zero and $c$ is its direction coefficient. A positive root exists exactly when $a$ and $c$ have opposite
nonzero signs.

The model collects every positive root from:

- all $m$ components of $u_k(t)$, because a zero changes $\operatorname{supp}(u_k(t))$;
- all $n$ components of $Au_k(t)$, because a zero or sign change changes $N_A(u_k(t))$.

Roots are exact positive rational pairs and are sorted by cross multiplication. Equal ratios are removed by the same exact
comparison; normalization by a greatest common divisor is unnecessary.

Between two consecutive roots, every relevant sign and every support membership is constant. At a root itself, a vector component
may leave the support or a product component is included in $N_A(u)$ because the coverage inequality is non-strict. A root can
therefore have a better signature than either adjacent open interval and must be evaluated separately.

## Complete Breakpoint Sweep

Let the distinct positive roots be

\[
0<r_1<r_2<\cdots<r_s.
\]

For one coordinate direction, the implementation evaluates:

1. one point in $(0,r_1)$, namely $r_1/2$;
2. every root $r_i$ itself;
3. the exact midpoint $(r_i+r_{i+1})/2$ of every bounded open interval;
4. one point after the final root, namely $r_s+1$.

The baseline point $t=0$ was already scored before the direction search. These samples exhaust all possible support and sign
signatures on $t\ge0$.

Roots at zero are not stored. Moving away from zero can only add a formerly zero vector component to the lower support and can only
retain or remove a formerly zero product component from the nonnegative upper set. It therefore cannot improve the $t=0$ signature
without a later positive root, whose preceding interval is already sampled.

If a direction has no positive root, its signature cannot improve the non-strict candidate, so it is skipped.

## Exact Representation Of A Candidate

Suppose the selected parameter is

\[
t=\frac pq,\qquad p\ge0,\quad q>0.
\]

Instead of storing a rational vector, the model uses

\[
z=qz_0+pz_k.
\]

Because

\[
z=Dq\,u_k(p/q),
\]

this is a positive scaling of the desired rational solution. Positive scaling preserves:

- which vector entries are zero;
- all vector and product signs;
- the Dickinson coverage interval;
- negative-witness and zero status.

All parameter numerators, denominators, cross products, midpoints, vector entries, and matrix products use FLINT arbitrary-precision
integers.

## Singular Principal Matrices

The right-hand-side family applies only when $A_I$ is nonsingular. For a singular principal matrix, the model retains the baseline
construction unchanged:

1. recover one nonzero exact vector $z$ from the nullspace of the partial LDLT factorization;
2. negate it if its current orientation has no positive entry;
3. embed and retain the resulting admissible vector.

The implementation uses one free transformed coordinate and does not construct or optimize over a nullspace basis. Consequently the
RHS heuristic does not improve coverage produced by singular supports.

## Strict Copositivity

Dickinson's published construction certifies non-strict copositivity. Coposit instead decides only strict copositivity. For every
generated vector $u$, it checks

\[
u\ge0
\quad\text{and}\quad
u^TAu=0
\]

with exact integers.

After all supports have been covered or processed, Dickinson's Lemma 5.2 and Corollary 5.3 imply that a completed non-strict
certificate contains every minimal zero up to positive scaling. Therefore

\[
A\text{ is strictly copositive}
\iff
\text{the completed certificate contains no nonnegative zero}.
\]

As in the maintained `dickinson_2019` model, finding a nonnegative zero returns `false` immediately. If the finite traversal completes
without encountering one, the completed certificate and Corollary 5.3 prove strict copositivity.

## Direct Exact Tests Through Order Three

For every visited principal support of order at most three, the shared exact strict-copositivity criterion runs before certificate
coverage. It checks the exact order-one, order-two, and order-three formulas and rejects immediately on a nonpositive face.

A passing direct test does not cover the support. The normal Dickinson coverage lookup and, if needed, solve or nullspace branch
still run. Inputs whose complete dimension is at most three return directly from this shared criterion.

## Complete Control Flow

1. Receive a parser-guaranteed nonempty, square, exactly symmetric input.
2. Return the shared direct result when the complete dimension is at most three.
3. Visit support sizes $1,2,\ldots,n$.
4. Within one size, visit supports in increasing numeric-bit-mask order.
5. Apply the direct exact strict test to supports through order three and reject on failure.
6. Skip a support already covered by a retained Dickinson signature.
7. Factor the uncovered principal matrix $A_I$ exactly.
8. If singular, recover, orient, and retain one baseline nullspace vector.
9. If nonsingular, solve $A_Iu_0=\mathbf1$ and reject if the numerator is all nonpositive.
10. Calculate the baseline vector's full product and initial coverage width.
11. If $|I|=1$, retain the baseline vector unchanged.
12. Otherwise solve all coordinate directions $A_Id_k=e_k$ from the same factorization.
13. For each $k$, construct and sweep all exact positive breakpoints.
14. Retain the single candidate with greatest coverage width, breaking a width tie by the larger upper set.
15. Return `false` immediately when the selected vector is a nonnegative zero; otherwise retain its signature.
16. After the finite support traversal, return `true`.

## Storage, Traversal, And Termination

The retained certificate collection still stores only two packed sets per selected vector:

- $\operatorname{supp}(u)$;
- $N_A(u)$.

Exact solution directions, products, breakpoints, and candidate parameters are reusable scratch and are discarded after one support.
Certificate signatures remain bucketed by the lowest support index and are searched newest first, exactly as in the baseline.

The outer algorithm visits at most $2^n-1$ nonempty supports. For every uncovered nonsingular support of order $m>1$, the added
search solves $m$ extra right-hand sides. Each of the $m$ direction sweeps has at most $m+n$ distinct
positive breakpoints and therefore finitely many candidates. Pairwise certificate combinations and recursive generated candidates
do not exist.

Timed Python builds check the cooperative timeout flag at support, certificate, factorization, product-row, and direction boundaries.
A timeout remains unresolved and is never returned as a negative classification.

## Source Behavior And Coposit Changes

Preserved from `dickinson_2019`:

- Dickinson's coverage condition;
- increasing-cardinality and numeric-mask support traversal;
- fraction-free factorization;
- the baseline all-ones solve and its negative witness;
- one exact singular nullspace vector;
- low-dimensional strict rejection;
- immediate strict-zero rejection and completed-certificate strict acceptance;
- packed signature storage and bucket lookup.

Added by RHS Dickinson:

- solve the coordinate right-hand sides on every nonsingular support above order one;
- enumerate the exact one-dimensional right-hand-side sign arrangements;
- score candidates by Boolean-interval width;
- replace the baseline vector only when a searched candidate improves the score.

The baseline `dickinson_2019` remains separately maintained; both models use the same strict-only zero termination.

## Known Difficult Inputs

The method is unfavorable when the fixed all-ones vector already has adequate coverage. It still solves all coordinate directions
and sweeps their sign arrangements because a root could shrink the lower support, even if none of the resulting vectors eliminates
an additional future factorization.

Coverage width counts every support in a Boolean interval equally. The actual traversal may already have processed many of those
supports or may never encounter them because another certificate intervenes. A mathematically wider interval can therefore have
little operational value.

Large supports with many distinct affine roots are costly. Each coordinate direction can create up to $m+n$ breakpoints, and every
breakpoint and intervening interval is scored exactly. Large arbitrary-precision coefficients make the cross products and candidate
evaluations more expensive.

The heuristic cannot improve singular-vector selection. Boundary matrices dominated by singular principal supports, especially
those whose decisive nonnegative zero has large support, retain the same weak nullspace choice and may require exponentially many
earlier supports before reaching the zero, although traversal stops as soon as that zero is generated.

Finally, the search covers only the rays $\mathbf1+t e_k$. A better strictly positive right-hand side may require coordinated changes
in several entries and lie on none of these rays. Searching the complete positive right-hand-side cone would require a more general
exact feasibility or optimization procedure and is intentionally outside this experiment.
