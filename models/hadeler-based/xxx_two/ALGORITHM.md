# XXX Two

Classification: coposit-created experimental CP/SCP classifier. `xxx_two` is copied from [`xxx`](../xxx/ALGORITHM.md), but replaces
the exact active-set walk with a binary64 floating-point walk. Exact arithmetic is used when the floating walk proposes either a
negative witness or a KKT point. The name means “the second XXX experiment”; it is intentionally temporary while the method is evaluated.

The model combines three ideas:

1. a persistent SAT representation of exactly proved support intervals;
2. a fast floating-point active-set walk that proposes a KKT support;
3. an exact fraction-free verification that alone may classify the matrix or add SAT intervals.

Floating-point values never prove copositivity, strict copositivity, non-copositivity, positive semidefiniteness, or support coverage.
An inconclusive floating solve, a failed exact proposal, or a walk with no admissible successor causes backtracking. There is no
exact-walk fallback.

## Optimization Problem

For a symmetric matrix $A\in\mathbb Z^{n\times n}$, consider

$$
\min\{q(x)=x^TAx:x\geq0,\ \mathbf1^Tx=1\}.
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

At such a point, $q(x)=\lambda$.

## SAT Support Family

One CaDiCaL instance represents the nonempty supports not yet removed by exact certificates. A sorting network supplies exact
cardinality assumptions. The scheduler maintains the lowest and highest cardinality that may still contain an open support. It runs
one complete walk from one SAT-selected support at the lower cardinality, then one complete walk from one support at the upper
cardinality, and repeats. An empty layer is skipped toward the centre. Thus walks, rather than whole layers, are interleaved as

$$
k_{\mathrm{low}},k_{\mathrm{high}},k_{\mathrm{low}},k_{\mathrm{high}},\ldots.
$$

A side remains at the same cardinality while that layer still contains open supports, but yields to the opposite side after every
walk. Certificates created by either walk are therefore available before the next seed is selected from the other side.

An interval $[L,U]=\{J:L\subseteq J\subseteq U\}$ is represented by one blocking clause. A cardinality literal retires a finite-upper
interval after its last relevant layer. SAT clauses are persistent across all layers.

Only exactly verified KKT intervals enter this SAT instance. Supports remembered as parts of earlier floating paths are not proof
clauses and do not contribute to a positive classification.

## Floating-Point Path

The complete integer matrix is converted once to binary64 using one common power-of-two scale determined by its largest absolute
entry. This preserves signs and relative values that remain representable; very small entries may underflow. Such loss can affect
the proposed path but cannot affect a certificate because the terminal point is recomputed exactly.

For the current support $S=\{i_1,\ldots,i_k\}$, choose $i_k$ as reference. Write every normalized point on the affine hull as

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

The model forms $H$ in binary64, scales $H$ and $r$ by the largest absolute entry of $H$, and solves the symmetric system with a
pivoted Bunch–Kaufman $LDL^T$ factorization adapted from FracESSA's fast candidate filter and LAPACK's `DSYTF2`/`DSYTRS` path.

The floating tolerance is

$$
256\,\varepsilon_{\rm dbl}(n+1)
\max\bigl(1,|\lambda|,\max_i|x_i|,\max_j|(Ax)_j|\bigr).
$$

It is only a pivot-selection tolerance. It is not a mathematical decision threshold.

The next support is selected as follows:

1. If some used coordinate is below the negative tolerance, try removing coordinates from most negative upward.
2. Otherwise, if some used coordinate is within the zero tolerance, first try dropping all such coordinates, then try them
   individually.
3. Otherwise, if some unused coordinate violates $(Ax)_j\geq\lambda$, try adding indices from most violated upward.
4. Otherwise, treat the support as a proposed terminal KKT support and verify it exactly.

Each list is deterministic, with the original index as final tie-breaker.

Before choosing a successor, a floating candidate with nonnegative coordinates within tolerance and payoff below negative tolerance
is treated as a possible counterexample. The same face system is then solved exactly. If its exact normalized solution is
nonnegative and has negative payoff, the matrix is not copositive and the model stops immediately. A failed proposal changes
nothing: it adds no interval and the floating path continues normally.

## Path Memory And Alternative Pivots

The model performs a depth-first search with an explicit stack. Each stack frame stores one support, its ordered preferred successors,
and the next successor still to try. Every support visited anywhere in that search is remembered, including supports on branches that
later become dead ends. This memory is separate from SAT.

When choosing a successor, candidates are tried in their mathematical priority order. A candidate is rejected when it

- already occurred on the current path;
- occurred on an earlier completed path; or
- is covered by an exact SAT interval.

The search tries the next candidate in the current frame. If the frame has no candidate left, it pops that frame and resumes at its
parent's next candidate. Thus a collision does not terminate the search: it causes ordinary depth-first backtracking. The floating
search stops only after finding an exactly verified KKT point or after popping the root because every preferred route from the seed
has been exhausted.

When a search finishes, all supports visited by it—including dead ends—join the global forbidden-path memory. This memory is partitioned
by support cardinality, so a lookup examines only supports of the candidate's size. Once SAT proves that a cardinality layer contains no
uncovered support, that bucket is discarded: SAT itself will reject every later attempt to enter the exhausted layer. If SAT nevertheless
selects a remembered support as a new uncovered seed before its layer is exhausted, the model does not skip that unresolved support: it
calculates the exact Halfspace-Rays certificate for that seed immediately.

