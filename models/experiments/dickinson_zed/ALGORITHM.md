# Dickinson Zed

Classification: coposit-created exact strict-copositivity realization of Dickinson's Algorithms 1–3 and Theorem 6.5. It copies the
ordinary Dickinson traversal and adds the paper's maximal principal $Z$-matrix blocks before support enumeration.

## Decision Mode

This experiment supports strict copositivity only. A non-strict request throws `std::invalid_argument`. External analysis calls may
independently enable or disable coposit's shared prechecks and connected-component stage; the maximal-$Z$-block stage described here
is part of the model and always runs.

## What The Algorithm Does

Dickinson's ordinary algorithm builds a finite collection of vectors that certifies copositivity. It considers supports: sets of
coordinates on which a possible critical nonnegative vector could live. For a support that has not already been explained by an
earlier certificate vector, it solves one exact linear system, or takes one nullspace vector when the corresponding principal matrix
is singular.

Dickinson Zed first finds every maximal principal block whose off-diagonal entries are nonpositive. Strict copositivity and positive
definiteness are equivalent on such a block. A block that is not positive definite therefore rejects the complete matrix. A positive-
definite block certifies every one of its principal subblocks at once. The ordinary Dickinson traversal then processes only supports
outside all such certified downsets.

One certificate vector can cover many larger supports, so the algorithm may skip a substantial part of the theoretical $2^n-1$
search space. The worst case still visits every nonempty principal subset. Unlike the geometric models, it does not divide the
simplex or the nonnegative cone.

## Model Identity

The identifier is `dickinson_zed`. “Zed” spells out the letter in “$Z$-matrix” so the additional certificate family remains visible
in model names and result tables. The model was copied from `dickinson_final`; its ordinary coverage test, exact solve, singular-vector
choice, strict termination, and support order remain unchanged.

The paper reports that the algorithm had not been implemented, so there is no author executable or author timing path to reproduce.
Increasing-cardinality subset order, numeric-mask order, the admissible null-vector selection, packed signatures, and exact LDLT are
documented implementation choices left open by the paper or lossless representations of its decisions.

## Name And Source

The identifier follows coposit's `<first-author>_<year>` rule. It names Peter J. C. Dickinson and the publication year of:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

The ordinary portion implements Algorithms 1 and 2 and Theorem 4.6. The new first stage realizes Section 6: Lemma 6.2, Algorithm 3,
and especially the maximal-set construction in the proof of Theorem 6.5. Dickinson explicitly enlarges a principal $Z$-matrix $A_I$
to a maximal $A_J$ and uses one certificate on $J$ to account for every $I\subseteq J$.

Dickinson does not specify how to enumerate all maximal sets $J$. Bron–Kerbosch enumeration is a coposit implementation choice. The
paper describes a positive-semidefiniteness check, such as Cholesky, as the direct way to decide a $Z$-matrix, and offers Algorithm 3
as a certificate-vector alternative. This strict model uses exact LDLT and the strict specialization “positive definite if and only
if strictly copositive”; it does not run Algorithm 3's component solves merely to reconstruct a vector that the Boolean API does not
return.

## Terms Used Below

For a vector $u$:

- its **support**, written $\operatorname{supp}(u)$, is the set of indices where $u_i\neq0$;
- its nonnegative-product set is
  \[
  N_A(u)=\{i:(Au)_i\geq0\};
  \]
- $A_I$ is the principal matrix formed by keeping the rows and columns in an index set $I$.
- a symmetric **$Z$-matrix** has $a_{ij}\leq0$ for every $i\neq j$;
- a **maximal $Z$-block** is an index set $J$ for which $A_J$ is a $Z$-matrix and no strict superset has that property;
- the **downset** of $J$ is $[\varnothing,J]=\{I:I\subseteq J\}$.

The implementation embeds every vector calculated for $A_I$ into the full space by putting zeros outside $I$.

## Finding Every Maximal Zed Block

Construct the compatibility graph $H_A$ on matrix indices by

\[
\{i,j\}\in E(H_A)\quad\Longleftrightarrow\quad a_{ij}\leq0.
\]

