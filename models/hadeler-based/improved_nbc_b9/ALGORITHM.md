# Improved NBC-B9

Classification: coposit-created exact CP/SCP experiment. Improved NBC-B9 alternates between low and high cardinalities. The low frontier uses
face curvature first and falls back to an exact Halfspace-Rays Dickinson certificate when curvature alone cannot prune upward. The
high frontier is an opportunistic floating-point positive-semidefiniteness scan; only candidates for downward pruning are verified
exactly. In addition, bounded active-set walks search for face-stationary and KKT points. Those walks may add only exact curvature
closures that cannot hide a useful curvature closure in the opposite direction.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

Improved NBC-B9 uses that observation as a search certificate.

- If a face is not strictly convex, neither that support nor any superset can be the support of the chosen minimal-support global
  minimizer. The model removes the whole upward closure.
- If the principal matrix is positive definite, or if it is singular positive semidefinite and its all-ones system is consistent,
  every nonzero nonnegative vector supported inside that face has positive quadratic value. The model removes the whole downward
  closure.
- If a low-frontier face is strictly convex, the retained factorization is reused to build and optimize a Dickinson interval. This
  replaces the former exact-support block.
- A high-frontier face is first tested by a floating-point $LDL^T$ filter. A positive-semidefinite candidate is factorized exactly.
  It contributes a downward closure only when exact arithmetic proves positive definiteness, or proves positive semidefiniteness
  together with consistency of $Bx=\mathbf1$. A rejected candidate is skipped only by the high scan and remains available to the
  exact low-frontier proof.
- A bounded active-set walk uses floating point only to choose its path. At a support whose reduced Hessian appears to have negative
  curvature, or whose face-stationary point appears to justify downward pruning, exact arithmetic rechecks the complete condition.
  The walk buffers every accepted closure and inserts nothing until it stops.

The remaining supports are represented by a resumable coposit derivative of NBC MiniSat All. Two cardinality frontiers start at $1$ and $n$. The model alternates one
open low support and one open high support, exactly as SAT-B3 does. Every new certificate enters both live NBC searches immediately,
so a downward certificate found at the high frontier can remove low candidates before the low cardinality is exhausted. The same
certificate is also retained outside NBC for compaction when either frontier finishes a cardinality.

Once the high frontier meets the low frontier, the exact low traversal continues alone until the proof is complete. The processing
order has the form

$$
\text{one from low},\ \text{one from high},\ \text{one from low},\ \text{one from high},\ldots.
$$

No cardinality is materialized as a list. Each frontier stores only a stack of disjoint unexplored prefix cubes. After Improved NBC returns a
model, those cubes advance past it, so the next query cannot return the same support. In particular, the model never inserts an
exact-support clause merely to request another support.

Unlike the original NBC wrapper retained by `nbc_b7`, Improved NBC is explicitly resumable. A callback may stop after one model and
the next call safely continues with another prefix. The solver removes call-local assumptions and enumeration scratch state before
returning while retaining permanent clauses and logically valid learned clauses. A conflict in the permanent formula is latched as
global exhaustion; a conflict caused only by the current cardinality or prefix assumptions is not.

Pruning is directional. A low-frontier support either contributes an upward curvature closure or a Dickinson interval. A
high-frontier support contributes an exactly proved downward strict-copositivity closure or a high-scan-only rejection. It never pays for a
Halfspace-Rays search. A support rejected by the high scan remains available to the low frontier and its exact Dickinson fallback.
Floating point therefore changes only which exact downward checks are attempted; it cannot remove a support from the proof or cause
the traversal to finish.

The additional walk is opportunistic, not part of completeness. The selected low or high support is first processed once by its
ordinary B7 rule, but any resulting certificate is withheld so it cannot block the walk that starts from the same support. When the
walk ends, its exact closures and the seed certificate are committed. The seed is not processed a second time, and the complete B7
traversal remains the proof procedure.

## Name, Sources, And Classification

The identifier is `improved_nbc_b9`.

- **Improved NBC** names the separately owned resumable derivative of NBC MiniSat All.
- **B9** marks the ninth experiment in the curvature-based B line.

