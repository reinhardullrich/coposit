# SAT-A1

Classification: coposit-created exact CP/SCP experiment. It is an isolated copy of SAT-Halfspace-Rays Dickinson that retains the
incomparable maximal upper endpoints encountered by the exact sweeps instead of discarding every endpoint except one winner.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Dickinson's certificate depends on a vector obtained from a principal matrix. For a nonsingular principal matrix, the usual choice
solves a system whose right-hand side is the all-ones vector. SAT-Halfspace Dickinson observes that any strictly positive right-hand
side is valid and searches coordinate directions for one that makes more entries of the full product nonnegative, breaking ties in
favor of the wider Dickinson interval.

A coordinate search can become stuck even when two individually unhelpful directions complement one another. As in
SAT-Halfspace-Rays Dickinson, this model remembers a bounded number of promising coordinate-ray points, forms at most two combined
directions, and sweeps them exactly.

The new A1 rule observes that the sweeps already encounter several upper endpoints that need not contain one another. It collects
their inclusion-maximal antichain as packed supports, then retains only a small dimension-dependent family with the largest exact
marginal Boolean-lattice coverage. The old final vector always contributes its complete Dickinson interval. Every selected additional
upper endpoint contributes the guaranteed anchored interval from the processed support to that endpoint. No extra exact
factorization, direction product, breakpoint sweep, or numerical optimization is introduced.

## Name, Sources, And Classification

The identifier is `sat_a1`.

- **SAT** names the persistent Boolean representation of the supports not yet covered by certificates.
- **A1** means the first antichain experiment: retain the maximal upper endpoints already encountered by the existing Rays search.

The model is an independent copy and experimental variation of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). The certificate theorem and the permission to replace
the all-ones right-hand side by any strictly positive vector come from Peter J. C. Dickinson, “A New Certificate for
Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6,
Algorithms 1–2, and the note following those algorithms. The coordinate search, synthesized rays, and A1 antichain are coposit
changes, not claims about Dickinson's published algorithm.

The mathematical structure behind A1 is developed in
[`RIGHT_HAND_SIDE_DICKINSON_CERTIFICATE_COMPLEX.md`](../../../aidocs/RIGHT_HAND_SIDE_DICKINSON_CERTIFICATE_COMPLEX.md). For one fixed
support, its attainable anchored intervals form a translated simplicial complex, and the incomparable maximal upper endpoints are
its facets.

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

## A1 Upper-Endpoint Antichain

During every ordinary and synthesized exact sweep, A1 maintains the current upper support as a packed bit set. At a breakpoint,
indices whose product becomes zero enter $U$ before the exact breakpoint is recorded; indices that become negative leave $U$ only
after that breakpoint. The stored support therefore matches the non-strict inequality $(Au)_j\geq0$.

One affine product coordinate crosses zero at most once on a sweep. An open sign region following a root is a subset of the endpoint
at that root, because coordinates leaving $U$ still belong to $U$ at equality. A root with no coordinate entering $U$ cannot enlarge
the preceding endpoint. A1 therefore performs antichain retention only for the initial upper endpoint and for root groups where at
least one product coordinate enters $U$. The final chosen endpoint is considered separately. This removes redundant antichain scans
without changing the maximal endpoint family produced by the visited sweeps.

Let $F$ be a newly encountered upper endpoint and let $F_1,\ldots,F_t$ be the endpoints currently retained for the same root $I$.
A1 applies inclusion subsumption immediately:

1. If $F\subseteq F_i$ for some retained endpoint, discard $F$.
2. Otherwise remove every $F_i\subseteq F$ and retain $F$.

The collected family is therefore an antichain: no endpoint contains another. This is the complete set of inclusion-maximal upper
endpoints **among the exact sign regions that the unchanged Rays search actually visits**. A1 does not claim to enumerate every
right-hand side in the positive simplex.

For the final vector chosen by the original lexicographic search, A1 always retains the unchanged full Dickinson interval $[L,U]$.
For each selected additional endpoint $F$, A1 inserts the anchored interval

$$
[I,F].
$$

This interval is valid because every swept right-hand side remains strictly positive, so $I\subseteq F$, while the corresponding
true lower support is contained in $I$. Anchoring additional endpoints at $I$ avoids materializing their full arbitrary-precision
vectors and cannot remove any support outside a valid Dickinson interval. Keeping the original full interval preserves any downward
coverage obtained from zeros in its final vector.

The total number of intervals retained from one nonsingular root, including the original full interval, is bounded by

$$
b(n)=\min\left\{8,\ \max\left\{2,\ 1+\left\lceil\frac{\sqrt n}{2}\right\rceil\right\}\right\}.
$$

