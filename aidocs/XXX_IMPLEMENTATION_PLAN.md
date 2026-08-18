# XXX Exact KKT Search: Implementation Plan

## Status

This is the implementation plan for the experimental model `xxx`. It is not a description of finished behavior.

The first implementation will use exact integer arithmetic throughout the KKT search. Floating-point acceleration is deliberately
postponed until the exact algorithm shows which successes, failures, and costs belong to the search policy itself.

Items under **Open Decisions** are deliberately unresolved and will be refined before or during the smallest focused implementation.

## Goal

For a symmetric matrix $A\in\mathbb S^n$, `xxx` searches for useful KKT points of the standard quadratic problem

$$
\min_{x\in\Delta_n}x^TAx,
\qquad
\Delta_n=\{x\in\mathbb R_+^n:\mathbf1^Tx=1\}.
$$

The search combines two exact mechanisms:

1. SAT represents the supports not yet covered by proved blocks.
2. An exact active-set path solves the face-KKT equations and adds or removes support indices.

Every sign, pivot, witness, and certificate is exact. The support heuristic may still make a poor choice or fail to complete the
coverage, but a completed classification cannot be wrong because of numerical error.

## Why Exact Arithmetic Comes First

A floating implementation would mix two different failure sources:

- the KKT/SAT search policy may be ineffective; or
- the approximate solve may choose the wrong sign, pivot, or support.

The exact model separates them. If it finds valuable supports but spends too much time in linear algebra, the same control flow can
later receive a binary64 front end. If it does not find valuable supports, changing the number representation cannot repair the
search policy.

## Scope Of The First Version

The first version does **not** traverse supports in cardinality order. There is no loop over $k=1,2,\ldots,n$.

Instead, SAT repeatedly returns an uncovered support. Seed selection alternates between:

- minimum available cardinality; and
- maximum available cardinality.

Small seeds seek KKT points with small lower endpoints and therefore potentially large upward blocks. Large seeds seek large convex
faces and therefore potentially large downward blocks.

The first unregistered prototype has no ordinary Dickinson fallback. If the exact KKT search exhausts its bounded paths while proof
supports remain uncovered, its internal attempt result is `stalled`.

This prototype must not be registered as a normal Coposit model: the public `solve` and `classify` contracts have no `unresolved`
return value. Before registration, the model needs a proof-complete completion policy. The minimal recommended policy preserves the
absence of cardinality traversal: ask proof SAT for one still-uncovered support, run one ordinary exact Dickinson step on precisely
that support, add the resulting interval, and return to KKT-guided search. Each completion step covers its selected support, so the
finite Boolean lattice gives termination without introducing a loop over $k=1,\ldots,n$.

The first version excludes:

- floating-point arithmetic;
- generic nonlinear optimization;
- Frank--Wolfe, projected-gradient, SMO, or random multistart search;
- MaxSAT nearest-support optimization;
- beam search and unrestricted branching;
- a general-purpose affine-family optimizer inside a higher-dimensional kernel;
- ordinary bounded Dickinson intervals during the KKT-guided phase (the recommended proof-complete promotion would use them only
  after that phase stalls); and
- a hidden cardinality traversal after the heuristic stalls.

## Starting Model And Source Reuse

The model directory already exists as an independent copy of `sat_halfspace_rays_dickinson`:

```text
models/hadeler-based/xxx/
```

That copy is only an implementation container. Its increasing-cardinality traversal and halfspace-ray search are not part of `xxx`
and will be removed.

The exact KKT engine will reuse Coposit's existing shared factorization rather than copying or rewriting it:

- `cpp/include/coposit/fraction_free_ldlt.hpp` already contains the reusable fraction-free symmetric $LDL^T$ workspace, exact rank
  and inertia, nonsingular and consistent-singular solves, one nullspace vector, and a complete nullspace basis;
- FracESSA's `cpp/src/exact_candidate_solver.cpp` remains the read-only source for constructing the reduced KKT system and interpreting
  its exact scaled solution; and
- FracESSA's `cpp/include/fracessa/fraction_free_ldlt_kkt.hpp` remains read-only historical evidence for the optimized elimination
  choices already present in Coposit.

FracESSA remains read-only. The model-local code will contain only the reduced-system construction, KKT interpretation, and path
policy. The shared Coposit factorization already retains the required FLINT/LGPL attribution.

