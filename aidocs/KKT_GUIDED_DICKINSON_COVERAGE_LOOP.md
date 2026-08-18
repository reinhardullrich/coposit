# KKT-Guided Dickinson Coverage Loop

## Status And Purpose

This is a research design, not a description of an implemented model.

The proposed solver alternates between two kinds of work:

1. a numerical heuristic proposes supports carrying useful KKT points;
2. exact arithmetic turns valid proposals into upward or downward blocks;
3. SAT returns a support that remains uncovered;
4. ordinary Dickinson processing supplies a certificate when the heuristic does not.

The objective is not to replace Dickinson's complete certificate. It is to give Dickinson a small number of unusually wide blocks
before and during its exact traversal, and to direct later heuristic searches toward parts of the Boolean lattice that remain open.

## The Optimization Problem

For a symmetric matrix $A\in\mathbb S^n$, consider the standard quadratic problem

$$
\min_{x\in\Delta_n} q(x):=x^TAx,
\qquad
\Delta_n:=\{x\in\mathbb R_+^n:\mathbf1^Tx=1\}.
$$

This is the same problem that FracESSA expresses as maximization of $x^T(-A)x$.

Let

$$
S=\operatorname{supp}(x)=\{i:x_i>0\}
$$

be the actual support of a candidate. A relative-interior KKT point on face $S$ satisfies

$$
A_{SS}x_S=\lambda\mathbf1,
\qquad
\mathbf1^Tx_S=1,
\qquad
x_S>0,
$$

where

$$
\lambda=x^TAx.
$$

It is a **full KKT point** of the whole simplex when it additionally satisfies

$$
(Ax)_j\geq\lambda
\qquad\text{for every }j\notin S.
$$

The word *full* here refers to checking every unused coordinate. It does not mean that $S=[n]$.

## Two Different Pruning Geometries

Upward and downward pruning have different mathematical justifications.

### Downward: a safe convex face

Let

$$
T_S:=\{d\in\mathbb R^S:\mathbf1^Td=0\}
$$

be the directions that redistribute mass while staying in face $S$. The quadratic form is convex on the face exactly when

$$
d^TA_{SS}d\geq0
\qquad\text{for every }d\in T_S.
$$

If the face is convex, a relative-interior KKT point is a global minimum on that face. Therefore

$$
\lambda\geq0
\quad\Longrightarrow\quad
z^TAz\geq0\quad\text{for every }z\in\Delta_n\text{ supported in }S.
$$

Every subface is contained in face $S$, so all supports $J\subseteq S$ are safe. The resulting downward block is

$$
[\varnothing,S].
$$

This argument says nothing about supersets of $S$. Adding a new coordinate can introduce a new direction of negative curvature.

Positive semidefiniteness on $T_S$ is sufficient. Positive definiteness only adds uniqueness of the face minimum.

### Upward: a Dickinson interval

For a vector $u$, define

$$
L(u):=\operatorname{supp}(u),
\qquad
U(u):=N_A(u):=\{i:(Au)_i\geq0\}.
$$

Dickinson's certificate covers every support in the Boolean interval

$$
[L(u),U(u)]
=
\{I:L(u)\subseteq I\subseteq U(u)\}.
$$

The intuition comes from an inclusion-minimal bad support. If $A$ is not copositive, at least one support is minimal among those
containing a negative nonnegative vector. Dickinson's lemma implies that such a minimal bad support cannot contain a vector $u$ that
has a positive coordinate and satisfies

$$
A_Iu_I\geq0.
$$

Whenever $L(u)\subseteq I\subseteq U(u)$, the same $u$ lives inside $I$ and satisfies

$$
A_Iu_I=(Au)_I\geq0.
$$

Thus $I$ cannot be the support on which noncopositivity appears for the first time. This does **not** necessarily prove that each
covered $A_I$ is copositive in isolation. It proves that none of those supports can be an inclusion-minimal obstruction. Covering
every nonempty support therefore rules out every possible obstruction.

### Why a full KKT point reaches the ceiling

If $\lambda>0$, set

$$
u=\frac{x}{\lambda}.
$$

The full KKT conditions give

$$
(Au)_i=1\quad(i\in S),
\qquad
(Au)_j\geq1\quad(j\notin S).
$$

Hence $U(u)=[n]$ and the upward block is

$$
[S,[n]].
$$

If $\lambda=0$, the full KKT conditions give $Ax\geq0$. The unscaled vector $x$ again yields $[S,[n]]$ and simultaneously proves
that strict copositivity is false. If $\lambda<0$, $x$ is already an exact noncopositivity witness and the solver stops.

The upward conclusion uses only first-order KKT information. It remains valid when the point is a saddle or a maximum on its face.
Convexity is needed only for the additional downward block.

