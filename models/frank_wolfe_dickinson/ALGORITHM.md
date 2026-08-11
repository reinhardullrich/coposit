# Frank–Wolfe Dickinson

Classification: Coposit-created strict-copositivity variant with a bounded Frank–Wolfe witness search in front of the maintained
`dickinson_2019` certificate algorithm.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to an ordinary query.

## Idea In Plain Language

A symmetric matrix $A$ is strictly copositive exactly when

\[
x^TAx>0
\]

for every nonzero vector $x\geq0$. Because the expression is homogeneous, it is enough to consider the standard simplex

\[
\Delta=\{x\geq0:\mathbf1^Tx=1\}.
\]

Frank–Wolfe Dickinson first tries to find one point of this simplex with $x^TAx\leq0$. It repeatedly chooses the simplex vertex
that gives the best first-order descent and applies the closed-form quadratic minimizer along the line segment to that vertex. This
search is cheap compared with enumerating principal supports, but it is only a local heuristic because the quadratic need not be
convex.

Floating-point arithmetic is never allowed to decide copositivity. It only proposes a point. Before rejecting a matrix, the model
converts that point to a nonzero nonnegative integer vector $z$ and verifies $z^TAz\leq0$ with FLINT integers. If the bounded search
does not produce such an exact witness, the model runs the maintained strict-only Dickinson traversal. Therefore Frank–Wolfe can add an
early rejection, but it cannot add a false rejection or a false acceptance.

## Name, Sources, And Classification

The name joins the two algorithms deliberately:

- the witness search uses the method introduced by Marguerite Frank and Philip Wolfe;
- the complete fallback uses Peter J. C. Dickinson's certificate construction.

Primary sources:

- Marguerite Frank and Philip Wolfe, “An Algorithm for Quadratic Programming,” *Naval Research Logistics Quarterly* 3(1–2),
  95–110 (1956), [DOI 10.1002/nav.3800030109](https://doi.org/10.1002/nav.3800030109);
- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

The model is not a faithful historical baseline. The bounded multi-start policy, floating-point proposal phase, integer
reconstruction, and placement before Dickinson are Coposit choices. The fallback was copied from the maintained
[`dickinson_2019`](../dickinson_2019/ALGORITHM.md) model. The authoritative implementation is [`solver.cpp`](solver.cpp).

## The Optimization Problem

Define

\[
f(x)=x^TAx.
\]

For symmetric $A$, the gradient is

\[
\nabla f(x)=2Ax.
\]

The model searches over $\Delta$. Every point in $\Delta$ is nonnegative and nonzero, so either of the following is a complete
strict-copositivity counterexample:

\[
f(x)<0
\]

or

\[
f(x)=0.
\]

A negative value also disproves ordinary copositivity. A zero value disproves strict copositivity but is compatible with ordinary
copositivity.

## Choosing The Frank–Wolfe Direction

At the current point $x$, Frank–Wolfe minimizes the linear approximation over the simplex:

\[
s\in\arg\min_{v\in\Delta}\nabla f(x)^Tv.
\]

A linear function on the simplex reaches its minimum at a vertex. Writing $y=Ax$, the implementation therefore chooses

\[
j\in\arg\min_i y_i,
\qquad s=e_j.
\]

Ties use the first index. The direction is

\[
d=e_j-x.
\]

Every point on the segment

\[
x(\alpha)=x+\alpha d=(1-\alpha)x+\alpha e_j,
\qquad 0\leq\alpha\leq1,
\]

remains in the simplex. This is movement inside one normalized cross-section of the nonnegative cone; it is not a cone partition.
Only one path is explored, and unexplored points are not certified.

The directional derivative at the current point is

\[
\frac12\frac{d}{d\alpha}f(x+\alpha d)\bigg|_{\alpha=0}
=d^TAx
=y_j-f(x).
\]

If $y_j-f(x)\geq0$, no simplex vertex supplies first-order descent and that restart stops.

## Closed-Form Line Minimum In Floating Proposal Arithmetic

Along the selected segment,

\[
f(x+\alpha d)=f(x)+2\alpha g+\alpha^2h,
\]

where

\[
g=d^TAx=y_j-f(x)
\]

and

\[
h=d^TAd=A_{jj}-2y_j+f(x).
\]

When $h>0$, the one-variable quadratic is convex and its unconstrained minimum is $-g/h$. The segment minimum is therefore

\[
\alpha=\min\left(1,-\frac gh\right),
\]

because this branch is entered only when $g<0$. When $h\leq0$, the function is linear or concave and is initially decreasing, so
its minimum on $[0,1]$ is the endpoint $\alpha=1$.

The model then updates both the point and its matrix product in linear time:

\[
x\leftarrow(1-\alpha)x+\alpha e_j,
\]

\[
Ax\leftarrow(1-\alpha)Ax+\alpha Ae_j.
\]

The second formula needs only column $j$ of the normalized matrix. It avoids a new matrix-vector multiplication at every iteration.

## Bounded Multi-Start Policy

The proposal phase uses at most eight deterministic starts:

1. the simplex centre $x=\mathbf1/n$;
2. up to seven coordinate vertices whose exact input diagonal entries have the smallest normalized floating values.

Each start performs at most 64 Frank–Wolfe steps. It stops earlier when the computed objective is non-finite, no negative
first-order slope exists, or no positive finite step can be formed.

The constants seven and 64 are fixed implementation limits, not mathematical thresholds. They bound the heuristic cost and prevent
a local optimizer from replacing the exact decision algorithm. The starting points and tie rules are deterministic.

This is standard Frank–Wolfe rather than pairwise or away-step Frank–Wolfe. It moves toward one vertex but does not directly transfer
mass between two selected coordinates. Corrective subproblems, random restarts, gradient tolerances, and convergence tests are not
implemented.

## Safe Floating Representation Of Arbitrary Integers

The input entries can contain far more digits than a `double` can represent. The proposal search first finds an exact entry of
maximum absolute value and obtains its binary exponent $E$. An entry with mantissa $m_{ij}$ and exponent $E_{ij}$ is represented as

\[
\widehat A_{ij}=m_{ij}2^{E_{ij}-E}.
\]

This is one common positive power-of-two scaling of the whole matrix. It keeps the largest entry near one and preserves all exact
quadratic signs in ideal arithmetic. It does not perform row or column equilibration, which would change the simplex problem.

An entry too small for the `double` exponent range becomes zero. Other rounding is also possible. Both are safe because
$\widehat A$ is used only to select a candidate. No sign conclusion about the exact matrix is taken from $\widehat A$.

The dense floating matrix is never stored. The centre start calculates all normalized row sums from one symmetric triangle, adding
each off-diagonal entry to both affected rows. The same pass caches the normalized diagonal used by seed ordering and line
curvatures. Later iterations convert only the selected exact matrix column. The proposal phase therefore uses $O(n)$ floating
storage rather than another $O(n^2)$ matrix and never converts the second symmetric triangle.

## Exact Witness Reconstruction

Nonpositive diagonal entries are checked before any floating allocation. If $A_{ii}\leq0$, the coordinate vector $e_i$ is already
an exact strict-copositivity counterexample.

The all-ones direction is then checked before floating search. While finding the maximum entry, the model also accumulates

\[
\mathbf1^TA\mathbf1
\]

exactly from the stored symmetric triangle. If this value is nonpositive, $\mathbf1$ is already an exact witness and the model
returns `false` immediately.

For a floating candidate $x$, every nonnegative component is rounded after multiplication by

\[
2^{40}=1{,}099{,}511{,}627{,}776.
\]

This produces integer weights

\[
z_i=\operatorname{round}(2^{40}\max(0,x_i)).
\]

Zero weights are omitted from the exact calculation. If rounding were ever to remove every coordinate, the largest floating
coordinate is assigned weight one. Thus $z\geq0$ and $z\ne0$ always hold.

The model evaluates

\[
z^TAz=\sum_{i\in\operatorname{supp}(z)}z_i
       \sum_{j\in\operatorname{supp}(z)}A_{ij}z_j
\]

with arbitrary-precision FLINT integers. It rejects only when this exact value is at most zero. Rounding may lose a genuine witness;
that causes only a missed shortcut, after which Dickinson still decides the matrix. Rounding cannot create an incorrect rejection
because the reconstructed vector itself is a valid exact witness whenever the displayed integer inequality holds.

The first clearly negative floating iterate in each restart is verified immediately, using `-1e-12` only as a performance trigger.
If that check does not succeed, the best final point across all restarts is verified once when its floating value is nonpositive.
Neither floating threshold is a classification threshold.

## Dickinson Fallback

When no exact Frank–Wolfe witness is found, the model executes the same finite construction as `dickinson_2019`.

For a vector $u$, define

\[
\operatorname{supp}(u)=\{i:u_i\ne0\}
\]

and

\[
N_A(u)=\{i:(Au)_i\geq0\}.
\]

A retained Dickinson vector covers a support $I$ when

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

The fallback visits nonempty supports by increasing size and, within one size, increasing numeric-mask order. A covered support is
skipped. For an uncovered support $I$, it forms the principal matrix $A_I$ exactly.

### Nonsingular support

The model solves

\[
A_Iw=\mathbf1
\]

with fraction-free LDLT. If $w\leq0$, then $-w\geq0$ and

\[
(-w)^TA_I(-w)=w^T\mathbf1<0,
\]

so Dickinson itself has found an exact negative witness. Otherwise the embedded $w$ becomes a certificate vector.

### Singular support

The model recovers one nonzero exact vector $w$ with

\[
A_Iw=0
\]

from the partial fraction-free factorization. It negates the vector when necessary so that at least one component is positive, as
required by Dickinson's construction.

### Retained information and strict conclusion

For each accepted nonzero vector the fallback retains only its packed support and packed $N_A(u)$ set. A generated nonnegative zero
returns `false` immediately. A completed Dickinson certificate contains every minimal zero up to positive scaling, so the model
returns `true` only when the finite traversal completes without encountering one.

Principal supports through order three receive the shared direct exact strict-copositivity test before coverage. A failure is an
exact nonpositive witness. A pass continues through ordinary Dickinson construction and does not create a foreign certificate.

## Complete Decision Flow

1. Require a nonempty, square, exactly symmetric integer matrix.
2. For complete order at most three, return the shared exact direct result.
3. Reject on a nonpositive diagonal entry.
4. In the same exact triangular scan, calculate $\mathbf1^TA\mathbf1$ and the largest absolute entry.
5. Reject if the all-ones value is nonpositive.
6. Allocate floating search storage and build the centre product from one symmetric triangle.
7. Run Frank–Wolfe from the simplex centre for at most 64 steps.
8. Run it from up to seven smallest-diagonal vertices, again for at most 64 steps each.
9. Reconstruct and exactly test promising candidates; reject only on an exact nonpositive integer quadratic value.
10. If no witness was verified, run the maintained strict-only Dickinson subset traversal.
11. Return Dickinson's exact strict-copositivity result.

## Correctness Boundary

Every normal return has an exact justification:

- `false` from the front end has a nonzero integer $z\geq0$ with $z^TAz\leq0$;
- `false` from Dickinson has its exact negative witness, direct low-order witness, or generated nonnegative zero;
- `true` comes only from a completed Dickinson certificate with no nonnegative zero.

Frank–Wolfe incompleteness cannot produce `true`; it merely transfers control to Dickinson. Floating overflow, underflow, rounding,
local stationarity, or the iteration limit likewise cannot become a classification.

Cooperative timeouts are checked during the exact input scan, floating row construction, every Frank–Wolfe iteration, exact witness
verification, and the Dickinson traversal. A timeout is unresolved rather than `false`.

## Source Behavior And Coposit Changes

From Frank and Wolfe:

- minimize a differentiable objective by solving a linear problem over the feasible polytope;
- move toward the selected extreme point;
- minimize the objective along that feasible segment.

From `dickinson_2019`:

- coverage by $\operatorname{supp}(u)\subseteq I\subseteq N_A(u)$;
- the all-ones nonsingular solve and its negative witness;
- one admissible exact singular nullspace vector;
- support traversal, packed signatures, and strict-zero conclusion.

Coposit additions:

- treat Frank–Wolfe only as a bounded witness proposal phase for a possibly nonconvex standard quadratic program;
- reject exact coordinate and all-ones witnesses before allocating the numerical search;
- use the centre and seven smallest-diagonal vertex starts;
- normalize arbitrary-size integers by one common binary exponent without storing a double matrix;
- reconstruct a dyadic-scale integer candidate and verify it exactly;
- use the maintained strict-only Dickinson rule that returns immediately on a generated nonnegative zero.

## Known Difficult Inputs

Strictly copositive matrices cannot benefit from the witness search. They pay for the exact all-ones scan, floating normalization,
up to eight short local searches, and then still require the complete Dickinson decision.

The quadratic over the simplex is generally nonconvex. Frank–Wolfe can stop at a local stationary point with positive value even
when a negative region exists elsewhere. Seven vertex starts do not provide global coverage, especially when the negative witness
has a structured support unrelated to the smallest diagonal entries.

Very narrow negative regions can disappear during floating conversion or integer reconstruction. Large precision spans can turn
small normalized entries into zero, and fixed $2^{40}$ reconstruction can remove tiny candidate coordinates. Both cases safely fall
back but reduce the shortcut's usefulness.

Matrices whose first obstruction is an exact zero rather than a robust negative value are particularly difficult for a numerical
proposal. The centre detects symmetric all-ones zeros exactly, but a general boundary zero is likely to be left to Dickinson.

Finally, if Frank–Wolfe finds no witness, the main Dickinson weaknesses remain: narrow coverage intervals can approach the full
$2^n-1$ support traversal, many singular principal matrices require repeated exact factorization, and a larger-support boundary zero
may be reached only after many earlier supports. Once generated, that zero terminates the strict-only traversal immediately.
