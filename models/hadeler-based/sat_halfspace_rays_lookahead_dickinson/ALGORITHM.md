# SAT-Halfspace-Rays Lookahead Dickinson

Classification: coposit-created exact CP/SCP experiment. It copies SAT-Halfspace-Rays Dickinson and adds one-cardinality look-ahead:
while processing a support of cardinality $k$, it also analyzes every immediate superset of cardinality $k+1$ and immediately inserts
every child interval that reaches outside the current interval.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Dickinson's certificate depends on a vector obtained from a principal matrix. SAT-Halfspace-Rays Dickinson searches strictly positive
right-hand sides and at most two synthesized rays for a strong certificate at the current support. This model then looks exactly one
cardinality ahead. For a current support $I$ of size $k$, it computes the certificate of every child
$J=I\cup\{j\}$ of size $k+1$. A child interval is inserted immediately whenever it contains any support not already contained in the
current interval. The current interval is omitted only when one inserted child interval contains it completely.

A coordinate search can become stuck even when two individually unhelpful directions complement one another. This model remembers a
bounded number of promising coordinate-ray points from the final unsuccessful pass. It chooses the two most complementary pairs,
forms one exact combined direction from each pair, and sweeps those directions. Thus it pays for at most two additional exact sweeps,
not for a two-dimensional optimization problem.

Different parents share children. A one-layer packed-support cache therefore stores every analyzed child and its exact certificate.
The first parent computes it and later parents reuse it. Thus look-ahead does not repeat a factorization merely because a child has
several parents. The cache is discarded when the active parent cardinality has been traversed.

## Name, Sources, And Classification

The identifier is `sat_halfspace_rays_lookahead_dickinson`.

- **SAT** names the persistent Boolean representation of the supports not yet covered by certificates.
- **Halfspace** refers to the inequalities $(Au)_j\geq0$, which are linear halfspaces in right-hand-side coefficient space.
- **Rays** refers to the at most two synthesized directions tried after coordinate ascent stalls.
- **Lookahead** means that every immediate cardinality-$k+1$ superset is analyzed while its cardinality-$k$ parent is active.
- **Dickinson** identifies the underlying certificate theorem and principal-support traversal.

The model is an independent copy and experimental variation of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). The certificate theorem and the permission to replace the
all-ones right-hand side by any strictly positive vector come from Peter J. C. Dickinson, “A New Certificate for Copositivity,”
*Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–2,
and the note following those algorithms. The coordinate search, adaptive shortlist, and synthesized rays are coposit changes, not
claims about Dickinson's published algorithm.

CaDiCaL 2.2.1 supplies incremental SAT solving. Exact-cardinality layers use the same Batcher bitonic sorting network as
`sat_dickinson`.

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

If $A_I$ is singular, the halfspace and synthesized-ray searches are not used. The model:

1. recover one nonzero nullspace vector;
2. retain its nonnegative orientation, when one exists, and record that SCP is false;
3. otherwise compute the full product exactly, compare the two orientations $u$ and $-u$, and choose the one with larger $|U|$;
   and
4. insert its Dickinson interval.

For a mixed-sign vector, let $p$, $m$, and $z$ count the positive, negative, and zero entries of $Au$. Then
$|U(u)|=p+z$ and $|U(-u)|=m+z$, so the implementation chooses $-u$ exactly when $m>p$. A tie retains the factorization orientation.
The product for the chosen negative orientation is obtained by exact negation, not a second matrix product.

The choice of one nullspace vector is deterministic but can produce a weaker interval than another vector in a higher-dimensional
kernel. This model does not add kernel-cone or affine-companion searches.

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

Every support returned by SAT is either sent to exact matrix processing or matched with an exact result already computed by the
preceding look-ahead layer. A negative witness terminates the requested predicate. Exhausting all cardinalities proves the remaining
requested facts.

## One-Cardinality Look-Ahead

Let the current exact certificate be

$$
A=[L_A,U_A].
$$

For every index $j\notin I$, the model analyzes the immediate child $J=I\cup\{j\}$ and obtains its exact certificate

