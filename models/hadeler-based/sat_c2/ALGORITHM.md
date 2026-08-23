# SAT-C2

Classification: coposit-created exact CP/SCP experiment. SAT-C2 copies SAT-C1 and adds bounded active-set walks that seek useful
stationary faces between ordinary SAT-selected supports. The walks use binary64 only to choose steps and nominate curvature
closures. Every witness and every installed closure is recomputed exactly. Clauses discovered along one walk remain buffered until
the walk ends, while clauses from earlier work remain active and may redirect or stop it.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

SAT-C2 uses that observation in both the inherited SAT-C1 traversal and the new scheduled walks.

- If a face is not strictly convex, neither that support nor any superset can be the support of the chosen minimal-support global
  minimizer. The model removes the whole upward closure.
- If the principal matrix is positive definite, or if it is singular positive semidefinite and its all-ones system is consistent,
  every nonzero nonnegative vector supported inside that face has positive quadratic value. The model removes the whole downward
  closure.
- If a low-frontier face is strictly convex, the retained factorization is reused to build and optimize a Dickinson interval. This
  replaces the former exact-support block. If the interval's upper endpoint is curvature-bad, SAT-C2 follows one descending chain
  inside the interval and installs the smallest exactly bad support found on that chain. Its complete upward closure can extend far
  beyond the Dickinson interval.
- A high-frontier face is first tested by a floating-point $LDL^T$ filter. A positive-semidefinite candidate is factorized exactly.
  It contributes a downward closure only when exact arithmetic proves positive definiteness, or proves positive semidefiniteness
  together with consistency of $Bx=\mathbf1$. A rejected candidate is skipped only by the high scan and remains available to the
  exact low-frontier proof.
- Before ordinary traversal, and periodically thereafter, the model follows one active-set path through unresolved faces. It removes
  a negative used coordinate, drops a zero coordinate, or adds the most violated unused coordinate. A walk never enters a support
  already covered by the global SAT state and never revisits a support on its current path. It has no backtracking and visits at most
  $n$ supports.
- Curvature closures found along a walk are buffered. Otherwise an upward closure at a current support could delete the preferred
  larger successor before the walk reaches it. After the walk stops, every nominated closure is checked exactly, redundant closures
  are discarded, and the remaining closures are committed together.

The remaining supports are represented by one incremental SAT instance. Two cardinality frontiers start at $1$ and $n$. The model
processes one unresolved support from the low frontier, then one from the high frontier, and repeats. An empty low layer advances the
low frontier immediately; an empty high layer moves the high frontier down immediately. Once the high frontier meets the low
frontier, the exact low traversal continues alone until the proof is complete. Before that point, the support order has the form

$$
\text{one from }1,\ \text{one from }n,\ \text{one from }1,\ \text{one from }n,\ldots,
$$

until one frontier exhausts its current layer. This exposes cheap small-face and potentially decisive large-face curvature without
requiring either layer to finish first.

Pruning is directional. A low-frontier support either contributes an upward curvature closure or a Dickinson interval. A
high-frontier support contributes an exactly proved downward strict-copositivity closure or a high-scan-only rejection. It never pays for a
Halfspace-Rays search. A support rejected by the high scan remains available to the low frontier and its exact Dickinson fallback.
Floating point therefore changes only which exact downward checks are attempted; it cannot remove a support from the proof or cause
the traversal to finish.

The additional C1 search obeys the same rule. A floating reduced-Hessian test may decide that an interval is not worth inspecting.
This can miss useful pruning and make the model slower, but it cannot change the final answer. Every curvature core installed in the
global SAT solver has an exact integer proof.

## Name, Sources, And Classification

The identifier is `sat_c2`.

- **SAT** names the incremental Boolean representation of the unresolved supports.
- **C2** means the second curvature experiment: it combines one-chain interval curvature search with SAT-aware stationary-face walks.

The model is an independent experiment copied from [`sat_c1`](../sat_c1/ALGORITHM.md). It retains SAT-C1's alternating traversal,
low-frontier Halfspace-Rays machinery, curvature search inside Dickinson intervals, high-frontier downward search, SAT clauses, and
deliberate omission of Dickinson work on the high frontier. Its SAT cardinality network comes from
[`sat_dickinson`](../sat_dickinson/ALGORITHM.md). Dickinson intervals come from Peter J. C.
Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The inertia tests are standard consequences of inertia
additivity for equality-constrained quadratic forms; see T. S. Han and H. Fujiwara, “An inertia theorem for projected matrices and
its application to constrained optimization,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7).

