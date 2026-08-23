# NBC-B6

Classification: coposit-created exact CP/SCP experiment. NBC-B6 keeps SAT-B3's low-frontier curvature and Halfspace-Rays
Dickinson mathematics, removes the high frontier and all downward pruning, and replaces one-model-at-a-time CaDiCaL selection with
NBC MiniSat All cardinality-layer enumeration.

The public identifier is `nbc_b6`. **NBC** names the adapted NBC MiniSat All enumerator. **B6** marks the sixth experiment in the
curvature-based B line. `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal.

## Idea In Plain Language

The algorithm visits support sets in increasing cardinality. At support size $k$, NBC MiniSat All returns every currently open
support of size $k$ without requiring the model to add a temporary clause after every answer. Each support is analyzed exactly:

1. If its simplex face is not strictly convex, the support and every superset are excluded by a full upward curvature certificate.
2. Otherwise the retained factorization constructs an exact Dickinson interval and applies the Halfspace-Rays optimization inherited
   from SAT-B3.
3. An exact negative witness ends copositivity classification immediately. An exact zero updates strict classification and ordinary
   copositivity classification continues.

Certificates discovered while enumerating layer $k$ are saved but do not change that enumeration. After the entire layer is
finished, redundant certificates are removed, compatible upward certificates are merged, and the result becomes active for layer
$k+1$. This deliberately trades some same-layer exact work for fewer SAT updates and a smaller retained formula.

In particular, a singular support of size $k$ may produce a Dickinson lower endpoint $L$ with $|L|<k$. NBC-B6 keeps that exact
$L$. It still delays the interval until the layer boundary, so it may visit a few other size-$k$ supports that the interval could
already cover. The model does not add special machinery for this uncommon, small saving.

## Sources And Changes

NBC-B6 is copied mathematically from [SAT-B3](../sat_b3/ALGORITHM.md), but retains only SAT-B3's low/upward path. It has:

- the exact pair-curvature prepass;
- exact reduced-Hessian curvature tests on selected supports;
- exact nonsingular and singular Dickinson constructions;
- exact coordinate breakpoint sweeps and at most two complementary combined-ray sweeps;
- no high-cardinality scan, floating-point filter, downward closure, or Johnson--Reams Schur reduction.

Dickinson intervals come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications*
569 (2019), 15–37, especially Theorem 4.6 and Algorithms 1–2, DOI
[10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025). The minimal-support minimizer argument follows Andrea
Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156 (2008),
2439–2448, DOI [10.1016/j.dam.2007.09.020](https://doi.org/10.1016/j.dam.2007.09.020). The projected-inertia tests follow the
standard equality-constrained inertia relation; see T. S. Han and H. Fujiwara, *Linear Algebra and its Applications* 72 (1985),
47–58, DOI [10.1016/0024-3795(85)90141-7](https://doi.org/10.1016/0024-3795(85)90141-7).

The support enumerator adapts Takahisa Toda's NBC MiniSat All 1.0.2, itself based on MiniSat-C 1.14.1. The local adaptation exposes
a callback API, supports assumptions for one exact cardinality, and cooperatively checks the coposit timeout. The mathematical
certificates, layer-boundary deferral, and certificate compaction are coposit additions.

## Exact Support Tests

Let $A\in\mathbb R^{n\times n}$ be symmetric and let nonempty $I\subseteq[n]$ identify a simplex face. Write $B=A_I$ and

\[
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
\]

The face is strictly convex precisely when $v^TBv>0$ for every nonzero $v\in\mathcal T_I$. NBC-B6 obtains this decision from
one exact fraction-free $LDL^T$ factorization rather than constructing a tangent-space basis.

For nonsingular $B$, set $\delta=\mathbf1^TB^{-1}\mathbf1$. Strict face convexity holds when either $B\succ0$, or $B$ has
exactly one negative eigenvalue and $\delta<0$. For singular $B$, it holds exactly when $B\succeq0$, the nullity is one, and a
kernel vector $z$ satisfies $\mathbf1^Tz\neq0$.

If strict face convexity fails, no superset containing $I$ can support a minimal-support global minimizer. The full upward closure
is therefore excluded. Before traversal, the pair case is checked directly:

\[
A_{ii}+A_{jj}-2A_{ij}>0.
\]

Every failing pair supplies an upward closure rooted at $\{i,j\}$.

If a nonsingular all-ones solution is nonpositive, its negative is a nonnegative vector with negative quadratic value, so the matrix
is not copositive. A one-signed singular kernel vector is an exact zero and therefore disproves strict copositivity.

## Halfspace-Rays Dickinson Certificate

When curvature does not prune the full upward closure, NBC-B6 reuses the factorization. It solves $Bx=\mathbf1$ in the nonsingular
case and uses an exact kernel ray in the singular case. After embedding the local vector $u$ into the full coordinate space, define

\[
L(u)=\{i:u_i\neq0\},
\qquad
U(u)=\{j:(Au)_j\geq0\}.
\]

Dickinson's theorem certifies every support $J$ in

\[
L(u)\subseteq J\subseteq U(u).
\]

The exact Halfspace-Rays search sweeps coordinate directions, prefers larger $|U|$, then larger width $|U|-|L|$, and tries at
most two selected combined rays after the coordinate sweeps stall. Every accepted endpoint and every sign decision uses exact
integers.

## NBC Layer Enumeration

Original-index variables $s_i$ encode membership in the current support. A Batcher sorting network supplies the assumptions for
exact cardinality $k$. Retained intervals are clauses of the form

\[
\left(\bigvee_{i\in L}\neg s_i\right)
\lor
\left(\bigvee_{j\notin U}s_j\right)
\lor c_{|U|+1},
\]

where the final sorting-network literal is omitted for $U=[n]$. NBC MiniSat All enumerates all satisfying assignments at the
requested cardinality by nonblocking backtracking; NBC-B6 does not insert one temporary exact-support blocker per answer.

The solver is rebuilt for each cardinality from the fixed sorting network and the compacted certificates active at the beginning of
that layer. Newly found certificates remain pending. This makes the current enumeration stable and keeps certificate insertion out
of the exact support-processing loop.

## Layer-Boundary Compaction

After layer $k$ is exhausted:

1. Every bounded interval with $|U|\leq k$ expires because it cannot cover a future layer.
2. An interval contained in another retained interval is removed.
3. Full upward roots are reduced to an inclusion-minimal antichain.
4. If every one-element extension $P\cup\{j\}$, $j\notin P$, remains as a retained upward root after antichain reduction and
   $|P|\leq k$, those roots may be replaced by $P$ for all future layers.

The fourth rule is a search-state compaction, not a claim that $P$ itself has a curvature certificate. Supports through layer $k$
are already finished. Every future support strictly containing $P$ contains at least one of its one-element extensions, so the
replacement preserves exactly the unresolved search above the completed boundary.

## Complete Decision Flow

1. Create the reusable NBC cardinality enumerator and install every failed pair-curvature closure.
2. Activate and compact the pair prepass before layer 1.
3. For $k=1,2,\ldots,n$, enumerate every support of size $k$ left open by previously committed certificates.
4. Exactly factor each principal matrix.
5. Add a pending full-upward curvature closure when strict face convexity fails; otherwise add a pending exact Halfspace-Rays
   Dickinson interval.
6. Stop immediately on an exact negative witness. Record an exact zero for strict classification and continue when ordinary
   copositivity is still requested.
7. After NBC exhausts layer $k$, compact all pending and retained certificates and activate them for layer $k+1$.
8. If compaction proves every future support covered, or if layer $n$ is exhausted, return the accumulated exact classification.

The proof is finite because NBC enumerates each satisfying support in a fixed layer once and there are finitely many nonempty
supports. Deferral may add work inside a layer but cannot omit a support required for correctness.

## Exact Arithmetic And Diagnostics

All matrix factorizations, inertia decisions, all-ones and kernel vectors, breakpoint comparisons, interval endpoints, zeros, and
negative witnesses use arbitrary-precision integers. The model has no floating-point proof path and no silent fallback.

Diagnostics report the active cardinality, every visited support, curvature and Dickinson certificates, and singular-support
nullities. Certificate events are recorded when discovered even though their Boolean clauses become active only after the layer
boundary.

## Known Difficult Inputs

NBC-B6 deliberately does not exploit a strong certificate until the next cardinality. It can therefore repeat exact work when a
certificate found early in a large layer covers many later supports of that same layer. Singular certificates with $|L|<k$ are a
small instance of this tradeoff.

The model also has no downward pruning or Schur reduction. Inputs whose useful structure appears mainly in large positive-definite
principal blocks can require many low-to-high layers. Certificate compaction removes containment and complete sibling families, but
large antichains of genuinely different bounded Dickinson intervals can still make rebuilding the NBC formula expensive.