The model is an independent copy of [`improved_nbc_b7`](../improved_nbc_b7/ALGORITHM.md). It preserves Improved NBC-B7's
two-frontier traversal, floating high-frontier screen, exact nonsingular and singular downward tests, low-frontier Halfspace-Rays
machinery, resumable Boolean engine, clauses, and interval compaction. B9 adds only the bounded, reproducibly jittered active-set
walk and its exact no-hiding closure policy. Improved NBC-B7 remains unchanged for direct comparison. Dickinson intervals come from Peter J. C.
Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The inertia tests are standard consequences of inertia
additivity for equality-constrained quadratic forms; see S.-P. Han and O. Fujiwara, “An inertia theorem for symmetric matrices and
its application to nonlinear programming,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7).

Using these curvature facts as permanent Boolean clauses, alternating individual low and high supports, and using the search as a complete exact
CP/SCP classifier are coposit experiments rather than algorithms from those papers. The Boolean engine derives from Takahisa Toda's
NBC MiniSat All 1.0.2, itself based on MiniSat-C 1.14.1. The copied derivative is maintained under
`cpp/third_party/improved_nbc_minisat_all/`; the C++ wrapper asks it for one model in one unexplored prefix cube at a time.
Certificates affect both live frontiers immediately and remain recorded outside Improved NBC for later compaction.

## Face Geometry

Let $A\in\mathbb{R}^{n\times n}$ be symmetric. A nonempty index set $I\subseteq[n]$ identifies a simplex face. Write $B=A_I$ for
the corresponding principal matrix. The tangent space of that face is

$$
\mathcal T_I=\{v\in\mathbb{R}^{|I|}:\mathbf 1^Tv=0\}.
$$

The quadratic form is strictly convex on the face exactly when

$$
v^TBv>0\qquad\text{for every nonzero }v\in\mathcal T_I.
$$

Equivalently, if the columns of $Z$ span $\mathcal T_I$, then $Z^TBZ$ is positive definite. The implementation does not construct
$Z$. It obtains the answer from one exact fraction-free $LDL^T$ factorization of $B$.

### Nonsingular principal matrix

Suppose $B$ is nonsingular and define

$$
\delta=\mathbf 1^TB^{-1}\mathbf 1.
$$

The reduced Hessian on $\mathcal T_I$ is positive definite exactly when either

1. $B$ is positive definite; or
2. $B$ has exactly one negative eigenvalue and $\delta<0$.

The first case gives a downward certificate when the support was proposed by the high floating-point filter and the exact
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, Improved NBC-B9 solves
$Bx=\mathbf 1$ using the existing factorization. The sign of $\delta$ is the sign of the sum of the integer numerators of $x$ because
their common denominator is positive.

If $x\leq0$, then $y=-x\geq0$ and

$$
y^TBy=\mathbf 1^Tx=\delta<0.
$$

After embedding $y$ in the full space by adding zeros, this is an exact non-copositivity witness. This branch is normally reached by
the low frontier. It can also be reached when the floating high filter produces a false positive and exact verification rejects
positive definiteness. On the low frontier, the same exact solution becomes the starting vector for Halfspace-Rays optimization and
a Dickinson interval.

### Singular principal matrix

The reduced Hessian is positive definite exactly when all three conditions hold:

1. $B$ is positive semidefinite;
2. $B$ has nullity one; and
3. a nonzero kernel vector $z$ satisfies $\mathbf 1^Tz\neq0$.

If these conditions fail, the upward curvature exclusion applies on the low frontier. On the high frontier, a likely
positive-semidefinite singular support proceeds to exact verification. If the all-ones system is inconsistent, no downward clause is
installed and the support remains available to the low frontier. If the low-frontier conditions hold and either $z\geq0$ or
$-z\geq0$, the embedded kernel vector is an exact copositive zero. It disproves strict copositivity but not ordinary copositivity. On
the low frontier, the kernel ray is oriented toward the larger Dickinson upper set and stored as an interval.

## Low-Frontier Dickinson Fallback

When low-frontier curvature does not already remove the upward closure, Improved NBC-B9 reuses the retained exact factorization. For a
nonsingular principal matrix it begins with the exact solution of

$$
B x=\mathbf1.
$$

For a singular principal matrix it uses the recovered kernel ray. The local vector is embedded in $\mathbb R^n$ by inserting zeros
outside $I$. For an embedded vector $u$, define

$$
L(u)=\operatorname{supp}(u),\qquad
U(u)=\{j\in[n]:(Au)_j\geq0\}.
$$

Dickinson's theorem certifies every support $J$ satisfying