## Face-KKT Mathematics

For a support $S\subseteq[n]$, let $A_{SS}$ be the principal matrix on $S$. A normalized stationary point on the affine hull of face
$S$ satisfies

$$
A_{SS}x_S=\lambda\mathbf1,
\qquad
\mathbf1^Tx_S=1.
$$

When $x_S>0$, it lies in the relative interior of face $S$. After extending $x$ by zeros outside $S$, it is a full KKT point of the
simplex when

$$
(Ax)_j-\lambda\geq0
\qquad(j\notin S).
$$

The objective value is

$$
\lambda=x^TAx.
$$

For minimization, a negative outside residual means that adding a little mass to that coordinate gives a first-order decrease.

## Reduced Symmetric KKT System

The implementation will not solve the bordered $(k+1)\times(k+1)$ system and will not require $A_{SS}^{-1}$.

Let $k=|S|$ and choose a reference coordinate $m\in S$. Let $Z\in\mathbb R^{k\times(k-1)}$ have columns $e_i-e_m$ for the other
coordinates of $S$. Every normalized vector on the affine hull of the face has the unique form

$$
x_S=e_m+Zy.
$$

The equal-payoff equations reduce exactly to

$$
Hy=r,
$$

where

$$
H=Z^TA_{SS}Z,
\qquad
r=-Z^TA_{SS}e_m.
$$

The entries are

$$
H_{ij}=A_{ij}-A_{im}-A_{mj}+A_{mm},
\qquad
r_i=A_{mm}-A_{im}.
$$

This system is preferable because:

- $H$ is symmetric;
- its order is $k-1$ instead of $k+1$;
- it remains valid when $A_{SS}$ is singular; and
- $H$ is nonsingular exactly when the bordered KKT matrix is nonsingular.

For $k=1$, normalization fixes $x_S=(1)$ and no factorization is required.

## The Hessian And Convexity Test

The matrix $H$ represents the quadratic form on zero-sum directions of face $S$; the Hessian with respect to $y$ is $2H$. For every
tangent direction

$$
d\in T_S:=\{d\in\mathbb R^S:\mathbf1^Td=0\},
$$

there is a vector $v$ with $d=Zv$, and

$$
d^TA_{SS}d=v^THv.
$$

Therefore $q$ is convex on face $S$ exactly when $H$ is positive semidefinite.

The fraction-free $LDL^T$ factorization already performs congruence transformations. Its inertia gives the numbers of positive,
negative, and zero directions. Consequently, the convexity decision requires no second matrix and no second factorization:

$$
H\succeq0
\quad\Longleftrightarrow\quad
\text{negative inertia count}=0.
$$

For nonsingular $H$, zero negative directions means that $H$ is positive definite. For singular $H$, the factorization must also
report its zero count rather than returning immediately with undefined inertia.

## Reusing Coposit's Exact $LDL^T$

Coposit's existing workspace performs the required in-place symmetric Bareiss elimination. Its useful optimized properties are:

- only the lower triangle is read and overwritten;
- matrices and right-hand sides are reused between supports;
- arithmetic remains integral throughout the hot loop;
- small FLINT integers use a specialized two-limb update;
- exact symmetric coordinate operations repair zero diagonal pivots in nonsingular indefinite systems;
- those operations are recorded and undone on the final solution;
- the solution uses one common positive denominator; and
- pivot signs provide exact inertia.

`xxx` will call this workspace directly. It will not create a model-local factorization copy. If diagnostics need the individual
inertia counts, the smallest shared change is to expose the already-maintained counts through read-only accessors.

### Changes required for minimization and navigation

FracESSA's exact candidate solver is a maximization filter. `xxx` needs the following changes:

1. Reverse the outside comparison. FracESSA requires unused payoff at most the support payoff; `xxx` requires unused value at least
   the support value.
2. Do not return at the first nonpositive probability. Retain every exact numerator so the path can rank drop pivots.
3. Do not return at the first outside violation. Calculate and rank all exact violations so covered choices can be skipped.
4. Use positive semidefiniteness of $H$ for the face-minimum block, rather than negative definiteness for an ESS/local maximum.
5. Return explicit KKT/search outcomes instead of one candidate-filter Boolean.
6. Remove candidate serialization, public rational objects, ESS stability reduction, outside-best-reply masks, and Bomze's final
   reduced matrix.

