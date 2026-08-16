# Ordinary Dickinson Certificates From Singular And Covered Supports

Status: research finding. The bounded first experiment described in Section 8 is implemented as `cbdd_dickinson_improved_1`; it has
not yet been benchmarked.

Model name: **CBDD Dickinson Improved One**, with identifier `cbdd_dickinson_improved_1`.

This note asks how the singular-support discoveries developed for ceiling-only Dickinson pruning transfer to ordinary Dickinson
certificates $[L,U]$. The practical conclusion is:

> Ordinary Dickinson should not discard a homogeneous or affine candidate merely because some outside products are negative.
> Those negative products only make the upper endpoint $U$ smaller. The most promising next experiment is therefore a CBDD
> Dickinson certificate engine that searches singular homogeneous and affine solution spaces for several exact, nonredundant
> bounded intervals and preserves their complete union.

This is not just the ceiling-only Kernel-Cone model with a relaxed final test. Bounded intervals have a second kind of opportunity
loss: a certificate used now may hide a later support whose certificate extends beyond the current upper endpoint. The candidate
search and the activation policy must account for both lower- and upper-endpoint escape.

The main source for the ceiling-only results is
[`SINGULAR_LIFT_DICKINSON_RESEARCH.md`](SINGULAR_LIFT_DICKINSON_RESEARCH.md). The maintained ordinary interval representation is
described in [`models/hadeler-based/cbdd_dickinson/ALGORITHM.md`](../models/hadeler-based/cbdd_dickinson/ALGORITHM.md), and the current
ceiling-only kernel search is described in
[`models/hadeler-based/kernel_cone_dickinson/ALGORITHM.md`](../models/hadeler-based/kernel_cone_dickinson/ALGORITHM.md).

## 1. Notation And Ordinary Dickinson Coverage

Let $A\in\mathbb Z^{n\times n}$ be symmetric and let

$$
[n]=\{1,\ldots,n\}.
$$

For $I\subseteq[n]$, $A_I$ is the principal matrix indexed by $I$. For a full vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\neq0\}
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

An admissible Dickinson vector has at least one positive component, equivalently

$$
u\notin-\mathbb R_+^n.
$$

It certifies every support in the Boolean interval

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

A ceiling certificate is the special case $U=[n]$. Ordinary CBDD Dickinson retains general bounded upper sets $U\subseteq[n]$.

For a processed support $I$, write

$$
k=|I|,
\qquad
z=k-|L|,
\qquad
r=n-|U|,
\qquad
d=|U|-|L|.
$$

The exact identity

$$
\boxed{d=(n-k)+z-r}
$$

holds for every candidate supported inside $I$. In the ceiling-only case $r=0$, so every vanished local coordinate increases the
interval width. For an ordinary bounded interval, each excluded upper index cancels one such gain.

## 2. What Transfers From Ceiling-Only Research

### 2.1 Results that transfer unchanged

The following statements concern exact linear algebra or Boolean intervals and remain valid for ordinary Dickinson.

1. **Symmetric-border nullity trichotomy.** Adding one row and column to a singular principal matrix changes its nullity according
   to the new border's restriction to the current kernel and, when that restriction vanishes, one exact Schur scalar.
2. **Persistent and ephemeral nullity.** For a kernel basis $Z$ and outside-product matrix $G=A_{[n]\setminus I,I}Z$,
   $\ker G$ consists of root-kernel directions that remain in the full kernel of $A$.
3. **Persistent-kernel certificates.** Every nonzero vector in the persistent kernel has upper endpoint $[n]$ and is therefore
   still an especially strong ordinary certificate.
4. **The free-index identity.** The equation $d=(n-k)+z-r$ remains exact.
5. **Circuit and reduced-Schur calculations.** Projected-row circuits, the circuit Schur scalar, and the reduced matrix $S_J$ still
   describe which lifted principal matrices acquire new kernel directions.
