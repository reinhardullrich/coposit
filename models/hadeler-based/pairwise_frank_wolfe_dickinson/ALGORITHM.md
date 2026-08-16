# Pairwise Frank–Wolfe Dickinson

Classification: coposit-created CP/SCP variant with a bounded pairwise Frank–Wolfe witness search followed by the
maintained exact Dickinson certificate algorithm.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

A symmetric matrix $A$ is strictly copositive exactly when

$$
x^TAx>0
$$

for every nonzero $x\geq0$. Homogeneity lets the model search the standard simplex

$$
\Delta=\{x\geq0:\mathbf1^Tx=1\}.
$$

The proposal phase tries to find one $x\in\Delta$ with $x^TAx\leq0$. Standard Frank–Wolfe moves toward one selected simplex vertex
and proportionally shrinks every existing coordinate. Pairwise Frank–Wolfe instead selects both a toward vertex and an active away
vertex, then transfers weight directly between them. This can remove unhelpful coordinates and explore sparse faces more quickly.

The proposal arithmetic is floating point and is never trusted as a classification. Every proposed witness is converted to a
nonzero nonnegative integer vector and tested with exact FLINT arithmetic. If no exact witness is verified, the complete exact
Dickinson traversal runs unchanged. Pairwise Frank–Wolfe can therefore save work, but it cannot cause a false rejection or false
acceptance.

## Name, Sources, And Classification

The identifier is `pairwise_frank_wolfe_dickinson`:

- **pairwise Frank–Wolfe** names the direct transfer between a toward vertex and an active away vertex;
- **Dickinson** names the complete exact fallback.

Primary sources are:

- Marguerite Frank and Philip Wolfe, “An Algorithm for Quadratic Programming,” *Naval Research Logistics Quarterly* 3(1–2),
  95–110 (1956), [DOI 10.1002/nav.3800030109](https://doi.org/10.1002/nav.3800030109);
- Simon Lacoste-Julien and Martin Jaggi, “On the Global Linear Convergence of Frank-Wolfe Optimization Variants,” *Advances in
  Neural Information Processing Systems* 28 (2015), [arXiv:1511.05932](https://arxiv.org/abs/1511.05932), especially Algorithm 2;
- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

This is not a historical baseline. It was copied from coposit's `frank_wolfe_dickinson` model and changes only the bounded proposal
step. The multi-start policy, floating representation, exact reconstruction, direct witnesses, and Dickinson fallback are retained.
The convergence guarantees in the pairwise Frank–Wolfe paper assume a convex setting and are not used here: $x^TAx$ may be
nonconvex on the simplex, and the proposal remains a bounded heuristic.

## Optimization Problem And State

Define

$$
f(x)=x^TAx,
\qquad \nabla f(x)=2Ax.
$$

One restart stores:

- the current simplex point $x$ as a dense `double` vector;
- its normalized matrix product $y=\widehat Ax$;
- the active coordinates $\{i:x_i>0\}$, represented directly by positive entries of $x$;
- the current floating value $x^Ty$.

The simplex vertex decomposition is unique, so the active vertex weights are exactly the positive coordinates of $x$. No separate
active-set container or duplicate coefficient map is needed.

## Toward And Away Vertices

The toward vertex minimizes the linearized objective over the whole simplex. With $y=\widehat Ax$, choose

$$
t\in\arg\min_i y_i.
$$

The away vertex maximizes the same product over the active coordinates:

$$
a\in\arg\max_{i:x_i>0} y_i.
$$

Both ties use the first index encountered. The pairwise direction is

$$
d=e_t-e_a.
$$

Moving by $\alpha$ gives

$$
x(\alpha)=x+\alpha(e_t-e_a).
$$

The coordinate sum remains one. Nonnegativity requires

$$
0\leq\alpha\leq x_a,
$$

so $x_a$ is the exact floating feasible-step bound. A step at that bound drops the away coordinate from the active set.

The half directional derivative is

$$
g=d^T\widehat Ax=y_t-y_a.
$$

If $g\geq0$, this pairwise rule supplies no descent direction and the restart stops. This is only local stationarity of the floating
proposal; it is not a copositivity certificate.

## Closed-Form Pairwise Line Minimum

Along the feasible segment,

$$
f(x+\alpha d)=f(x)+2\alpha g+\alpha^2h,
$$

where

$$
h=d^T\widehat Ad
 =\widehat A_{tt}+\widehat A_{aa}-2\widehat A_{ta}.
$$

When $h>0$, the one-dimensional quadratic is convex and its unconstrained minimizer is $-g/h$. The implementation takes

$$
\alpha=\min\left(x_a,-\frac gh\right).
$$

When $h\leq0$, the line is linear or concave and initially decreasing, so its minimum over the feasible interval is the endpoint

$$
\alpha=x_a.
$$

The update changes only two coordinates:

$$
x_a\leftarrow x_a-\alpha,
\qquad
x_t\leftarrow x_t+\alpha.
$$

The maintained product is updated in one matrix-column scan:

$$
y\leftarrow y+\alpha(\widehat Ae_t-\widehat Ae_a).
$$

The objective is recomputed as $x^Ty$ after the update. This avoids a dense matrix-vector multiplication and keeps the value
consistent with the current stored point and product.

## Bounded Deterministic Starts

The proposal phase retains the parent model's limits:

1. start at the simplex centre $x=\mathbf1/n$;
2. start at up to seven coordinate vertices with the smallest normalized diagonal values;
3. take at most 64 pairwise steps per start.

A restart stops earlier for a non-finite objective, absence of a negative pairwise slope, or absence of a positive finite step. The
limits bound heuristic work; they are not convergence or classification thresholds. There are no random starts, corrective
subproblems, gradient tolerances, or threads.

## Floating Representation Of Arbitrary Integers

The exact entry of maximum absolute value supplies a binary exponent $E$. If an entry has floating mantissa $m_{ij}$ and exponent
$E_{ij}$, the proposal uses

$$
\widehat A_{ij}=m_{ij}2^{E_{ij}-E}.
$$

This is one common positive power-of-two scaling. It does not change the ideal quadratic signs or the ideal pairwise directions.
Underflow and rounding can change the heuristic path, but cannot change a returned classification because every witness is checked
against the original integer matrix.

The model stores no dense floating matrix. It builds the centre product from one exact symmetric triangle, caches the normalized
diagonal, and converts only the two selected columns during a pairwise update. Floating storage is $O(n)$.

## Direct And Reconstructed Exact Witnesses

Before allocating floating vectors, the model rejects two exact cases:

1. if $A_{ii}\leq0$, then $e_i^TAe_i\leq0$;
2. while scanning the symmetric triangle, calculate $\mathbf1^TA\mathbf1$ exactly and reject when it is nonpositive.

For a floating proposal $x$, define integer weights

$$
z_i=\operatorname{round}(2^{40}\max(0,x_i)).
$$

Zero weights are omitted. If all weights round to zero, the largest floating coordinate receives weight one. Hence $z\geq0$ and
$z\ne0$. FLINT then evaluates $z^TAz$ exactly. The model rejects only when

$$
z^TAz\leq0.
$$

The first floating iterate below `-1e-12` in a restart triggers an immediate exact attempt. If that attempt fails, the best final
point across the deterministic starts is tested once when its floating value is nonpositive. The threshold only avoids repeated
exact reconstruction; it never decides a sign.

Rounding can miss a genuine witness. It cannot invent an invalid rejection because the reconstructed integer vector is itself an
exact witness whenever the final inequality holds.

## Dickinson Fallback

When the proposal supplies no exact witness, the model executes the complete maintained Dickinson construction on the original
matrix.

For a retained vector $u$, define

$$
\operatorname{supp}(u)=\{i:u_i\ne0\},
\qquad
N_A(u)=\{i:(Au)_i\geq0\}.
$$

The signature covers a principal support $I$ when

$$
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
$$

Supports are visited by increasing cardinality and then increasing numeric-mask order. Covered supports are skipped. For an
uncovered support $I$, the exact principal matrix $A_I$ is factorized once with fraction-free LDLT.

If $A_I$ is nonsingular, solve

$$
A_Iw=\mathbf1.
$$

An all-nonpositive $w$ yields the exact negative witness $-w\geq0$. Otherwise the embedded vector becomes a Dickinson certificate.

If $A_I$ is singular, recover one nonzero exact null vector from the retained partial factorization and orient it to contain a
positive component. The fallback returns `false` immediately when it generates a nonnegative zero. Dickinson's completed certificate
contains every minimal zero needed for the strict test, so completing traversal without encountering one returns `true`.

Principal supports through order three receive the shared exact strict-copositivity test before coverage. Failure gives an exact
nonpositive principal-face witness; passing does not create a certificate and normal Dickinson processing continues.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty, square, exactly symmetric integer matrix.
2. For complete order at most three, use the shared exact direct criterion.
3. Reject an exact nonpositive diagonal witness.
4. Calculate $\mathbf1^TA\mathbf1$ and the maximum absolute entry in one triangular scan.
5. Reject an exact nonpositive all-ones witness.
6. Build the normalized centre point, product, and diagonal cache.
7. Run at most 64 pairwise steps from the centre.
8. Run at most 64 pairwise steps from each of up to seven smallest-diagonal vertices.
9. Reject only after exact integer reconstruction proves a nonpositive quadratic value.
10. If no decisive negative witness was verified, run the maintained Dickinson traversal in the selected or combined mode.
11. Return the selected predicate or full CP/SCP classification.

## Correctness Boundary

Every `false` result from the proposal phase has a nonzero integer $z\geq0$ with $z^TAz\leq0$. Every other result comes from the
complete exact Dickinson fallback. Therefore:

- floating roundoff, underflow, local stationarity, or the iteration limit can only miss an optimization;
- pairwise Frank–Wolfe never returns `true`;
- pairwise Frank–Wolfe never returns `false` without an exact witness;
- timeout and resource interruption remain unresolved outcomes rather than negative classifications.

The pairwise update itself remains feasible because it transfers at most the away coordinate's current mass. It neither partitions
the simplex nor certifies unexplored points.

## Source Behavior, coposit Changes, And Representation Optimizations

The Lacoste-Julien–Jaggi pairwise rule contributes:

- a globally minimizing linear-oracle vertex;
- an active away vertex maximizing the gradient product;
- direction $e_t-e_a$ and maximum step equal to the away coefficient;
- line minimization on that feasible interval.

The copied `frank_wolfe_dickinson` model contributes the centre-plus-seven starts, 64-step bound, common binary scaling, exact
$2^{40}$ reconstruction, direct witnesses, and the maintained selected-predicate or combined Dickinson fallback.

Representation-only choices are dense $x$ and $Ax$ vectors, implicit active membership via $x_i>0$, cached diagonal values, product
updates from two exact columns, and deterministic first-index ties. No pairwise state is shared with another model.

## CP and SCP classification

Every floating pairwise proposal is reconstructed and checked exactly before it affects the answer. An exact negative quadratic
value rejects CP and SCP. An exact zero marks SCP false; strict-only mode stops, while CP and combined mode continue into the same
Dickinson traversal because another support may still be negative. The Dickinson phase applies the same rule to singular
nonnegative null vectors. A completed traversal proves CP and proves SCP exactly when neither phase found a zero. Thus `both` is one
pairwise search followed by at most one Dickinson traversal.

## Known Difficult Inputs

Strictly copositive matrices cannot be rejected by a witness search, so they pay the complete proposal cost before Dickinson.
Pairwise updates convert two exact columns per iteration rather than one, which can make the front end slower when no sparse witness
is found.

The quadratic need not be convex. Pairwise Frank–Wolfe may stop at a local stationary point with positive value while another face
contains a negative or zero witness. Vertex starts can also perform repeated full swaps, and the fixed 64-step budget may end before
a useful sparse support emerges.

Boundary matrices remain difficult because a floating approximation to an exact zero may reconstruct to a small positive value.
Very large coefficient spans can underflow normalized entries, and fixed $2^{40}$ weights can omit tiny active coordinates. Both
cases safely fall back to Dickinson but reduce the proposal's benefit.

If the proposal fails, the main Dickinson weaknesses remain: narrow coverage intervals can approach all $2^n-1$ supports, singular
principal matrices still need exact factorizations, and a larger-support boundary zero may be reached only after many earlier
supports. Once generated, that zero terminates strict-only mode but combined mode must continue the CP proof.
