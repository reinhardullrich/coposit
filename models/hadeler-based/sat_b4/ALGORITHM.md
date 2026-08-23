# SAT-B4

Classification: coposit-created exact CP/SCP experiment. SAT-B4 copies SAT-B3's alternating low/high traversal and adds one exact
Johnson--Reams block reduction on positive-definite supports from either frontier. When the block's minimizing map preserves the
nonnegative orthant, the original matrix is replaced by its smaller Schur complement and classification restarts on that matrix.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

SAT-B4 uses that observation as a search certificate.

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
- Before the usual frontier-specific action for an exact positive-definite block, SAT-B4 tests whether that block can be eliminated
  from the complete copositivity problem. A successful test replaces the current problem by an exactly equivalent lower-order
  matrix; a failed low test continues to Halfspace-Rays and a failed high test continues to the ordinary downward closure.

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

## Name, Sources, And Classification

The identifier is `sat_b4`.

- **SAT** names the incremental Boolean representation of the unresolved supports.
- **B4** means the fourth experiment in the curvature-based SAT line.

The model is an independent experiment copied from [`sat_b3`](../sat_b3/ALGORITHM.md). It retains SAT-B3's traversal, curvature
rules, low-frontier Halfspace-Rays machinery, SAT clauses, and singular positive-semidefinite downward certificate. Its only
mathematical addition is the exact positive-definite block reduction described below. Its SAT cardinality network comes from
[`sat_dickinson`](../sat_dickinson/ALGORITHM.md). Dickinson intervals come from Peter J. C.
Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The block theorem is Theorem 4 of Charles R. Johnson and Robert Reams, “Spectral theory of copositive matrices,” *Linear Algebra and
its Applications* 395 (2005), 275–281, DOI
[`10.1016/j.laa.2004.08.008`](https://doi.org/10.1016/j.laa.2004.08.008). Immanuel M. Bomze presents its dimension-reduction role and
generalizations in “Perron--Frobenius property of copositive matrices, and a block copositivity criterion,” *Linear Algebra and its
Applications* 429 (2008), 68–71, DOI [`10.1016/j.laa.2008.02.003`](https://doi.org/10.1016/j.laa.2008.02.003).

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The inertia tests are standard consequences of inertia
additivity for equality-constrained quadratic forms; see T. S. Han and H. Fujiwara, “An inertia theorem for projected matrices and
its application to constrained optimization,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7).

Using these curvature facts as permanent SAT clauses, alternating the cardinality order, and using the search as a complete exact
CP/SCP classifier are coposit experiments rather than algorithms from those papers. CaDiCaL 2.2.1 supplies incremental SAT solving.

## Exact Positive-Definite Block Reduction

After permuting a selected high-frontier support to the first coordinates, write the full matrix as

$$
A=\begin{pmatrix}B&C\\C^T&D\end{pmatrix},
$$

where $B$ is the selected principal block. Suppose exact factorization proves $B\succ0$. Define

$$
W=-B^{-1}C,
\qquad
S=D-C^TB^{-1}C.
$$

SAT-B4 applies the reduction only when $W\geq0$ entrywise. Completing the square gives

$$
\begin{pmatrix}x\\y\end{pmatrix}^{\!T}
A
\begin{pmatrix}x\\y\end{pmatrix}
=(x-Wy)^TB(x-Wy)+y^TSy.
$$

The first term is nonnegative because $B\succ0$. More importantly, $W\geq0$ makes $x=Wy$ feasible whenever $y\geq0$, so the lower
bound is attained inside the nonnegative orthant. Therefore

$$
A\text{ is copositive}\iff S\text{ is copositive},
$$

and the same equivalence holds for strict copositivity. Intuitively, $W\geq0$ says that minimizing over the eliminated coordinates
never asks those coordinates to become negative. The entire $B$ block can then be optimized away without losing a feasible witness.

The implementation constructs the reduction without rational storage. Let $p=|\det B|$. The retained factorization solves

$$
BX=-pC,
$$

so $X=pW$ is an integer matrix and $W\geq0$ is exactly the entrywise test $X\geq0$. The reduced integer matrix is

$$
R=pD+C^TX=pS.
$$

Because $p>0$, $R$ has exactly the same CP and SCP classification as $S$. SAT-B4 divides out the positive common integer content of
$R$, destroys the current SAT search, and starts a fresh classification loop on the smaller matrix. The restart is iterative rather
than recursive, so a chain of reductions retains only the active matrix and SAT solver. No result from the floating high-frontier
filter can trigger this reduction: floating arithmetic only nominates the support, while positive definiteness, $X\geq0$, and every
entry of $R$ are established exactly.

Before solving all columns of $BX=-pC$, SAT-B4 solves the single all-ones system

$$
Bz=p\mathbf1.
$$

For an outside column $c$ of $C$, the condition $W=-B^{-1}C\geq0$ would imply

$$
c^TB^{-1}\mathbf1\leq0.
$$

Consequently, the integer sign test $c^Tz>0$ disproves the reduction immediately. Passing this necessary prefilter does not prove
$W\geq0$; the complete exact multiple-right-hand-side solve still follows. When the prefilter rejects a low-frontier block, its
already computed $z$ is reused by the Halfspace-Rays fallback instead of solving the all-ones system again.

The check is confined to supports already factorized exactly and proved positive definite. No extra factorization and no floating
decision are introduced. A successful low block may remove only one or a few coordinates, but iterative restart permits a chain of
such reductions. A successful high block gives a larger immediate dimension reduction. A failed sign test falls directly back to
SAT-B3's existing low or high action.

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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, SAT-B4 solves
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

When low-frontier curvature does not already remove the upward closure, SAT-B4 reuses the retained exact factorization. For a
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

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. SAT-B4 performs the inherited exact
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

SAT-B4 adds one singular case. Suppose

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
5. Copy and exactly factor its principal matrix. Add an upward closure when strict face convexity fails. For a positive-definite
   block with nonempty complement, first apply the exact all-ones prefilter and then test the complete Johnson--Reams sign condition.
   On success, discard the current SAT state and restart iteratively on the Schur complement. Otherwise reuse the factorization and
   all-ones solution to construct and optimize one Halfspace-Rays Dickinson interval, unless an exact witness decides the problem.
6. Ask SAT for one unresolved high-frontier support under the high activation literal. Descend across high layers after every support
   in a layer has either been globally pruned or rejected by the floating filter.
7. Copy its floating principal matrix and run the fast $LDL^T$ filter. Add a high-only rejection when it is not a
   positive-semidefinite candidate. Otherwise copy and exactly factor the integer principal matrix. For a positive-definite block
   with nonempty complement, apply the exact prefilter and Johnson--Reams sign condition. On success, construct the exact Schur
   complement and restart iteratively; on failure, add the ordinary downward closure. For a singular positive-semidefinite block, add a
   downward closure after a successful exact consistency solve for $Bx=\mathbf1$.
8. When the high frontier meets the low frontier, stop the optional high scan and let the exact low frontier continue upward.
9. If an exact nonnegative negative-value witness is found, return not copositive.
10. If an exact nonnegative kernel vector is found, record not strictly copositive and continue ordinary CP classification when
    required.
11. Continue the low traversal until no globally unresolved support remains. Then the matrix is copositive; it is strictly
    copositive unless a zero was found.

The proof search is finite because every low-selected support is removed permanently, and the Boolean lattice contains $2^n-1$
nonempty supports. High-only rejections affect only optional high selection and cannot terminate the proof.

## Exact Representation And Diagnostics

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, inertia
signs, kernel vectors, Schur reductions, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high
supports for exact positive-semidefiniteness verification. A floating rejection is guarded by the high activation literal and cannot
affect the low proof or final classification.

Runtime diagnostics report the active reduced problem's current low or high cardinality, selected and processed supports, installed
SAT exclusions, and joint singular-cardinality/nullity distribution. Source diagnostics distinguish low- and high-selected visits,
including which side selected a support when the frontiers meet, as well as floating high rejections, Schur reductions, pair-upward,
support-upward, Dickinson, downward, and exact-support clauses in the focused model tests.

## Known Difficult Inputs

SAT-B4 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. The block reduction is restrictive: a positive-definite block may still have a mixed-sign map $-B^{-1}C$. The
all-ones prefilter rejects many such failures before the exact multiple-right-hand-side solve; failures that pass the prefilter still
pay for that solve before falling back to Halfspace-Rays on the low frontier or the ordinary downward clause on the high frontier.
Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that produces no downward
clause. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact directions are swept
before finding only a narrow interval.

Individual alternation can expose many large floating principal factorizations before small-cardinality exclusions have accumulated.
It can spend time optimizing low Dickinson certificates before a later large positive-definite face supplies a downward closure that
would have removed those supports. SAT clauses remain compact, but a large family of high-only rejections can still make SAT search
expensive.
