# XXX

Classification: coposit-created exact CP/SCP experiment. `xxx` combines the complete certificate engine of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md) with exact KKT active-set paths. One persistent SAT
instance exhausts cardinality layers in the order $1,n,2,n-1,\ldots$. Every processed support contributes an ordinary Dickinson
interval when its local path ends, while the KKT calculation may add stronger upward or downward intervals.

`copositive` and `strictly_copositive` select one predicate. `both` searches for both results in one traversal and is the default.

## Idea In Plain Language

For a symmetric matrix $A$, copositivity is equivalent to nonnegativity of the quadratic form $x^TAx$ on the standard simplex. A
global minimum occurs at a KKT point: on its used coordinates the payoffs are equal, and every unused coordinate has no smaller
payoff.

SAT chooses one uncovered support in the active cardinality layer. At that support, the model computes the complete halfspace-rays
Dickinson interval $[L,U]$ and an exact face-stationary candidate. The interval is buffered rather than inserted into SAT. Exact
linear algebra then proposes a move toward a smaller or larger face. Because the current path's intervals are still absent from SAT,
the path cannot cut off its own continuation.

Every proposed successor is checked against intervals committed by earlier paths and against a path-local visited set. The walk ends
when it reaches a KKT point or when none of its mathematically proposed successors remains open. It then commits every buffered
interval at once and asks SAT for another uncovered seed. There is no path memoization or arbitrary-neighbor search.

Two kinds of exact argument remove many supports at once:

- a nonnegative minimum on a convex face proves every subface copositive; and
- a nonnegative KKT point supplies Dickinson coverage for every superface of its positive support.

Dickinson coverage does not claim that each covered superface is independently copositive. It proves that the superface cannot be an
inclusion-minimal obstruction. Any noncopositive covered superface still contains a smaller minimal bad support, and no valid
Dickinson vector can cover that support. Consequently, complete coverage rules out every possible obstruction.

The ordinary interval makes the global traversal proof-complete without a fallback: after a path ends, SAT continues with another
uncovered support in the same layer. An unsatisfiable layer is finished, and the scheduler advances to the opposite side.

## Name, Sources, And Classification

`xxx` is the working name selected for this research model. It will be replaced only after the behavior is understood well enough to
choose a descriptive name.

The model began as an isolated copy of `sat_halfspace_rays_dickinson`. Its exact principal-system solve, coordinate sweeps,
adaptive shortlist, two synthesized-ray sweeps, singular orientation rule, persistent SAT representation, and cardinality-aware
Dickinson clauses are retained.

The interval idea is related to Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications*
569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6. The reduced face-stationarity equations follow the exact
candidate construction formerly used by FracESSA. The implementation reuses Coposit's shared fraction-free symmetric $LDL^T$
factorization and CaDiCaL 2.2.1. The alternating layer scheduler, KKT paths, downward face blocks, and singular boundary moves are
coposit changes, not claims about Dickinson's published algorithm.

## Optimization Problem

The given data are a symmetric matrix $A\in\mathbb Z^{n\times n}$. The decision vector belongs to the standard simplex

$$
\Delta_n=\{x\in\mathbb R_+^n:\mathbf1^Tx=1\}.
$$

The model studies

$$
\min_{x\in\Delta_n} q(x),
\qquad q(x)=x^TAx.
$$

The minimum is negative exactly when $A$ is not copositive. It is zero exactly when $A$ is copositive but not strictly copositive.
It is positive exactly when $A$ is strictly copositive.

For a nonempty support $S\subseteq[n]$, a stationary point on the affine hull of face $S$ satisfies

$$
A_{SS}x_S=\lambda\mathbf1,
\qquad \mathbf1^Tx_S=1.
$$

It belongs to the relative interior of the face when every component of $x_S$ is positive. After extending $x_S$ by zeros outside
$S$, it is a KKT point of the simplex when

$$
(Ax)_j-\lambda\geq0
\qquad(j\notin S).
$$

The objective value at such a point is $q(x)=\lambda$.

## Reduced Exact KKT System

Let $k=|S|$ and choose the last index $m\in S$ as a deterministic reference. For the other indices, let the columns of $Z$ be
$e_i-e_m$. Every normalized vector on the affine hull of the face can be written uniquely as

$$
x_S=e_m+Zy.
$$

The equal-payoff equations reduce to the symmetric system

$$
Hy=r,
$$

where

$$
H_{ij}=A_{ij}-A_{im}-A_{mj}+A_{mm},
\qquad
r_i=A_{mm}-A_{im}.
$$

The reduced order is $k-1$. For $k=1$, normalization already fixes $x_S=(1)$ and no factorization is needed.

The shared fraction-free $LDL^T$ factorization returns a common positive denominator $D$ and integer numerators $Y$ satisfying

$$
HY=Dr.
$$

The normalized support numerators are