### Singular factorization status

The historical FracESSA solve returns immediately when the remaining active block is zero. Coposit's shared factorization already
retains that singular state and provides `rank()`, `is_positive_semidefinite()`, `solve_consistent_inplace()`,
`one_nullspace_vector()`, and `nullspace_basis()`. The KKT layer will report:

- positive inertia count;
- negative inertia count;
- zero inertia count;
- rank and nullity;
- whether the transformed right-hand side is consistent; and
- when consistent, one exact particular solution obtained by setting free transformed coordinates to zero.

When the trailing Schur block is zero, the remaining transformed right-hand-side entries decide consistency. No general-purpose
rational elimination is needed.

`solve_consistent_inplace()` leaves its right-hand-side buffer unspecified after an inconsistent result. Once factorization reports
singularity, `xxx` therefore preserves one copy of the original $r$ before calling it. That copy is reused for the exact $v^Tr$
orientation test; no second reduced system is built.

The singular extension gives three immediate benefits:

1. $H\succeq0$ is recognized exactly even when $H$ is singular.
2. A consistent singular system provides an exact affine stationary point, which may already prove a downward block.
3. A nullspace direction identifies exact boundary moves to proper sub-supports, so singularity does not end the local path.

The first version will not add a general exact linear-programming solver over a higher-dimensional affine family. It will use the
nullspace for exact boundary navigation and use an exact particular solution when that solution is feasible. A single arbitrary
particular solution is enough for the downward convexity proof, but its coordinate signs do not decide whether another member of the
affine family is feasible.

## Singular Systems Reduce To The Boundary

A singular reduced Hessian is not a reason to stop. Let $v\ne0$ satisfy $Hv=0$ and put

$$
d=Zv.
$$

Then $\mathbf1^Td=0$, so $d$ is a tangent direction of the normalized face. Moreover, $Z^TA_{SS}d=0$, hence

$$
A_{SS}d=\alpha\mathbf1
$$

for one exact scalar $\alpha$. Along every normalized point $x$ on the face,

$$
q(x+td)=q(x)+2t\alpha.
$$

There is no quadratic term because $d^TA_{SS}d=0$. A nonzero zero-sum vector has both positive and negative coordinates, so the line
$x+td$ reaches the boundary in both directions. Exact ratio tests find those endpoints. For example, in the positive direction,

$$
t_+=\min_{d_i<0}\frac{x_i}{-d_i}.
$$

At $x+t_+d$, at least one coordinate is zero and no coordinate is negative. All coordinates tied at the limiting ratio can be
removed together.

This gives the following exact reduction theorem:

> If $H$ is singular, the minimum of $q$ over face $S$ is attained on the boundary of that face.

If $\alpha\ne0$, orient $d$ so the objective decreases until the boundary. If $\alpha=0$, the objective is constant along the line,
so either endpoint has the same value. Consequently, a singular face has no isolated minimum that exists only in its relative
interior.

The theorem does **not** identify one globally redundant matrix index. The null direction may involve many coordinates, and the
coordinate reaching zero depends on the current feasible point. Removing one arbitrary coordinate would therefore be incomplete.
For local heuristic navigation, `xxx` tries deterministic exact endpoints in order until it finds one whose resulting support is
uncovered and unvisited, then follows only that support. A future proof-complete boundary reduction would have to cover every facet
that can be reached as the first endpoint.

Consistency sharpens the construction:

- If $Hy=r$ is consistent, then $v^Tr=0$ for every $v\in\ker H$, hence $\alpha=0$. Any feasible stationary member can be moved to a
  boundary stationary member with the same payoff. Repeating this operation eventually reaches a proper support whose reduced KKT
  system is nonsingular.
- If $Hy=r$ is inconsistent, some nullspace basis vector has $v^Tr\ne0$. Since $\alpha=-v^Tr$, its sign selects a strict descent
  direction. The path moves
  a simple feasible reference point, such as the face barycenter, to the corresponding boundary and continues there.

For a consistent system, one vector from `one_nullspace_vector()` is enough. Only an inconsistent system may require scanning
`nullspace_basis()` until finding $v^Tr\ne0$. The recovered $v$ is then mapped through $Z$ into the original face coordinates. The
implementation must not interpret the location of a zero transformed pivot as the matrix index to delete, because the symmetric
coordinate operations may mix several original coordinates.

