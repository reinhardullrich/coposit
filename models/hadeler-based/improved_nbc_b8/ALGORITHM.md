# Improved NBC-B8

Classification: coposit-created exact CP/SCP experiment. Improved NBC-B8 is the upward-only counterpart of
[`improved_nbc_b7`](../improved_nbc_b7/ALGORITHM.md). It keeps B7's resumable NBC MiniSat All backend and exact low-frontier mathematics,
but deletes the high-frontier traversal and every downward-pruning rule.

The public modes `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one traversal and is the
analysis-interface default.

## Idea In Plain Language

The model examines supports in ascending cardinality. For each open support it proves one of three things with exact arithmetic:

1. the support is not strictly convex on its simplex face, so it and all supersets can be excluded;
2. a nonpositive vector proves that the matrix is not copositive, or a zero vector proves that it is not strictly copositive; or
3. a Halfspace-Rays Dickinson certificate excludes one exact Boolean interval.

Only upward curvature closures and Dickinson intervals are stored. There is no descending scan from the full support and no downward
certificate. The Boolean engine returns one support at a time and resumes later without inserting a temporary exact-support clause.

The intuition is simple: B8 keeps the part of B7 that can eliminate future, larger supports. It deliberately gives up B7's attempt to prove
large faces strictly copositive and remove their subsets.

## Name, Sources, And Classification

- **Improved NBC** names the locally maintained resumable derivative of NBC MiniSat All.
- **B8** marks this upward-only experiment in the curvature-based B line.

The model is an independent copy of `improved_nbc_b7`. The removal of the high frontier is the only mathematical control-flow change. The
exact factorization, curvature rule, Halfspace-Rays optimization, Boolean interval representation, and compaction remain inherited.

Dickinson intervals come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI [`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.
The minimal-support curvature argument is Theorem 1 of Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic
programming,” *Discrete Applied Mathematics* 156 (2008), 2439–2448, DOI
[`10.1016/j.dam.2007.09.020`](https://doi.org/10.1016/j.dam.2007.09.020). Using these facts as clauses in a complete exact CP/SCP
classifier is a coposit construction, not an algorithm claimed by either paper.

The Boolean engine derives from Takahisa Toda's NBC MiniSat All 1.0.2, itself based on MiniSat-C 1.14.1. The resumable derivative lives in
`cpp/third_party/improved_nbc_minisat_all/`; its wrapper returns one model from one unexplored prefix cube at a time.

## Face Curvature And Upward Pruning

Let $A\in\mathbb R^{n\times n}$ be symmetric, let $I\subseteq[n]$ be a nonempty support, and write $B=A_I$. The tangent space of the
corresponding simplex face is

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
$$

The face is strictly convex exactly when $v^TBv>0$ for every nonzero $v\in\mathcal T_I$. If this fails, no superset of $I$ can restore
strict convexity because the same bad direction remains available after more coordinates are added. B8 therefore inserts the upward clause

$$
\bigvee_{i\in I}\neg s_i,
$$

where $s_i$ means that index $i$ belongs to the selected support. This clause excludes $I$ and every superset.

Before traversal, every pair $\{i,j\}$ is checked by the exact one-dimensional tangent condition

$$
A_{ii}+A_{jj}-2A_{ij}>0.
$$

Every failing pair contributes its upward clause immediately.

For larger supports the implementation obtains the curvature decision from one exact fraction-free $LDL^T$ factorization of $B$. If $B$ is
nonsingular and

$$
\delta=\mathbf1^TB^{-1}\mathbf1,
$$

then the reduced Hessian is positive definite exactly when either $B$ is positive definite, or $B$ has one negative eigenvalue and
$\delta<0$. For a singular $B$, strict face convexity requires positive semidefiniteness, nullity one, and a kernel vector $z$ with
$\mathbf1^Tz\ne0$.

The intuition is that ordinary definiteness describes every direction in ambient space, while the reduced condition uses only directions
that keep the coordinate sum fixed and therefore stay on the simplex face.

## Exact Decisions And Witnesses

The same exact factorization is reused to solve $Bx=\mathbf1$ or recover a kernel vector.

- If an exact nonnegative vector has negative quadratic value, the requested copositivity predicate is false.
- If an exact nonzero nonnegative vector has zero quadratic value, strict copositivity is false.
- In `both` mode, a zero witness records the non-strict result and the traversal continues until ordinary copositivity is also decided.

Witness signs and values are checked with arbitrary-precision integers. B8 has no floating-point mathematical screen or proof path.

## Halfspace-Rays Dickinson Fallback

When curvature does not exclude the whole upward closure, B8 reuses the retained exact factorization to construct a Dickinson vector $u$.
For nonsingular $B$ the starting vector comes from $Bx=\mathbf1$; for an eligible singular matrix it comes from an oriented kernel ray.
The local vector is embedded in the full space by inserting zeros outside $I$.

Define

$$
L(u)=\{i:u_i\ne0\},\qquad U(u)=\{j:(Au)_j\ge0\}.
$$

Dickinson's theorem certifies every support $J$ satisfying

$$
L(u)\subseteq J\subseteq U(u).
$$

The inherited Halfspace-Rays search sweeps exact breakpoints along coordinate directions. It prefers larger $|U|$ and then larger interval
width $|U|-|L|$. A bounded shortlist supplies at most two complementary combined-ray trials after the coordinate sweep stalls. Every
accepted endpoint is recomputed exactly.

The interval becomes the Boolean clause

$$
\left(\bigvee_{i\in L(u)}\neg s_i\right)
\lor
\left(\bigvee_{j\notin U(u)}s_j\right)
\lor c_{|U(u)|+1}.
$$

The cardinality literal retires the interval above $|U(u)|$, where no selected support can lie inside it. If $U(u)=[n]$, this is simply a
permanent upward closure from $L(u)$.

## Resumable NBC Traversal

The support generator maintains one ascending stream. A Batcher sorting network provides exact-cardinality assumptions. For the active
cardinality $k$, the generator returns one open support and advances its prefix-cube cursor beyond that support. The next call resumes from
the remaining disjoint prefix cubes, so no exact-support blocker is needed merely to ask for another model.

Every proved certificate is added immediately. When a cardinality is exhausted, B8 compacts the externally retained intervals and commits
the finished frontier before moving to $k+1$. The compaction is a bounded breadth-first heuristic: it first seeks useful merges one level
below the completed layer, then two levels below, and so on. Failure to find every possible merge affects speed only; it cannot alter the
covered set.

A conflict under cardinality or prefix assumptions ends only that query. A conflict in the permanent clauses is latched as global
exhaustion. This distinction is what makes repeated one-model calls sound.

## Complete Decision Flow

1. Install every exact failing-pair upward clause.
2. Start at cardinality $k=1$.
3. Ask the resumable NBC generator for one open support of size $k$.
4. Factorize its principal matrix exactly.
5. If an exact negative or zero witness decides a requested predicate, record it immediately.
6. If the reduced Hessian is not positive definite, insert the full upward curvature clause.
7. Otherwise build and optimize the exact Halfspace-Rays Dickinson interval and insert it.
8. Request the next open support of the same cardinality.
9. When the layer is empty, compact the retained certificates, commit the finished frontier, and increment $k$.
10. Stop when a witness decides the request or the permanent clauses prove that no support remains.

The loop never starts a high-cardinality stream and never creates a downward clause.

## Exact Arithmetic, Diagnostics, And Termination

All matrix decisions, factorizations, inertia counts, signs, kernel vectors, witnesses, and retained certificates use exact
arbitrary-precision integer arithmetic. Standard floating arithmetic is used only to size a bounded ray shortlist; it cannot change any
mathematical result or pruning decision.

Diagnostics report the current low cardinality, selected supports, singularity statistics, and chronological pair-curvature,
support-curvature, and Dickinson certificates. B8 emits no `frontier=high` visit and no `coverage=downward` certificate.

The finite nonempty support lattice and the resumable no-repeat cursor guarantee termination unless an explicit timeout or resource limit
interrupts the run. Such an interruption remains unresolved; it is never converted into a copositivity result.

## Known Difficult Inputs

B8 deliberately gives up the high-frontier certificates that can make B7 fast when a large principal matrix is positive definite or
singular positive semidefinite with a consistent all-ones system. It is therefore difficult on strictly copositive matrices whose useful
proof information appears mainly near the top of the lattice.

It is also difficult when low-cardinality faces stay strictly convex while their Halfspace-Rays intervals have small upper endpoints. Then
many exact principal factorizations remain necessary. Large integer entries and nearly singular principal matrices additionally increase the
cost of exact arithmetic. Interval compaction controls Boolean state growth but cannot compensate for intrinsically weak certificates.
