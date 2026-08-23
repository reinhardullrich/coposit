# NBC-B7

Classification: coposit-created exact CP/SCP experiment. NBC-B7 alternates between low and high cardinalities. The low frontier uses
face curvature first and falls back to an exact Halfspace-Rays Dickinson certificate when curvature alone cannot prune upward. The
high frontier is an opportunistic floating-point positive-semidefiniteness scan; only candidates for downward pruning are verified
exactly.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

NBC-B7 uses that observation as a search certificate.

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

The remaining supports are represented by NBC MiniSat All. Two cardinality frontiers start at $1$ and $n$. The model alternates one
open low support and one open high support, exactly as SAT-B3 does. Every new certificate enters both live NBC searches immediately,
so a downward certificate found at the high frontier can remove low candidates before the low cardinality is exhausted. The same
certificate is also retained outside NBC for compaction when either frontier finishes a cardinality.

Once the high frontier meets the low frontier, the exact low traversal continues alone until the proof is complete. The processing
order has the form

$$
\text{one from low},\ \text{one from high},\ \text{one from low},\ \text{one from high},\ldots.
$$

No cardinality is materialized as a list. Each frontier stores only a stack of disjoint unexplored prefix cubes. After NBC returns a
model, those cubes advance past it, so the next query cannot return the same support. In particular, the model never inserts an
exact-support clause merely to request another support.

Pruning is directional. A low-frontier support either contributes an upward curvature closure or a Dickinson interval. A
high-frontier support contributes an exactly proved downward strict-copositivity closure or a high-scan-only rejection. It never pays for a
Halfspace-Rays search. A support rejected by the high scan remains available to the low frontier and its exact Dickinson fallback.
Floating point therefore changes only which exact downward checks are attempted; it cannot remove a support from the proof or cause
the traversal to finish.

## Name, Sources, And Classification

The identifier is `nbc_b7`.

- **NBC** names the adapted NBC MiniSat All Boolean solver.
- **B7** marks the seventh experiment in the curvature-based B line.

The model is an independent experiment copied from [`sat_b3`](../sat_b3/ALGORITHM.md). It preserves SAT-B3's two-frontier traversal,
floating high-frontier screen, exact nonsingular and singular downward tests, low-frontier Halfspace-Rays machinery, and Boolean
clauses. Its only mathematical-control-flow change is the Boolean engine: NBC MiniSat All replaces CaDiCaL. Dickinson intervals come
from Peter J. C.
Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The inertia tests are standard consequences of inertia
additivity for equality-constrained quadratic forms; see T. S. Han and H. Fujiwara, “An inertia theorem for projected matrices and
its application to constrained optimization,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7).

Using these curvature facts as permanent Boolean clauses, alternating individual low and high supports, and using the search as a complete exact
CP/SCP classifier are coposit experiments rather than algorithms from those papers. The Boolean engine adapts Takahisa Toda's NBC
MiniSat All 1.0.2, itself based on MiniSat-C 1.14.1. The local wrapper asks NBC for one model in one unexplored prefix cube at a time.
Certificates affect both live frontiers immediately and remain recorded outside NBC for later compaction.

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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, NBC-B7 solves
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

When low-frontier curvature does not already remove the upward closure, NBC-B7 reuses the retained exact factorization. For a
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

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. NBC-B7 performs the inherited exact
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

NBC-B7 retains SAT-B3's singular case. Suppose

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

## Live Certificates And Boundary Compaction

Every exact upward, downward, or Dickinson certificate is added immediately to both live NBC solvers and copied into a pending
vector. Immediate insertion preserves SAT-B3's cross-frontier pruning. When either frontier exhausts its current cardinality, the
pending and previously active families are compacted:

1. intervals that cannot intersect any later exact low cardinality are discarded;
2. an interval contained in another interval is discarded;
3. if all upward children of a smaller root are covered and that root's cardinality has already been checked exactly, those children
   are replaced by the root's single full-upward closure.

Partial overlap alone is not enough to merge two intervals: their union need not itself be one certified interval. After compaction,
both live NBC instances are rebuilt from the sorting network and retained certificates. Their external prefix stacks remain intact,
so rebuilding cannot repeat an already returned support. This rebuild is what actually removes superseded clauses; NBC's internal
learned-clause cleanup does not understand interval containment.

For one frontier and cardinality, a prefix cube fixes the first $p$ Boolean support variables and leaves the rest free. If NBC returns
a model $x$ from that cube, the remaining cube is partitioned by the first index at which another model differs from $x$. These
disjoint child cubes cover every other assignment exactly once. The stack therefore acts as a resumable iterator without adding a
blocking clause for $x$. Intuitively, it remembers where the search has not looked rather than recording every place already seen.

## Complete Decision Flow

1. Build the reusable exact-cardinality sorting-network clauses and install every failed pair-curvature certificate.
2. Start a low frontier at cardinality $1$ and a high frontier at cardinality $n$.
3. Ask the low frontier for one open support. Copy and exactly factor its principal matrix. Install an upward closure immediately
   when strict face convexity fails. Otherwise reuse that
   factorization to construct and optimize one Halfspace-Rays Dickinson interval, unless an exact witness decides the problem.
4. If the frontiers have not met, ask the high frontier for one open support. Run the floating $LDL^T$ filter first. A floating
   rejection stores nothing. A positive-semidefinite candidate is checked exactly and contributes an immediate downward closure
   only after exact positive definiteness, or exact positive semidefiniteness plus consistency of
   $Bx=\mathbf1$.
5. Alternate steps 3 and 4. When either frontier exhausts a cardinality, compact the retained certificate family, rebuild the live
   solvers, and advance that frontier. A floatingly rejected high support remains available to the exact low frontier.
6. If an exact nonnegative negative-value witness is found, return not copositive immediately.
7. If an exact nonnegative kernel vector is found, record not strictly copositive and continue ordinary CP classification when
   required.
8. Continue the exact low traversal through cardinality $n$. If no witness exists and no support remains, return copositive; return
    strictly copositive unless an exact zero was found.

The proof search is finite because the prefix cubes return every open support in a fixed cardinality at most once, and the exact low
traversal eventually reaches every cardinality. Certificates can only remove still-unexplored cubes or parts of them.

## Exact Representation And Diagnostics

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, inertia
signs, kernel vectors, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high supports for exact
positive-semidefiniteness verification. A floating rejection is not stored and cannot affect the low proof or final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, retained NBC exclusions, and the
joint singular-cardinality/nullity distribution. Source diagnostics distinguish low- and high-selected visits and record floating
high rejections, pair-upward, support-upward, Dickinson, and downward certificates.

## Known Difficult Inputs

NBC-B7 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward clause. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact
directions are swept before finding only a narrow interval.

Support alternation can expose many large floating principal factorizations before enough small-cardinality exclusions have
accumulated. When certificates are weak, the prefix iterator also asks NBC to solve many separate unexplored cubes. Boundary
compaction limits the durable clause family, but it cannot reduce the number of genuinely open supports. NBC avoids one blocker per
visited support while still applying every proof certificate immediately.
