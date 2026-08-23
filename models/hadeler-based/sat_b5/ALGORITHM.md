# SAT-B5

Classification: coposit-created exact CP/SCP experiment. SAT-B5 is SAT-B4 without the high-frontier scan and without downward SAT
pruning. It traverses unresolved supports in increasing cardinality, uses curvature and Halfspace-Rays Dickinson certificates to
prune upward, and retains SAT-B4's exact Johnson--Reams Schur-complement reduction.

The model identifier is sat_b5. The public modes copositive and strictly_copositive select one predicate. The both mode classifies
both predicates in one traversal and is the analysis-interface default.

## Idea In Plain Language

SAT-B5 asks one incremental SAT solver for an unresolved support of size 1, then size 2, and so on. For every selected support
\(I\), it factorizes the principal matrix \(B=A_I\) exactly and takes one of three actions:

1. If the quadratic form is not strictly convex on the simplex face \(I\), then \(I\) and every superset of \(I\) can be excluded
   from the search for a minimal-support global minimizer. SAT receives the full upward closure of \(I\).
2. If \(B\) is positive definite and satisfies the Johnson--Reams sign condition, the coordinates in \(I\) can be eliminated
   exactly. The algorithm constructs a smaller Schur-complement matrix, discards the current SAT state, and restarts on that matrix.
3. Otherwise the retained factorization supplies a Dickinson vector. Exact Halfspace-Rays sweeps improve its upper endpoint, and
   SAT receives the resulting interval.

There is no high-cardinality frontier, floating-point filter, or downward closure. Positive definiteness alone does not remove the
subsets of \(I\) in this model. The Schur step is not a downward clause: it replaces the complete classification problem by an
equivalent smaller problem.

## Name, Sources, And Classification

- SAT names the incremental Boolean representation of unresolved supports.
- B5 means the fifth experiment in the curvature-based SAT line.

The model is an independent copy of [SAT-B4](../sat_b4/ALGORITHM.md). It keeps SAT-B4's pair and support curvature tests,
Halfspace-Rays optimization, SAT interval representation, and exact block reduction. Its only control-flow change is removal of the
high-frontier/downward-pruning path.