### Memory change

FracESSA caches reduced entries by `(reference,row,column)` in dense $O(n^3)$ storage because that path was optimized for small game
dimensions. Coposit must not inherit that growth.

`xxx` will build the current lower triangle directly in $O(k^2)$ work and reuse the current reduced-system workspace. It will not
allocate an $n^3$ cache. If later measurements justify caching, the first candidate is a cache local to the current support or
reference.

## Exact Scaled Representation

The parser supplies an integer matrix $B=cA$, where $c>0$ is the common denominator-clearing scale. This does not change any sign,
KKT support, or inertia.

The implementation constructs

$$
\widehat H=Z^TB_{SS}Z=cH,
\qquad
\widehat r=-Z^TB_{SS}e_m=cr.
$$

Positive scaling preserves the solution family, kernel, rank, and inertia signs. The source-level variables may remain `reduced` and
`right_hand_side`; the hats only distinguish the integer systems in this document.

For a consistent reduced system, the fraction-free solve returns integer numerators $Y_i$ and a common positive denominator $D$
satisfying $\widehat HY=D\widehat r$. For every nonreference coordinate $i\in S\setminus\{m\}$, set

$$
X_i=Y_i,
$$

and recover the reference numerator by

$$
X_m=D-\sum_{i\in S\setminus\{m\}}Y_i.
$$

Then

$$
x_i=\frac{X_i}{D}
\qquad(i\in S),
$$

with

$$
\sum_{i\in S}X_i=D.
$$

For $|S|=1$, use $D=1$ and $X_m=1$ directly.

Let

$$
P=\sum_{i\in S}B_{mi}X_i.
$$

Then

$$
\lambda=\frac{P}{cD}.
$$

For every unused coordinate $j$, define

$$
G_j=\sum_{i\in S}B_{ji}X_i-P.
$$

Because $cD>0$,

$$
\operatorname{sgn}(G_j)=\operatorname{sgn}\big((Ax)_j-\lambda\big).
$$

Thus all probability, payoff, and outside-residual decisions use integer signs and integer comparisons. No rational object is needed
inside the search loop.

## Exact Outcomes From One Support

One exact solve may provide several results. Process them without throwing away information.

### Downward convex-face block

If the reduced system is consistent, $H\succeq0$, and $P\geq0$, its stationary point is a global minimum on the complete normalized
affine hull of face $S$. Hence every feasible point supported in $S$ has nonnegative value, and the solver may add

$$
[\varnothing,S].
$$

The particular stationary solution need not itself be nonnegative for this conclusion. Convexity and stationarity give

$$
q(x+Zv)=q(x)+v^THv\geq q(x)
$$

for every normalized affine displacement. If the affine minimum is nonnegative, the simplex face is safe.

This is why the positive-semidefiniteness result is almost free: $H$ and its inertia already exist for the KKT solve.

### CP and SCP coverage are not always identical

A downward block with $P>0$ proves strict positivity on face $S$ and is valid for both copositivity (CP) and strict copositivity
(SCP). A block with $P=0$ proves only nonnegativity unless the stationary solution is itself feasible and therefore supplies an
exact zero witness.

The implementation will maintain this invariant:

> Every SAT proof clause must be valid for every predicate that is still undecided.

Consequently:

- in ordinary CP mode, a downward block may use $P\geq0$;
- in strict SCP mode, it may use only $P>0$, while a feasible point with $P=0$ immediately decides SCP as false; and
- in combined mode, $P=0$ with an infeasible stationary point is not inserted while SCP remains undecided. Once an exact zero
  witness has decided SCP as false, such nonnegative blocks may be used to finish the CP decision. The model retains these CP-only
  blocks in a pending list and inserts them at that transition; it does not rediscover their supports.

An upward block with $P=0$ always comes from a feasible vector, so that same vector first decides SCP as false; its block can then be
used for the remaining CP proof. A negative feasible witness decides both predicates as false.

### Negative used coordinates

If the chosen exact solution has some $X_i<0$, it is not feasible on face $S$. For a nonsingular system, rank negative coordinates by
increasing exact numerator and propose dropping the most negative one first. Ties use the smallest matrix index.

