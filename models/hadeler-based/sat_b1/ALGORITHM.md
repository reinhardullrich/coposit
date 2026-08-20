# SAT-B1

Classification: coposit-created exact CP/SCP experiment. It copies SAT-Halfspace-Rays Dickinson and adds exact strict-convex-face
exclusions before its expensive right-hand-side search.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Dickinson's certificate depends on a vector obtained from a principal matrix. SAT-Halfspace-Rays searches exact right-hand-side
directions for a vector whose Dickinson interval covers many supports. SAT-B1 first asks a cheaper structural question: can the
quadratic form be strictly convex on this simplex face?

If the answer is no, no larger face containing it can be strictly convex. Every quadratic function on the compact simplex has a
global minimizer in the relative interior of some strictly convex face. Therefore the current support and all its supersets can be
removed from the search for a decisive minimizer support. This is a **curvature exclusion**, not a claim that each removed principal
submatrix is independently copositive.

The test uses inertia already produced by the exact principal factorization. When it excludes a support, the model avoids all unit
solves and ray sweeps for that support. Only a support whose face remains strictly convex reaches the unchanged Halfspace-Rays path.

A coordinate search can become stuck even when two individually unhelpful directions complement one another. This model remembers a
bounded number of promising coordinate-ray points from the final unsuccessful pass. It chooses the two most complementary pairs,
forms one exact combined direction from each pair, and sweeps those directions. Thus it pays for at most two additional exact sweeps,
not for a two-dimensional optimization problem.

Before traversal, the same curvature rule is specialized to every pair. A failed pair blocks every support containing that pair with
one permanent SAT clause. All curvature, witness, and Dickinson decisions use exact integers.

## Name, Sources, And Classification

The identifier is `sat_b1`.

- **SAT** names the persistent Boolean representation of the supports not yet covered by certificates.
- **B1** means the first experiment in the second, curvature-based line of SAT models.

The model is an independent copy and experimental variation of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). Dickinson intervals come from Peter J. C. Dickinson,
“A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The exact inertia reductions are standard constrained-
quadratic-form consequences of inertia additivity; see, for example, T. S. Han and H. Fujiwara, “An inertia theorem for projected
matrices and its application to constrained optimization,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7). The pair exclusions are the edge inequalities of the
convexity graph used in standard quadratic programming. Their use as permanent SAT exclusions before Dickinson traversal is a
coposit experiment, not part of those papers or Dickinson's algorithm.

CaDiCaL 2.2.1 supplies incremental SAT solving. Exact-cardinality layers use the same Batcher bitonic sorting network as
`sat_dickinson`.

## Strict-Convex-Face Exclusion

For a nonempty support $I$, let $B=A_I$. The tangent space of its simplex face is

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
$$

The face restriction is strictly convex exactly when

$$
v^TBv>0\qquad\text{for every nonzero }v\in\mathcal T_I.
$$

Equivalently, if the columns of $Z$ span $\mathcal T_I$, the reduced Hessian $Z^TBZ$ is positive definite. SAT-B1 never constructs
$Z^TBZ$. It reads the answer from the retained exact factorization of $B$.

If $B$ is nonsingular, let

$$
\delta=\mathbf1^TB^{-1}\mathbf1.
$$

The reduced Hessian is positive definite exactly in either of these cases:

1. $B$ is positive definite; or
2. $B$ has exactly one negative eigenvalue and $\delta<0$.

The ordinary Dickinson solve already computes an integer numerator vector for $B^{-1}\mathbf1$ with a positive common denominator.
Thus the sign of $\delta$ is simply the sign of the sum of those numerators; no extra solve or matrix is needed.

If $B$ is singular, let $q=\operatorname{nullity}(B)$. The reduced Hessian is positive definite exactly when

1. $q=1$;
2. $B$ is positive semidefinite; and
3. for a nonzero kernel vector $z$, $\mathbf1^Tz\neq0$.

