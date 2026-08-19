# XXX Exact KKT Search: Implementation Record

## Status

`xxx` is implemented as an exact experimental CP/SCP classifier. The authoritative algorithm description is
[`models/hadeler-based/xxx/ALGORITHM.md`](../models/hadeler-based/xxx/ALGORITHM.md).

The original proposal used KKT blocks alone, guarded tried-support clauses, two SAT views, and an explicit stalled-search error.
Smoke testing showed that design was too weak: a six-dimensional boundary example produced useful ordinary Dickinson intervals at
its visited edges, but the model discarded them and stalled. The implemented design keeps those intervals and needs none of that
extra state.

## Settled Design

The model is an isolated copy of `sat_halfspace_rays_dickinson` with an exact KKT path added around its existing certificate engine.

The global invariants are:

1. There is one persistent CaDiCaL instance.
2. Its clauses are mathematical proof intervals only.
3. Cardinality layers are exhausted in the order $1,n,2,n-1,\ldots$.
4. Every processed support runs the complete halfspace-rays optimizer and buffers its ordinary Dickinson interval for the current
   path.
5. The KKT calculation may add stronger downward or full-ceiling intervals.
6. Every proposed KKT successor is rejected when it is path-local visited or when a complete-assignment SAT query shows that an
   earlier committed path covers it. The current buffer is not in SAT and therefore cannot block its own path.
7. A path ends at a KKT point or when no proposed successor remains open. In either case it commits its whole interval buffer before
   SAT supplies the next uncovered seed.
8. There is no arbitrary path budget: a path ends exactly when no uncovered successor remains.
9. There is no tried-support clause, second SAT view, path memoization, arbitrary-neighbor search, stalled result, or hidden fallback.
10. All mathematical decisions use exact integer arithmetic.

## Why The Global Search Is Complete

At a processed support $I$, the retained halfspace-rays engine produces a vector $u$ with

$$
L(u)\subseteq I\subseteq U(u).
$$

Therefore its Dickinson interval $[L(u),U(u)]$ contains $I$. A path-local visited set prevents cycles before the batch is committed.
After the path ends, its intervals cover every processed support, so neither SAT nor a later path can select them again. Every active
cardinality and the complete Boolean lattice are finite. Only an external timeout or resource limit can prevent termination.

This is not a fallback to a different model. The ordinary Dickinson interval is the mandatory proof produced at each support inside
`xxx`; the KKT calculations are additional attempts to cover more of the lattice.

## Cardinality Scheduling

For layer number $r=0,1,\ldots,n-1$, the active cardinality is

$$
k_r=
\begin{cases}
r/2+1, & r\text{ even},\\
n-\lfloor r/2\rfloor, & r\text{ odd}.
\end{cases}
$$

This gives $1,n,2,n-1,3,n-2,\ldots$. SAT remains at $k_r$ until an exact-cardinality query is unsatisfiable.

The persistent Dickinson clause for $[L,U]$ is automatically irrelevant outside $|L|\leq k\leq|U|$:

- if $k<|L|$, a support of size $k$ cannot contain $L$;
- if $k>|U|$, the stored cardinality-network literal satisfies the clause.

Consequently, completing a low or high layer does not require scanning or deleting certificates. Their clauses are already satisfied
when the scheduler moves outside their useful range.

## Mandatory Dickinson Step

At each support $I$:

1. Factor $A_I$ exactly.
2. If it is nonsingular, solve the all-ones system.
3. Reuse the same factorization for all unit right-hand sides.
4. Run exact coordinate breakpoint sweeps, maximizing $|U|$ first and $|U|-|L|$ second.
5. After a stalled coordinate pass, retain the adaptive bounded shortlist and test at most two synthesized rays.
6. If $A_I$ is singular, use the established nullspace orientation rule.
7. Stop on an exact negative witness and record an exact zero witness for strict copositivity.
8. Buffer the resulting interval and commit it with the complete path.

No part of this engine was replaced by the KKT code.

## Exact KKT Step

For current support $S$ with last reference coordinate $m$, write $x_S=e_m+Zy$. Solve

$$
Hy=r,
$$

where

$$
H=Z^TA_{SS}Z,
\qquad
r=-Z^TA_{SS}e_m.
$$

The shared fraction-free symmetric $LDL^T$ factorization supplies rank, inertia, a nonsingular or consistent-singular solution, and
nullspace directions. The implementation reconstructs an integer numerator vector $X$, positive denominator $D$, payoff numerator
$P$, and outside residuals $G_j$.

Exact outcomes:

- $X\geq0$ and $P<0$: negative witness, so the matrix is not copositive;
- $X\geq0$ and $P=0$: zero witness, so strict copositivity is false;
- $H\succeq0$, the system is consistent, and $P\geq0$: add the downward interval $[\varnothing,S]$;
- $X\geq0$, $P\geq0$, and every $G_j\geq0$: add the full-ceiling interval $[\operatorname{supp}_+(X),[n]]$.

Zero-valued intervals are valid for ordinary copositivity but not strict copositivity. In combined mode they remain pending while the
strict predicate is still true and are released after an exact zero witness decides it false.

## Active-Set Path

For a nonsingular KKT system:

1. remove the most negative used coordinate;
2. otherwise remove all exact zero coordinates;
3. otherwise add the most violated unused coordinate;
4. otherwise end the path.

For a singular system, exact nullspace directions propose a boundary support. Consistent systems try both flat orientations;
inconsistent systems orient a direction toward decreasing objective. Tied boundary coordinates are removed together.

These moves choose where to search next; they prove nothing themselves. Each proposed support receives its own mandatory Dickinson
calculation before it can affect global coverage. A path may end without finding a KKT point; its exact buffered intervals are still
committed before SAT chooses another seed.

## Verification Obligations

The focused test must retain the inherited halfspace-rays checks and additionally verify:

- the exact $1,n,2,n-1,\ldots$ layer order;
- agreement with the complete order-two CP/SCP criterion;
- a visited support exposes its optimized ordinary Dickinson interval; and
- current-path intervals do not block a walk from reaching its KKT point;
- a path with no open proposed successor commits all exact intervals accumulated before it stopped; and
- the six-dimensional boundary regression completes as copositive but not strictly copositive instead of stalling.

After a mathematical or control-flow change, run the focused test, the Smoke set in combined mode with preprocessing, the complete
Release test suite, and SQLite integrity checking.

## Deliberately Deferred

No floating-point KKT filter, beam search, random multistart, generic nonlinear optimizer, affine Phase-I search, MaxSAT proximity
objective, or multithreaded SAT search is scaffolded. Add one only after diagnostics show that the exact model's search policy is
useful and the missing feature addresses a measured bottleneck.