$$
X_i=Y_i\quad(i\ne m),
\qquad
X_m=D-\sum_{i\ne m}Y_i.
$$

Thus $x_S=X/D$. No rational objects are created in the hot path. The payoff numerator is

$$
P=\sum_{i\in S}A_{mi}X_i,
$$

so $\lambda=P/D$. Every outside comparison uses the integer residual

$$
G_j=\sum_{i\in S}A_{ji}X_i-P.
$$

Only signs and exact integer comparisons are required.

## Mandatory Halfspace-Rays Certificate

Every visited support $I$ first runs the unchanged exact certificate engine from `sat_halfspace_rays_dickinson`. For nonsingular
$A_I$, it factors $A_I$ once, begins with $A_Ix=\mathbf1$, reuses that factorization for every unit right-hand side, and searches
exact breakpoint sweeps. It maximizes

$$
(|U|,|U|-|L|)
$$

lexicographically. After the coordinate sweeps stall, it retains at most
$\min\{|I|,64,\lceil3\sqrt n\rceil\}$ promising rays and sweeps at most two exact synthesized directions. For singular $A_I$, it
uses one exact nullspace vector and retains the orientation with larger $|U|$ when the signs are mixed.

The nonsingular right-hand side remains strictly positive, so $I\subseteq U$; the vector's true support satisfies $L\subseteq I$.
Thus $I\in[L,U]$. The interval is buffered for the current path and committed only when that path ends. An exact nonnegative
negative-value vector stops the model immediately without needing any buffered proof, and an exact zero vector decides strict
copositivity as false.

## Convexity And Downward Proofs

The matrix $H$ represents the quadratic form on zero-sum directions of face $S$. For $d=Zv$,

$$
d^TA_{SS}d=v^THv.
$$

Therefore the quadratic form is convex on the complete face exactly when $H$ is positive semidefinite. The same $LDL^T$
factorization supplies this decision through its exact inertia; no second factorization is performed.

If $Hy=r$ is consistent, $H\succeq0$, and $P\geq0$, the affine stationary value is the global minimum on face $S$. Hence $A_{SS}$
is copositive. Every principal submatrix $A_{JJ}$ with $J\subseteq S$ is then copositive as well: extend any nonnegative vector on
$J$ by zeros to $S$. This proves the downward block

$$
[\varnothing,S]=\{J:J\subseteq S\}.
$$

The same block proves strict copositivity only when $P>0$.

## KKT Points And Upward Proofs

Suppose $X\geq0$, $P\geq0$, and every outside residual satisfies $G_j\geq0$. Let

$$
L=\{i\in S:X_i>0\}
$$

be the true positive support. The extended vector has $Au\geq0$, so its Dickinson upper endpoint is the full set $[n]$. It supplies
the upward coverage block

$$
[L,[n]]=\{J:L\subseteq J\subseteq[n]\}.
$$

This block excludes its supports as inclusion-minimal obstructions; it does not prove each principal matrix independently
copositive. When $P>0$, the block is valid for both ordinary and strict copositivity. When $P=0$, it is valid only for ordinary
copositivity and the feasible vector is an exact zero witness, proving that strict copositivity is false.

For the strict claim, suppose instead that a copositive matrix had a minimal zero $z$ with support $I$. Dickinson's Lemma 5.2 says
that any certificate vector covering $I$ must be a positive multiple of $z$ and therefore have zero quadratic value. A positive-value
KKT vector cannot cover that minimal-zero support. Hence complete positive-value coverage, together with positive downward blocks,
rules out every minimal zero.

If $X\geq0$ and $P<0$, the model has an exact negative witness and returns not copositive immediately.

## Nonsingular Active-Set Moves

When $H$ is nonsingular, its stationary solution is unique.

1. If some $X_i<0$, rank the negative numerators from most negative upward. Try removing the corresponding support index.
2. Otherwise, if some $X_i=0$, collapse all zero coordinates at once to the true support.
3. Otherwise, rank the unused coordinates with $G_j<0$ from most violated upward. Try adding the corresponding index.
4. Test each proposal against the committed SAT proof clauses and stop the local path if no globally open exact support remains.

Empty, path-local repetitions, and supports covered by earlier committed paths are rejected. The model tries the remaining candidates
in the same deterministic order. If collapsing all zero coordinates is unavailable, it also tries dropping one zero coordinate at a
time before stopping the path. The current path's buffered intervals do not participate in these SAT checks.

## Singular Active-Set Moves

A singular reduced system does not terminate the path. Let $v\ne0$ satisfy $Hv=0$, and extend it to the zero-sum face direction

$$
d=Zv.
$$

Along this direction,

$$
q(y+tv)=q(y)-2t\,v^Tr.
$$

There are two cases.

- If $Hy=r$ is consistent, every nullspace vector has $v^Tr=0$. The objective is flat along that direction. The model tests both
  orientations and moves the face barycenter exactly to a boundary.
