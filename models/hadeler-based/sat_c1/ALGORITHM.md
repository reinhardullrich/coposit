# SAT-C1

Classification: coposit-created exact CP/SCP experiment. SAT-C1 copies SAT-B3 and adds a curvature search inside every promising
low-frontier Dickinson interval. It uses the Dickinson vector for almost-free one-child curvature tests, then reduces the complete
interval to an exact tangent Schur-residual matrix and searches that smaller monotone problem with a local SAT solver. Floating point
may skip this optional search; it never creates a proof clause. The inherited high frontier remains an opportunistic
positive-semidefiniteness scan whose downward clauses are verified exactly.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

SAT-C1 uses that observation as a search certificate.

- If a face is not strictly convex, neither that support nor any superset can be the support of the chosen minimal-support global
  minimizer. The model removes the whole upward closure.
- If the principal matrix is positive definite, or if it is singular positive semidefinite and its all-ones system is consistent,
  every nonzero nonnegative vector supported inside that face has positive quadratic value. The model removes the whole downward
  closure.
- If a low-frontier face is strictly convex, the retained factorization is reused to build and optimize a Dickinson interval. This
  replaces the former exact-support block. SAT-C1 then looks inside that interval for smaller supports whose face curvature already
  fails. Each such support removes its complete upward closure, including supports beyond the Dickinson interval.
- A high-frontier face is first tested by a floating-point $LDL^T$ filter. A positive-semidefinite candidate is factorized exactly.
  It contributes a downward closure only when exact arithmetic proves positive definiteness, or proves positive semidefiniteness
  together with consistency of $Bx=\mathbf1$. A rejected candidate is skipped only by the high scan and remains available to the
  exact low-frontier proof.

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

The identifier is `sat_c1`.

- **SAT** names the incremental Boolean representation of the unresolved supports.
- **C1** means the first experiment that searches the curvature boundary hidden inside a Dickinson interval.

The model is an independent experiment copied from [`sat_b3`](../sat_b3/ALGORITHM.md). It retains SAT-B3's traversal, low-frontier
Halfspace-Rays machinery, high-frontier downward search, SAT clauses, and deliberate omission of Dickinson work on the high
frontier. Its SAT cardinality network comes from
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
The local grow-and-shrink enumeration follows the monotone-boundary idea used by MARCO; see Mark H. Liffiton, Alessandro Previti,
Ammar Malik, and Joao Marques-Silva, “Fast, Flexible MUS Enumeration,” *Constraints* 21 (2016), 223–250. The tangent elimination is
the standard Schur-complement identity; see Fuzhen Zhang, ed., *The Schur Complement and Its Applications*, Springer, 2005.

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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, SAT-C1 solves
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

When low-frontier curvature does not already remove the upward closure, SAT-C1 reuses the retained exact factorization. For a
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

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. SAT-C1 performs the inherited exact
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
SAT-C1 nevertheless reuses two pieces of information already computed with the certificate.

### An almost-free one-child test

Let $p=Au$, $s=\mathbf1^Tu\neq0$, and let $q=u^TAu$. For an optional index $j\in U\setminus L$, define

$$
v_j=e_j-\frac{u}{s}.
$$

The entries of $v_j$ sum to zero, so it is a tangent direction on the face $L\cup\{j\}$. Its curvature is

$$
v_j^TAv_j=A_{jj}-\frac{2p_j}{s}+\frac{q}{s^2}.
$$

The solution vector, $p$, and $q$ already exist. The implementation tests the sign after multiplying by $s^2$, so it uses only
exact integers and no division. If the result is nonpositive, $L\cup\{j\}$ is exactly proved curvature-bad and its upward closure is
installed immediately.

The intuition is simple: the new coordinate is compared with the normalized old certificate vector. One nonpositive direction is
already enough to show that the enlarged face is not strictly convex. Passing this test proves nothing, because another tangent
direction may still fail.

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

### Local monotone SAT search

Before constructing either the residual or a local SAT solver, a binary64 reduced-Hessian test examines the complete upper support
$U$. If it looks positive definite, SAT-C1 skips the local search. Otherwise, SAT-C1 constructs the exact residual and tests $U$
exactly. An exactly positive-definite $U$ ends the attempt immediately, before any local SAT solver is constructed. Only an exactly
bad $U$ opens a separate small SAT solver representing the optional indices $U\setminus L$. Previously known curvature cores are
loaded first. A previously verified bad core contained in $U$ already proves $U$ exactly bad, so SAT-C1 does not repeat the exact
$U$ factorization in that case. SAT-C1 tries the complete optional set; if those cores block it, one positive-phase SAT solve
supplies a large remaining assignment, which receives the same cheap floating test.
This is deliberately heuristic: the proposal need not expose every bad region, and rounding may miss a useful bad support, but
skipping optional pruning cannot invalidate the global proof. If the floating tests see a possible failure, SAT-C1 constructs the
exact residual and continues with exact queries.