$$
L(u)\subseteq J\subseteq U(u).
$$

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. Improved NBC-B9 performs the inherited exact
breakpoint sweeps along those directions, preferring larger $|U|$ and then larger width $|U|-|L|$. It retains a bounded shortlist of
coordinate rays and tests at most two complementary combined rays after a coordinate-wise stall. Every accepted candidate is
represented with exact integers; no floating-point comparison enters the search or certificate.

The Boolean clause for the interval is

$$
\left(\bigvee_{i\in L(u)}\neg s_i\right)
\lor
\left(\bigvee_{j\notin U(u)}s_j\right)
\lor c_{|U(u)|+1}.
$$

The last literal retires the clause automatically at cardinalities above $|U(u)|$, where no support can lie inside the interval.

## NBC Clauses

Each original index has a Boolean variable $s_i$, true exactly when that index belongs to the selected support. A Batcher bitonic
sorting network supplies exact-cardinality assumptions for any requested layer.

### Upward closure

If the reduced Hessian on $I$ is not positive definite, NBC receives

$$
\bigvee_{i\in I}\neg s_i.
$$

This removes $I$ and every support containing it. The rule is sound because strict convexity is inherited by subfaces: if a larger
face containing $I$ were strictly convex, its restriction to $\mathcal T_I$ would also be strictly convex.

Before traversal, the model applies the same rule to every pair. For $I=\{i,j\}$, strict face convexity is the single exact test

$$
A_{ii}+A_{jj}-2A_{ij}>0.
$$

Every failing pair immediately contributes $\neg s_i\lor\neg s_j$.

### Downward closure

If $B=A_I$ is positive definite, every principal submatrix indexed by a nonempty subset of $I$ is positive definite. NBC therefore
receives

$$
\bigvee_{j\notin I}s_j.
$$

This removes $I$ and every nonempty subset of $I$. For $I=[n]$ the clause is empty, so no support remains and the proof is complete.

Improved NBC-B9 retains SAT-B3's singular case. Suppose

$$
B\succeq0
\qquad\text{and}\qquad
Bx=\mathbf1
$$

is consistent, meaning that at least one solution $x$ exists. For every $z\in\ker B$,

$$
\mathbf1^Tz=x^TBz=0.
$$

A nonzero nonnegative vector has a positive coordinate sum, so no such vector can lie in $\ker B$. Positive semidefiniteness then
gives $y^TBy>0$ for every nonzero $y\geq0$. Thus $B$ is strictly copositive, every principal submatrix inside $I$ is strictly
copositive, and the same downward clause is valid. This is an elementary consequence of
$\operatorname{range}(B)=\ker(B)^\perp$ for symmetric matrices.

Strict convexity only on the simplex face is still not enough for this downward rule. The implementation requires either positive
definiteness of the entire principal matrix or the exact singular positive-semidefinite consistency certificate above.

### Floating high-frontier filter

Before the first high query, the complete integer matrix is converted once to a symmetric binary64 matrix using one common
power-of-two scale. Every high support then copies only its floating principal submatrix and runs an unpivoted $LDL^T$
positive-semidefiniteness filter. Its pivot margin is relative to the selected submatrix size and largest magnitude, so an unrelated
large entry elsewhere cannot suppress a useful candidate. A comfortably negative pivot rejects the candidate. A near-zero pivot is
accepted only when the remaining residual column is also near zero, as positive semidefiniteness requires. Acceptance is not a
certificate: the integer principal matrix is then copied and factorized exactly before any downward clause is installed.

A floating rejection must not remove a support from the mathematical proof because rounding can reject an exactly
positive-semidefinite matrix. The high frontier's prefix cursor advances past that support without storing a rejection clause. The
independent exact low frontier can still reach the same support later.

## Bounded Jittered KKT Walk

The walk solves the face-stationarity system on its current support. It starts once from the first open low seed and once from the
first open high seed. Later walks alternate seed sides. After an unsuccessful scheduled walk the next gap doubles from $n$ to
$2n,4n,\ldots$ ordinary B7 support visits. Only an exact KKT endpoint resets the gap to $n$. Exact curvature closures found by a
non-KKT walk are retained, but they do not reset the scheduler; the walk still counts as unsuccessful. The seed that triggered a
walk is not counted toward the next gap. This exponential backoff prevents a heuristic that has stopped reaching stationary points
from dominating the complete traversal.

