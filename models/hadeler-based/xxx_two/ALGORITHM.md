# XXX Two

Classification: coposit-created experimental CP/SCP classifier. `xxx_two` is copied from
[`xxx`](../xxx/ALGORITHM.md), but uses a bounded binary64 active-set walk to search cheaply for a KKT support. It always computes an
exact Dickinson certificate at the starting support unless that support is itself confirmed as KKT. A verified KKT endpoint uses only
its stronger KKT-derived intervals. The name means “the second XXX experiment”; it is intentionally temporary while the method is evaluated.

The model combines three ideas:

1. a persistent SAT representation of exactly certified support intervals;
2. a fast floating-point walk through candidate KKT supports; and
3. an exact SAT-Halfspace-Rays Dickinson certificate at each non-KKT seed.

Floating-point values never classify a matrix and never remove a support. They choose where exact work is performed.

## Optimization Problem

For a symmetric matrix $A\in\mathbb Z^{n\times n}$, consider

$$
\min\{x^TAx:x\geq0,\ \mathbf1^Tx=1\}.
$$

The minimum is negative exactly when $A$ is not copositive, zero exactly when $A$ is copositive but not strictly copositive, and
positive exactly when $A$ is strictly copositive.

For a nonempty support $S\subseteq[n]$, a stationary point on the affine hull of face $S$ satisfies

$$
A_{SS}x_S=\lambda\mathbf1,
\qquad
\mathbf1^Tx_S=1.
$$

After extending $x_S$ by zeros outside $S$, it is a KKT point of the full simplex when

$$
x_S\geq0,
\qquad
(Ax)_j\geq\lambda\quad(j\notin S).
$$

The floating walk tries to approach such a support. It does not prove that the endpoint is a KKT support.

## SAT Support Family And Seed Order

One CaDiCaL instance represents the nonempty supports not yet removed by exact certificates. A sorting network supplies exact
cardinality assumptions. The required runtime parameter selects one of two seed schedules:

- `alternating` maintains the lowest and highest cardinalities that may still contain an open support and alternates one path from
  each side,

$$
k_{\rm low},k_{\rm high},k_{\rm low},k_{\rm high},\ldots.
$$

- `ascending` exhausts open seeds from the smallest cardinality before advancing:

$$
1,2,3,\ldots,n.
$$

The selected side remains at the same cardinality while that layer contains open supports. An empty layer is skipped. The schedule
changes only which SAT-selected support starts the next path; path construction and exact certification are identical.

An exact Dickinson interval

$$
[L,U]=\{J:L\subseteq J\subseteq U\}
$$

is represented by one blocking clause. A cardinality literal makes a finite-upper interval inactive after its last relevant layer.
SAT clauses persist across the complete run.

## Floating-Point Walk

The integer matrix is converted once to binary64 using one common power-of-two scale determined by its largest absolute entry. This
preserves signs and relative values that remain representable; very small entries may underflow. Such numerical loss can change the
path, but it cannot change the result because only exact endpoint calculations add certificates.

For the current support $S=\{i_1,\ldots,i_k\}$, choose $i_k$ as reference and write

$$
x=e_{i_k}+Zy,
\qquad
Z=[e_{i_1}-e_{i_k},\ldots,e_{i_{k-1}}-e_{i_k}].
$$

The stationary equations reduce to

$$
Hy=r,
\qquad
H=Z^TA_{SS}Z,
\qquad
r=-Z^TA_{SS}e_{i_k}.
$$

The model solves this symmetric system with a pivoted binary64 Bunch–Kaufman $LDL^T$ factorization adapted from FracESSA's fast
candidate filter and LAPACK's `DSYTF2`/`DSYTRS` path.

The floating tolerance is

$$
256\,\varepsilon_{\rm dbl}(n+1)
\max\bigl(1,|\lambda|,\max_i|x_i|,\max_j|(Ax)_j|\bigr).
$$

It is only a path-selection tolerance, not a mathematical threshold.

The preferred successor order is:

1. remove used coordinates whose values are below the negative tolerance, most negative first;
2. otherwise remove coordinates within the zero tolerance, first together and then individually;
3. otherwise add unused indices violating $(Ax)_j\geq\lambda$, most violated first;
4. otherwise propose a floating KKT candidate and verify it exactly.

Ties use the original matrix index. A candidate successor is unavailable if it already occurred in the current walk or is covered by
SAT. The model may try the next candidate proposed at the current support, but it never returns to an earlier support.

## Bounded Non-Backtracking Rule

Starting from the SAT-selected seed, the model performs at most $n$ support-to-support moves, where $n$ is the matrix order. The seed
is step zero. The walk stops earlier when

- the floating solve is inconclusive;
- floating arithmetic proposes a negative value;
- exact arithmetic confirms a proposed KKT point;
- every preferred successor at the current support is unavailable; or
- the external timeout is reached.