An index set $J$ is a $Z$-block exactly when every two distinct vertices in $J$ are adjacent, meaning that $J$ is a clique. A maximal
$Z$-block is therefore a maximal clique. “Maximal” means that the set cannot be enlarged; it does not mean that it has globally
maximum cardinality. Several incomparable maximal blocks may exist and may overlap.

The implementation stores each adjacency row as the same dynamic packed support used by Dickinson certificates. It runs the
Bron–Kerbosch maximal-clique recursion with three sets:

- $R$, the current clique;
- $P$, vertices that can still extend $R$; and
- $X$, compatible vertices already handled through an earlier branch.

At each recursion it chooses one pivot from $P\cup X$ and branches only on $P\setminus N(p)$. For a chosen vertex $v$, the child state
is

\[
(R\cup\{v\},\;P\cap N(v),\;X\cap N(v)).
\]

The condition $P=X=\varnothing$ emits $R$ exactly once as a maximal clique. Cliques of cardinality one are ignored by this additional
stage: they add no new pruning, and the unchanged cardinality-one Dickinson traversal still handles them when no larger downset does.

## Exact Strict Decision For A Zed Block

For a real symmetric $Z$-matrix $B$,

\[
B\text{ is copositive}\iff B\succeq0,
\qquad
B\text{ is strictly copositive}\iff B\succ0.
\]

The second equivalence is decisive. If $B$ is PSD but singular, a nonzero kernel vector $y$ gives a nonnegative zero after taking
componentwise absolute values, because the nonpositive off-diagonal entries ensure

\[
|y|^TB|y|\leq y^TBy=0.
\]

PSD gives the reverse inequality, hence equality. If $B$ has a negative direction, the same absolute-value inequality gives a
nonnegative negative direction. Thus every non-PD $Z$-block proves that the complete matrix is not strictly copositive.

Before factorization, the model applies Dickinson Algorithm 3's connected-component split. Inside $J$, form the strictly negative
graph

\[
\{i,j\}\in E(G^-(A_J))\quad\Longleftrightarrow\quad a_{ij}<0.
\]

Two different components have no negative entry between them. Because $A_J$ is a $Z$-matrix, those entries are also nonpositive and
must therefore be zero. After a symmetric permutation, $A_J$ is block diagonal with these component principal matrices. Hence

\[
A_J\succ0\quad\Longleftrightarrow\quad A_C\succ0
\text{ for every component }C.
\]

The implementation finds the components by a direct breadth-first scan over the indices of $J$. Each component is copied into
reusable principal-matrix storage and factorized with exact fraction-free LDLT. A non-PD component returns `false` immediately. If
every component is PD, then $A_J$ is PD and certifies every $I\subseteq J$, since every principal submatrix of a positive-definite
matrix is positive definite. The packed upper endpoint $J$ is retained once; its individual subsets are never generated or stored.

Overlapping downsets are harmless. Coverage asks whether the current support is contained in at least one retained $J$; the Boolean
union automatically treats a support covered by several blocks as one already-proved obligation.

## When A Subset Is Already Covered

A previously generated vector $u$ covers a subset $I$ exactly when

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

This is exactly the coverage condition in Theorem 4.6. A certificate vector must also lie outside $-\mathbb R_+^n$, meaning that
it has at least one positive component. In the singular branch the implementation orients the vector to ensure this.

When the displayed containment holds, the certificate already accounts for the subset $I$, so the algorithm skips $A_I$. coposit
stores only $\operatorname{supp}(u)$ and the sign of every component of $Au$, because those are
the only facts needed for later coverage tests.

The current subset $I$, $\operatorname{supp}(u)$, and $N_A(u)$ use coposit's shared packed support representation. Each support stores
$\lceil n/64\rceil$ unsigned 64-bit words, so coverage is exactly two wordwise subset tests. The traversal also keeps its ordered
index vector because forming $A_I$ requires those indices directly; maintaining both views avoids converting the packed support on
every uncovered subset.

