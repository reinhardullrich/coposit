# SAT-A2

Classification: coposit-created exact CP/SCP experiment. It copies SAT-Halfspace-Rays Dickinson, then adds one bounded numerical
relaxation of the maximum-upper-endpoint problem and at most one LP-guided monotone extension. Every interval admitted to SAT is
reconstructed and verified with exact integers.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Dickinson's certificate depends on a vector obtained from a principal matrix. For a nonsingular principal matrix, the usual choice
solves a system whose right-hand side is the all-ones vector. SAT-Halfspace Dickinson observes that any strictly positive right-hand
side is valid and searches coordinate directions for one that makes more entries of the full product nonnegative, breaking ties in
favor of the wider Dickinson interval.

After the exact coordinate and synthesized-ray search finishes, its certificate is the incumbent: an attainable lower bound on the
best possible upper-endpoint size. SAT-A2 then relaxes the binary maximum-halfspace model to one linear program. The LP supplies both
a numerical upper estimate and a proposed right-hand side. The proposal is useful only when exact reconstruction produces a strictly
better certificate. If the estimated gap still exceeds one, the model makes one additional monotone extension attempt. It does not
branch, enumerate conflicts, or claim that its final upper endpoint is globally maximum.

## Name, Sources, And Classification

The identifier is `sat_a2`.

- **SAT** names the persistent Boolean representation of the supports not yet covered by certificates.
- **A2** identifies the second bounded experiment derived from the fixed-support right-hand-side certificate-complex analysis. A1
  retains an antichain of endpoints; A2 instead seeks one stronger endpoint from an LP relaxation.

The model is an independent copy and experimental variation of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). The certificate theorem and the permission to replace the
all-ones right-hand side by any strictly positive vector come from Peter J. C. Dickinson, “A New Certificate for Copositivity,”
*Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–2,
and the note following those algorithms. The coordinate search, adaptive shortlist, synthesized rays, LP relaxation, and monotone
extension are coposit changes, not claims about Dickinson's published algorithm. The structural motivation and distinction between
an attainable endpoint and an upper bound are developed in
[`aidocs/RIGHT_HAND_SIDE_DICKINSON_CERTIFICATE_COMPLEX.md`](../../../aidocs/RIGHT_HAND_SIDE_DICKINSON_CERTIFICATE_COMPLEX.md).

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

## One Maximum-Halfspace LP Relaxation

After the ray search has produced its exact incumbent, let

$$
P=A_{I^c,I}A_I^{-1}.
$$

Positive scaling of the right-hand side does not matter. The numerical model therefore restricts $b$ to

$$
\mathcal B_\varepsilon=
\left\{b:\mathbf1^Tb=1,\ b_i\geq\varepsilon\right\},
\qquad
\varepsilon=\min\left\{10^{-7},\frac1{2k}\right\}.
$$

For each outside row $p_j^T$ whose sign can vary on $\mathcal B_\varepsilon$, introduce a relaxed satisfaction variable
$0\leq z_j\leq1$. Let

$$
\mu_j=\min_{b\in\mathcal B_\varepsilon}p_j^Tb,
\qquad
M_j=-\mu_j>0.
$$

The relaxation is

$$
\begin{aligned}
\max_{b,z}\quad & \sum_{j\in I^c}z_j,\\
\text{subject to}\quad &p_j^Tb\geq-M_j(1-z_j),\\
&b\in\mathcal B_\varepsilon,\\
&0\leq z_j\leq1.
\end{aligned}
$$

Rows that are nonnegative everywhere on $\mathcal B_\varepsilon$ are counted without a variable. Rows that are negative everywhere
cannot enter the numerical candidate. With binary $z_j$, this would be the maximum feasible halfspace model. Relaxing $z_j$ to an
interval makes the problem one dense LP.

The LP has two outputs:

1. its objective gives a numerical upper estimate for the largest attainable $|U|$; and
2. its primal vector $b$ proposes an actual right-hand side.

The objective is not treated as an exact proof because the simplex calculation uses binary64 arithmetic. The proposed $b$ is rounded
to integer coefficients at scales $10^6$, $10^9$, $10^{12}$, and $10^{15}$. Each reconstruction is multiplied by the original exact
inverse columns and exact full products. Only a strict exact improvement of $(|U|,d)$ replaces the ray incumbent.