6. **Boolean interval dominance.** For two intervals,

   $$
   [L_2,U_2]\subseteq[L_1,U_1]
   \quad\Longleftrightarrow\quad
   L_1\subseteq L_2\ \text{and}\ U_2\subseteq U_1.
   $$

7. **Exact coverage counts.** A single interval contains $2^{|U|-|L|}$ supports, and its contribution to cardinality layer $s$ is

   $$
   \left|[L,U]\cap\binom{[n]}s\right|=\binom{|U|-|L|}{s-|L|}.
   $$

8. **Modular rejection and exact reconstruction.** Modular calculations may reject impossible rank, kernel, or circuit cases;
   any accepted certificate must still be reconstructed and checked exactly.

### 2.2 Ceiling-only statements that do not transfer

The following tempting conclusions fail once $U$ may be bounded.

1. **An empty ceiling cone does not mean that the singular support has no useful certificate.** Every admissible kernel direction
   gives an ordinary interval; negative outside products merely exclude their indices from $U$.
2. **The projected outside rows alone are not the complete arrangement.** For ordinary intervals, zeros of the local vector can
   shrink $L$ while outside signs enlarge or shrink $U$. Both the rows of $Z$ and the rows of $G$ matter.
3. **A positive-spanning obstruction proves only that no root-confined ceiling direction exists.** It says nothing against bounded
   ordinary intervals.
4. **Ceiling dominance no longer protects every covered support.** A later candidate can be stronger either by dropping an old
   lower index or by extending outside the active upper set.
5. **The emitted-certificate singularity theorem for $d>n-k$ is ceiling-specific.** With bounded certificates, a smaller support
   can be suppressed without suppressing a larger support. The positive-definite nonsingular counterexample recorded in the earlier
   research therefore remains a genuine obstruction to a general ordinary theorem.
6. **The shallowest useful lift need not give the best ordinary interval.** Lift depth does not order both interval endpoints.
7. **The ceiling hypergraph and hitting-set monotonicity do not describe general intervals.** Ordinary intervals impose both
   required-present coordinates $L$ and required-absent coordinates $[n]\setminus U$.

## 3. Homogeneous Search At A Singular Support

Let $A_I$ be singular with nullity

$$
q=\dim\ker A_I,
$$

and let the columns of $Z\in\mathbb Z^{k\times q}$ form an exact kernel basis. Every root-confined homogeneous candidate is

$$
u_I=Zy,
\qquad y\in\mathbb R^q.
$$

Let $R=[n]\setminus I$ and define

$$
G=A_{R,I}Z.
$$

The complete embedded product is

$$
Au=
\begin{pmatrix}
0_I\\
Gy
\end{pmatrix}.
$$

Kernel-Cone Dickinson keeps $y$ only when $Gy\geq0$, because it demands $U=[n]$. Ordinary Dickinson can use every $y$ for which
$Zy$ has a positive component. Its exact endpoints are

$$
L(y)=\{i\in I:(Zy)_i\neq0\},
$$

and

$$
U(y)=I\cup\{j\in R:(Gy)_j\geq0\}.
$$

Thus outside negative entries are information, not failure: they specify $R\setminus U(y)$.

### 3.1 A finite exact search for dominating homogeneous intervals

Stack the local-coordinate and outside-product rows:

$$
H=
\begin{pmatrix}
Z\\
G
\end{pmatrix}
\in\mathbb Z^{n\times q}.
$$

The rows of $Z$ mark where an index enters or leaves $L$. The rows of $G$ mark where an outside index enters or leaves $U$.

#### Theorem 3.1: stacked-flat domination

For every admissible $y\neq0$, there is an admissible $y^\star$ lying on a one-dimensional flat of the central row arrangement of
$H$ such that

$$
L(y^\star)\subseteq L(y)
\qquad\text{and}\qquad
U(y)\subseteq U(y^\star).
$$

Consequently,

$$
[L(y),U(y)]\subseteq[L(y^\star),U(y^\star)].
$$

Therefore it is enough, in principle, to enumerate the one-dimensional flats obtained by setting $q-1$ linearly independent rows
of $H$ to zero, test both orientations of each resulting ray, and retain the intervals that add coverage.