Thus an order-25 matrix retains at most four intervals: the original winner and at most three additions. The square-root growth gives
larger matrices more chances to gain a valuable upper index, while the cap of eight bounds both SAT clause growth and selection work.

The additions are chosen greedily by exact marginal coverage, not by $|F|$ alone. All candidate intervals have the same lower anchor
$I$. If upper endpoints $F_1,\ldots,F_t$ have already been selected, the number of supports newly covered by candidate $F$ is

$$
\Delta(F)=
\sum_{R\subseteq\{1,\ldots,t\}}
(-1)^{|R|}
2^{\left|F\cap\bigcap_{r\in R}F_r\right|-|I|}.
$$

This is ordinary inclusion-exclusion for Boolean intervals. A1 evaluates it with arbitrary-precision integers. At most seven
previous upper endpoints participate, so one candidate needs at most $2^7=128$ small packed-support intersections. A candidate with
$\Delta(F)=0$ is omitted. Exact ties prefer larger $|F|$ and then the packed support order, making selection deterministic.

The general certificate diagnostics count every interval actually inserted. Thus the existing $(k,d,|U|)$ histogram exposes both
how many antichain endpoints survive and how wide their anchored intervals are.

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

Every support returned by SAT is sent to exact matrix processing. A negative witness terminates the requested predicate. Otherwise
the final full interval and the bounded marginal-coverage selection of anchored A1 intervals are added before traversal continues.
Exhausting all cardinalities proves the remaining requested facts.

## Complete Decision Flow

For each cardinality $k=1,\ldots,n$:

1. Ask SAT for an uncovered support $I$ of cardinality $k$.
2. Factor $A_I$ exactly.
3. If $A_I$ is singular, use the singular rule and add its interval.
4. Otherwise solve the all-ones system and reject immediately if it supplies a nonnegative negative witness.
5. Reuse the factorization for the $k$ unit directions and maximize $(|U|,d)$ lexicographically by exact coordinate sweeps.
6. At the initial point and every root where $U$ grows, update the inclusion-maximal upper-endpoint antichain.
7. If a complete pass stalls, retain the adaptive shortlist and try at most two synthesized rays, updating the same antichain.
8. Add the full interval belonging to the final exact vector.
9. Greedily add at most $b(n)-1$ antichain endpoints by exact marginal coverage.
10. Continue until SAT has no uncovered support at that cardinality.

In `both` mode, ordinary copositivity failure ends the traversal immediately. A nonnegative zero changes the SCP result to false while
the same traversal continues until CP is decided.

## Exact Representation And Termination

All matrix entries, factorization state, directions, breakpoints, coefficients, witnesses, sign decisions, and marginal-coverage
scores use arbitrary-precision integers. Rational steps are represented by a positive numerator and denominator. Common integer
content is removed after accepted moves to limit coefficient growth. Floating point is used only to compute the harmless integer
bounds $\lceil3\sqrt n\rceil$ and $b(n)$; it does not enter any mathematical decision.

The Boolean support space is finite. Coordinate ascent accepts only a strict lexicographic increase of the bounded integer pair
$(|U|,d)$. Synthesized-ray work is bounded by two sweeps at each nonsingular support. Every sweep has finitely many exact sign
regions, antichain insertion either discards one endpoint or stores one packed support, and at most eight intervals are inserted per
nonsingular support. Therefore the model terminates unless an external timeout or resource limit stops it first.

## Known Difficult Inputs

- A useful combined direction can involve a coordinate candidate outside the adaptive shortlist, a pair ranked below the first two,
  different positive weights, or three or more directions. The heuristic then misses it, but the ordinary exact certificate remains
  valid.
- Large nonsingular supports require $k$ exact unit solves and up to $k$ full direction products. The two extra sweeps are bounded, but
  constructing the original direction family can still dominate.
- Exact breakpoint numerators and denominators can grow substantially when matrix entries have many digits or several moves are
  accepted.
- A root can expose many incomparable upper endpoints. A1 still collects their antichain and performs pairwise packed-support subset
  checks. The endpoint budget bounds inserted SAT clauses, but collecting a very large antichain can remain expensive.
- The dimension-dependent budget is a heuristic. A discarded endpoint may later have been more useful to SAT than the greedily
  selected endpoints, even though every selected interval is exact.
- A1 sees only endpoints visited by the existing coordinate and synthesized-ray sweeps. A useful facet elsewhere in the
  right-hand-side simplex remains undiscovered.
- Higher-nullity singular supports still use one arbitrary exact nullspace vector and can generate weak intervals.
- On inputs whose valid intervals remain narrow, SAT may still return a large part of the Boolean lattice.