For a singular affine family, the signs of one arbitrary particular solution do not characterize the family. If that particular
solution is feasible, the solver uses an exact nullspace-to-boundary step and collapses its zero coordinates. If it is infeasible,
its negative coordinates are not used as proof; singular navigation instead applies a flat nullspace direction to the face
barycenter and uses the resulting exact boundary support only as a heuristic next support.

### Exact zero coordinates

If a nonsingular solution has no negative coordinate but some $X_i=0$, its actual support is

$$
L=\{i\in S:X_i>0\}.
$$

The same exact vector may already supply a witness or an upward block with lower endpoint $L$. After recording valid blocks, the path
may collapse directly to $L$ instead of dropping zero coordinates individually.

### Negative witness

If every $X_i\geq0$ and $P<0$, the zero-extended vector is an exact nonnegative vector of negative quadratic value. Stop with
`not_copositive`.

### Zero witness

If every $X_i\geq0$ and $P=0$, the same vector proves that strict copositivity is false. This conclusion does not require the outside
KKT inequalities: one nonzero nonnegative vector of quadratic value zero is already a complete SCP counterexample. In combined mode,
ordinary CP remains undecided and the search continues after inserting the pending CP-only blocks.

### Upward KKT block

If every $X_i\geq0$, $P\geq0$, and every outside residual satisfies $G_j\geq0$, the vector is a full KKT point. Dickinson's theorem
gives the ceiling block

$$
[L,[n]],
$$

where $L=\{i:X_i>0\}$ is the actual support.

If $P=0$, the vector is also the zero witness already handled above.

A singular consistent system may contain a nonnegative full-KKT member even when the chosen particular solution does not. Finding
such a member in nullity greater than one is an exact linear-feasibility problem in the affine family and remains deferred; the
boundary navigation is exact but heuristic with respect to which proper sub-support it chooses first.

### Outside add pivot

For a nonsingular feasible face solution, if some $G_j<0$, rank all violations by increasing exact numerator and propose adding the
most negative coordinate first. Ties use the smallest matrix index.

If the preferred support is covered or already visited, try the next exact violation.

### Inconsistent reduced system

If $Hy=r$ is inconsistent, the face has no stationary point on its normalized affine hull. Add no proof block. Recover a nullspace
vector $v$ with $v^Tr\ne0$, orient its face direction $Zv$ toward decreasing objective value, move a feasible reference point to the
first exact boundary endpoint, and continue on the resulting proper sub-support.

## Exact Active-Set Path

Given a SAT-selected seed $S_0$, the local path keeps a visited-support set and a bounded pivot budget.

Before following any add or drop proposal, the solver asks SAT about that **exact** resulting support by passing one assumption for
every support variable. The proposal is admissible only when it is not covered by permanent proof clauses, not excluded by active
heuristic-tried clauses, and not present in the path-local visited set. SAT must not be allowed to replace the proposed support by a
different satisfying assignment. If a proposal is inadmissible, the path tries the next ranked add, drop, kernel direction, or
boundary endpoint.

For the current support $S$:

1. Build $H$ and $r$ exactly.
2. Run the shared fraction-free symmetric $LDL^T$ factorization.
3. Record exact inertia, rank, nullity, and consistency.
4. When consistent, reconstruct the exact particular solution and payoff.
5. Add or defer a valid downward block according to the CP/SCP coverage invariant, even if the stationary point is not feasible or
   not full KKT.
6. If a nonnegative negative-value vector exists, terminate the whole model.
7. If a nonnegative zero-value vector exists, decide SCP as false before inserting CP-only clauses.
8. Add a valid upward block for every predicate still undecided.
9. For a unique nonsingular solution, choose the first SAT-admissible drop or add pivot from the exact ranked lists.
10. For a singular system, choose the first admissible exact nullspace boundary pivot and drop every coordinate tied at its limiting
   ratio. Try another endpoint only when the preferred resulting support is covered or already visited.
11. Continue until the uncovered family changes, no admissible pivot remains, a support repeats, or the path budget is exhausted.

The first path is deterministic and holds only one live support. Trying another offending index means skipping a covered or repeated
choice, not branching into another path.

## SAT Representation

### What Dickinson coverage proves

An upward interval is not a claim that every covered principal submatrix is independently copositive. Its vector $u$ satisfies