Retained signatures are partitioned by the lowest index in $\operatorname{supp}(u)$. Any signature that covers $I$ must have that
index in $I$, so coverage checks inspect only the buckets named by the indices of $I$. Each bucket is searched newest first. The
result remains the same Boolean existence test over all eligible signatures; only signatures that cannot cover $I$ are omitted from
the scan. Dickinson Zed first tests whether $I\subseteq J$ for a retained maximal PD $Z$-block, then searches the ordinary signature
buckets only when no such downset covers $I$.

## Processing An Uncovered Subset

Let $C=A_I$.

### Nonsingular principal matrix

Solve the single system

\[
Cw=\mathbf 1
\]

exactly. If $w\leq0$, then $z=-w\geq0$ and

\[
z^TCz=w^TCw=w^T\mathbf1<0.
\]

Thus $z$, embedded in the full coordinate space, is an explicit negative witness and the complete matrix is not copositive.

If $w$ has a positive component, embed it as the next certificate vector.

### Singular principal matrix

Choose one nonzero exact vector in the nullspace of $C$:

\[
Cw=0.
\]

Orient its sign so that it has a positive component. This gives the certificate vector required by the paper's singular branch. If
the resulting embedded vector is nonnegative, it is also a zero of the quadratic form and will matter for the final strict test.

The fraction-free LDLT factorization stops with exact rank $r<m$. The implementation sets one free coordinate in its transformed
system, solves the completed triangular equations backwards, and reverses the factorization's coordinate operations. The result is
one nonzero exact integer vector $w$ with $Cw=0$, for every positive nullity $m-r$. It does not construct a nullspace basis.

### Information retained from the vector

For every accepted $w$, the implementation calculates:

1. the support of its full embedded vector $u$;
2. the sign pattern of $Au$, used for future coverage;
3. whether $u\geq0$ and $u^TAu=0$.

The full vector is then discarded. Keeping only this signature saves memory without changing any later decision.

Progress reporting distinguishes four counts. `visited` is every support reached by enumeration; `covered` is a support skipped by
an existing signature; `processed` is an uncovered support sent to exact solve or nullspace work; and `certificates` increments only
after a new signature has been retained. A terminating negative witness or strict zero is processed but not retained, so the final
processed count can exceed the certificate count.

## Strict Zero Termination

The published algorithms decide non-strict copositivity. coposit must distinguish a strictly copositive matrix from a copositive
matrix that has a nonnegative zero.

Any generated nonnegative zero is already a direct proof that strict copositivity fails; Corollary 5.3 is not needed for that
direction. The converse is the important part: if the matrix has a nonnegative zero, Lemma 5.2 and Corollary 5.3 ensure that a
completed non-strict certificate contains a minimal one, up to positive scaling. Consequently:

\[
A\text{ is strictly copositive}
\iff
\text{the completed certificate contains no nonnegative zero}.
\]

This strict-only model returns `false` immediately on a generated nonnegative zero. Corollary 5.3 supplies the converse: if the finite
ordinary traversal finishes without such a zero, its completed Dickinson certificate proves that no minimal nonnegative zero exists.

## When A Zed Downset Dominates A Dickinson Certificate

The following criterion is a local deduction for this hybrid, not a theorem stated in either source paper. Let
$\mathcal B$ be the retained PD Zed blocks and let their combined nonempty downset be

\[
\mathcal D_{\mathcal B}
=
\{K\ne\varnothing:K\subseteq J\text{ for some }J\in\mathcal B\}.
\]

Suppose processing a support $I\in\mathcal D_{\mathcal B}$ would produce a Dickinson vector $u$. Write

\[
S=\operatorname{supp}(u),
\qquad
N=N_A(u),
\qquad
\mathcal C_u=[S,N].
\]

The principal equation gives $S\subseteq I\subseteq N$. The certificate is completely redundant with the Zed downsets exactly
under the following equivalent conditions:

\[
\mathcal C_u\subseteq\mathcal D_{\mathcal B}
\quad\Longleftrightarrow\quad
N\in\mathcal D_{\mathcal B}
\quad\Longleftrightarrow\quad
N\subseteq J\text{ for some }J\in\mathcal B.
\]

The proof is short. $N$ is the unique largest member of $[S,N]$. If a block contains $N$, its downset contains every member of the
interval. Conversely, if the whole interval is already covered, then its largest member $N$ is covered and therefore lies inside
at least one retained block.

