# FracESSA First-Order Global-Minimum Model

Classification: Coposit-created strict-copositivity model adapted from FracESSA's exact safe candidate search.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to a non-strict query.

## Idea In Plain Language

A symmetric matrix is strictly copositive exactly when its quadratic form has a positive minimum on the standard simplex. This model
changes the sign of the input matrix and uses FracESSA's original first-order candidate search on the resulting game. It stops as soon
as an exact candidate payoff proves that the minimum is nonpositive. It deliberately does not ask whether a candidate is an ESS, a
strict local maximizer, a saddle, or a local minimum.

For an input matrix $A$, define

$$
Q=-A,
\qquad
\Delta_n=\{x\in\mathbb R^n:x\geq0,\ \mathbf1^Tx=1\}.
$$

The model decides the sign of

$$
M=\max_{x\in\Delta_n}x^TQx
  =-\min_{x\in\Delta_n}x^TAx
$$

without necessarily computing its value. It returns `false` immediately when any accepted KKT candidate has payoff at least zero.
If the complete pruned search finds only negative candidate payoffs, it returns `true`, which is equivalent to $M<0$.

## Name And Sources

The model is named `fracessa` because its support traversal and exact candidate equations come from FracESSA. It is not the original
FracESSA program and is not a historical baseline: Coposit changes the mathematical objective from enumerating and classifying ESSs
to deciding the sign of the global value of a standard quadratic program.

The source implementation was read locally from FracESSA revision
`95e0ec019cf11a60c6423508e8768536a0b88860`, principally:

- `cpp/include/fracessa/supports.hpp`, `NonCircularSupportGenerator`;
- `cpp/src/find_candidate_safe.cpp`, especially `build_reduced_system()`, `calculate_integer_payoff()`, and `find()`;
- `cpp/src/fracessa.cpp`, the non-circular candidate-search orchestration.

The candidate and pruning structure originates in Immanuel M. Bomze, “Detecting All Evolutionarily Stable Strategies,” *Journal
of Optimization Theory and Applications* 75(2), 1992, pages 313–329. A local transcription and paper are retained as
`research/papers/bomze_1992.md` and `research/papers/bomze_1992.pdf`.

## Why The Simplex Minimum Decides Strict Copositivity

The quadratic form is homogeneous:

$$
(tx)^TA(tx)=t^2x^TAx.
$$

Every nonzero $x\geq0$ can therefore be normalized by its positive coordinate sum into a point of $\Delta_n$. Consequently,

$$
A\text{ is strictly copositive}
\quad\Longleftrightarrow\quad
\min_{x\in\Delta_n}x^TAx>0
\quad\Longleftrightarrow\quad
\max_{x\in\Delta_n}x^TQx<0.
$$

The simplex is compact and the quadratic objective is continuous, so a global maximum of $x^TQx$ exists.

## Shared Direct Test Through Order Three

For every generated support $S$ with $|S|\leq3$, the model first applies Coposit's shared exact strict-copositivity criterion to
the principal matrix $A_S$. The check is made on the original input $A$, before constructing the reduced KKT system for $Q=-A$.
It uses the positive-diagonal rule at order one, the exact determinant rule at order two, and the exact
determinant-and-adjugate rule at order three. The implementation lives in
`cpp/include/coposit/small_copositivity.hpp` and reads indexed entries directly without copying $A_S$.

If the check fails, some nonzero $z_S\geq0$ satisfies $z_S^TA_Sz_S\leq0$. Padding it with zeros outside $S$ proves immediately
that the full input is not strictly copositive. This rejection does not require the point to satisfy the full-simplex outside-payoff
inequalities.

If the check passes, it does not prove that $S$ is a KKT support and it creates no pruning rule. The model therefore continues with
its normal reduced solve, positivity test, outside-payoff test, and candidate pruning. Supports removed earlier by FracESSA's valid
forbidden-support pruning are not regenerated merely to repeat this shortcut. When the complete input has order at most three, the
shared criterion returns the final answer directly and no KKT traversal is needed.

## First-Order Candidates