If the floored numerical estimate exceeds the resulting exact $|U|$ by more than one, SAT-A2 makes one further attempt. It selects
the currently omitted outside row with the largest relaxed $z_j$, forces that row and every outside row already in the incumbent
upper endpoint, and solves the relaxation once more. This is a monotone extension: it can add an index without sacrificing an
incumbent upper index, but it cannot discover a better incomparable endpoint that requires a swap. Exact reconstruction rechecks
every forced sign and rejects a rounded proposal that would lose any incumbent upper index.

The root and extension solves share a 20 millisecond deadline and an eight-million-entry scaled-input/tableau guard. Interruption,
numerical infeasibility, or failed exact reconstruction leaves the ray incumbent unchanged. The model deliberately performs no MILP
branching and extracts no Farkas conflicts.

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

Every support returned by SAT is sent to exact matrix processing. A negative witness terminates the requested predicate. Otherwise a
new exact interval is added and traversal continues. Exhausting all cardinalities proves the remaining requested facts.

## Complete Decision Flow

For each cardinality $k=1,\ldots,n$:

1. Ask SAT for an uncovered support $I$ of cardinality $k$.
2. Factor $A_I$ exactly.
3. If $A_I$ is singular, use the singular rule and add its interval.
4. Otherwise solve the all-ones system and reject immediately if it supplies a nonnegative negative witness.
5. Reuse the factorization for the $k$ unit directions and maximize $(|U|,d)$ lexicographically by exact coordinate sweeps.
6. If a complete pass stalls, retain the adaptive shortlist and try at most two synthesized rays.
7. If the exact upper endpoint is not full, solve one maximum-halfspace LP relaxation and exactly check its proposed right-hand side.
8. If the numerical gap exceeds one, try one LP-guided monotone extension and exactly check that proposal.
9. Add the single interval belonging to the best exactly verified vector.
10. Continue until SAT has no uncovered support at that cardinality.

In `both` mode, ordinary copositivity failure ends the traversal immediately. A nonnegative zero changes the SCP result to false while
the same traversal continues until CP is decided.

## Exact Representation And Termination

All matrix entries, factorization state, directions, breakpoints, accepted coefficients, witnesses, and sign decisions use
arbitrary-precision integers. Rational ray steps are represented by a positive numerator and denominator. Common integer content is
removed after accepted moves to limit coefficient growth. Binary64 arithmetic ranks the shortlist and solves the optional LP. It can
lose an optimization opportunity, underestimate the diagnostic gap, or propose a useless point, but it cannot install a certificate
or a negative witness without exact reconstruction and verification.

The Boolean support space is finite. Coordinate ascent accepts only a strict lexicographic increase of the bounded integer pair
$(|U|,d)$. Synthesized-ray work is bounded by two sweeps, and LP work by two solves sharing one local deadline, at each nonsingular
support. Therefore the model terminates unless an external timeout or resource limit stops it first.

## Known Difficult Inputs

- A useful combined direction can involve a coordinate candidate outside the adaptive shortlist, a pair ranked below the first two,
  different positive weights, or three or more directions. The heuristic then misses it, but the ordinary exact certificate remains
  valid.
- A fractional LP objective can overstate what any single right-hand side attains. Its proposed point can fail to improve the rays,
  and the one monotone extension cannot make beneficial swaps among outside indices. SAT-A2 does not prove global optimality.
- Dense LP tableaus become unattractive when both the root support and its complement are large. The entry guard skips them, while the
  local deadline prevents one optional relaxation from dominating a support calculation.
- Large nonsingular supports require $k$ exact unit solves and up to $k$ full direction products. The two extra sweeps are bounded, but
  constructing the original direction family can still dominate.
- Exact breakpoint numerators and denominators can grow substantially when matrix entries have many digits or several moves are
  accepted.
- Higher-nullity singular supports still use one arbitrary exact nullspace vector and can generate weak intervals.
- On inputs whose valid intervals remain narrow, SAT may still return a large part of the Boolean lattice.