Using these curvature facts as permanent SAT clauses, alternating the cardinality order, and using the search as a complete exact
CP/SCP classifier are coposit experiments rather than algorithms from those papers. CaDiCaL 2.2.1 supplies incremental SAT solving.
The tangent elimination is the standard Schur-complement identity; see Fuzhen Zhang, ed., *The Schur Complement and Its
Applications*, Springer, 2005.

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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, SAT-C2 solves
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

When low-frontier curvature does not already remove the upward closure, SAT-C2 reuses the retained exact factorization. For a
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

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. SAT-C2 performs the inherited exact
breakpoint sweeps along those directions, preferring larger $|U|$ and then larger width $|U|-|L|$. It retains a bounded shortlist of
coordinate rays and tests at most two complementary combined rays after a coordinate-wise stall. Every accepted candidate is
represented with exact integers; no floating-point comparison enters the search or certificate.

The SAT clause for the interval is

$$
\left(\bigvee_{i\in L(u)}\neg s_i\right)
\lor
\left(\bigvee_{j\notin U(u)}s_j\right)
\lor c_{|U(u)|+1}.
$$

The last literal retires the clause automatically at cardinalities above $|U(u)|$, where no support can lie inside the interval.

## Curvature Search Inside A Dickinson Interval

The ordinary Dickinson clause removes the interval $[L,U]$, so the main traversal will never visit its interior. That is normally
desirable. The possible loss is that a support $M$ inside the interval may already fail strict face convexity. Its curvature clause
would remove every superset of $M$, including supports above and outside $U$, and can therefore be much stronger than the interval
that hid it.

The set $U$ alone gives no curvature theorem: it records signs of $Au$, not the sign of the quadratic form on tangent directions.
SAT-C2 therefore treats $U$ only as a promising large support on which to start a separate curvature check.

### Exact tangent Schur residual

The remaining local search does not refactor every full principal matrix. Choose an anchor $a\in L$. Use the tangent basis

$$
e_i-e_a\quad(i\in L\setminus\{a\}),
\qquad
e_j-e_a\quad(j\in U\setminus L).
$$

In that basis, the reduced Hessian on $U$ has the block form

$$
H_U=
\begin{pmatrix}
B&C\\
C^T&D
\end{pmatrix}.
$$

The lower face is already known to be strictly convex, so $B\succ0$. Eliminate its tangent directions once and form

$$
S=D-C^TB^{-1}C.
$$

For any optional set $Q\subseteq U\setminus L$,

$$
H_{L\cup Q}\succ0
\quad\Longleftrightarrow\quad
S_Q\succ0.
$$

This is the key reduction. The original support $L$ disappears from subsequent curvature tests. Each query factorizes only a
principal submatrix of the residual matrix indexed by the optional coordinates selected in $Q$.

The implementation keeps this exact without storing rationals. Its fraction-free factorization solves

$$
BX=dC,
$$

with an integer matrix $X$ and positive integer denominator $d$. It stores

$$
\widetilde S=dD-C^TX.
$$

Because $d>0$, $S_Q$ and $\widetilde S_Q$ have the same definiteness. Every later residual query therefore uses only exact integer
matrix entries.

### One descending curvature chain

A binary64 reduced-Hessian test first examines the complete upper support $U$. If $U$ looks positive definite, SAT-C2 skips the
optional search. If a previously verified curvature core is already contained in $U$, its upward clause already covers every
superset of that core, so SAT-C2 also skips the interval. Otherwise it constructs the exact residual and tests $U$ exactly. An
exactly positive-definite $U$ ends the attempt.

For an exactly bad $U$, SAT-C2 builds one nested chain

$$
U=S_0\supset S_1\supset\cdots\supset S_t\supseteq L.
$$

It scans the optional indices once in increasing order. An index is deleted when the fast binary64 test still regards the smaller
support as curvature-bad. The lower support $L$ is never changed. This creates at most $|U\setminus L|$ floating-point queries and
does not construct another SAT solver.

The floating chain is only a proposal. Exact badness is upward closed, so along the descending chain the exact states have the form

$$
\text{bad},\ldots,\text{bad},\text{good},\ldots,\text{good}.
$$

The implementation already knows that $S_0=U$ is exactly bad. It tests $S_t$ exactly and, if that support is good, uses binary search
on the stored deletion sequence to locate the last exactly bad member. This costs at most one terminal exact query plus
$\lceil\log_2(|U\setminus L|)\rceil$ exact residual queries after the exact $U$ check. Only that one exactly verified support is
installed as an upward clause.

The result is the smallest bad support on this particular chain, not the globally smallest bad support inside $[L,U]$. A floating
false positive merely makes the exact boundary search move upward; a floating false negative may stop the chain too early and miss
a stronger optional clause. Neither case affects correctness because floating point never installs a clause.

The intuition is the same as Halfspace-Rays: do not optimize the whole combinatorial region. Follow one cheap deterministic path,
keep its best exact point, and continue the main proof.

