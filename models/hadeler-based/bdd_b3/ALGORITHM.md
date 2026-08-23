# BDD-B3

Classification: coposit-created exact CP/SCP experiment. BDD-B3 alternates between low and high cardinalities. The low frontier uses
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

BDD-B3 uses that observation as a search certificate.

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

The remaining supports are represented by ordinary reduced ordered binary decision diagrams (BDDs). One root stores the globally
covered family, while separate low- and high-frontier roots retain the unresolved supports at their current cardinalities. Floating
rejections are subtracted only from the live high root; they never affect low queries and disappear when the high cardinality
changes. Two cardinality frontiers start at $1$ and $n$. The model processes one unresolved support from the low frontier, then one
from the high frontier, and repeats. An empty low layer advances the low frontier immediately; an empty high layer moves the high
frontier down immediately. Once the high frontier meets the low frontier, the exact low traversal continues alone until the proof is
complete. Before that point, the support order has the form

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

The identifier is `bdd_b3`.

- **BDD** names the reduced ordered binary decision diagram used for support coverage and selection.
- **B3** identifies the unchanged mathematical algorithm copied from SAT-B3.

The model is an independent experiment copied from [`sat_b3`](../sat_b3/ALGORITHM.md). It changes only the Boolean-lattice backend.
The curvature rules, floating high filter, exact verification, alternating traversal, low-frontier Halfspace-Rays machinery,
high-frontier behavior, witnesses, and stopping conditions are byte-for-byte copies where they do not call the backend. The BDD
reduction, union, difference, exact-cardinality construction, and low-before-high support extraction are copied from
[`bdd_dickinson`](../bdd_dickinson/ALGORITHM.md) and follow Randal E. Bryant, “Graph-Based Algorithms for Boolean Function
Manipulation,” *IEEE Transactions on Computers* C-35(8), 1986, 677–691. Dickinson intervals come from Peter J. C.
Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI
[`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The existence of a global minimizer in the relative interior of a strictly convex face is Theorem 1 of Andrea Scozzari and Fabio
Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). The inertia tests are standard consequences of inertia
additivity for equality-constrained quadratic forms; see T. S. Han and H. Fujiwara, “An inertia theorem for projected matrices and
its application to constrained optimization,” *Linear Algebra and its Applications* 72 (1985), 47–58, DOI
[`10.1016/0024-3795(85)90141-7`](https://doi.org/10.1016/0024-3795(85)90141-7).

Using these curvature facts as permanent Boolean-family exclusions, alternating the cardinality order, and using the search as a
complete exact CP/SCP classifier are coposit experiments rather than algorithms from those papers.

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
factorization confirms it. On the low frontier it proceeds to the Dickinson construction instead. In the second case, BDD-B3 solves
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
positive-semidefinite singular support proceeds to exact verification. If the all-ones system is inconsistent, no downward closure is
installed and the support remains available to the low frontier. If the low-frontier conditions hold and either $z\geq0$ or
$-z\geq0$, the embedded kernel vector is an exact copositive zero. It disproves strict copositivity but not ordinary copositivity. On
the low frontier, the kernel ray is oriented toward the larger Dickinson upper set and stored as an interval.

## Low-Frontier Dickinson Fallback

When low-frontier curvature does not already remove the upward closure, BDD-B3 reuses the retained exact factorization. For a
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

For a nonsingular $B$, the same factorization also solves all coordinate right-hand sides. BDD-B3 performs the inherited exact
breakpoint sweeps along those directions, preferring larger $|U|$ and then larger width $|U|-|L|$. It retains a bounded shortlist of
coordinate rays and tests at most two complementary combined rays after a coordinate-wise stall. Every accepted candidate is
represented with exact integers; no floating-point comparison enters the search or certificate.

The BDD stores the interval's characteristic function directly. A support variable is fixed true for every index in $L(u)$, fixed
false for every index outside $U(u)$, and free for every index in $U(u)\setminus L(u)$. Ordinary BDD reduction removes every free
variable, so one isolated interval needs at most

$$
|L(u)|+n-|U(u)|
$$

nonterminal nodes before sharing. The interval is also placed in an expiry bucket indexed by $|U(u)|$. Once the low frontier has
advanced beyond that cardinality, neither frontier can revisit a support in the interval, so its live contribution may be removed.

## BDD Support Families

Each original index has a Boolean variable $s_i$, true exactly when that index belongs to the selected support. The BDD terminal zero
is false and terminal one is true. Ordinary reduction removes a node when its low and high children are equal; a unique table shares
nodes with the same variable and children. A memoized recurrence constructs the family $K_k$ of all supports of cardinality $k$.
If $C$ is the globally covered family, the next low candidate comes from $K_k\setminus C$.

### Upward closure

If the reduced Hessian on $I$ is not positive definite, the BDD receives the upward interval

$$
[I,[n]].
$$

This removes $I$ and every support containing it. The rule is sound because strict convexity is inherited by subfaces: if a larger
face containing $I$ were strictly convex, its restriction to $\mathcal T_I$ would also be strictly convex.

Before traversal, the model applies the same rule to every pair. For $I=\{i,j\}$, strict face convexity is the single exact test

$$
A_{ii}+A_{jj}-2A_{ij}>0.
$$

Every failing pair immediately contributes $[\{i,j\},[n]]$.

### Downward closure

If $B=A_I$ is positive definite, every principal submatrix indexed by a nonempty subset of $I$ is positive definite. The BDD
therefore receives the downward interval

$$
[\varnothing,I].
$$

This removes $I$ and every nonempty subset of $I$. The represented interval also contains the empty support, which is harmless because
the traversal queries only cardinalities $1$ through $n$. For $I=[n]$, no nonempty support remains and the proof is complete.

BDD-B3 adds one singular case. Suppose

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
copositive, and the same downward interval is valid. This is an elementary consequence of
$\operatorname{range}(B)=\ker(B)^\perp$ for symmetric matrices.

Strict convexity only on the simplex face is still not enough for this downward rule. The implementation requires either positive
definiteness of the entire principal matrix or the exact singular positive-semidefinite consistency certificate above.

### Floating high-frontier filter

Before the first high query, the complete integer matrix is converted once to a symmetric binary64 matrix using one common
power-of-two scale. Every high support then copies only its floating principal submatrix and runs an unpivoted $LDL^T$
positive-semidefiniteness filter. Its pivot margin is relative to the selected submatrix size and largest magnitude, so an unrelated
large entry elsewhere cannot suppress a useful candidate. A comfortably negative pivot rejects the candidate. A near-zero pivot is
accepted only when the remaining residual column is also near zero, as positive semidefiniteness requires. Acceptance is not a
certificate: the integer principal matrix is then copied and factorized exactly before any downward closure is installed.

A floating rejection must not remove a support from the mathematical proof because rounding can reject an exactly
positive-semidefinite matrix. BDD-B3 therefore keeps a separate exact-support family $H_k$ for the current high cardinality. The next
high candidate comes from

$$
K_k\setminus(C\cup H_k),
$$

whereas the low frontier always uses $K_k\setminus C$ and never sees $H_k$. When the high frontier leaves cardinality $k$, $H_k$ is
discarded. Thus floating rejection changes only the optional high scan and cannot contribute to the final proof.

## Complete Decision Flow

1. Build one ordinary BDD manager, an initially empty global covered-family root, and empty live low- and high-frontier roots.
2. Install every failed pair-curvature upward interval.
3. Start a low frontier at cardinality $1$ and a high frontier at cardinality $n$.
4. Extract one support from $K_{\rm low}\setminus C$. Advance across empty low layers, but do not exhaust a nonempty layer.
5. Copy and exactly factor its principal matrix. Add an upward closure when strict face convexity fails. Otherwise reuse that
   factorization to construct and optimize one Halfspace-Rays Dickinson interval, unless an exact witness decides the problem.
6. Extract one support from $K_{\rm high}\setminus(C\cup H_{\rm high})$. Descend across high layers after every support in a layer
   has either been globally pruned or rejected by the floating filter; discard those high-only rejections when reinitializing the
   live high root on descent.
7. Copy its floating principal matrix and run the fast $LDL^T$ filter. Add a high-only rejection when it is not a
   positive-semidefinite candidate. Otherwise copy and exactly factor the integer principal matrix. Add a downward closure after
   exact positive definiteness, or after exact positive semidefiniteness and a successful exact consistency solve for
   $Bx=\mathbf1$.
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
signs, kernel vectors, and witnesses use arbitrary-precision integers. Binary64 is used only to nominate high supports for exact
positive-semidefiniteness verification. A floating rejection enters only the disposable high-layer BDD and cannot affect the low
proof or final classification.

Runtime diagnostics report the current low or high cardinality, selected and processed supports, installed BDD exclusions, and the
joint singular-cardinality/nullity distribution. Source diagnostics distinguish low- and high-selected visits, including which side
selected a support when the frontiers meet, as well as floating high rejections, pair-upward, support-upward, Dickinson, downward,
and exact-support families in the focused model tests.

## Known Difficult Inputs

BDD-B3 still omits Dickinson intervals on the high frontier. It is difficult when many large principal matrices fail the floating
positive-semidefiniteness filter: the high scan can cheaply reject many supports, but those supports still require later exact
low-frontier proof. Near-semidefinite indefinite supports can also pass the floating filter and pay for an exact factorization that
produces no downward closure. The low Halfspace-Rays fallback can prune sideways and upward, but it may be expensive when many exact
directions are swept before finding only a narrow interval.

Individual alternation can expose many large floating principal factorizations before small-cardinality exclusions have accumulated.
It can spend time optimizing low Dickinson certificates before a later large positive-definite face supplies a downward closure that
would have removed those supports. More importantly, the canonical BDD for a union of many irregular, overlapping intervals can be
exponentially larger than the interval list. This implementation has a fixed variable order and no garbage collector, complemented
edges, or dynamic variable reordering. Expired and discarded roots stop participating in later operations, but their allocated nodes
remain in the model-local arena until the matrix call ends.
