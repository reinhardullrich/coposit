# SAT-C4

Classification: coposit-created exact CP/SCP experiment. SAT-C4 copies SAT-B3 and changes only the nonsingular low-frontier fallback.
It tests curvature at three successively optimized Dickinson upper endpoints: the traditional all-ones endpoint, the Halfspace
endpoint, and the synthesized-rays endpoint. The first endpoint whose reduced Hessian is not positive definite gives a full upward
closure. The model always completes the Halfspace-Rays optimization and also stores its final Dickinson interval exactly as SAT-B3
does.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Every point of the standard simplex lies in the relative interior of exactly one face, identified by the indices of its positive
coordinates. A quadratic function on the simplex has a global minimizer. Among all global minimizers, choose one whose support is as
small as possible. On that support the quadratic form must be strictly convex along the face: otherwise a flat or descending tangent
direction reaches the boundary without increasing the value and produces a minimizer with smaller support.

SAT-C4 uses that observation as a search certificate.

- If a face is not strictly convex, neither that support nor any superset can be the support of the chosen minimal-support global
  minimizer. The model removes the whole upward closure.
- If the principal matrix is positive definite, or if it is singular positive semidefinite and its all-ones system is consistent,
  every nonzero nonnegative vector supported inside that face has positive quadratic value. The model removes the whole downward
  closure.
- If a low-frontier face is strictly convex, the retained factorization is reused to build and optimize a Dickinson interval. This
  replaces the former exact-support block. SAT-C4 checks the interval's upper endpoint before each more expensive optimization
  stage until it finds the first curvature-bad endpoint. That endpoint contributes its full upward closure, while the final
  Halfspace-Rays vector always contributes the optimized Dickinson interval.
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
Floating point changes only which exact downward and endpoint-curvature checks are attempted; it cannot install a clause, remove a
support from the proof, or cause the traversal to finish.

## Name, Sources, And Classification

The identifier is `sat_c4`.

- **SAT** names the incremental Boolean representation of the unresolved supports.
- **C4** means the fourth experiment in the SAT line that uses Dickinson certificates to seek stronger curvature closures.

The model is an independent experiment copied from [`sat_b3`](../sat_b3/ALGORITHM.md). It retains SAT-B3's alternating traversal,
high-frontier downward checks, singular-support behavior, exact Halfspace-Rays search, and SAT clauses. Its only mathematical change
is to test the reduced Hessian at each distinct nonsingular Dickinson upper endpoint before continuing to the next optimization
stage. Its SAT cardinality network comes from
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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, SAT-C4 solves
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

## Staged Low-Frontier Endpoint Checks

When low-frontier curvature does not already remove the upward closure, SAT-C4 reuses the retained exact factorization. For a
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

For a nonsingular $B$, SAT-C4 uses three stages.

1. **Traditional endpoint.** Use $Bx=\mathbf1$, embed $x$ as $u$, and form $U_0=U(u)$.
2. **Halfspace endpoint.** Reuse the same factorization to solve all coordinate right-hand sides and run
   the inherited exact breakpoint sweeps. They prefer larger $|U|$ and then larger width $|U|-|L|$. The resulting endpoint is $U_1$.
3. **Synthesized-rays endpoint.** Use the retained bounded shortlist to test at most two complementary
   combined rays. The final endpoint is $U_2$.

At every distinct endpoint $U_r$, form a basis of the tangent space of that simplex face and test its reduced Hessian. In the
implementation the last index of $U_r$ is the anchor, so the exact reduced Hessian is the $(|U_r|-1)\times(|U_r|-1)$ matrix

$$
(H_{U_r})_{pq}=A_{pq}-A_{pa}-A_{aq}+A_{aa},
\qquad p,q\in U_r\setminus\{a\}.
$$

If $H_{U_r}$ is not positive definite, strict convexity already fails on $U_r$. It therefore fails on every superset of $U_r$, and
the model installs the upward closure $[U_r,[n]]$. It nevertheless completes the remaining Halfspace-Rays optimization and stores
the final Dickinson interval $[L(u),U_2]$, because that interval can cover supports below its endpoint that the upward closure does
not. The objective makes $|U_0|,|U_1|,|U_2|$ nondecreasing, so the first curvature-bad endpoint has the lowest cardinality among the
staged endpoints. Later curvature tests are unnecessary after that first exact hit, but the Dickinson optimization is not.

The intuition is to use Dickinson as a sequence of increasingly expensive jumps. After each jump, curvature asks whether the landing
point opens a route all the way to the ceiling. The earliest successful landing starts that route as low as these staged endpoints
permit; the final Dickinson interval is a second, complementary cut through the lattice and is retained independently.

Endpoint screening first uses the already prepared binary64 copy of $A$. A floating result can only nominate an endpoint for exact
factorization. Every installed upward closure is proved by the exact integer reduced Hessian. Equal endpoints are checked once. An
endpoint equal to the selected root is skipped because the root curvature was already proved positive definite, and the full
endpoint $[n]$ is skipped because its Dickinson interval already reaches the ceiling.

The singular-support path is unchanged from SAT-B3: it uses the recovered kernel ray and installs its Dickinson interval without the
three endpoint stages. Every accepted Dickinson vector and breakpoint comparison is represented with exact integers.

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

The same clause is used when $I$ is one of the three Dickinson upper endpoints. In that case the endpoint is not the support that
was originally selected by SAT; it is a larger face reached algebraically from that support. Exact reduced-Hessian failure is still
the complete justification for the upward closure.

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

SAT-C4 inherits SAT-B3's singular downward case. Suppose

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
5. Copy and exactly factor its principal matrix. Add an upward closure when strict face convexity fails. Otherwise construct the
   traditional Dickinson endpoint and test its curvature. If it is curvature-bad, store the endpoint's upward closure.
6. Always run the exact Halfspace sweeps and then at most two synthesized-ray sweeps. Until the first curvature-bad endpoint is
   found, test each new distinct endpoint and store the first such upward closure. Always store the final Halfspace-Rays Dickinson
   interval as well.
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

All proof-producing matrix entries, factorization state, right-hand-side directions, breakpoint comparisons, products, reduced
Hessians, inertia signs, kernel vectors, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high
supports for exact positive-semidefiniteness verification and endpoint faces for exact reduced-Hessian verification. A floating
endpoint result can merely skip optional curvature pruning or trigger an exact check; it cannot install a clause. A floating high
rejection is guarded by the high activation literal and cannot affect the low proof or final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, installed SAT exclusions, and the
joint singular-cardinality/nullity distribution. Source diagnostics distinguish low- and high-selected visits, including which side
selected a support when the frontiers meet, as well as floating high rejections, pair-upward, root support-upward, traditional-
endpoint-upward, Halfspace-endpoint-upward, rays-endpoint-upward, Dickinson, downward, and exact-support clauses in the focused model
tests.

## Known Difficult Inputs

SAT-C4 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward clause.

The new endpoint checks lose when most endpoint faces are strictly convex. A floating ambiguity then triggers a second exact
factorization without adding coverage. Even when an endpoint is curvature-bad, a large endpoint near the Boolean ceiling may add
little beyond the Dickinson interval that reached it. The model deliberately checks only the three endpoints already generated by
SAT-B3; it does not search downward for a minimal curvature-bad subset or create another SAT instance.

Individual alternation can expose many large floating principal factorizations before small-cardinality exclusions have accumulated.
It can spend time optimizing low Dickinson certificates before a later large positive-definite face supplies a downward closure that
would have removed those supports. SAT clauses remain compact, but a large family of high-only rejections can still make SAT search
expensive.