$$
u\notin-\mathbb R_+^n,
\qquad
L(u)=\operatorname{supp}(u)\subseteq I\subseteq U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's Theorem 4.6 says that a collection of such vectors proves copositivity when every nonempty support $I$ is covered. The
collection is a global certificate; the theorem does not require the vectors to have been generated in cardinality order. Therefore
SAT may discover and add valid intervals in any order. Cardinality order is an algorithmic policy of the published traversal, not a
logical prerequisite of the final certificate.

Downward convex-face blocks are stronger local statements: they directly prove nonnegativity or strict positivity on every nonempty
subface contained in $S$.

One Boolean variable $z_i$ represents support membership. SAT always contains the nonempty-support clause

$$
z_1\lor z_2\lor\cdots\lor z_n.
$$

Exact proof blocks are permanent.

An upward block $[L,[n]]$ adds

$$
\bigvee_{i\in L}\neg z_i.
$$

A downward block $[\varnothing,S]$ adds

$$
\bigvee_{j\notin S}z_j.
$$

### Tried supports are not proofs

After an exact support has been processed and remains proof-uncovered, its exact assignment is blocked from heuristic reselection by

$$
\left(\bigvee_{i\in S}\neg z_i\right)
\lor
\left(\bigvee_{j\notin S}z_j\right).
$$

This clause is not a copositivity certificate. Every tried-support clause is guarded by the same positive selector literal $h$.
Adding it immediately prevents an earlier support from being selected again when a later support adds a proof block and restarts the
global scheduler. The first prototype deliberately does not backtrack to an earlier processed support merely to choose its
second-ranked pivot.

The same incremental SAT solver performs two query types:

- heuristic query: assume $\neg h$ and thereby activate tried-support exclusions;
- proof query: assume $h$ and thereby satisfy and deactivate every tried-support exclusion.

The invariant is that heuristic history cannot make the proof query unsatisfiable.

### Minimum and maximum uncovered seeds

The copied bitonic cardinality network exposes monotone cardinality bounds. To find the minimum seed, `xxx` binary-searches the
smallest satisfiable upper bound $|S|\leq k$. To find the maximum seed, it binary-searches the largest satisfiable lower bound
$|S|\geq k$. It must not binary-search exact-cardinality satisfiability, because satisfiability at exactly $k$ is not monotone under
the accumulated proof clauses.

The preferred implementation uses logarithmic search over the bound, followed by one final SAT call at the selected cardinality.
Seed selection alternates deterministically between minimum and maximum. If one side has no admissible assignment, use the other.

## First-Version Control Flow

```text
input <- one unresolved matrix or component delegated by the companion launcher's optional shared preprocessing

initialize incremental SAT over every nonempty support
initialize predicate state and pending CP-only blocks
next_seed_side <- minimum cardinality

loop:
    request an uncovered, untried seed from next_seed_side

    if the heuristic query is unsatisfiable:
        repeat the query with tried-support clauses disabled

        if the proof query is unsatisfiable:
            return the exact CP/SCP state proved by accumulated blocks
        else:
            return stalled from the unregistered prototype

    alternate next_seed_side between minimum and maximum
    start one bounded exact active-set path

    while the path can continue:
        solve the reduced face-KKT system with exact fraction-free LDLT
        record inertia and consistency

        if an exact downward block is valid for every undecided predicate:
            add it permanently
        else if it is valid only for CP while SCP is undecided:
            retain it in the pending CP-only list

        if a nonnegative negative-value vector is found:
            return not copositive

        if an exact zero witness is found:
            decide SCP as false
            insert every pending CP-only block

        if an exact upward block is valid for every predicate still undecided:
            add it permanently

        if a new proof block changes the uncovered family:
            restart global seed selection

        mark the processed support as heuristic-tried if it remains proof-uncovered

        if the reduced system is nonsingular:
            move to the first uncovered, unvisited exact add/drop pivot

        else:
            recover an exact nullspace direction
            move to an exact boundary endpoint
            drop the coordinate or tied coordinates that become zero

    end the local path when no admissible next support remains
```

The finite support family, bounded local paths, and guarded tried assignments make the prototype finite unless an external resource
limit intervenes. A `stalled` result is internal analysis data, not a CP/SCP classification. After a proof-complete completion policy
is added, every fallback step must cover its exact SAT-selected support so the registered model terminates unless an external resource
limit intervenes.