#### Proof

Fix an admissible $y$. Build a cone $K_y$ in coefficient space as follows.

- If $(Zy)_i>0$, require $(Zx)_i\geq0$.
- If $(Zy)_i<0$, require $(Zx)_i\leq0$.
- If $(Zy)_i=0$, require $(Zx)_i=0$.
- For every outside row $j$ with $(Gy)_j\geq0$, require $(Gx)_j\geq0$.
- Impose no condition on outside rows for which $(Gy)_j<0$.

The vector $y$ lies in $K_y$. The cone is pointed: if both $x$ and $-x$ lie in it, every component of $Zx$ is zero, and the full
column rank of $Z$ then gives $x=0$.

A pointed polyhedral cone is generated by its extreme rays. Because $y$ has a positive local coordinate, at least one extreme ray
$y^\star$ must retain a positive value in some local coordinate; otherwise no nonnegative combination of the extreme rays could
reconstruct that positive coordinate of $Zy$. Hence $y^\star$ is admissible.

The defining inequalities prevent a nonzero local coordinate of $y^\star$ from appearing where $Zy$ was zero, so
$L(y^\star)\subseteq L(y)$. They also preserve every outside product that was nonnegative for $y$, so
$U(y)\subseteq U(y^\star)$. Finally, an extreme ray of a pointed cone in $\mathbb R^q$ is cut out by $q-1$ independent active
hyperplanes. Every such hyperplane is a row hyperplane of $Z$ or $G$, hence a row hyperplane of $H$. $\square$

### 3.2 Cost by nullity

The crude complete search considers at most

$$
2\binom{n}{q-1}
$$

oriented row subsets before rank rejection and deduplication.

- If $q=1$, no rows need be selected; test the two orientations of the unique kernel ray.
- If $q=2$, each nonzero row of $H$ defines one candidate line, so there are at most $n$ unoriented candidates.
- If $q=3$, row pairs give $O(n^2)$ candidates.
- For large $q$, complete flat enumeration is likely too expensive without circuit, rank, or marginal-coverage guidance.

The theorem is stronger than the ceiling-only extreme-ray result. It gives a finite dominating family for all root-confined ordinary
kernel certificates, including candidates whose outside products contain negative entries.

## 4. The Affine Companion Search

At the same singular support, test whether

$$
A_Ix=\mathbf1
$$

is consistent. This can reuse the exact factorization that produced $\ker A_I$.

If it is consistent, all solutions form

$$
x=x_0+Zy,
$$

where $x_0$ is one particular solution. Each solution falls into one of two cases.

1. If $x$ has a positive component, its zero extension is an admissible Dickinson vector and gives an exact ordinary interval.
2. If $x\leq0$, then $-x\geq0$ and

   $$
   (-x)^TA_I(-x)=x^TA_Ix=x^T\mathbf1<0.
   $$

   The strict inequality holds because $A_Ix=\mathbf1$ forbids $x=0$. Thus $-x$ is an immediate non-copositivity witness.

The affine family is divided into cells by the hyperplanes

$$
(x_0+Zy)_i=0
$$

and

$$
A_{j,I}(x_0+Zy)=0,
\qquad j\notin I.
$$

Within one open cell, the signs and therefore $L$ and $U$ are constant. On a cell boundary, one or more local coordinates can leave
$L$, or outside indices can enter $U$, so boundaries can yield stronger intervals.

For affine nullity one, all expressions are linear functions of one scalar. An exact breakpoint sweep through their rational roots,
including the roots themselves and one representative from every open interval, enumerates every possible $(L,U)$ signature. This
is a particularly attractive complete search. Higher-dimensional affine arrangement enumeration is exact but can be expensive.

Unlike the homogeneous theorem, this note does not claim that testing only affine one-dimensional flats always yields a dominating
family in arbitrary affine dimension. That requires a separate proof or complete cell-and-face enumeration.