## What One Middle Support Actually Covers

Suppose $n=25$ and $|S|=12$. If $S$ gives both a full-ceiling upward block and a downward block, then it covers

$$
2^{12}-1=4{,}095
$$

nonempty subsets of $S$ and

$$
2^{25-12}=8{,}192
$$

supersets of $S$. The two families intersect at $S$, so their union contains $12{,}286$ nonempty supports. The full lattice contains

$$
2^{25}-1=33{,}554{,}431
$$

nonempty supports. Moreover, there are $\binom{25}{12}=5{,}200{,}300$ different supports of cardinality 12, and this certificate
removes only the particular support $S$ from that layer.

Many well-distributed KKT supports can remove much more, but even hundreds of them may leave large incomparable regions. The relevant
quantity is therefore new coverage, not the number of KKT points found.

## Why Finding All KKT Points First Is Not Enough

If every full KKT point of the standard quadratic problem were known exactly, comparing their values would determine the global
minimum. The difficulty is proving that the list is complete. A numerical multi-start search can propose KKT points but cannot prove
that none were missed. Complete KKT enumeration is itself an exponential global method in the difficult cases.

An initial KKT-only phase also receives no information about which parts of the support lattice its certificates failed to cover.
It may repeatedly find points whose blocks overlap almost completely.

The proposed loop avoids both problems:

- KKT discovery remains heuristic and bounded;
- ordinary Dickinson remains the complete exact method;
- the current uncovered family tells the heuristic where another certificate would be useful.

## Representing The Remaining Supports

Introduce one Boolean variable $z_i$ for each matrix index. A support is represented by

$$
I=\{i:z_i=1\}.
$$

A general Dickinson interval $[L,U]$ is removed by the clause

$$
\left(\bigvee_{i\in L}\neg z_i\right)
\;\vee\;
\left(\bigvee_{j\notin U}z_j\right).
$$

The clause says that a remaining support must either omit an index required by $L$, or include an index forbidden by $U$.

The two special blocks simplify to:

- upward block $[S,[n]]$:
  $$
  \bigvee_{i\in S}\neg z_i;
  $$
- downward block $[\varnothing,S]$:
  $$
  \bigvee_{j\notin S}z_j.
  $$

SAT therefore represents the exact family of supports not yet accounted for. SAT returning `unsatisfiable` means that every nonempty
support is covered.

## The Alternating Coverage Loop

The complete proposed control flow is:

1. Run shared preprocessing. Stop on an exact decision.
2. Initialize the SAT representation of all nonempty supports.
3. Run a bounded initial KKT-support search from sparse and dense starting points.
4. Verify every promising support exactly and insert every useful non-dominated block.
5. Ask SAT for an uncovered support $I$.
6. If no uncovered support exists, the Dickinson certificate is complete; return copositive, together with the separately maintained
   strict result.
7. Use $I$ as a seed for a small local KKT-support search:
   - solve the floating face-KKT system on $I$;
   - try a bounded number of add/drop pivots;
   - inspect nearby supports that remain uncovered;
   - prefer candidates predicted to add substantial new coverage.
8. Verify every proposed candidate exactly. Depending on the exact result:
   - a negative value is a noncopositivity witness;
   - a zero value disproves strict copositivity;
   - a nonnegative full KKT point inserts $[S,[n]]$;
   - a nonnegative KKT point on a convex face inserts $[\varnothing,S]$;
   - a nonnegative face-KKT point may additionally insert its ordinary Dickinson interval $[L,U]$.
9. If the heuristic inserted a worthwhile block, return to step 5.
10. Otherwise process the original uncovered support $I$ with ordinary exact Dickinson. It either finds a witness or inserts a valid
    interval covering $I$.
11. Return to step 5.

In compact form:

```text
bounded KKT proposals
        |
        v
exactly verified upward/downward blocks
        |
        v
SAT chooses an uncovered support
        |
        +--> bounded KKT search around that gap --> useful exact block --+
        |                                                             |
        +--> no useful block --> ordinary exact Dickinson interval ----+
                                                                      |
                                                                      v
                                                        SAT chooses the next gap
```

The loop is finite in exact arithmetic because ordinary Dickinson is always available as the fallback. Whenever the heuristic does
not advance the proof, processing the SAT-selected uncovered support either terminates with a witness or covers at least that support.
Resource limits can still leave the computation unresolved.

## Heuristic KKT-Support Oracle

The numerical stage returns support proposals, not trusted numerical answers. The KKT conditions can be written as a complementarity
system

$$
w=Ax-\lambda\mathbf1,
\qquad
x\geq0,
\qquad
w\geq0,
\qquad
\mathbf1^Tx=1,
\qquad
x_iw_i=0\quad(i\in[n]).
$$