## Diagnostics

Diagnostics must distinguish support-search cost from exact linear-algebra cost. When enabled, record:

- minimum- and maximum-cardinality seed queries;
- seed cardinalities and path lengths;
- reduced-system orders;
- reduced-matrix construction time;
- fraction-free factorization/solve time;
- outside-product time;
- maximum integer bit length entering and leaving each factorization;
- rank, nullity, consistency, and exact inertia;
- negative, zero, and positive coordinate counts;
- drop and outside-violation candidate counts;
- add pivots, drop pivots, covered alternatives, and repeated alternatives;
- upward blocks, downward blocks, and supports producing both;
- lower- and upper-endpoint cardinalities;
- heuristic-SAT exhaustion versus proof-SAT exhaustion; and
- final stop reason.

Do not materialize rational strings or public candidate objects solely for diagnostics.

## Efficiency Requirements

Exact arithmetic is intentionally slower than binary64, but the reference implementation must retain FracESSA's optimized design:

- reduce the system from order $k+1$ to $k-1$;
- build only the lower triangle;
- use fraction-free symmetric $LDL^T$, not rational Gaussian elimination;
- preserve FLINT's immediate-integer fast path;
- reuse all matrices, vectors, coordinate-operation storage, and support-index buffers;
- retain one common positive denominator rather than building rational coordinates;
- derive signs and rankings directly from integer numerators;
- obtain convexity from the same factorization's inertia;
- avoid the dense $O(n^3)$ FracESSA reduced-entry cache;
- test a fully specified support pivot with one incremental SAT call under complete-assignment assumptions, stopping at the first
  admissible ranked proposal rather than maintaining a second interval-membership data structure; and
- bound every local path.

The first performance review must time reduced-matrix construction, factorization, singular handling, outside products, SAT seed
selection, and support-membership gates separately.

## Implementation Stages

### Stage 1: Replace the copied placeholder

- Remove inherited halfspace-ray and cardinality-traversal control flow from `xxx`.
- Keep `xxx` unregistered while incomplete.
- Leave its `ALGORITHM.md` as a visible placeholder until a working flow exists; then replace it with implemented behavior.

### Stage 2: Add the exact reduced KKT layer

- Reuse `coposit::fraction_free_ldlt_factorization` directly.
- Expose the already-maintained inertia counts only if diagnostics require them.
- Interpret rank, nullity, positive semidefiniteness, and right-hand-side consistency in the model-local KKT result.
- Return one particular solution for consistent singular systems.
- Recover exact nullspace vectors in original face coordinates.
- Build $H$ and $r$ directly per support without the $O(n^3)$ cache.

### Stage 3: Add the exact KKT result

Use one small model-local result containing:

- factorization and consistency status;
- rank, nullity, and inertia;
- common positive denominator;
- support-coordinate numerators;
- payoff numerator;
- ranked negative and zero coordinates;
- ranked outside violations; and
- exact witness/upward/downward flags.

Do not add a generic solver interface, class hierarchy, or public abstraction.

### Stage 4: Add the SAT seed scheduler

- Reuse the incremental CaDiCaL solver and cardinality network from the copied model.
- Remove the cardinality loop.
- Add minimum/maximum uncovered-cardinality queries.
- Add guarded heuristic-tried clauses.
- Separate heuristic exhaustion from proof exhaustion.

### Stage 5: Add the exact active-set path

- Implement deterministic exact drop/add ranking.
- Implement exact singular nullspace-to-boundary pivots and tied-coordinate collapse.
- Gate every fully specified proposed support through SAT assumptions against current proof coverage and active tried-support
  exclusions.
- Prevent cycles with a path-local visited set.
- Add exact upward/downward blocks immediately.

### Stage 6: Integrate and document

- Keep the prototype unregistered while it can return `stalled`.
- After selecting and testing a proof-complete completion policy, implement the standard `solve` and one-traversal `classify`
  contract.
- Complete diagnostics.
- Replace `ALGORITHM.md` with the implemented algorithm.
- Register the model in CMake and Python only after focused tests pass.
- Update inventories and human documentation in one merge-safe pass.

### Stage 7: Measure