## 5. Exact Opportunity Loss Under Ordinary Pruning

Let the active certificate be

$$
Q=[L,U],
$$

and suppose it covers a support $S$, so $L\subseteq S\subseteq U$. If processing $S$ would produce

$$
Q'=[L',U'],
$$

then

$$
Q'\subseteq Q
\quad\Longleftrightarrow\quad
L\subseteq L'\ \text{and}\ U'\subseteq U.
$$

The hidden candidate is therefore non-dominated exactly when at least one of the following occurs.

1. **Lower escape:** $L\nsubseteq L'$. The later vector loses at least one coordinate required by the active certificate.
2. **Upper escape:** $U'\nsubseteq U$. The later vector certifies at least one index excluded by the active certificate.

Ceiling certificates have no upper-escape risk because $U=[n]$. Ordinary bounded certificates have both risks.

### 5.1 A singularity criterion for lower escape

Suppose $A_S$ has nullity at least two. For every $i\in L$, the kernel contains a nonzero vector $w$ satisfying $w_i=0$: imposing
one coordinate equation on a space of dimension at least two leaves a nonzero subspace. One of $w$ and $-w$ is admissible, and its
lower support omits $i$. Thus a lower-escape certificate exists at $S$ relative to $Q$.

This is an exact warning that immediate activation may hide a non-dominated interval. It does not prove that the interval adds global
coverage, because it may already be covered by the union of other certificates.

No comparable rank or nullity criterion rules out upper escape. The positive-definite counterexample in Section 6 shows that upper
escape can occur with nonsingular principal matrices, full-support solutions, and no zero coordinates.

### 5.2 Global redundancy is a union question

Pairwise dominance is insufficient. Let $C$ be the exact union of all active certificate intervals and let $Q$ be a new valid
interval. Its exact marginal contribution is

$$
Q_{\mathrm{new}}=Q\setminus C.
$$

The candidate is globally redundant exactly when $Q_{\mathrm{new}}=\varnothing$. A CBDD can represent and test this difference
without enumerating the covered supports.

When one singular or affine search produces several candidates, collect the complete local candidate family first. Then add only
the parts not already represented by $C$ or by earlier retained local candidates. This preserves the exact union even when no
single interval dominates another but several intervals jointly cover it.

### 5.3 Safe activation policies

Certificate validity and activation are separate. Delaying or declining to activate a valid certificate cannot make Dickinson
incorrect; it only causes additional exact support solves.

One exact low-risk policy is:

- activate immediately when $L=I$;
- when $L\subsetneq I$, delay activation until the current cardinality layer is complete.

The reason is simple. If $L=I$ and another support $S$ of the same cardinality is covered by $[I,U]$, then $I\subseteq S$ and
$|I|=|S|$ force $S=I$. Such a certificate cannot suppress a peer support. Only a support-contracting interval $L\subsetneq I$ can
hide another support in the current layer.

This policy is exact but may perform substantially more same-layer work. Its value must be measured.

## 6. Counterexamples To Tempting Claims

### 6.1 Positive-definite upper escape with exponential lost coverage

For any $m\geq1$, let $n=m+2$ and define

$$
B=
\begin{pmatrix}
1&2\\
2&5
\end{pmatrix},
\qquad
b=
\begin{pmatrix}
-1\\
-4
\end{pmatrix},
$$

and

$$
A_m=
\begin{pmatrix}
B & b\mathbf1_m^T\\
\mathbf1_m b^T & (5m+1)I_m
\end{pmatrix}.
$$

This matrix is positive definite. Indeed,

$$
b^TB^{-1}b=5,
$$

so the Schur complement of $B$ is

$$
(5m+1)I_m-5J_m.
$$

Its eigenvalue in the all-ones direction is $1$, and its remaining eigenvalues are $5m+1$.

The singleton Dickinson certificates for coordinates 1 and 2 both have upper set

$$
U=\{1,2\}.
$$

Thus either singleton interval suppresses the support $S=\{1,2\}$. But