There is no backtracking. Supports visited during ordinary floating steps are kept only in the temporary current-path set to prevent a
cycle. They are discarded when the path ends and never enter SAT or a global path cache.

### Critical-point exception

When floating arithmetic proposes a KKT point, the model solves that face system exactly. If exact arithmetic confirms the KKT
conditions, the path ends. If it disproves them, the support is a **critical point**: floating arithmetic has lost information needed
to choose the next move. From that support onward, the same forward path remains in exact arithmetic. It never returns to floating
arithmetic and still cannot exceed the original $n$-move limit.

At every exact step, the model buffers every valid exact KKT-derived interval already established by that calculation:

- a feasible nonnegative KKT point with nonnegative payoff supplies the upward interval from its positive support to $[n]$;
- a feasible stationary point whose reduced Hessian is positive semidefinite and whose payoff is nonnegative supplies the downward
  interval from the empty support to the current support.

The buffered intervals enter SAT together only after the forward path stops. They therefore cannot block the exact continuation that
produced them. If the exact point is not yet KKT, its exact signs choose the next forward successor. If that successor was already
unavailable before this path began, or belongs to the current path, the path stops; it does not backtrack.

## Exact Seed Dickinson Certificate

Let $S_0$ be the seed. After a path that does not finish at $S_0$ as a verified KKT point, the model calculates an exact
SAT-Halfspace-Rays Dickinson certificate for $S_0$. It never calculates this ordinary certificate for a verified KKT endpoint: the
KKT calculation already supplies the maximal upward interval and, when its reduced Hessian is positive semidefinite, the maximal
downward interval. A step-limit, blocked-successor, negative-candidate, or numerically inconclusive stop therefore triggers only the
seed calculation and never an otherwise unused exact factorization of the final support.

For a nonsingular principal matrix $A_{SS}$, the exact engine starts from

$$
A_{SS}z=\mathbf1.
$$

It reuses the fraction-free $LDL^T$ factorization to sweep coordinate directions and up to two complementary combined rays. Among the
exact candidates it prefers a larger upper support and then a wider interval. For a singular principal matrix, it obtains an exact
nullspace vector, checks both orientations when necessary, and retains the orientation with the larger upper support.

For the selected embedded vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's certificate removes every support in $[L(u),U(u)]$. The calculation uses arbitrary-precision integers throughout. A
nonnegative vector with negative quadratic value rejects copositivity immediately; a nonnegative zero records failure of strict
copositivity while ordinary copositivity remains open.

Each exact certificate contains the support from which it was constructed. A verified KKT seed is covered by its KKT intervals;
otherwise the seed certificate removes it. Every completed path therefore makes proof-level progress. If an exact certificate failed
to contain its own support, the model would report an explicit invariant error rather than silently continue.

## Combined CP/SCP Classification

In `both` mode, the model begins with both claims provisionally true.

- An exact negative KKT or seed-certificate witness rejects CP and SCP immediately.
- An exact zero KKT or seed-certificate witness rejects SCP and releases zero-safe ordinary intervals.
- Positive Dickinson intervals are valid for both claims.
- Complete exact SAT coverage proves every still-live claim.

Timeouts and invariant failures remain unresolved errors; neither is converted to `false`. A floating failure ends the current walk
and triggers only its exact seed calculation, except that a floating KKT proposal rejected exactly activates exact continuation.

## Diagnostics

With diagnostics enabled, `xxx_two_path_seed` records the selected seed and `xxx_two_path_step` records every floating move. A
completed walk records its outcome, final support, and number of moves. Every exact seed calculation records
`xxx_two_path_certificate` with `role=seed`. Verified KKT endpoints do not emit an ordinary path-certificate event. A rejected floating
KKT proposal records `xxx_two_critical_point`; every exact stationary calculation records `xxx_two_exact_kkt`. No backtracking event
exists in this model.

## Origin And References

The interval certificate follows Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications*
569 (2019), 15–37, especially Theorem 4.6. The SAT interval representation, cardinality retirement, Halfspace-Rays certificate engine,
and alternating scheduler come from Coposit's `sat_halfspace_rays_dickinson` and `xxx` experiments. The floating symmetric solve derives
from FracESSA's fast candidate filter. The adapted Bunch–Kaufman kernel retains the LAPACK copyright notice in `solver.cpp`.

The bounded non-backtracking walk and two-endpoint policy are Coposit experiments, not claims about Dickinson's published algorithm.

## Known Difficult Inputs

- A numerically inconclusive floating factorization stops the path; exact continuation is reserved for a floating KKT proposal that
  can be checked and rejected exactly.
- Once a false floating KKT proposal activates exact mode, every remaining step is more expensive.
- Extreme entry ranges may underflow during binary64 conversion and lead to an unhelpful endpoint.
- A path blocked by an existing SAT interval stops instead of searching another branch from an earlier support.
- Exact seed certificates can still be expensive for matrices with very large entries.