An earlier path does not logically prove that its supports are copositive. Consequently, path memory may redirect search but may
never be inserted into the proof SAT instance.

## Exact Terminal Verification

When the floating walk reports no negative used coordinate, no zero used coordinate, and no violated unused coordinate, the model
rebuilds the reduced KKT system from the original arbitrary-precision integers. Coposit's fraction-free symmetric $LDL^T$
factorization computes:

- exact consistency and nullity;
- an exact normalized stationary vector;
- the exact payoff sign;
- every exact outside KKT inequality; and
- exact positive semidefiniteness of $H$.

The proposal is accepted only when the exact vector is feasible and satisfies every full-simplex KKT inequality. If floating point
proposed a terminal point that fails this test, that support is called a **critical point**. The model reuses the exact result to
select the first admissible successor by the same pivot rules above. This correction requires no second exact factorization.

After the first critical point in a path, every later support in that path is solved exactly; the model does not return to floating
arithmetic. Exact mode ends when the path reaches an exact KKT point, finds an exact negative witness, exhausts every admissible
successor, or is interrupted by its external resource limit. Backtracking may choose another stored branch, but its supports are
also processed exactly because the path has already entered critical mode.

An exact negative payoff is a nonnegative negative witness, so the matrix is not copositive. An exact zero payoff proves that strict
copositivity is false; ordinary copositivity remains open.

## The Two Exact KKT Intervals

Let $P=\operatorname{supp}(x)$ be the positive support of the exactly verified KKT vector.

### Upward interval

Every nonnegative KKT point with nonnegative payoff supplies the Dickinson interval

$$
[P,[n]].
$$

It excludes those supports as inclusion-minimal obstructions. If the payoff is positive, the interval is valid for CP and SCP. If the
payoff is zero, it is retained for ordinary copositivity after strict copositivity has been rejected.

### Downward interval

If the exact reduced Hessian satisfies $H\succeq0$ and the exact payoff is nonnegative, the stationary point is a global minimum on
face $S$. Therefore $A_{SS}$ and every principal submatrix indexed by $J\subseteq S$ are copositive. This supplies

$$
[\varnothing,S].
$$

The interval also proves strict copositivity only when the payoff is positive.

The upward interval is always added for an accepted nonnegative KKT point. The downward interval is added only when the exact
positive-semidefiniteness condition holds. These are the only KKT-derived intervals produced by `xxx_two`.

## Closing The Starting Support

After a verified KKT point or complete backtracking exhaustion, the model asks SAT whether the original path seed is now covered. If
it remains open, the model runs the inherited exact SAT-Halfspace-Rays Dickinson calculation once on that seed and installs its
interval. A Dickinson interval produced from a support contains that support, so this closes the seed. The same direct calculation
is used when SAT selects a support already present in the forbidden-path memory. If the exact seed calculation gives a negative
witness, the matrix is rejected immediately; if its interval somehow fails to cover the seed, the model reports an explicit error.

The extra Halfspace-Rays calculation is therefore conditional. It is not paid when the KKT intervals already contain the starting
support, and it is never run on the floating intermediate supports.

## Combined CP/SCP Classification

In `both` mode, the model begins with both claims provisionally true.

- A negative exactly verified intermediate or KKT value rejects CP and SCP immediately.
- A zero exact KKT value rejects SCP and releases zero-safe ordinary intervals.
- Positive intervals are valid for both claims.
- Complete exact SAT coverage proves every still-live claim.

Timeouts and impossible exact-certificate invariant failures remain unresolved errors; neither is converted to `false`. Floating
numerical failures, failed exact terminal proposals, and blocked branches cause backtracking, not a Boolean decision.

## Diagnostics

With diagnostics enabled, every chosen floating transition is recorded as `xxx_two_path_step`, and every exhausted frame as
`xxx_two_path_backtrack`. A completed search records its identifier, number of distinct visited supports, and either its exact terminal
support or root exhaustion. Exact terminal candidates are recorded separately as `xxx_two_exact_kkt`; a conditional exact certificate
on the original seed records `xxx_two_seed_certificate`. An exactly confirmed negative candidate before a terminal KKT support records
`xxx_two_terminal outcome=intermediate_negative`.

## Origin And References

The interval certificate follows Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications*
569 (2019), 15–37, especially Theorem 4.6. The SAT interval representation and alternating layer scheduler come from Coposit's
`sat_halfspace_rays_dickinson` and `xxx` experiments. The reduced KKT equations and floating symmetric solve derive from FracESSA's
fast candidate filter. The adapted Bunch–Kaufman kernel retains the LAPACK copyright notice in `solver.cpp`.

The floating walk, depth-first backtracking, global forbidden-path memory, and exactly verified proposal policy are Coposit experiments,
not claims about Dickinson's published algorithm.

## Known Difficult Inputs

- A nearly singular reduced KKT system may be numerically inconclusive even when the exact system is regular, forcing backtracking.
- Extreme entry ranges may underflow during the one-time binary64 conversion and lead to a failed exact terminal proposal.
- A genuinely singular stationary family is not traversed in floating point; that branch is abandoned and may force exact seed work.
- Different active-set paths can merge. Path memory and backtracking avoid repeating them, but extensive merging can exhaust many
  alternative branches before the seed is certified.
- KKT points with indefinite face restrictions supply only upward intervals, which may leave a large part of the support lattice open.
- Exact terminal verification can still be expensive for matrices with very large integer entries, although it occurs once per
  completed path rather than once per visited support.