$$
B^{-1}\mathbf1=
\begin{pmatrix}
3\\
-1
\end{pmatrix},
$$

and every outside product equals

$$
b^T
\begin{pmatrix}
3\\
-1
\end{pmatrix}=1.
$$

Processing $S$ would therefore produce the ceiling interval

$$
[\{1,2\},[n]],
$$

which covers $2^{n-2}$ supports. The earlier bounded interval hides this certificate solely through upper escape.

This counterexample is nondegenerate in the usual linear-algebra sense: $A_m$ is positive definite, every principal matrix involved
is nonsingular, the hidden solution has no zero coordinate, and all decisive inequalities are strict. The phenomenon therefore
survives sufficiently small perturbations.

### 6.2 One selected kernel vector is not enough

Let the principal matrix on $I=\{1,2,3\}$ be $A_I=J_3$. Add two outside rows whose restrictions to $I$ are

$$
g_4=(1,0,1),
\qquad
g_5=(0,1,0).
$$

The remaining entries of the symmetric full matrix can be chosen arbitrarily because they do not affect the following embedded
kernel products. The kernel vectors

$$
u=(1,-1,0)^T,
\qquad
v=(0,1,-1)^T
$$

give the incomparable intervals

$$
[\{1,2\},I\cup\{4\}]
$$

and

$$
[\{2,3\},I\cup\{5\}].
$$

The third kernel vector $(1,0,-1)^T$ gives another incomparable interval with lower set $\{1,3\}$ and upper set $[5]$.

No scalar score and no single “best” kernel basis vector preserves this union. A singular certificate engine must retain every local
interval that adds exact marginal coverage.

## 7. Concrete Algorithm Variants

The variants below are ordered by expected cost and benefit. Every one must leave ordinary Dickinson traversal as the correctness
fallback unless its replacement is separately proved complete.

### 7.1 Proved-safe, low-cost changes

1. **Both nullity-one orientations.** For a mixed-sign one-dimensional kernel ray, test both $w$ and $-w$. They can have different
   upper sets. Discard only the orientation in $-\mathbb R_+^n$.
2. **Singular affine companion.** After factoring singular $A_I$, test consistency of $A_Ix=\mathbf1$. Use one admissible particular
   solution immediately; if it is nonpositive, return the exact negative witness.
3. **Local batch before activation.** Generate the local homogeneous and affine candidate family before allowing one candidate to
   suppress discovery of another.
4. **Exact marginal-coverage filtering.** Keep $[L,U]$ only when its CBDD difference from the current covered union is nonempty.
5. **Contracted-support probe.** If a candidate from $I$ has $L\subsetneq I$, reuse the relation $A_Lu_L=0$ or
   $A_Lu_L=\mathbf1$ to search the smaller support $L$ before activation.
6. **Same-layer delayed activation.** Delay only support-contracting intervals until the current cardinality layer ends.

### 7.2 Exact searches that may be expensive

7. **Stacked-flat homogeneous search for $q=2$.** Enumerate the at most $n$ row lines of $H=(Z^T,G^T)^T$, test both
   orientations, and retain every interval with positive marginal coverage.
8. **Stacked-flat homogeneous search for $q=3$.** Enumerate independent row pairs. This is complete for root-confined homogeneous
   coverage up to dominance but has quadratic candidate growth.
9. **Complete affine nullity-one breakpoint sweep.** Enumerate all local-coordinate and outside-product breakpoints and all open
   intervals between them.
10. **All-nullity stacked-flat search.** Theorem 3.1 makes this exact, but the $\binom{n}{q-1}$ growth likely limits it to small $q$.
11. **Complete higher-dimensional affine arrangement.** Exact cell-and-face enumeration is possible but should wait for evidence
    that affine nullity above one matters often enough.
12. **Circuit-guided genuine lifting.** Use projected-row circuits and the reduced Schur test to propose coordinate-adding lifts,
    but accept useful bounded intervals rather than demanding $U=[n]$.

### 7.3 Heuristics that preserve exact fallback

