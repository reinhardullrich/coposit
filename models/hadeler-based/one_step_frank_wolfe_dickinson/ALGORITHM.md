# Exact One-Step Frank–Wolfe Dickinson

Classification: coposit-created CP/SCP variant with one exact centre-to-vertex Frank–Wolfe line minimization before
the maintained exact Dickinson certificate traversal.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## What The Algorithm Does

The model first makes one cheap attempt to disprove strict copositivity. It starts at the centre of the standard simplex, chooses the
coordinate vertex with the smallest exact row sum, and minimizes the quadratic exactly along that one segment. The minimizer is a
rational point, but positive homogeneity turns it directly into integer weights. One exact integer quadratic evaluation either
supplies a nonpositive witness or transfers control to Dickinson.

Dickinson's fallback builds a finite collection of vectors that certifies copositivity. It considers supports: sets of coordinates
on which a possible critical nonnegative vector could live. For a support that has not already been explained by an earlier
certificate vector, it solves one exact linear system, or takes one nullspace vector when the corresponding principal matrix is
singular.

One certificate vector can cover many larger supports, so the algorithm may skip a substantial part of the theoretical $2^n-1$
search space. The worst case still visits every nonempty principal subset. Unlike the geometric models, it does not divide the
simplex or the nonnegative cone.

## Name, Sources, And Classification

The descriptive identifier is `one_step_frank_wolfe_dickinson`:

- **one step** means exactly one line minimization from the simplex centre, with no iteration or restart;
- **Frank–Wolfe** names the conditional-gradient choice of a simplex vertex;
- **Dickinson** names the complete exact fallback.

Primary sources are:

- Marguerite Frank and Philip Wolfe, “An Algorithm for Quadratic Programming,” *Naval Research Logistics Quarterly* 3(1–2),
  95–110 (1956), [DOI 10.1002/nav.3800030109](https://doi.org/10.1002/nav.3800030109);

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

This combination is not a historical model from either paper. The exact single-step front end is a coposit construction. The
fallback is a self-contained copy of [`dickinson_2019`](../dickinson_2019/ALGORITHM.md), which reconstructs Dickinson's Algorithms 1
and 2. The non-strict certificate is Theorem 4.6, and the strict conclusion uses Lemma 5.2 and Corollary 5.3. Dickinson explicitly
reports that the paper's algorithm had not been implemented, so there is no original program whose traversal order can be copied.
The authoritative local implementation is [`solver.cpp`](solver.cpp).

## Exact One-Step Front End

### Centre, row sums, and linear oracle

Let

\[
x_0=\frac1n\mathbf1,
\qquad
T=\mathbf1^TA\mathbf1,
\qquad
r_i=\sum_k a_{ik}.
\]

The implementation calculates every $r_i$ and $T$ in one exact symmetric-triangle scan. The same scan rejects if some diagonal
$a_{ii}\leq0$, because $e_i$ is then a nonzero nonnegative witness. If $T\leq0$, the all-ones vector is an exact witness.

For $f(x)=x^TAx$, the gradient is $2Ax$. Minimizing its linearization over the simplex selects

\[
j\in\arg\min_i r_i,
\]

with the first index breaking ties. The only searched segment is

\[
x(\alpha)=(1-\alpha)x_0+\alpha e_j,
\qquad 0\leq\alpha\leq1.
\]

### Exact descent and curvature

Along this segment,

\[
f(x(\alpha))=f(x_0)+2\alpha g+\alpha^2h,
\]

where

\[
g=\frac{nr_j-T}{n^2},
\qquad
h=\frac{n^2a_{jj}-2nr_j+T}{n^2}.
\]

Define the integer numerators

\[
p=T-nr_j,
\qquad
q=n^2a_{jj}-2nr_j+T.
\]

If $p\leq0$, no simplex vertex gives first-order descent from the centre, so the one-step phase stops. If $q\leq0$, the line is
linear or concave and reaches its minimum at an endpoint. If $p\geq q>0$, the unconstrained minimizer is at or beyond $e_j$. The
centre and every vertex were already proved positive, so both endpoint cases stop without rejection.

The remaining case has $0<p<q$, and the exact constrained minimizer is

\[
\alpha=\frac pq.
\]

This is the clamped line-minimum rule written without division or floating-point comparisons.

### Integer witness without rational storage

The rational minimizer is

\[
x(\alpha)=\frac{q-p}{nq}\mathbf1+\frac pq e_j.
\]

Multiplying by the positive denominator $nq$ gives

\[
z=(q-p)\mathbf1+np\,e_j\geq0.
\]

Homogeneity means that $x(\alpha)^TAx(\alpha)$ and $z^TAz$ have the same sign. The implementation does not allocate $z$; it evaluates

\[
z^TAz=(q-p)^2T+2(q-p)(np)r_j+(np)^2a_{jj}
\]

directly with FLINT integers. A negative value rejects both predicates; zero rejects SCP and leaves CP unresolved. A positive value
proves nothing. Every unresolved CP question invokes the maintained Dickinson fallback. Thus the front end has no `double`,
tolerance, reconstruction grid, iteration, random choice, or acceptance path.

## Terms Used Below

For a vector $u$:

- its **support**, written $\operatorname{supp}(u)$, is the set of indices where $u_i\neq0$;
- its nonnegative-product set is
  \[
  N_A(u)=\{i:(Au)_i\geq0\};
  \]
- $A_I$ is the principal matrix formed by keeping the rows and columns in an index set $I$.

The implementation embeds every vector calculated for $A_I$ into the full space by putting zeros outside $I$.

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
the scan.

## Shared Direct Test Through Order Three

Before consulting certificate coverage for a principal subset $I$ with $|I|\leq3$, coposit applies the shared exact
strict-copositivity criterion to $A_I$. The rule is implemented in `cpp/include/coposit/small_copositivity.hpp` and tests:

- positive diagonal entries at order one;
- positive diagonal entries and, for a negative off-diagonal, a positive determinant at order two;
- all order-two faces followed by the exact determinant-and-adjugate criterion at order three.

If this direct test fails, $A_I$ has a nonzero $z\geq0$ with $z^TA_Iz\leq0$. Padding $z$ with zeros outside $I$ gives the same
nonpositive value for the full matrix, so strict copositivity fails immediately. No Dickinson certificate is needed for that
conclusion.

Passing the direct test does not create a Dickinson vector and therefore cannot cover any later subset. The implementation still
performs the normal coverage lookup and, when $I$ is uncovered, the paper's solve or nullspace branch. Thus the shortcut only adds
early strict rejection; it does not substitute a Hadeler vector for a Dickinson certificate. When the complete input has order at
most three, the direct criterion supplies the final answer without starting certificate construction.

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

## CP/SCP Zero Handling

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

Apart from the exact one-step precheck and the explicit direct test through order three, the maintained implementation uses the
published Dickinson traversal until the requested predicate or classification is decided. A negative witness rejects immediately.
A generated nonnegative zero ends an SCP-only query; CP and combined mode record the boundary, retain its certificate, and continue.
If traversal completes without one, Corollary 5.3 proves strict copositivity.

## Complete Control Flow

1. Receive a parser-guaranteed nonempty, square, exactly symmetric integer matrix.
2. For complete order at most three, return the shared exact direct result.
3. In one exact triangular scan, reject a nonpositive diagonal and calculate all row sums and $T$.
4. Reject when $T\leq0$; otherwise choose the first minimum-row-sum vertex.
5. Calculate $p$ and $q$. Continue only for the interior case $0<p<q$.
6. Evaluate the integer-scaled exact line minimizer and reject when its quadratic value is nonpositive.
7. If the one-step phase did not reject, visit Dickinson subset sizes $1,2,\ldots,n$.
8. Within one size, visit subsets in increasing numeric-bit-mask order.
9. If $|I|\leq3$, apply the shared exact direct criterion and reject immediately when it fails.
10. Skip $I$ when a retained certificate signature covers it.
11. Otherwise form $A_I$ and either solve $A_Iw=\mathbf1$ or take a nullspace vector.
12. Reject immediately if the nonsingular solve produces $w\leq0$.
13. Return `false` immediately when the new vector is a nonnegative zero; otherwise retain its certificate signature.
14. After all uncovered subsets have been processed, return `true`.

The traversal is finite because there are only $2^n-1$ nonempty subsets.

## CP and SCP classification

The exact centre-to-vertex calculation keeps the sign of its verified quadratic value. A negative value rejects CP and SCP. A zero
marks SCP false; strict-only mode stops, while CP and combined mode continue into the Dickinson traversal because another support may
still be negative. Dickinson singular zeros are handled the same way. A completed traversal proves CP and proves SCP exactly when no
zero occurred. Thus `both` performs one exact line step and at most one Dickinson traversal.

## Known Difficult Inputs

The exact front end examines only the segment from the simplex centre to one minimum-row-sum vertex. It cannot see a witness on a
different centre-to-vertex segment, between two non-centre points, or inside a face that this one segment misses. It has no restart
or corrective step. Such an input pays one $O(n^2)$ exact row-sum scan and then behaves exactly like Dickinson.

Strictly copositive inputs can never benefit from witness rejection, although the one-step scan is much cheaper than the copied
multi-start floating proposal engine. Boundary zeros are detected only when this exact segment passes through one; a different
support or line is left entirely to Dickinson.

Dickinson avoids subsets only when an existing vector covers a wide interval

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

It struggles when generated vectors have $N_A(u)$ only slightly larger than their own support. Such a vector covers few additional
subsets, so the algorithm approaches the full $2^n-1$ traversal and performs an exact factorization for many of them.

On boundary matrices whose first nonnegative zero has large support, the model may still enumerate many earlier supports before
generating that decisive zero. Once generated, it stops without processing any remaining support.

Matrices with many singular principal submatrices are also unfavorable. They still require exact factorization and can produce
kernel vectors with weak coverage, although recovering one vector from the retained partial factorization avoids a second
elimination.

## Exact Arithmetic And Fidelity

The one-step phase uses only exact integer additions, multiplications, sign comparisons, and one minimum-row-sum comparison. The
rational step is represented by its positive integer numerator and denominator and is never divided. Common positive scaling of
the input scales $T$, $r_j$, $p$, $q$, and the final quadratic value consistently, so it changes no branch or result.

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

The selected-predicate termination rule is a coposit change, not part of the paper's non-strict Algorithm 1: a generated
nonnegative zero returns `false` immediately only for SCP, while CP and combined mode retain its certificate and continue. Lemma 5.2
and Corollary 5.3 justify CP after a completed traversal. The shared order-at-most-three test is another coposit-added shortcut. It
runs before coverage and classifies a low-order face; after it passes, certificate construction is unchanged. The model adds only the exact one-step rejection front
end: it adds no certificate vectors or coverage claims. It adds no cone subdivision, connected-component reduction, or other
solver's pruning rules.

Timed model-companion builds observe a shared signal flag during the one-step row scan and at principal-subset, certificate,
factorization, and matrix-row boundaries. A timeout is a distinct unresolved outcome. Standalone model and test builds compile those
checkpoints to no-ops, so they add no timer thread, clock read, signal handler, or changed certificate decision.