This gives an exact no-loss rule: **if some retained Zed block contains $N_A(u)$, skipping the solve loses no Dickinson coverage and
is strictly preferable once the Zed-block work has already been paid.** If no retained block contains $N_A(u)$, the certificate
would add at least its maximal support $N_A(u)$ beyond the Zed downsets. Another ordinary certificate may already cover some or all
of that interval, so this is an exact comparison with the Zed coverage—not by itself a claim about the complete current state.
Set containment alone also does not prove whether paying for the current solve is faster overall.

For one block $J$, write $\mathcal D_J=\{K\ne\varnothing:K\subseteq J\}$. The comparison can be counted exactly by support
cardinality. Let

\[
s=|S|,\qquad q=|N|,\qquad j=|J|,\qquad c=|J\cap N|.
\]

At cardinality $r$, using the convention that an impossible binomial coefficient is zero,

\[
\begin{aligned}
|\mathcal C_u\setminus\mathcal D_J|_r
&=\binom{q-s}{r-s}-\binom{c-s}{r-s},\\
|\mathcal D_J\setminus\mathcal C_u|_r
&=\binom{j}{r}-\binom{c-s}{r-s}.
\end{aligned}
\]

Thus the certificate has more unique coverage at layer $r$ exactly when

\[
\binom{q-s}{r-s}>\binom{j}{r}.
\]

Summing these quantities over the unvisited cardinalities gives the exact unweighted set-count comparison at the start of a
cardinality layer. Inside a layer, the same statement holds after intersecting both families with the still-unvisited supports.
With several overlapping Zed blocks, the containment theorem remains exact, while exact counts require taking the union of their
downsets rather than adding the per-block counts.

This is not a complete runtime theorem. Principal problems of different orders have different exact-arithmetic costs, later
certificates can overlap the same supports, and $N_A(u)$ is generally unknown until $A_Iw=\mathbf1$ or the nullspace problem has
already been solved. Therefore no rule based only on $|I|$, $|J|$, or the Zed sign graph can always choose the faster action.
Singletons are the useful exception. If singleton $\{i\}$ is covered by a retained PD Zed block, then $a_{ii}>0$, its solution is a
positive multiple of $e_i$, and

\[
N_A(e_i)=\{k:a_{ki}\geq0\}.
\]

Its complete certificate interval is known from one matrix column without a factorization. This makes retaining singleton
certificates before activating positive Zed downsets an exact, cheap way to protect the widest possible lower endpoints. For larger
supports, once the vector has been computed to learn $N_A(u)$, retaining its nonredundant signature is always cheaper than throwing
away information that has already been paid for.

## Complete Control Flow

1. Build $H_A$ from every nonpositive off-diagonal entry.
2. Enumerate every maximal clique $J$ of cardinality at least two.
3. Split $A_J$ into its strictly negative connected components and factor each component exactly. Return `false` if one is not PD;
   otherwise retain the downset endpoint $J$.
4. Visit subset sizes $1,2,\ldots,n$ in the unchanged numeric-mask order.
5. Skip $I$ if $I\subseteq J$ for a retained PD $Z$-block or an ordinary Dickinson signature covers it.
6. Otherwise form $A_I$ and either solve $A_Iw=\mathbf1$ or take a nullspace vector.
7. Reject immediately if the nonsingular solve produces $w\leq0$ or a singular solve produces a nonnegative zero.
8. Otherwise retain the ordinary Dickinson signature.
9. Return `true` after every remaining support has been discharged.

The traversal is finite because there are only $2^n-1$ nonempty subsets.

## Known Difficult Inputs

### Important: positive Zed coverage can suppress stronger Dickinson certificates

**A positive Zed downset is not automatically beneficial in an increasing-cardinality Dickinson traversal.** Coverage is tested
before an uncovered support is solved. Consequently, when a retained PD Zed block $J$ covers a small support $I\subseteq J$, the
algorithm skips $I$ and never generates the ordinary Dickinson vector that $I$ would have produced. The Zed block removes only the
downward interval $[\varnothing,J]$, whereas the missing ordinary vector $u$ could have removed the much larger upward interval