## Scheduled Stationary-Face Walks

For a support $I$, the walk solves the equality-constrained stationary system on that face,

$$
A_Ix=\lambda\mathbf1,
\qquad
\mathbf1^Tx=1.
$$

The binary64 solve uses a pivoted Bunch--Kaufman $LDL^T$ factorization of the reduced symmetric system. It is only a direction
finder. From the resulting candidate, the walk tries the following one-index moves in deterministic order:

1. remove the used coordinate with the most negative $x_i$;
2. if no coordinate is negative, remove a zero coordinate;
3. if $x$ is positive on the face but some unused index satisfies $(Ax)_j<\lambda$, add the most violated such index.

Ties use the original matrix index. The walk tries the next preferred move when the first one is already on the current path or is
covered by the global SAT state. It does not backtrack. It stops at an exact KKT point, when no allowed preferred move remains, when
the floating solve is inconclusive, or after visiting $n$ supports.

The intuition is an active-set optimization path through the Boolean lattice: negative weights say that the current face is too
large, while a violated unused coordinate says that the face is too small. The global SAT clauses act as walls. Refusing to cross a
wall encourages later walks to enter a different unresolved region instead of returning through a previously certified attraction
region.

At every visited support, floating curvature tests may nominate three exact closures:

- if the reduced Hessian is not positive definite, the complete upward closure;
- if $x\geq0$, $\lambda\geq0$, and the reduced Hessian is positive semidefinite, the complete downward closure;
- if, in addition, every unused coordinate satisfies $(Ax)_j\geq\lambda$, the upward KKT closure from the positive support of $x$.

The last two statements come directly from the face minimum and Dickinson conditions. A feasible stationary point with
positive-semidefinite reduced Hessian is a global minimum on its face, so a nonnegative value proves every subface copositive. The
outside KKT inequalities say that the embedded vector has nonnegative full product after subtracting its nonnegative level, so its
positive support reaches the full Dickinson ceiling.

No floating result installs a clause. A nomination is factorized again with exact integers after the path ends. An exact negative
nonnegative vector stops the complete solve. An exact zero records failure of strict copositivity. When floating point claims a KKT
endpoint but exact arithmetic rejects it, the support is called critical and the remainder of that path uses exact arithmetic.

All closures found by the current walk stay outside SAT until the walk ends. This is essential: an upward closure at $I$ would
otherwise cover every add-successor of $I$, so the walk could destroy the route it was created to explore. Previously installed
global clauses remain active throughout, as requested by the purpose of the walk.

### Initial and alternating schedule

After the exact pair-curvature prepass, SAT-C2 performs two initial walks:

1. from the first globally open singleton;
2. from the full support when it is still open, otherwise from a support of the largest still-open cardinality.

The ordinary low/high SAT-C1 traversal then begins with one global schedule. After every $n-1$ ordinarily processed supports, the
next walk must come from the opposite frontier from the preceding walk. The two initial walks end on the high side, so the first
scheduled walk waits for the next low-frontier support. The following scheduled walk waits for a high-frontier support, and the
pattern continues low, high, low, high. If the counter becomes due while the wrong frontier is selected, the walk is deferred until
the required frontier appears; ordinary processing continues in the meantime.

After a scheduled walk, its seed is processed by the ordinary C1 rule only when the new closures did not already cover it. Thus the
complete low-frontier proof still makes permanent progress. The fixed schedule deliberately does not change frequency according to
whether an earlier walk happened to find a new closure.

## SAT Clauses

Each original index has a Boolean variable $s_i$, true exactly when that index belongs to the selected support. A Batcher bitonic
sorting network supplies exact-cardinality assumptions for any requested layer.

### Upward closure

If the reduced Hessian on $I$ is not positive definite, SAT receives

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

If $B=A_I$ is positive definite, every principal submatrix indexed by a nonempty subset of $I$ is positive definite. SAT therefore
receives

$$
\bigvee_{j\notin I}s_j.
$$

This removes $I$ and every nonempty subset of $I$. For $I=[n]$ the clause is empty, so no support remains and the proof is complete.

The inherited C1 downward rule also covers one singular case. Suppose

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

A floating rejection must not remove a support from the mathematical proof because rounding can reject an exactly positive-semidefinite
matrix. SAT therefore has one high-frontier activation variable $h$. The rejection clause at $k=|I|$ is

$$
\left(\bigvee_{i\in I}\neg s_i\right)\lor c_{k+1}\lor\neg h,
$$

where $c_{k+1}$ is the sorting-network output meaning that at least $k+1$ indices are selected. At cardinality $k$, $c_{k+1}$ is
false. High queries assume $h$, so the clause skips exactly $I$ in future high scans. Low queries assume $\neg h$, disabling the
clause completely. Thus a rejected support is still handled by the exact low-frontier proof. This uses one SAT solver and one extra
literal per high rejection; it does not duplicate the cardinality network.