Dickinson intervals come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” Linear Algebra and its Applications 569
(2019), 15–37, especially Theorem 4.6 and Algorithms 1–2, DOI
[10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

The block reduction is Theorem 4 of Charles R. Johnson and Robert Reams, “Spectral theory of copositive matrices,” Linear Algebra
and its Applications 395 (2005), 275–281, DOI
[10.1016/j.laa.2004.08.008](https://doi.org/10.1016/j.laa.2004.08.008). Immanuel M. Bomze explains its dimension-reduction role in
“Perron--Frobenius property of copositive matrices, and a block copositivity criterion,” Linear Algebra and its Applications 429
(2008), 68–71, DOI [10.1016/j.laa.2008.02.003](https://doi.org/10.1016/j.laa.2008.02.003).

The minimal-support minimizer argument follows Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic
programming,” Discrete Applied Mathematics 156 (2008), 2439–2448, DOI
[10.1016/j.dam.2007.09.020](https://doi.org/10.1016/j.dam.2007.09.020). Using these facts as permanent clauses in a complete exact
CP/SCP classifier is a coposit experiment. CaDiCaL 2.2.1 supplies incremental SAT solving.

## State And Traversal

For every current matrix of order \(m\), SAT-B5 owns:

- Boolean variables \(s_i\), where \(s_i\) means \(i\in I\);
- a Batcher sorting network whose outputs impose an exact support cardinality;
- permanent upward-curvature and Dickinson-interval clauses;
- one current cardinality \(k\), beginning at 1.

At cardinality \(k\), SAT repeatedly returns an uncovered support \(I\) with \(|I|=k\). Every processed support is covered by the
certificate produced from that support, so it cannot be returned again. When SAT reports that the layer is empty, \(k\) increases.
The traversal ends after the last nonempty layer or earlier when an exact witness decides the requested predicate.

## Curvature Upward Pruning

The tangent space of the simplex face \(I\) is

\[
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
\]

If \(B=A_I\) is not positive definite on \(\mathcal T_I\), no superset containing \(I\) can be the support of a strictly convex
minimal-support global minimizer. SAT-B5 therefore installs

\[
\bigvee_{i\in I}\neg s_i,
\]

which excludes \(I\) and every superset.

Before traversal, every pair \(\{i,j\}\) receives the equivalent one-dimensional test

\[
A_{ii}+A_{jj}-2A_{ij}>0.
\]

A failing pair immediately contributes \(\neg s_i\lor\neg s_j\).

For a nonsingular \(B\), let

\[
\delta=\mathbf1^TB^{-1}\mathbf1.
\]

The reduced Hessian is positive definite exactly when either \(B\succ0\), or \(B\) has one negative eigenvalue and
\(\delta<0\). SAT-B5 gets the inertia and the sign of \(\delta\) from the retained exact fraction-free \(LDL^T\) factorization.

For singular \(B\), strict tangent-space convexity holds exactly when \(B\succeq0\), its nullity is one, and its kernel vector \(z\)
satisfies \(\mathbf1^Tz\neq0\). The kernel ray is recovered exactly. A nonnegative orientation is also an exact zero witness, so it
disproves strict copositivity while ordinary copositivity classification continues.

If the all-ones solution is nonpositive, its negative orientation supplies a nonnegative vector of negative quadratic value and
immediately disproves copositivity.

## Exact Schur-Complement Reduction

For an exactly positive-definite selected block, permute the matrix into

\[
A=\begin{pmatrix}B&C\\C^T&D\end{pmatrix},
\]

where \(B=A_I\). Define

\[
W=-B^{-1}C,
\qquad
S=D-C^TB^{-1}C.
\]

SAT-B5 applies the reduction only if \(W\geq0\) entrywise. Completing the square gives

\[
\begin{pmatrix}x\\y\end{pmatrix}^{T}
A
\begin{pmatrix}x\\y\end{pmatrix}
=(x-Wy)^TB(x-Wy)+y^TSy.
\]

Because \(B\succ0\) and \(W\geq0\), the minimizing choice \(x=Wy\) is feasible whenever \(y\geq0\). Consequently,

\[
A\text{ is CP/SCP}\iff S\text{ is CP/SCP}.
\]

The implementation avoids rational matrix storage. If \(p=|\det B|>0\), it solves \(BX=-pC\), so that \(X=pW\) is integral. The
sign test becomes \(X\geq0\), and the reduced integer matrix is

\[
R=pD+C^TX=pS.
\]

Positive common integer content is removed from \(R\). The current SAT solver and matrix are then discarded, \(R\) becomes the
active matrix, and traversal restarts from cardinality 1. Repeated successful reductions are iterative, so only one active matrix
and SAT instance are retained.

Before solving all columns, the retained factorization solves \(Bz=p\mathbf1\). If an outside column \(c\) satisfies \(c^Tz>0\),
then \(W\geq0\) is impossible and the reduction is rejected immediately. A rejection reuses \(z\) in the Dickinson fallback.

## Halfspace-Rays Dickinson Fallback

When curvature does not prune upward and Schur reduction does not apply, the exact factorization supplies a local vector. For a
nonsingular \(B\), SAT-B5 solves \(Bx=\mathbf1\). For a singular \(B\), it uses an exact kernel ray. Embed the local vector in the
full current dimension by adding zeros outside \(I\). For the embedded vector \(u\), define

\[
L(u)=\{i:u_i\neq0\},
\qquad
U(u)=\{j:(Au)_j\geq0\}.
\]

Dickinson's theorem certifies every support \(J\) satisfying

\[
L(u)\subseteq J\subseteq U(u).
\]

For nonsingular \(B\), the same factorization solves all coordinate right-hand sides. SAT-B5 performs exact breakpoint sweeps along
those directions. It prefers larger \(|U|\), then larger width \(|U|-|L|\). A bounded shortlist supplies at most two complementary
combined-ray sweeps after the coordinate sweeps stall. Every accepted interval is exact.

SAT stores the interval as

\[
\left(\bigvee_{i\in L(u)}\neg s_i\right)
\lor
\left(\bigvee_{j\notin U(u)}s_j\right)
\lor c_{|U(u)|+1},
\]

where \(c_{|U|+1}\) is the sorting-network output saying that at least \(|U|+1\) indices are selected. This last literal retires the
interval automatically above its largest relevant cardinality.

## Complete Decision Flow

1. Build the incremental SAT instance and exact-cardinality sorting network.
2. Install every failed pair-curvature upward clause.
3. Set \(k=1\).
4. Ask SAT for an unresolved support \(I\) of cardinality \(k\). If none exists, increment \(k\).
5. Copy and exactly factor \(A_I\).
6. If an exact negative witness exists, return not copositive.
7. If an exact nonnegative kernel vector exists, record not strictly copositive and continue ordinary copositivity classification
   when required.
8. If the reduced Hessian is not positive definite, install the full upward curvature closure.
9. Otherwise, if \(A_I\succ0\), test the exact Johnson--Reams reduction. On success, replace the matrix by its Schur complement and
   restart at step 1.
10. If no reduction occurs, construct and optimize one Halfspace-Rays Dickinson interval and add it to SAT.
11. Continue until every nonempty support is covered. Return copositive, and return strictly copositive unless an exact zero was
    found.

The proof search is finite because each selected support is covered permanently and the current Boolean lattice is finite. Every
Schur reduction strictly lowers the matrix order.

## Exact Representation And Diagnostics

All factorizations, inertia decisions, systems, kernel vectors, ray breakpoints, products, certificates, reductions, and witnesses
use arbitrary-precision integers. SAT-B5 contains no floating-point arithmetic.

Diagnostics report the active reduced problem's cardinality, visited supports, singular nullities, interval widths and upper sizes,
pair/support upward closures, ray sweeps, and Schur reductions.

## Known Difficult Inputs

SAT-B5 is difficult when small supports remain strictly convex, the Johnson--Reams sign condition usually fails, and Dickinson upper
sets are narrow. It then receives neither the former high-frontier downward closures nor a useful dimension reduction and may have
to process a large fraction of the support lattice. Exact arithmetic also becomes expensive when principal factorizations create
large intermediate integers. The model intentionally accepts these risks to isolate the value of ascending-only upward pruning and
exact block reduction.