For $q\geq2$, the kernel necessarily meets $\mathcal T_I$ nontrivially. For $q=1$ and $\mathbf1^Tz=0$, the kernel vector itself is a
zero-curvature tangent direction. If $q=1$ and $\mathbf1^Tz\neq0$, the kernel is transverse to the tangent space, so the nonzero
inertia of $B$ is exactly the inertia of the reduced Hessian.

If the reduced Hessian is not positive definite, SAT receives

$$
\bigvee_{i\in I}\neg z_i,
$$

which blocks precisely every support containing $I$. This upward closure is sound because strict convexity is inherited by every
subface: if a larger face were strictly convex, its restriction to $\mathcal T_I$ would also be strictly convex.

This does not lose a negative or zero global minimum. Choose a global minimizer with inclusion-minimal support $S$. First-order
optimality and global minimality make the reduced Hessian on $S$ positive semidefinite. If it had a zero tangent direction, moving
along that direction until the boundary would preserve the minimum and produce a smaller support, a contradiction. Hence the
minimal-support minimizer lies in a strictly convex face, and SAT-B1 cannot exclude $S$.

### Pair prepass

For $I=\{i,j\}$, the tangent space is spanned by $(1,-1)$, so strict convexity is the single exact inequality

$$
c_{ij}=A_{ii}+A_{jj}-2A_{ij}>0.
$$

Whenever $c_{ij}\leq0$, the model installs $\neg z_i\lor\neg z_j$ before the first SAT solve. The prepass costs $O(n^2)$ immediate
integer additions and comparisons and can remove large upward regions without any principal factorization.

## Dickinson Intervals

For a full vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)
$$

and

$$
U(u)=\{j:(Au)_j\geq0\}.
$$

If $u$ has at least one positive coordinate and $L(u)\subseteq U(u)$, Dickinson's theorem certifies every support in

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

The interval contains $2^d$ supports, where

$$
d=|U|-|L|.
$$

The exact objective is lexicographic:

$$
(|U|,d).
$$

A larger $|U|$ always wins. Only when $|U|$ is tied does the larger width $d$ win. This puts future cardinality layers first while
still taking additional interval coverage when the upper endpoint does not move.

## Reusing One Exact Factorization

Let $I\subseteq[n]$, $k=|I|$, and suppose the principal matrix $A_I$ is nonsingular. For a strictly positive right-hand side $b$, solve

$$
A_Ix=b
$$

and form $u\in\mathbb R^n$ by placing $x$ on $I$ and zero outside $I$. Then

$$
(Au)_I=b>0,
$$

so $I\subseteq U(u)$. The remaining conditions are

$$
(Au)_{I^c}=A_{I^c,I}A_I^{-1}b\geq0.
$$

Each outside condition is a linear halfspace in $b$. The model starts with $b=\mathbf1$ and factors $A_I$ once. The same exact
factorization solves all unit systems

$$
A_Id_r=e_r,\qquad r=1,\ldots,k.
$$

These columns represent $A_I^{-1}$ with a shared positive integer denominator. The model also calculates the zero-extended products
$A(d_r)^I$. It never refactors $A_I$ during the coordinate or synthesized-ray searches.

## Exact Coordinate Sweeps

From the current exact representatives $x$ and $Au$, direction $d_r$ gives

$$
x(t)=x+t d_r,
\qquad
Au(t)=Au+tA(d_r)^I,
\qquad t\geq0.
$$

Every coordinate is affine in $t$. Its zero or sign can change only at an exact positive root. The model collects all such roots from
the $k$ entries of $x(t)$ and the $n$ entries of $Au(t)$, sorts them by exact cross multiplication, and evaluates:

1. one point before the first root;
2. every root;
3. one point between consecutive roots; and
4. one point after the last root.

These points represent every distinct pair of supports $L(t)$ and $U(t)$ on the ray. Counts are updated from sign-change events; full
vectors are materialized only for an accepted move.

A coordinate move is accepted only when it strictly improves $(|U|,d)$ in lexicographic order. The coordinates are swept repeatedly
until a complete pass accepts no move or $d=n-1$, the largest possible width of an admissible certificate. Both score components are
bounded integers, so this loop terminates.