## Complete Decision Flow

1. Build one incremental SAT instance and one exact-cardinality sorting network.
2. Install every failed pair-curvature clause.
3. Run one bounded walk from the first open singleton and one from the full support or the largest open replacement. Buffer all
   candidate closures, verify them exactly, and commit them only after each walk ends.
4. Start one global walk schedule with gap $n-1$ and require its first scheduled walk to use the low frontier. Start the ordinary
   low frontier at cardinality $1$ and high frontier at cardinality $n$.
5. Ask SAT for one unresolved low-frontier support. Advance across empty low layers, but do not exhaust a nonempty layer. When the
   global schedule is due for the low side, walk first from this support without entering globally covered faces, reset the counter,
   and require the next scheduled walk to use the high side.
6. If the walk did not cover its seed, copy and exactly factor that principal matrix. Add an upward closure when strict face
   convexity fails. Otherwise reuse that
   factorization to construct and optimize one Halfspace-Rays Dickinson interval, unless an exact witness decides the problem.
7. Test the Dickinson upper support in binary64. Only if it looks curvature-bad, construct the exact tangent Schur residual and test
   the upper support exactly. For an exactly bad upper support, follow one floating deletion chain, recover its exact bad/good
   boundary by binary search, and install its one smallest exactly bad member in the global SAT solver.
8. Ask SAT for one unresolved high-frontier support under the high activation literal. Descend across high layers after every support
   in a layer has either been globally pruned or rejected by the floating filter. When the global schedule is due for the high side,
   apply the same bounded globally-open walk before ordinary high processing, then require the next scheduled walk to use the low
   side.
9. If the walk did not cover its seed, copy its floating principal matrix and run the fast $LDL^T$ filter. Add a high-only rejection
   when it is not a
   positive-semidefinite candidate. Otherwise copy and exactly factor the integer principal matrix. Add a downward closure after
   exact positive definiteness, or after exact positive semidefiniteness and a successful exact consistency solve for
   $Bx=\mathbf1$.
10. When the high frontier meets the low frontier, stop the optional high scan and let the exact low frontier continue upward.
11. If an exact nonnegative negative-value witness is found anywhere, return not copositive.
12. If an exact nonnegative zero is found, record not strictly copositive and continue ordinary CP classification when
    required.
13. Continue the low traversal until no globally unresolved support remains. Then the matrix is copositive; it is strictly
    copositive unless a zero was found.

The proof search is finite because every low-selected support is removed permanently, and the Boolean lattice contains $2^n-1$
nonempty supports. High-only rejections affect only optional high selection and cannot terminate the proof.

## Exact Representation And Diagnostics

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, inertia
signs, kernel vectors, Schur residuals, and witnesses use arbitrary-precision integers. Binary64 is used only to choose walk steps,
nominate walk curvature checks, nominate high supports for exact positive-semidefiniteness verification, screen Dickinson ceilings,
and choose deletions on the optional curvature chain. A floating decision can skip optimization, but it cannot install a global
clause or affect the final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, installed SAT exclusions, and the
joint singular-cardinality/nullity distribution. Walk diagnostics identify seeds, steps, critical points, termination reasons, and
the exact upward or downward closures committed at the end. Focused source diagnostics additionally distinguish curvature-chain
searches and their exact higher-order curvature clauses.

## Known Difficult Inputs

SAT-C2 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward clause. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact
directions are swept before finding only a narrow interval.

The C1-derived addition is difficult when $U\setminus L$ is large. Constructing the residual requires one exact factorization of the
tangent matrix on $L$ and exact multi-right-hand-side solves; large integer entries can make that setup more expensive than the one
clause it eventually finds. A single deletion order can also miss a much smaller incomparable curvature core. The floating gate
deliberately avoids the setup cost on ceilings that look strictly convex, at the price of occasionally missing useful pruning.

Individual alternation can expose many large floating principal factorizations before small-cardinality exclusions have accumulated.
It can spend time optimizing low Dickinson certificates before a later large positive-definite face supplies a downward closure that
would have removed those supports. SAT clauses remain compact, but a large family of high-only rejections can still make SAT search
expensive.

The C2 walks are heuristic. A preferred successor may already be globally covered even though another route through that covered
region would eventually reach a new stationary face; C2 deliberately refuses that route to reduce repeated attraction to known
regions. A floating solve can also be inconclusive near a singular face, in which case the walk stops without optional pruning.
Exact verification of many floating curvature nominations can cost more than the closures save. The fixed $n-1$ spacing is a
heuristic and does not prove that this scheduling frequency is optimal.
