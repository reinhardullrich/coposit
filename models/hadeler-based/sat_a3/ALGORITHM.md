# SAT-A3

Classification: coposit-created exact CP/SCP experiment. It copies SAT-Halfspace-Rays Dickinson, then tries to enlarge that model's
single best upper endpoint without losing any index already present. Every interval admitted to SAT is verified with exact integers.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal
and is the analysis-interface default.

## Idea In Plain Language

Dickinson's certificate depends on a vector obtained from a principal matrix. For a nonsingular principal matrix, the usual choice
solves a system whose right-hand side is the all-ones vector. SAT-Halfspace Dickinson observes that any strictly positive right-hand
side is valid and searches coordinate directions for one that makes more entries of the full product nonnegative, breaking ties in
favor of the wider Dickinson interval.

SAT-A3 first runs that complete Rays procedure unchanged. If its exact result is $u_{\rm rays}$, A3 fixes

$$
F=U(u_{\rm rays}).
$$

For each index outside $F$, it asks whether some strictly positive right-hand side can keep every index of $F$ nonnegative and add
that index. A numerically proposed enlargement is accepted only after exact reconstruction proves that it preserves all of $F$ and
strictly increases $|U|$. The accepted set becomes the new $F$, and the process repeats. A3 therefore searches one monotone chain of
upper endpoints; it never exchanges a Rays index for a different index and never claims that the final endpoint is globally maximum.

## Name, Sources, And Classification

The identifier is `sat_a3`.

- **SAT** names the persistent Boolean representation of the supports not yet covered by certificates.
- **A3** identifies the third bounded experiment derived from the fixed-support right-hand-side certificate-complex analysis. A1
  retains an antichain of endpoints, A2 uses a maximum-halfspace relaxation, and A3 isolates monotone enlargement of the Rays result.

The model is an independent copy and experimental variation of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). The certificate theorem and the permission to replace the
all-ones right-hand side by any strictly positive vector come from Peter J. C. Dickinson, “A New Certificate for Copositivity,”
*Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–2,
and the note following those algorithms. The coordinate search, adaptive shortlist, synthesized rays, and monotone enlargement are
coposit changes, not claims about Dickinson's published algorithm. The structural motivation is developed in
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

## Monotone Enlargement Of The Rays Endpoint

After Rays has produced its exact incumbent, define

$$
P=A_{I^c,I}A_I^{-1}
\quad\text{and}\quad
F=U(u_{\rm rays}).
$$

The rows belonging to $I$ need no LP constraints: every admissible right-hand side is strictly positive, so they always belong to
$U$. For an omitted outside index $q$, A3 asks whether there is a vector $b>0$ satisfying

$$
p_j^Tb\geq0\quad(j\in F\setminus I),
\qquad
p_q^Tb\geq0.
$$

This is an exact mathematical test for the existence of an endpoint that contains $F\cup\{q\}$. Homogeneity removes the strict
inequality: a feasible $b>0$ can be scaled so that $b\geq\mathbf1$. Writing $b=\mathbf1+y$ gives the ordinary feasibility LP

$$
\begin{aligned}
\text{find}\quad &y,\\
\text{subject to}\quad &-p_j^Ty\leq p_j^T\mathbf1
&& (j\in(F\setminus I)\cup\{q\}),\\
&y\geq0.
\end{aligned}
$$

No binary variables, objective, big-$M$ constants, or refactorization are needed. The rows of $P$ are already available as the exact
full products of the unit-system directions used by Rays.

The small simplex implementation uses row-scaled binary64 coefficients only to propose a feasible $b$. It is not trusted to prove
infeasibility. A proposed $b$ is normalized and rounded to positive integer coefficients at scales $10^6$, $10^9$, $10^{12}$, and
$10^{15}$. Those coefficients are then multiplied by the exact direction and product matrices. A3 accepts the candidate only if the
exact signs prove all of the following:

1. every index of the old $F$ remains in the new upper endpoint;
2. the forced target $q$ enters the new upper endpoint; and
3. the exact score $(|U|,d)$ strictly improves.

An accepted endpoint becomes the new $F$, and A3 restarts the target scan. Thus the model builds a strictly increasing chain

$$
F_0\subsetneq F_1\subsetneq\cdots\subseteq[n],
$$

where $F_0=U(u_{\rm rays})$. Because $|F|$ rises after every accepted step, at most $n-|F_0|$ extensions can be installed.

All probes for one root support share a 20 millisecond deadline. The dense scaled input and each simplex tableau are capped at eight
million entries. Interruption, numerical infeasibility, or failed exact reconstruction leaves the current exact Rays/A3 incumbent
unchanged. Consequently those outcomes mean only “no verified enlargement was found within this bounded numerical search,” not
“no enlargement exists.” A3 performs no MILP branching, index exchange, or conflict extraction.

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
7. Let $F$ be the exact Rays upper endpoint. For each omitted index, solve the bounded monotone feasibility probe preserving $F$.
8. Exactly verify a proposed strict enlargement, install it as the new $F$, and repeat; otherwise keep the current incumbent.
9. Add the single interval belonging to the final exactly verified vector.
10. Continue until SAT has no uncovered support at that cardinality.

In `both` mode, ordinary copositivity failure ends the traversal immediately. A nonnegative zero changes the SCP result to false while
the same traversal continues until CP is decided.

## Exact Representation And Termination

All matrix entries, factorization state, directions, breakpoints, accepted coefficients, witnesses, and sign decisions use
arbitrary-precision integers. Rational ray steps are represented by a positive numerator and denominator. Common integer content is
removed after accepted moves to limit coefficient growth. Binary64 arithmetic ranks the shortlist and proposes LP points. It can
miss an enlargement or propose a useless point, but it cannot install a certificate or negative witness without exact reconstruction
and verification.

The Boolean support space is finite. Coordinate ascent accepts only a strict lexicographic increase of the bounded integer pair
$(|U|,d)$. Synthesized-ray work is bounded by two sweeps. Every accepted monotone extension strictly increases $|F|$, while all LP
probes share one local deadline. Therefore the model terminates unless an external timeout or resource limit stops it first.

## Known Difficult Inputs

- A useful combined direction can involve a coordinate candidate outside the adaptive shortlist, a pair ranked below the first two,
  different positive weights, or three or more directions. The heuristic then misses it, but the ordinary exact certificate remains
  valid.
- A stronger attainable endpoint may require losing a Rays index before gaining several others. The monotone constraint forbids that
  exchange, so SAT-A3 can stop at a non-maximal endpoint.
- A feasible monotone enlargement can be numerically missed or can lie on a boundary that the finite reconstruction scales fail to
  recover. SAT-A3 never treats that miss as an infeasibility proof.
- Dense LP tableaus become unattractive when both the root support and its complement are large. The entry guard skips them, while the
  local deadline prevents one optional relaxation from dominating a support calculation.
- Large nonsingular supports require $k$ exact unit solves and up to $k$ full direction products. Constructing that original direction
  family can dominate even when the bounded enlargement finds nothing.
- Exact breakpoint numerators and denominators can grow substantially when matrix entries have many digits or several moves are
  accepted.
- Higher-nullity singular supports still use one arbitrary exact nullspace vector and can generate weak intervals.
- On inputs whose valid intervals remain narrow, SAT may still return a large part of the Boolean lattice.