Each walk visits at most $n$ supports and never backtracks. A support already on the current path or already covered by an existing
B7 interval is not an admissible successor. The path uses the following active-set moves:

1. remove a used coordinate whose face-stationary weight is negative;
2. remove a used zero coordinate;
3. add an unused coordinate whose outside product is below the current stationary value.

Candidates are ordered by violation size. The walk usually takes the largest violation, but a deterministic pseudorandom draw can
take the second, third, or later candidate. Rank zero has probability $2/3$, rank one has probability $2/9$, rank two has
probability $2/27$, and the remaining probability continues geometrically. Rejected candidates are removed and the draw is repeated.
The generator is seeded from the exact matrix entries and is therefore reproducible: the jitter changes attraction paths without
making benchmark runs nondeterministic.

The floating solve is only a path proposal. If floating point reports a terminal KKT candidate, the candidate is recomputed exactly.
When exact arithmetic rejects that terminal claim, the walk enters exact mode and remains exact until it stops. This prevents the
walk from repeatedly returning to a numerically false stationary point. For a singular exact system, a nullspace direction proposes
a smaller support; the proposal is heuristic, and every later certificate is still verified independently on that support.

### Exact no-hiding actions

Let $H_I$ be the reduced Hessian of $A_I$ on $\{d:\mathbf1^Td=0\}$. Every proposed action is verified with the fraction-free exact
$LDL^T$ factorization.

| Exact result at the visited support $I$ | Buffered action |
|---|---|
| $H_I$ has at least one negative eigenvalue | Upward closure $[I,[n]]$ |
| $H_I\succ0$ and the exact feasible face-stationary point has nonnegative value | Downward closure $[\varnothing,I]$ |
| The walk stops and $A_I\succ0$ | Downward closure $[\varnothing,I]$ |
| $H_I\succeq0$ is singular, or another required condition fails | No walk closure |
| An exact feasible point has negative value | Stop: not copositive |
| An exact feasible point has zero value | Record: not strictly copositive |

The negative eigenvalue must belong to $H_I$, not merely to the principal matrix $A_I$. One negative tangent direction survives in
every superset, so upward pruning cannot hide a positive-semidefinite downward opportunity. Positive definiteness survives in every
subface, so either downward rule cannot hide a flat or negative curvature root below it. Singular semidefinite cases are deliberately
left to ordinary B7 because, although additional pruning can be valid, it can hide an opposite useful curvature opportunity.

A verified full KKT point does not receive an extra KKT/Dickinson upward closure from the walk. Such a closure can be mathematically
valid but can hide later curvature opportunities. The walk uses the KKT test only to recognize its endpoint and exact value. All
closures discovered along one path remain buffered until the path stops; only then are non-dominated new intervals inserted. The
ordinary B7 rule has already processed the seed once; its deferred certificate is inserted after the walk closures.

## Live Certificates And Boundary Compaction

Outside a due walk, every exact upward, downward, or Dickinson certificate is added immediately to both live NBC solvers and copied
into a pending vector. A due seed's ordinary certificate and the walk certificates are instead buffered until that walk stops, as
described above. Insertion after either point preserves SAT-B3's cross-frontier pruning. When either frontier exhausts its current cardinality, the
pending and previously active families are compacted:

1. intervals that cannot intersect any later exact low cardinality are discarded;
2. an interval contained in another interval is discarded;
3. if all upward children of a smaller root are covered and that root's cardinality has already been checked exactly, those children
   are replaced by the root's single full-upward closure.

Partial overlap alone is not enough to merge two intervals: their union need not itself be one certified interval. After compaction,
both live Improved NBC instances are rebuilt from the sorting network and retained certificates. Their external prefix stacks remain intact,
so rebuilding cannot repeat an already returned support. This rebuild is what actually removes superseded clauses; NBC's internal
learned-clause cleanup does not understand interval containment.

For one frontier and cardinality, a prefix cube fixes the first $p$ Boolean support variables and leaves the rest free. If Improved NBC returns
a model $x$ from that cube, the remaining cube is partitioned by the first index at which another model differs from $x$. These
disjoint child cubes cover every other assignment exactly once. The stack therefore acts as a resumable iterator without adding a
blocking clause for $x$. Intuitively, it remembers where the search has not looked rather than recording every place already seen.

## Complete Decision Flow