13. **Upper-endpoint probe.** For an active bounded interval $[L,U]$, process the covered support $U$ as a discovery probe. The
    positive-definite family in Section 6 shows exactly why this can reveal a much broader certificate.
14. **One-border probes.** Reuse the current factorization and symmetric-border formula to test a small number of supports obtained
    by adding an index outside $U$.
15. **Risk-based delayed activation.** Spend look-ahead on bounded intervals, contracted lower supports, singular covered supports,
    and intervals with small exact marginal coverage.
16. **Pairwise kernel-plane sweeps.** At large nullity, search selected two-dimensional coefficient subspaces instead of the full
    arrangement.
17. **Bounded small-circuit jumps.** Test only projected-row circuits up to a small cardinality, then return to ordinary traversal.

These searches are heuristics only in deciding where to spend extra work. Every emitted interval is still checked exactly, and the
unchanged Dickinson traversal handles everything the search does not find.

### 7.4 Ideas unlikely to justify implementation now

1. Porting Kernel-Cone Dickinson unchanged and merely renaming it. It still discards most ordinary certificates.
2. Choosing one certificate by a scalar such as $d$, $|U|$, or raw interval size. Incomparable intervals and union coverage defeat
   every such one-number rule.
3. Another fixed 75%, 90%, or 95% width threshold. The existing threshold experiment did not establish a useful cutoff.
4. Deep BFS or A* singular lifting before trying the direct small-nullity arrangement search.
5. Probing every singular or covered support. That recreates the support enumeration the certificate structure is meant to avoid.
6. Full high-nullity affine arrangements without corpus evidence.
7. Replacing the CBDD representation before improving candidate generation. CBDD already supplies exact union, difference, and
   marginal-coverage operations needed by this investigation.

## 8. Recommended First Experiment

Create the isolated `cbdd_dickinson_improved_1` experiment by copying `cbdd_dickinson` and changing only the singular certificate
engine.

At each singular support $I$:

1. factor $A_I$ exactly and obtain a basis $Z$ of its kernel;
2. test both orientations when $q=1$;
3. test consistency of $A_Ix=\mathbf1$ and use the affine particular solution when admissible;
4. when $q=2$, build $G=A_{[n]\setminus I,I}Z$ and enumerate the distinct one-dimensional row flats of
   $H=(Z^T,G^T)^T$;
5. construct every candidate's exact $L$ and $U$ without requiring $U=[n]$;
6. retain every interval whose difference from the current CBDD union is nonempty;
7. add the complete retained local union only after the local search is finished; and
8. leave all higher-nullity and nonsingular processing unchanged.

This first model has a narrow purpose. It tests whether exact bounded intervals from small singular solution spaces improve ordinary
CBDD Dickinson. It does not yet need genuine lifting, high-nullity arrangement enumeration, or a new Boolean-family representation.

The next independent experiment should test the bounded upper-endpoint probe from Section 7.3. The positive-definite counterexample
shows that singular-only improvements cannot address all ordinary opportunity loss.

## 9. Correctness Boundary And Verification Needed

The proposed certificate generation is exact because every retained vector and every endpoint sign is checked with arbitrary-
precision arithmetic. Omitting a candidate or declining to activate a valid interval cannot change the final answer while ordinary
Dickinson enumeration remains the fallback.

Before implementation, the following should receive focused proof-level tests.

1. The stacked-flat search dominates brute-force sign-cell samples for small rational kernels.
2. Both orientations are retained exactly when admissible and produce the expected different upper sets.
3. Affine inconsistency, admissible affine solutions, and nonpositive affine witnesses remain distinct.
4. CBDD marginal filtering preserves the full union of an incomparable local interval family.
5. Delayed activation changes traversal work but never classification.
6. The positive-definite family in Section 6 reproduces the bounded-certificate opportunity loss for several values of $m$.

No project benchmark was run for this research note, and no algorithm claim here should be presented as a performance improvement
until an isolated implementation is compared with ordinary `cbdd_dickinson` in combined CP/SCP mode.