Let $S$ be a nonempty support and let $x_i>0$ exactly for $i\in S$. A full-simplex KKT candidate for maximizing $x^TQx$ satisfies

$$
Q_Sx_S=u\mathbf1,
\qquad
\mathbf1^Tx_S=1,
\qquad
x_S>0,
\qquad
(Qx)_k\leq u\quad(k\notin S).
$$

The common support payoff is the objective value:

$$
x^TQx=\sum_{i\in S}x_i(Qx)_i=u.
$$

These are first-order conditions only. A point passing them may be a saddle, a local minimum, a non-strict local maximum, or a
strict local maximum. The model keeps its payoff without classifying its local geometry.

Every global maximum satisfies these conditions. For a used coordinate $i$ and any coordinate $j$, the feasible one-sided
direction $e_j-e_i$ gives $(Qx)_j\leq(Qx)_i$. Directions between two used coordinates are feasible both ways, so all used
coordinates have equal payoff. This argument also covers boundary and non-isolated global maxima.

## Exact Support Solve

For each support, the lowest set index $m$ is the reference strategy. Write every normalized support vector uniquely as

$$
x=e_m+Zy,
$$

where the columns of $Z$ are $e_i-e_m$ for the remaining indices $i\in S\setminus\{m\}$. Eliminating the normalization and common
payoff from $Q_Sx_S=u\mathbf1$ gives

$$
Hy=r,
\qquad
H=Z^TQ_SZ,
\qquad
r=-Z^TQ_Se_m.
$$

For non-reference support indices $i,j$, the exact entries are

$$
H_{ij}=Q_{ij}-Q_{mj}+Q_{mm}-Q_{im},
\qquad
r_i=Q_{mm}-Q_{im}.
$$

The shared fraction-free LDLT implementation factors the integer matrix $H$ once and solves this one right-hand side. It produces a
positive common denominator $D$ and integer numerators for $y$. The candidate is rejected if $H$ is singular, any non-reference
numerator is nonpositive, or

$$
D-\sum_i y_i^{\mathrm{num}}\leq0,
$$

because the final expression is the numerator of the reference probability $x_m$.

For a positive support vector, the implementation calculates the support payoff numerator from row $m$. It then calculates every
outside payoff with the same probability numerators and rejects the support if an outside payoff is larger. No rational candidate
vector is materialized.

## Why Singular Support Systems Do Not Lose The Global Value

The candidate routine follows FracESSA and rejects singular $H$. Completeness for the global value follows by choosing a global
maximizer with inclusion-minimal support. If its $H$ were singular, a nonzero zero-sum support direction $d$ would satisfy

$$
d^TQd=0.
$$

The KKT equal-payoff condition also gives $d^TQx=0$. Hence

$$
(x+td)^TQ(x+td)=x^TQx
$$

for every feasible $t$. Moving until one positive coordinate reaches zero produces a global maximizer with smaller support, a
contradiction. At least one inclusion-minimal global maximizer therefore has a nonsingular support system and is representable by
the exact candidate solve.

## Support Traversal And Pruning

The model retains FracESSA's non-circular traversal. Supports are visited by increasing cardinality; within a cardinality, their
conceptual binary masks increase numerically. When a support produces a full KKT candidate, that support becomes a forbidden subset
from the next cardinality onward. The depth-first generator skips every branch whose partial support already contains a forbidden
support.

A support is stored as an array of

$$
\left\lceil\frac{n}{64}\right\rceil
$$

unsigned 64-bit words. Membership sets or tests one bit. A forbidden support is contained in the current partial support exactly
when, for every word position $j$,

$$
F_j\mathbin{\&}\mathord\sim S_j=0.
$$

Forbidden supports remain grouped by their lowest selected index, so a rule is tested only when that index is added and the partial
support can first contain the whole rule. This is the same pruning point as the original one-word representation.

This pruning does not claim that arbitrary points in a larger face have smaller objective values. It is valid because the search
needs only to detect whether a KKT payoff is nonnegative. Let $p$ and $x$ be KKT candidates with

$$
T=\operatorname{supp}(p)\subseteq\operatorname{supp}(x)=S,
$$