- Run focused tests and a small direct prototype sample first.
- Run the Smoke set only after proof-complete promotion and registration.
- Reuse matching existing evidence rather than repeating runs.
- Run Core and Stress in `both` mode only after differential correctness passes.
- Compare coverage, completed decisions, exact factorization cost, and stall reasons with `sat_halfspace_rays_dickinson`.

## Verification Plan

### Exact $LDL^T$

The existing `test_fraction_free_ldlt` target already checks nullspace recovery across ranks, complete bases, consistent and
inconsistent singular solves, arbitrary-precision singular elimination, and singular positive-semidefinite recognition. Do not
duplicate those tests in `xxx`. Add only checks for shared behavior that actually changes, such as new read-only inertia accessors.

The model-local reduced-KKT checks must cover:

- positive definite, negative definite, and indefinite reduced systems;
- a nonsingular matrix with zero diagonal requiring an exact symmetric coordinate operation;
- singular positive-semidefinite and singular indefinite systems;
- consistent and inconsistent singular right-hand sides;
- very small and very large integer entries;
- positive common-denominator normalization; and
- exact reconstruction of $\widehat HY=D\widehat r$ and of the normalized support numerators $X$.

### Exact KKT outcomes

Test:

- deterministic negative-coordinate drop ranking;
- exact zero coordinates and actual-support collapse;
- a negative witness;
- deterministic outside-violation add ranking;
- an upward-only block;
- a downward-only block;
- one support producing both blocks;
- a singular positive-semidefinite consistent system producing a downward block;
- the all-ones order-two matrix, whose consistent singular stationary family can be reduced to either boundary point with unchanged
  payoff;
- the integer matrix $\begin{pmatrix}2&1\\1&0\end{pmatrix}$, whose reduced system is singular and inconsistent and whose objective
  on the simplex is $2x_1$; this verifies that the nullspace sign selects the correct boundary and that deleting an arbitrary pivot
  index would be wrong;
- a nullspace direction with a tied ratio that removes several coordinates exactly;
- an indefinite face producing no downward block; and
- an inconsistent system producing no unsupported proof conclusion.

### SAT behavior

Test:

- minimum and maximum uncovered seed selection;
- deterministic alternation;
- exact-support assumption checks that reject a covered preferred pivot and accept the next admissible pivot;
- upward and downward proof clauses;
- deferral and later insertion of CP-only blocks after an exact zero witness decides SCP as false;
- guarded tried-support clauses;
- proof queries ignoring every tried support;
- heuristic exhaustion while proof supports remain; and
- exact proof exhaustion.

### Differential correctness

For small matrices, compare every completed CP/SCP result with exhaustive exact support enumeration. The unregistered prototype may
return `stalled`; it must never return a conflicting classification. The registered model must not expose `stalled` as `false`.

## Deferred Floating-Point Optimization

Only after the exact model is understood should another optimization add FracESSA's binary64 Bunch--Kaufman front end. That version
may propose the same path approximately and send final or ambiguous states to the exact solver.

The exact model remains the reference for support paths, blocks, witnesses, classifications, and numerical-difference analysis.
Floating point is therefore an optimization of a known exact algorithm, not part of the first algorithm.

## Open Decisions

1. **Path budget:** fixed, proportional to $n$, or proportional to seed cardinality.
2. **Restart policy:** restart global seed selection after every new block, or finish a path whose next support remains uncovered.
3. **Zero collapse:** continue immediately from the actual support $L$, or add blocks and return to global SAT selection.
4. **Singular affine feasibility:** keep the first version's exact boundary navigation, or add a small exact Phase-I feasibility step
   that finds a nonnegative full-KKT member of a higher-dimensional affine family before reducing the support.
5. **Reference coordinate:** first support index or a deterministic choice intended to reduce intermediate integer growth.
6. **Seed search:** binary search over cardinality-network outputs or another incremental bound search with fewer SAT calls.
7. **Proof-complete promotion:** keep the first implementation as an unregistered `stalled`-capable prototype, or add the recommended
   SAT-selected ordinary Dickinson completion before the first corpus run.
8. **Additional intervals:** whether a non-full exact face-stationary point should also contribute its ordinary bounded Dickinson
   interval.
9. **Final name:** keep `xxx` until behavior is measured, then choose a descriptive identifier.

The initial choices should remain deterministic and minimal so diagnostics explain the main search rather than secondary heuristics.