\[
[\operatorname{supp}(u),N_A(u)]
=
\{K:\operatorname{supp}(u)\subseteq K\subseteq N_A(u)\}.
\]

Corpus matrix 10322, of order 20, demonstrates this interaction exactly. With $\mathbf1\in\mathbb R^{18}$, it has the block form

\[
A=
\begin{pmatrix}
2 & -5 & 4\mathbf1^T\\
-5 & 14 & -9\mathbf1^T\\
4\mathbf1 & -9\mathbf1 & 6\mathbf1\mathbf1^T
\end{pmatrix}.
\]

Using one-based indices, let $V=\{1,\ldots,20\}$ and let $L=V\setminus\{2\}$ be the 19 leaves. The graph of nonpositive
off-diagonal entries is a star centred at index 2: its edges are $\{1,2\}$ and $\{2,j\}$ for $3\leq j\leq20$. No two leaves
are adjacent. Consequently, these 19 edges are exactly the 19 maximal Zed blocks. Every edge block is PD—each has determinant
three—and the union of their downsets covers every singleton and the 19 star-edge pairs.

Plain Dickinson does the following:

1. It processes all 20 singletons. For every leaf $i\in L$, the singleton vector is a positive multiple of $e_i$ and
   $N_A(e_i)=V\setminus\{2\}$. Its certificate therefore covers every support that contains $i$ but does not contain the centre.
   Together, the 19 leaf certificates cover every nonempty support contained in $L$. The centre certificate has
   $N_A(e_2)=\{2\}$ and covers only its own singleton.
2. At cardinality two, the 171 leaf-leaf pairs $\binom{19}{2}$ are already covered. The only uncovered pairs are the 19 star edges.
3. Solving $A_{\{1,2\}}w=\mathbf1$ gives $w=(19,7)^T/3$. Solving $A_{\{2,j\}}w=\mathbf1$ for $j\geq3$ gives
   $w=(15,23)^T/3$. In both cases the embedded vector has $N_A(w)=V$. These 19 certificates cover every larger support that
   contains the centre and at least one leaf.
4. Nothing remains uncovered. Plain Dickinson has solved 20 one-dimensional and 19 two-dimensional systems: 39 systems in total.

Dickinson Zed changes the order of events:

1. Before cardinality traversal starts, the 19 PD edge downsets mark all 20 singletons and all 19 star edges as covered.
2. Cardinality one therefore produces no ordinary certificate. In particular, the 19 broad leaf certificates with upper endpoint
   $V\setminus\{2\}$ never exist.
3. At cardinality two, the downsets cover only the 19 star edges. The other 171 pairs are no longer covered by singleton
   certificates, so all 171 must be solved. The 153 pairs $\{j,k\}$ with $3\leq j<k\leq20$ are singular because their matrix
   columns are identical; $e_j-e_k$ is a null vector with $A(e_j-e_k)=0$, so each has upper endpoint $V$. For each of the other
   18 leaf pairs $\{1,j\}$, the exact solution is $(-1,1)^T/2$ and its embedded product is negative only at index 2. Those
   certificates therefore have upper endpoint $V\setminus\{2\}$.
4. The pair certificates cover every triple containing two indices from $\{3,\ldots,20\}$ and every triple containing $\{1,j\}$
   but not index 2. Exactly the 18 triples $\{1,2,j\}$, $3\leq j\leq20$, remain. Each must also be solved. For each such triple,
   the exact solution is $(7,6,5)^T/4$ in the coordinate order $(1,2,j)$, and its embedded product is the full all-ones vector.
   These triple certificates finally cover every remaining larger support.
5. Dickinson Zed has therefore solved 171 two-dimensional and 18 three-dimensional systems: 189 systems in total.

| Traversal | Cardinality 1 solved | Cardinality 2 solved | Cardinality 3 solved | Total exact systems |
|---|---:|---:|---:|---:|
| Plain Dickinson | 20 | 19 | 0 | 39 |
| Dickinson Zed | 0 | 171 | 18 | 189 |