and let $u=p^TQp$ and $v=x^TQx$. KKT for $p$ gives $(Qp)_i\leq u$ for every $i$, hence

$$
x^TQp\leq u.
$$

KKT for $x$ gives $(Qx)_i=v$ for every $i\in S$. Since $p$ is supported inside $S$,

$$
p^TQx=v.
$$

Symmetry of $Q$ gives $p^TQx=x^TQp$, so $v\leq u$. A nonnegative payoff $u$ rejects strict copositivity immediately. Otherwise
$u<0$, and every pruned larger-support KKT payoff also satisfies $v<0$, so none can reject strict copositivity. The pruning may
therefore omit KKT points and even alternative global maximizers, but it cannot change the strict-copositivity decision.

## Complete Decision Flow

```text
validate that A is nonempty, square, and symmetric
Q = -A
candidate_found = false

for each surviving nonempty support S in FracESSA order:
    if |S| <= 3 and the exact direct test on A_S fails: return false
    build the exact reduced KKT system H y = r
    if H is singular: reject S
    solve once with fraction-free LDLT
    recover every support probability numerator
    if any support probability is nonpositive: reject S
    calculate the exact common support payoff
    if any outside strategy has larger payoff: reject S
    candidate_found = true
    if the payoff is nonnegative: return false
    prune every later strict superset of S

if no KKT candidate was found: report an internal error
return true
```

The payoff sign is read directly from its exact integer numerator because the common denominator is positive. Equality rejects strict
copositivity; there is no floating-point tolerance. Cooperative timeout checkpoints in support generation, matrix construction, and
factorization propagate an unresolved timeout rather than a Boolean classification.

## Source Behavior, Mathematical Changes, And Representation Changes

Retained from FracESSA:

- cardinality-first, numeric-mask traversal;
- exact reduced first-order candidate equations;
- strict positivity of every support probability;
- full outside-strategy inequalities;
- delayed activation of accepted-support pruning.

Mathematical and output changes made for Coposit:

- use $Q=-A$ and decide strict copositivity from exact KKT payoff signs;
- reject immediately when the shared exact order-at-most-three test finds a nonpositive principal face;
- stop at the first accepted payoff greater than or equal to zero instead of computing the global value;
- remove ESS, NSS, inertia, local-maximum, and second-order stability classification;
- remove the reduced Schur matrix and recursive copositivity stability test;
- return only whether the resulting minimum of $A$ is strictly positive.

Representation-only changes:

- replace the fixed 63-bit support mask with the shared packed support class using `ceil(n / 64)` words;
- accept Coposit's integer matrix directly instead of clearing rational input denominators;
- reuse Coposit's shared exact fraction-free LDLT factorization;
- retain only the current exact payoff numerator rather than candidate objects, vectors, extended supports, logs, or a best value;
- omit FracESSA's floating-point prefilter, circular-game symmetry path, and public candidate materialization.

## Termination And Mathematical Limits

There are at most $2^n-1$ nonempty supports. Every generated support performs finite exact tests and at most one finite exact linear
solve, so the algorithm is finite in exact arithmetic. Its exponential support space remains the practical limitation.

The packed support representation has no fixed-width dimension limit. Increasing the matrix order adds another 64-bit word whenever
needed. The exponential number of possible supports, exact-system size, memory used by forbidden supports, and recursive traversal
depth remain practical resource limits.

## Known Difficult Inputs

The search is difficult when few small supports satisfy the full KKT conditions, because little superset pruning occurs and many
supports survive until an exact factorization or outside-payoff rejection. Large-support boundary zeros are especially unfavorable:
the maximum of $Q=-A$ is exactly zero, but a representing KKT point may appear only after many smaller supports have been examined.
The shared direct test removes this problem only when a nonpositive witness is already contained in a principal face of order at
most three; it does not help when the first witness has larger support.

Hildebrand circulant boundary forms with minimal-zero support $n-2$, including corpus matrix 10289, are reproducible examples of
this structure. Dense matrices with large arbitrary-precision entries also enlarge the exact systems even when the number of
visited supports is unchanged.