If an exact point has no positive entry in $x(t)$, then $-x(t)$ is a nonnegative vector with negative quadratic value. The model stops
immediately with a valid non-copositivity witness.

## The Adaptive Ray Shortlist

During the final coordinate pass, the base point does not change. For each coordinate direction, the model remembers its best exact
point even when that point does not increase $|U|$, provided it gains at least one index that is currently outside $U$.

Let $n$ be the matrix order and $k=|I|$. At most

$$
s(n,k)=\min\left\{k,\ 64,\ \left\lceil3\sqrt n\right\rceil\right\}
$$

coordinate-ray candidates are retained. They are ranked by:

1. more newly gained upper indices;
2. fewer lost upper indices;
3. larger resulting $|U|$;
4. larger resulting width $d$; and
5. smaller coordinate index for deterministic ties.

The cap of 64 bounds the pair scan by

$$
\binom{64}{2}=2016
$$

pairs. Before scoring pairs, the model materializes each shortlisted upper support once as a packed bit set. Pair scoring then uses
only sign membership, not arbitrary-precision arithmetic.

## Two Synthesized Rays

For shortlisted candidates $p$ and $q$, let $G_p,G_q$ be the indices they newly gain and $R_p,R_q$ the indices they lose. Pairs are
ranked lexicographically by:

1. larger $|G_p\cup G_q|$;
2. smaller $|R_p\cap R_q|$;
3. larger $|G_p|+|G_q|$;
4. smaller $|R_p\cup R_q|$; and
5. the coordinate pair for deterministic ties.

Only the best two distinct pairs are used. If the saved exact steps are $t_p,t_q>0$, the synthesized direction is

$$
w=t_p d_p+t_q d_q.
$$

The implementation clears denominators and common integer content, then performs the same complete exact breakpoint sweep along

$$
x(\alpha)=x+\alpha w,\qquad \alpha\geq0.
$$

Both synthesized rays are searched from the unchanged stalled base point. The model applies only the better result, and only if it
strictly improves $(|U|,d)$. It stops early if the first ray reaches $d=n-1$. No further ordinary coordinate pass follows; the extra
work at one support is therefore bounded by two sweeps.

The pair score is a cheap prediction. The actual combined signs are recomputed exactly because the union of the two separate gains is
not, by itself, a mathematical guarantee that their sum gains those indices.

## Singular Supports

If $A_I$ is singular, the model first recovers one exact nullspace vector. A one-signed vector is still used immediately to record a
nonnegative zero and therefore disprove SCP. After that decisive check, the reduced-Hessian criterion is applied. Most
higher-nullity supports are excluded at this point without a full matrix product.

Only a singular support whose reduced Hessian is positive definite continues to the inherited singular Dickinson rule. The model:

1. recover one nonzero nullspace vector;
2. retain its nonnegative orientation, when one exists, and record that SCP is false;
3. otherwise compute the full product exactly, compare the two orientations $u$ and $-u$, and choose the one with larger $|U|$;
   and
4. insert its Dickinson interval.

For a mixed-sign vector, let $p$, $m$, and $z$ count the positive, negative, and zero entries of $Au$. Then
$|U(u)|=p+z$ and $|U(-u)|=m+z$, so the implementation chooses $-u$ exactly when $m>p$. A tie retains the factorization orientation.
The product for the chosen negative orientation is obtained by exact negation, not a second matrix product.

The choice of one nullspace vector is deterministic. A higher-dimensional kernel never reaches this rule because it already proves
that the face is not strictly convex. This model does not add kernel-cone or affine-companion searches.

## Persistent SAT Representation

One Boolean variable records whether each matrix index belongs to the current support. A bitonic sorting network exposes the exact
cardinality. Cardinalities are visited in increasing order.

For a certificate $[L,U]$, SAT receives the clause