- If $Hy=r$ is inconsistent, some nullspace vector has $v^Tr\ne0$. Its sign selects a direction of strict decrease, and the model
  moves the barycenter to the boundary in that orientation.

At the barycenter all support coordinates are equal. The first boundary is reached where the most negative entry of $d$ becomes
zero. All tied entries are removed together. A nonempty result is followed only when it is absent from the path-local visited set and
remains outside the SAT family committed by earlier paths.

The first version does not solve a separate affine feasibility problem inside a higher-dimensional stationary family. It examines
the exact nullspace basis in deterministic order and uses the first uncovered admissible boundary move.

## SAT Representation

One Boolean variable $z_i$ records whether index $i$ belongs to the selected support. The empty support is forbidden. A Batcher
bitonic sorting network exposes monotone cardinality bounds.

The active layers are exhausted in the deterministic order

$$
1,n,2,n-1,3,n-2,\ldots.
$$

Within a layer, SAT repeatedly returns an uncovered support of exactly that cardinality. When the exact-cardinality query becomes
unsatisfiable, all supports in that layer are covered and the scheduler advances to the next layer from the opposite side.

An upward block with lower endpoint $L$ adds

$$
\bigvee_{i\in L}\neg z_i.
$$

A downward block with upper endpoint $S$ adds

$$
\bigvee_{j\notin S}z_j.
$$

These clauses are permanent mathematical proofs. No tried-support clause, second SAT view, or memoized KKT lattice is used. A local
path never adds its own buffer to SAT; after it ends, its batch contains a Dickinson interval covering every support it processed.

The cardinality-aware interval clause is automatically inactive outside its useful range. Below $|L|$, an assignment cannot contain
$L$. Above $|U|$, the sorting-network literal satisfies the clause. Thus finishing a low or high layer requires no interval scan,
explicit deletion, activation variable, or SAT rebuild; expired clauses are already satisfied by the active cardinality assumptions.

## Ordinary Versus Strict Blocks

A zero-valued block is safe for ordinary copositivity but not for strict copositivity. During combined classification, such a block
is held in a pending list while strict copositivity remains undecided. If an exact zero witness later proves strict copositivity false,
all pending ordinary blocks become safe for the remaining CP search and are inserted. A positive-valued block is safe for both
predicates and is inserted when its path batch is committed.

## Complete Decision Flow

1. Initialize one SAT instance and the exact-cardinality sorting network.
2. Select layer $1$, then $n$, then $2$, then $n-1$, and continue inward.
3. Ask SAT for an uncovered seed in the active layer.
4. Start an exact active-set path with an empty interval buffer and path-local visited set.
5. At every visited support, run the complete halfspace-rays optimizer and append its ordinary Dickinson interval to the buffer.
6. Build and factor the reduced KKT system; append every valid downward convex-face or KKT interval to the same buffer.
7. Return immediately on an exact negative witness. Record an exact zero witness and release pending ordinary blocks when applicable.
8. If the candidate is a KKT point, commit the whole buffer and end the path.
9. Otherwise follow the first exact add, drop, zero-collapse, or singular boundary move that is neither path-visited nor covered by
   previously committed SAT intervals.
10. If no such move remains, commit the whole buffer and end the path.
11. Request another seed in the same cardinality.
12. Advance to the opposite layer only after SAT proves the active layer exhausted. Return the classification after every layer is
    exhausted.

## Exact Arithmetic And Termination

All matrix construction, factorization, consistency tests, nullspace vectors, ratios, residuals, witnesses, and proof clauses are
exact. CaDiCaL chooses supports but does not validate matrix mathematics.

The support family is finite. A path-local set prevents a support from occurring twice in one walk, so every walk ends after finitely
many moves. When the walk ends, its mandatory Dickinson intervals cover every support it visited. Those committed clauses prevent a
later SAT seed or active-set successor from entering the same supports. Therefore the global traversal also terminates after finitely
many processed supports unless an external timeout or resource limit intervenes.

## Known Difficult Inputs

- Every visited support pays for both the principal Dickinson factorization and the reduced KKT factorization. When KKT intervals add
  little coverage, the model is slower than `sat_halfspace_rays_dickinson`.
- A covered support may in principle lead to a stronger later certificate, but this model deliberately refuses that route to avoid
  repeatedly traversing regions already removed from the global search.
- Higher-dimensional singular stationary families may contain a useful feasible point that the deterministic basis-boundary moves
  do not find. The first version intentionally has no exact affine Phase-I search.
- Exact $LDL^T$ arithmetic can grow expensive when matrix entries contain hundreds or thousands of digits.
- The SAT sorting network and complete outside products cost work proportional to the full matrix order even when a selected support
  is small.
- KKT blocks can overlap heavily. Many valid local proofs may still leave large gaps in the Boolean lattice.