Here $S=\{i:x_i>0\}$ is the unknown support. For $i\in S$, complementarity forces $w_i=0$ and hence
$A_{SS}x_S=\lambda\mathbf1$. For $j\notin S$, the required inequality is $w_j=(Ax)_j-\lambda\geq0$. Searching for a KKT point is
therefore largely the discrete problem of finding the correct support.

### A direct support-pivot search

Given a proposed support $S$, solve the bordered system approximately:

$$
\begin{pmatrix}
A_{SS}&-\mathbf1\\
\mathbf1^T&0
\end{pmatrix}
\begin{pmatrix}
x_S\\ \lambda
\end{pmatrix}
=
\begin{pmatrix}
0\\1
\end{pmatrix}.
$$

The smallest useful search then pivots through supports:

1. If some component of $x_S$ is nonpositive, propose removing one such index.
2. Otherwise $S$ carries a relative-interior face-KKT point. Compute every outside residual
   $r_j=(Ax)_j-\lambda$ for $j\notin S$.
3. If some $r_j<0$, propose adding one violating index.
4. If every $r_j\geq0$, record $S$ as a full-KKT proposal.

This is a primal-dual active-set or complementarity-pivot heuristic specialized to the simplex. It need not be a descent method:
solving the face equations can propose saddle-type and maximum-type face-stationary points as well as minima. That matters because
every exact full KKT point with nonnegative value can yield an upward certificate, while a negative one terminates with a witness;
a pure minimization method tends to find only local minima.

Different starts and different choices among offending indices can reach different supports. The first version should retain only a
small bounded number of alternatives. A visited-support set prevents cycling. Suitable seeds are SAT-selected uncovered supports,
supports returned by the existing Frank-Wolfe machinery, sparse and dense random supports, and one-index neighbours of successful
supports.

Sequential minimal optimization (SMO) is a useful second seed generator. It changes only two simplex coordinates per iteration, and
each two-variable quadratic subproblem has an analytic solution. Bisori, Lapucci, and Sciandrone prove convergence of their selection
rule to stationary points and report that a simple multistart strategy is effective for standard quadratic problems. SMO remains a
local minimization method, so it complements rather than replaces the support-pivot search.

### Restricting the search to SAT-uncovered faces

Let $\mathcal F$ be the family of supports satisfying the currently active SAT clauses. Geometrically, the corresponding feasible
region is a union of simplex faces, generally disconnected and nonconvex. Projecting a numerical point directly onto that union would
introduce another combinatorial optimization problem. SAT should therefore remain outside the continuous search and control only
support changes.

For a SAT-selected seed $S_0\in\mathcal F$, run a bounded support-pivot search. Whenever the numerical oracle proposes a new support
$S'$, apply the following gate:

1. If $S'$ satisfies the current clauses, accept the pivot and continue from $S'$.
2. If $S'$ is covered, reject that pivot and try another offending add/drop index.
3. If every one-index alternative is covered, optionally try a bounded number of two-index repairs.
4. If no repair remains, stop this local search and process the original uncovered support $S_0$ by ordinary exact Dickinson.

Checking a fully specified proposed support does not require a fresh SAT search: the active clauses can be evaluated directly, or the
same assignment can be tested through incremental SAT assumptions when the implementation already keeps the clauses only inside the
solver. SAT search is needed to obtain the next uncovered seed, not for every interior numerical iteration. In particular, an SMO
step that changes values without changing the support needs no SAT interaction.

Rejecting every covered intermediate support is a performance policy, not a completeness requirement. A path through a covered face
could reach a useful uncovered face or produce a non-subsumed certificate. The first model should not pay for that possibility. One
covered bridge step can be tested later if diagnostics show that the strict gate ends many otherwise promising searches.

The first seed policy should also stay simple:

- request a small uncovered support to seek a small lower set and a large upward block;
- request a large uncovered support to seek a large convex face and a large downward block;
- occasionally use one unconstrained or randomized uncovered support for diversity.

Exact nearest-support search would be a MaxSAT problem. It is unnecessary until one- and two-index alternatives have been measured
and shown inadequate.

### Exact acceptance and singular systems

The floating vector is never evidence. The exact verifier reconstructs the candidate from $S$, checks positivity on its actual
support, checks every outside inequality, evaluates $\lambda$, and performs the exact tangent-space semidefiniteness test when
downward pruning is requested. A singular bordered system may be explored numerically with a rank-revealing solve, but exact
acceptance requires the existing affine-family machinery rather than an arbitrary pseudoinverse solution.

### Literature basis and apparent new combination

The individual ingredients are established:

- Cristofari, De Santis, Lucidi, and Rinaldi give a nonconvex simplex active-set framework and instantiate it with Frank-Wolfe,
  away-step Frank-Wolfe, and projected-gradient directions: [*An Active-Set Algorithmic Framework for Non-Convex Optimization
  Problems over the Simplex*](https://arxiv.org/abs/1703.07761).
- Liang combines projected-gradient exploration with a reduced-space conjugate-gradient phase after the active set appears stable:
  [*Gradient Projection for Solving Quadratic Programs with Standard Simplex Constraints*](https://arxiv.org/abs/2006.06934).
- Bisori, Lapucci, and Sciandrone specialize SMO to the standard quadratic problem:
  [*A study on sequential minimal optimization methods for standard quadratic
  problems*](https://flore.unifi.it/handle/2158/1247901).
- Júdice, Sessa, and Fukushima formulate the standard quadratic problem through linear complementarity constraints and compute a
  sequence of stationary points before a separate global-certification phase:
  [*A two-phase sequential algorithm for global optimization of the standard quadratic programming
  problem*](https://link.springer.com/article/10.1007/s10898-024-01423-y).

The literature search found active-set KKT search, complementarity formulations, and Boolean face traversal separately. It did not
find a method that repeatedly restricts KKT-support discovery to the dynamically changing SAT family left uncovered by Dickinson
intervals. The SAT-gated coverage loop should therefore be described as a coposit research synthesis, not yet as a proved literature
novelty.

### Recommended first experiment

The first experiment does not need a general nonlinear optimizer, SMO, MaxSAT, or a beam search. On a fixed support the stationary
equations are linear, so the minimum useful oracle is:

1. Ask SAT for one uncovered support $S_0$.
2. Solve the bordered face system in floating point.
3. If some $x_i\leq0$, try dropping the most negative component first.
4. Otherwise, if some outside residual $r_j<0$, try adding the most negative residual first.
5. If the preferred pivot is covered, try the next few offending indices in sorted order.
6. Stop after a small fixed pivot budget or when a support repeats.
7. Exactly verify only the resulting face-KKT or full-KKT support.
8. On failure, run ordinary Dickinson on $S_0$.

This directly tests the central hypothesis: whether SAT-selected gaps lie near certificate-producing KKT supports. More elaborate
local solvers are justified only if diagnostics show that this cheap oracle frequently approaches stationarity but exhausts its
pivot budget.

## Choosing Which Certificates To Activate

The goal is maximum **marginal coverage** of the currently uncovered family. Raw interval width is only a proxy because intervals may
overlap heavily.

A practical first scoring order is:

1. exact witness or nonnegative zero;
2. full-ceiling interval with small lower support;
3. downward block with large upper support;
4. general interval with large $|U|-|L|$;
5. predicted new coverage after accounting for active clauses.

Before activation, discard exact duplicates and intervals subsumed by an active interval. Expensive exact model counting is not a
necessary first implementation; width, subsumption, and a small sample of SAT solutions provide cheaper guidance.

Restricting proposals to uncovered supports is a performance policy, not a mathematical requirement. A covered support can sometimes
generate a non-dominated certificate, particularly when its exact vector loses coordinates or extends beyond an earlier bounded upper
set. The first model should focus on uncovered supports, while diagnostics should record enough information to test later whether
occasional probes of covered supports are worthwhile.

## Correctness Boundary

Numerical search may miss every useful KKT point without compromising correctness. Only exact results modify the proof state.

The exact solver may conclude:

- **not copositive**, only from an exact negative nonnegative witness;
- **copositive but not strict**, only after complete ordinary coverage plus an exact nonnegative zero;
- **strictly copositive**, only after complete coverage and the strict Dickinson conditions;
- **unresolved**, when a time, memory, or other resource limit interrupts the proof.

The KKT oracle changes search order and coverage efficiency. It does not change these conclusions.

## Main Open Performance Questions

1. How much new coverage do numerically discovered KKT supports add after existing Dickinson certificates are active?
2. Is a SAT-selected uncovered support a better KKT seed than independent random or Frank-Wolfe starts?
3. How often does a full KKT point also lie on a convex face and therefore produce both blocks?
4. Does exact tangent-space semidefiniteness cost less than the exact support work it removes?
5. How often do KKT proposals overlap so strongly that clause and exact-verification overhead exceeds their benefit?
6. Should the heuristic run after every new gap, once per cardinality, or only after a measured Dickinson/SAT stall?
7. How often can a covered support generate a genuinely non-dominated interval that the uncovered-only policy misses?

The first experiment should answer these questions with strict budgets and diagnostics before any larger architecture is promoted.
The worst case remains exponential because exact copositivity testing is co-NP-complete; the loop is intended to improve difficult
instances, not to create a polynomial-time guarantee.