$$
\bigvee_{i\in L}\neg z_i
\;\lor\;
\bigvee_{j\notin U}z_j.
$$

It is false exactly for assignments $J$ with $L\subseteq J\subseteq U$, so those supports are not returned. The clause also contains
the existing cardinality-network output for $|U|+1$. That literal becomes true after cardinality $|U|$, making the expired interval
cheap for later SAT calls without changing its earlier meaning.

A curvature exclusion has $U=[n]$, so its clause contains only the negative literals for $L=I$ and remains useful at every later
cardinality. A Dickinson clause certifies each support in its interval. A curvature clause instead proves that no support in its
upward closure can be the inclusion-minimal support of a global minimizer. SAT can combine the two clause families, but their proof
meanings must not be confused.

Every support returned by SAT is sent to exact matrix processing. A negative witness terminates the requested predicate. Otherwise a
new exact interval is added and traversal continues. Exhausting all cardinalities proves the remaining requested facts.

## Complete Decision Flow

First, inspect every pair $\{i,j\}$ and install the permanent curvature clause whenever
$A_{ii}+A_{jj}-2A_{ij}\leq0$.

Then, for each cardinality $k=1,\ldots,n$:

1. Ask SAT for an uncovered support $I$ of cardinality $k$.
2. Factor $A_I$ exactly.
3. If $A_I$ is singular, recover one kernel vector and apply any immediate exact zero decision.
4. If the exact inertia and kernel-sum test says that the reduced Hessian is not positive definite, add the curvature exclusion
   $[I,[n]]$ and continue without Halfspace-Rays.
5. If $A_I$ is nonsingular, solve the all-ones system and reject immediately if it supplies a nonnegative negative witness.
6. Apply the nonsingular reduced-Hessian test using the factorization inertia and the already-computed sum for $\delta$; on failure,
   add $[I,[n]]$ and continue.
7. Otherwise run the unchanged singular Dickinson rule or nonsingular Halfspace-Rays search.
8. Add its one exact Dickinson interval and continue until SAT has no uncovered support at that cardinality.

In `both` mode, ordinary copositivity failure ends the traversal immediately. A nonnegative zero changes the SCP result to false while
the same traversal continues until CP is decided.

## Exact Representation And Termination

All matrix entries, pair curvatures, inertia counts, kernel sums, factorization state, directions, breakpoints, coefficients,
witnesses, and sign decisions use exact integers. Rational steps are represented by a positive numerator and denominator. Common
integer content is removed after accepted moves to limit coefficient growth. Floating point is used only to compute the harmless
shortlist size $\lceil3\sqrt n\rceil$; it does not enter any mathematical decision.

The Boolean support space is finite. Coordinate ascent accepts only a strict lexicographic increase of the bounded integer pair
$(|U|,d)$. Synthesized-ray work is bounded by two sweeps at each nonsingular support. Therefore the model terminates unless an
external timeout or resource limit stops it first.

## Known Difficult Inputs

- A useful combined direction can involve a coordinate candidate outside the adaptive shortlist, a pair ranked below the first two,
  different positive weights, or three or more directions. The heuristic then misses it, but the ordinary exact certificate remains
  valid.
- Large nonsingular supports require $k$ exact unit solves and up to $k$ full direction products. The two extra sweeps are bounded, but
  constructing the original direction family can still dominate.
- Exact breakpoint numerators and denominators can grow substantially when matrix entries have many digits or several moves are
  accepted.
- When most visited faces are strictly convex, SAT-B1 pays the $O(k)$ inertia/$\delta$ scan at each nonsingular support and then does
  exactly the same expensive work as SAT-Halfspace-Rays.
- The $O(n^2)$ pair prepass can be noticeable on very large matrices that preprocessing does not already decide, even when it finds
  few failed pairs.
- Curvature exclusions may overlap heavily. CaDiCaL must still maintain every installed clause; excluding more supports is useful
  only when that saving exceeds the clause-management cost.
- On inputs whose valid intervals remain narrow, SAT may still return a large part of the Boolean lattice.