The 171 pair certificates are valid and can cover many larger supports, but collectively they are a fragmented replacement for the
19 leaf-singleton certificates whose intervals begin one Boolean-lattice level lower. The Zed variant therefore performs 4.85 times
as many ordinary exact solves. The cost is not mainly the 19 packed block lookups: the valid downward cuts prevent the creation of
substantially stronger upward certificates. This does not affect correctness; it changes which valid certificates the traversal
gets a chance to generate.

The star is a particularly transparent example, not a necessary condition. The same weakness can occur whenever retained PD Zed
blocks touch many vertices and thereby cover low-cardinality supports whose ordinary Dickinson vectors would have large sets
$N_A(u)$. It becomes more damaging as the matrix order grows: moving an interval's lower endpoint from a singleton to a pair can
remove an exponentially large part of that interval. Conversely, the interaction is absent when there is no nonpositive
off-diagonal edge, and it is mild when the skipped low-cardinality supports would themselves have produced narrow certificates.

Dickinson avoids subsets only when an existing vector covers a wide interval

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

It struggles when generated vectors have $N_A(u)$ only slightly larger than their own support. Such a vector covers few additional
subsets, so the algorithm approaches the full $2^n-1$ traversal and performs an exact factorization for many of them.

On boundary matrices whose first nonnegative zero has large support, the algorithm may still enumerate many earlier supports before
generating that decisive zero. Once it is generated, strict mode stops without processing any remaining supports, whereas non-strict
mode keeps the signature and finishes the copositivity certificate.

Matrices with many singular principal submatrices are also unfavorable. They still require exact factorization and can produce
kernel vectors with weak coverage, although recovering one vector from the retained partial factorization avoids a second
elimination.

The additional first stage can itself be exponential. A graph may have exponentially many maximal cliques; Dickinson's Example 6.7
constructs exactly this obstruction for the extended certificate. Near-random sign patterns may therefore spend substantial time
enumerating overlapping blocks before ordinary Dickinson begins. Positive off-diagonal entries also break compatibility cliques, so
matrices dominated by positive entries may obtain little downset coverage despite paying for the graph traversal.

The component split avoids an unnecessary large exact factorization when a maximal $Z$-block contains many zero links. For example,
an identity matrix is one maximal $Z$-block but has only singleton negative components. A block whose negative graph is connected
still requires one full exact factorization, so large dense $Z$-blocks can remain arithmetically expensive.

## Exact Arithmetic And Fidelity

The nonsingular solve uses fraction-free LDLT. It stores the numerator of $w$ together with a positive denominator. Multiplying a
vector by a positive value preserves its signs, support, coverage, witness status, and zero status, so the denominator need not be
carried into later checks. Singular vectors are exact integers recovered from the same partial factorization.

The principal-matrix, one-column solution, and full-product storage are reused between uncovered subsets. Only the lower triangle
of each principal matrix is copied because the symmetric fraction-free LDLT implementation reads and overwrites that triangle
exclusively. These are representation and allocation optimizations and do not change the generated vectors or certificate.

The paper fixes the coverage theorem and both branches for every uncovered subset but leaves the subset order and admissible
singular nullspace vector open. Increasing cardinality, numeric-mask order, and one vector obtained from the first free coordinate
of coposit's LDLT factorization are deterministic implementation choices. For nullity greater than one this vector can differ from
another valid basis choice and therefore cover different later subsets; Dickinson's Algorithm 2 explicitly permits any nonzero
null vector outside the nonpositive orthant. Retaining only the coverage signature and storing its two sets as packed words are
lossless representation optimizations.

The strict zero-termination rule and PD rather than PSD block test adapt the paper's non-strict statements to strict copositivity.
Bron–Kerbosch enumeration, packed adjacency, and storing one downset endpoint are implementation choices. The model adds no low-order
test, cone subdivision, Zischg–Bomze Level-2 support skip, or other solver's certificate vectors. Analysis callers may independently
select shared preprocessing; choosing none still runs this model's maximal-$Z$-block stage because it is part of the algorithm being
measured.

Timed native-module builds observe a shared signal flag at principal-subset, certificate, factorization, and matrix-row boundaries
and return a distinct timeout outcome. Standalone model and test builds compile those checkpoints to no-ops, so they add no timer
thread, clock read, signal handler, or changed certificate decision.