$$
B_j=[L_j,U_j].
$$

The whole intervals are compared, not only their widths. The containment rule is

$$
B_j\subseteq A
\quad\Longleftrightarrow\quad
L_A\subseteq L_j\ \text{and}\ U_j\subseteq U_A.
$$

If this condition is false, $B_j$ covers at least one support not covered by $A$, so $B_j$ is inserted into SAT immediately. This
includes incomparable intervals and intervals with the same or even smaller width than $A$; width alone cannot detect whether their
covered supports differ.

The current interval can be omitted only if one inserted child contains it:

$$
A\subseteq B_j
\quad\Longleftrightarrow\quad
L_j\subseteq L_A\ \text{and}\ U_A\subseteq U_j.
$$

Equality does not cause both intervals to be inserted. In that case the child is already contained in the current interval, and the
current interval is retained. The implementation does not attempt the more expensive question whether a union of several child
intervals covers the current interval.

Every child support is represented by a packed bit key. Its exact lower set, upper set, width, upper size, and insertion state are
stored in a one-layer hash table. A child shared by several parents is factored only for the first parent; later parents read the same
cached result. Every analyzed child is covered immediately: either its own interval is inserted, or its interval is contained in the
current interval and the current interval covers the child support. SAT therefore cannot return an analyzed child at the next
cardinality. The cache is discarded after the active parent layer rather than being carried forward.

## Complete Decision Flow

For each cardinality $k=1,\ldots,n$:

1. Ask SAT for an uncovered support $I$ of cardinality $k$.
2. Factor and optimize it exactly as in SAT-Halfspace-Rays Dickinson.
3. For each immediate cardinality-$k+1$ child, reuse its cached result or analyze and cache it exactly.
4. Insert every child interval not contained in the current interval.
5. Insert the current interval unless one of those inserted children contains it completely.
6. Continue until SAT has no uncovered support at cardinality $k$, then discard the child cache.

In `both` mode, ordinary copositivity failure ends the traversal immediately. A nonnegative zero changes the SCP result to false while
the same traversal continues until CP is decided.

## Exact Representation And Termination

All matrix entries, factorization state, directions, breakpoints, coefficients, witnesses, and sign decisions use arbitrary-precision
integers. Rational steps are represented by a positive numerator and denominator. Common integer content is removed after accepted
moves to limit coefficient growth. Floating point is used only to compute the harmless shortlist size $\lceil3\sqrt n\rceil$; it does
not enter any mathematical decision.

The Boolean support space is finite. Coordinate ascent accepts only a strict lexicographic increase of the bounded integer pair
$(|U|,d)$. Synthesized-ray work is bounded by two sweeps at each nonsingular support. Each current support has at most $n-k$
immediate children, and the layer cache prevents repeated exact processing of a shared child. Therefore the model terminates unless
an external timeout or resource limit stops it first.

## Known Difficult Inputs

- A useful combined direction can involve a coordinate candidate outside the adaptive shortlist, a pair ranked below the first two,
  different positive weights, or three or more directions. The heuristic then misses it, but the ordinary exact certificate remains
  valid.
- Large nonsingular supports require $k$ exact unit solves and up to $k$ full direction products. The two extra sweeps are bounded, but
  constructing the original direction family can still dominate.
- Exact breakpoint numerators and denominators can grow substantially when matrix entries have many digits or several moves are
  accepted.
- Higher-nullity singular supports still use one arbitrary exact nullspace vector and can generate weak intervals.
- One-cardinality look-ahead can factor much of the next layer before current-layer certificates show that the work was unnecessary.
- The cache stores packed keys and two packed interval endpoints for every distinct analyzed child in the active next layer. A layer
  containing many uncovered or weakly covered supports can therefore require substantial memory.
- Incomparable child intervals are all inserted. On inputs producing many distinct intervals, the larger SAT clause database can cost
  more than the additional coverage saves.
- On inputs whose valid intervals remain narrow, SAT may still return a large part of the Boolean lattice.
