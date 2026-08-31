# CaDiCaL X1

Classification: coposit-created exact CP/SCP experiment. `cadical_x1` uses CaDiCaL to store only exact pruning intervals. CaDiCaL
chooses an uncovered support, the model builds one maximal chain through it, floating-point Schur pivots guide the chain toward the
two curvature frontiers, and exact arithmetic certifies the few supports needed to cover that chain. All certificates found during
one pass are buffered and simplified before any clause is added. Before this loop, the model checks every singleton exactly, retains
only singleton certificates that reach the full support, and installs the exact pair-curvature clauses.

The name means the first CaDiCaL-based experiment (`X1`). This is not a literature baseline. It is an isolated copy of the exact
curvature and Halfspace–Rays Dickinson machinery in [`improved_nbc_b7`](../improved_nbc_b7/ALGORITHM.md), with its two-frontier NBC
traversal replaced by a general incremental CaDiCaL search and one curvature-guided chain per CaDiCaL model.

The Dickinson intervals use Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI [`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms
1–2. The minimal-support curvature argument is Theorem 1 of Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard
quadratic programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). CaDiCaL supplies only Boolean search and propagation; it
does not decide any matrix property.

## Geometry and exact pruning rules

Let $A\in\mathbb Z^{n\times n}$ be symmetric. A nonempty support $I\subseteq[n]$ identifies a face of the standard simplex, and
$A_I$ is the principal submatrix on $I$. Choose an anchor $i_0\in I$. The reduced Hessian on that face is the matrix indexed by
$I\setminus\{i_0\}$ with entries

$$
(H_I)_{jk}=A_{jk}-A_{ji_0}-A_{i_0k}+A_{i_0i_0}.
$$

The model uses three exact regions.

1. **Downward pruning.** If $A_I\succ0$, every principal submatrix inside $I$ is positive definite, so every $J\subseteq I$ is
   certified. The copied singular rule also accepts $A_I\succeq0$ when $A_Ix=\mathbf1$ is consistent.
2. **Upward pruning.** If $H_I\not\succ0$, every superset contains the same nonpositive tangent direction, so every $J\supseteq I$
   is removed from the minimal-support search.
3. **Middle region.** If $H_I\succ0$ but $A_I\not\succ0$, curvature alone gives neither closure. The model constructs an exact
   Dickinson interval.

For a valid Dickinson vector $u$, embedded by zero outside its generating support, define

$$
L=\operatorname{supp}(u),\qquad U=\{j\in[n]:(Au)_j\ge0\}.
$$

The required conditions are $u\notin-\mathbb R_+^n$ and $L\subseteq U$. They certify every support in

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

For a nonsingular $A_I$, the initial vector is the exact solution of $A_Iu_I=\mathbf1$. The inherited Halfspace–Rays procedure then
uses exact inverse-column directions, breakpoint events, and selected combined rays to enlarge the interval cheaply. Singular
supports use the copied oriented-kernel construction. A nonnegative zero vector also proves failure of strict copositivity; an exact
negative witness proves failure of copositivity.

## Boolean representation

CaDiCaL variable $x_i$ says whether $i$ belongs to the support. The permanent clause

$$
x_1\vee\cdots\vee x_n
$$

excludes the empty support from enumeration. An exact interval $[L,U]$ is blocked by the single clause

$$
\bigvee_{i\in L}\neg x_i\;\vee\!\bigvee_{j\notin U}x_j.
$$

This clause is false exactly when $L\subseteq I\subseteq U$. Thus upward closure $[L,[n]]$, downward closure $[\varnothing,U]$, and
a bounded Dickinson interval all use the same representation. To ask whether a proposed support is still open, the model calls
CaDiCaL under one temporary assumption for every variable. Permanent clauses remain installed; assumptions disappear after that
query. CaDiCaL's independent lucky-phase search is disabled and every support variable has preferred phase false. This is not a
cardinality constraint, but it biases CaDiCaL toward sparse satisfying assignments whenever propagation permits them.

Before the main loop, every singleton is processed by the exact curvature and Halfspace–Rays machinery. Curvature closures are kept,
but a Dickinson interval is installed only when its upper endpoint is $[n]$; bounded singleton intervals are discarded. Every pair
is then checked exactly. If

$$
A_{ii}+A_{jj}-2A_{ij}\le0,
$$

the pair has a nonpositive tangent direction, so its upward-closure clause is added immediately.

## One pass

CaDiCaL first returns any support $S$ satisfying all permanent clauses. A pass constructs a maximal chain containing $S$:

$$
I_1\subset I_2\subset\cdots\subset I_n=[n],\qquad |I_k|=k.
$$

The first segment grows from the empty set to $S$ using only indices of $S$; the second grows from $S$ to $[n]$ using the remaining
indices. Reading the first segment backward is the corresponding removal walk, so a separate downdate implementation is not needed.

At each step, candidate additions are ranked in floating point:

- while $A_I$ appears positive definite, factor $A_I$ and choose the safely positive principal Schur pivot
  $$s_j=A_{jj}-A_{jI}A_I^{-1}A_{Ij}$$
  with largest value;
- otherwise, while $H_I$ appears positive definite, factor $H_I$ and choose the smallest tangent Schur pivot, moving locally toward
  loss of positive curvature;
- if neither score is available, use the smallest candidate index.

CaDiCaL then tests candidates in that order and stops at the first uncovered extension. This produces the same best uncovered choice
without solving once for every possible extension. If every extension is covered, the highest-ranked one is used only to cross that
covered part virtually; no matrix work is done on the covered support.

The floating region found while ranking a prefix is stored with that prefix. The model therefore does not factor the completed chain
a second time merely to nominate the last apparent downward support and first apparent upward support. Floating point fixes only the
order and these nominations. It never adds a clause, returns a classification, or permits the proof to skip an uncovered support.

After the complete chain is known, the model proceeds exactly:

1. verify the nominated downward support and buffer its downward interval if valid;
2. verify the nominated upward support and buffer its upward interval if valid;
3. walk over the chain once more and exactly process every support not already covered by a permanent or buffered interval;
4. for a middle support, construct the full exact Halfspace–Rays Dickinson interval; if additional supports remain open, repeat the
   exact step on them;
5. simplify the buffered intervals by inclusion and only then add the survivors to CaDiCaL.

Step 3 is part of the algorithm, not a hidden fallback. Exact processing must cover its own generating support; failure to do so is
an internal error. In the common case the pass needs the two exact curvature endpoints and one exact Dickinson interval. Numerical
uncertainty or a short interval can require more exact supports.

Buffering is important. If an upward curvature interval $[C,[n]]$ is found first and a later Dickinson interval $[L,[n]]$ has
$L\subseteq C$, the Dickinson interval contains the curvature interval. The curvature clause is therefore discarded before either
one reaches CaDiCaL. The same general containment rule removes any buffered interval contained in another buffered interval.

## Complete decision flow

1. Check every singleton exactly and install only curvature closures and full-ceiling Dickinson intervals.
2. Install the exact pair-curvature clauses.
3. Ask CaDiCaL, with false preferred phases, for an uncovered nonempty support. If none exists, the proof is complete.
4. Build and exactly cover one maximal chain through that support as described above.
5. Commit the simplified exact intervals and return to step 3.
6. Stop immediately on an exact negative witness. In `both` mode, retain exact non-strict zero information and continue the same
   traversal to classify CP and SCP together.

Every successful pass removes at least the support returned by CaDiCaL. The Boolean lattice is finite, and permanent clauses are
never removed, so the model terminates unless an explicit time or memory limit interrupts it. Such a resource limit remains
unresolved; it is never converted into a negative classification.

## Diagnostics

Diagnostics record accepted singleton certificates and each CaDiCaL seed immediately, then record its completed chain, the nominated
curvature endpoints, the number of exact bridge supports, and every interval that survives pass-local subsumption. Discarded bounded
singleton intervals and other discarded proposals are not reported as certificates.

## Known Difficult Inputs

The Schur rule explores one local chain per seed, not all curvature-frontier components. A poor chain may expose narrow Dickinson
intervals and require many later seeds. Near-singular floating pivots are intentionally conservative; they can increase exact bridge
work but cannot change the answer. Candidate coverage queries use full CaDiCaL assumptions. Usually the highest-ranked candidate is
open, but a heavily covered chain step may still test many candidates. For large dimensions with many narrow intervals, Boolean
propagation and repeated chain planning can therefore dominate the exact linear algebra.