For each remaining local SAT assignment:

1. If its residual principal matrix is not positive definite, delete optional indices one at a time while badness remains. The
   resulting support is inclusion-minimal inside $[L,U]$. Add its upward clause to both the local and global SAT solvers.
2. If its residual principal matrix is positive definite, add optional indices one at a time while goodness remains. Block the
   downward closure of that inclusion-maximal good assignment only in the local SAT solver.
3. Repeat until the local SAT instance is unsatisfiable.

Badness is upward closed and goodness is downward closed. Consequently, a failed deletion or addition never needs to be tried again
during the same shrink or grow pass. The local good clauses are search bookkeeping, not copositivity certificates. Only exactly
verified bad supports reach the global proof.

The intuition is that C1 searches the boundary between bowl-shaped faces and faces with a flat or descending direction. It does not
enumerate all $2^{|U\setminus L|}$ supports individually: one maximal-good block and one minimal-bad block can remove large parts of
the local cube at once.

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

SAT-C1 adds one singular case. Suppose

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
3. Start a low frontier at cardinality $1$ and a high frontier at cardinality $n$.
4. Ask SAT for one unresolved low-frontier support. Advance across empty low layers, but do not exhaust a nonempty layer.
5. Copy and exactly factor its principal matrix. Add an upward closure when strict face convexity fails. Otherwise reuse that
   factorization to construct and optimize one Halfspace-Rays Dickinson interval, unless an exact witness decides the problem.
6. Use the final Dickinson vector for the exact one-child tangent tests. Test the complete upper support in binary64. Only if it
   looks curvature-bad, construct the exact tangent Schur residual and test the complete upper support exactly. Stop immediately if
   it is exactly positive definite. Otherwise, preload a local SAT solver with known curvature cores, request a large remaining
   support inside the interval, search its monotone boundary, and install every exactly verified bad core in the global SAT solver.
7. Ask SAT for one unresolved high-frontier support under the high activation literal. Descend across high layers after every support
   in a layer has either been globally pruned or rejected by the floating filter.
8. Copy its floating principal matrix and run the fast $LDL^T$ filter. Add a high-only rejection when it is not a
   positive-semidefinite candidate. Otherwise copy and exactly factor the integer principal matrix. Add a downward closure after
   exact positive definiteness, or after exact positive semidefiniteness and a successful exact consistency solve for
   $Bx=\mathbf1$.
9. When the high frontier meets the low frontier, stop the optional high scan and let the exact low frontier continue upward.
10. If an exact nonnegative negative-value witness is found, return not copositive.
11. If an exact nonnegative kernel vector is found, record not strictly copositive and continue ordinary CP classification when
    required.
12. Continue the low traversal until no globally unresolved support remains. Then the matrix is copositive; it is strictly
    copositive unless a zero was found.

The proof search is finite because every low-selected support is removed permanently, and the Boolean lattice contains $2^n-1$
nonempty supports. High-only rejections affect only optional high selection and cannot terminate the proof.

## Exact Representation And Diagnostics

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, inertia
signs, kernel vectors, Schur residuals, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high
supports for exact positive-semidefiniteness verification and Dickinson ceilings for optional exact local curvature search. A
floating decision can skip optimization, but it cannot install a global clause or affect the final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, installed SAT exclusions, and the
joint singular-cardinality/nullity distribution. Focused source diagnostics additionally distinguish local curvature searches,
maximal-good local blocks, one-child certificate-vector cuts, and exact higher-order curvature clauses.

## Known Difficult Inputs

SAT-C1 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward clause. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact
directions are swept before finding only a narrow interval.

The C1 addition is difficult when $U\setminus L$ is large and its Schur residual has many incomparable minimal non-positive-definite
principal submatrices. Their number can be exponential, so the local SAT boundary itself can be large. Constructing the residual
also requires one exact factorization of the tangent matrix on $L$ and exact multi-right-hand-side solves; large integer entries can
make that setup more expensive than the clauses it eventually finds. The floating gate deliberately avoids that cost on ceilings
that look strictly convex, at the price of occasionally missing useful optional pruning.

Individual alternation can expose many large floating principal factorizations before small-cardinality exclusions have accumulated.
It can spend time optimizing low Dickinson certificates before a later large positive-definite face supplies a downward closure that
would have removed those supports. SAT clauses remain compact, but a large family of high-only rejections can still make SAT search
expensive.