1. Build the reusable exact-cardinality sorting-network clauses and install every failed pair-curvature certificate.
2. Start a low frontier at cardinality $1$ and a high frontier at cardinality $n$.
3. Use the first selected low support and the first selected high support as the two initial KKT-walk seeds. Thereafter alternate
   walk seed sides after gaps of $n,2n,4n,\ldots$ ordinary support visits, resetting the gap to $n$ after a new exact KKT point,
   closure, or witness.
4. When a walk is due, process its selected seed once with the ordinary low or high B7 rule, but defer any resulting certificate.
   Then start the walk from that same seed, take at most $n$ floating-guided active-set steps without backtracking, and avoid the
   current path and supports already covered before the seed was selected. If a floating KKT claim fails exact verification,
   continue exactly. Commit the exact walk closures and deferred seed certificate when the walk stops; do not process the seed again.
5. On an ordinary low step, copy and exactly factor the selected support's principal matrix. Install an upward closure immediately
   when strict face convexity fails. Otherwise reuse that factorization to construct and optimize one Halfspace-Rays Dickinson
   interval, unless an exact witness decides the problem.
6. If the frontiers have not met, process one ordinary high step. Run the floating $LDL^T$ filter first. A floating
   rejection stores nothing. A positive-semidefinite candidate is checked exactly and contributes an immediate downward closure
   only after exact positive definiteness, or exact positive semidefiniteness plus consistency of
   $Bx=\mathbf1$.
7. Alternate steps 5 and 6. When either frontier exhausts a cardinality, compact the retained certificate family, rebuild the live
   solvers, and advance that frontier. A floatingly rejected high support remains available to the exact low frontier.
8. If an exact nonnegative vector with negative quadratic value is found, return not copositive immediately.
9. If an exact nonnegative zero vector is found, record not strictly copositive and continue ordinary CP classification when
   required.
10. Continue the exact low traversal through cardinality $n$. If no witness exists and no support remains, return copositive; return
    strictly copositive unless an exact zero was found.

The proof search is finite because the prefix cubes return every open support in a fixed cardinality at most once, and the exact low
traversal eventually reaches every cardinality. Certificates can only remove still-unexplored cubes or parts of them.

## Exact Representation And Diagnostics

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, inertia
signs, kernel vectors, walk closures, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high supports
for exact positive-semidefiniteness verification and to choose active-set walk steps. A floating rejection is not stored and cannot
affect the low proof or final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, retained NBC exclusions, and the
joint singular-cardinality/nullity distribution. Source diagnostics distinguish low- and high-selected visits and record floating
high rejections, pair-upward, support-upward, Dickinson, and downward certificates. When diagnostics capture is enabled, the model
also retains these events chronologically using the shared support-history contract: exact certificate regions remain compact, and
high-frontier supports without certificates retain their support plus whether floating-point and exact arithmetic inspected them.
Walk source diagnostics additionally identify exact-continuation events, new or repeated KKT endpoints, and committed upward or
downward no-hiding closures. The retained history records every walk support with its walk and step number, seed, arithmetic mode,
floating stationarity and curvature outcomes, exact rank/inertia consequences, feasibility and KKT result, jitter draws, rejected
candidate counts, selected successor, and stopping outcome. A separate terminal record reports the floating screen and any exact
principal-matrix factorization. The path can therefore be replayed without inferring missing heuristic steps from certificate events.

## Known Difficult Inputs

Improved NBC-B9 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward clause. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact
directions are swept before finding only a narrow interval.

Support alternation can expose many large floating principal factorizations before enough small-cardinality exclusions have
accumulated. When certificates are weak, the prefix iterator also asks NBC to solve many separate unexplored cubes. Boundary
compaction limits the durable clause family, but it cannot reduce the number of genuinely open supports. Improved NBC avoids one blocker per
visited support while still applying every proof certificate immediately.

The KKT walk can repeatedly enter the same attraction basin despite jitter, particularly when one stationary point dominates many
seeds. Only a verified KKT endpoint resets the exponential backoff; a retained curvature closure from a dead-end walk does not.
Backoff limits that cost but cannot create another basin. Coverage checks scan the retained exact interval family;
walks are deliberately sparse because this read-only scan is linear in the number of retained certificates. Near-singular floating
systems may also enter exact continuation early and spend the remaining bounded walk in arbitrary-precision arithmetic.
