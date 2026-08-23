# Pólya Lower Bounds for Downward Pruning

## Purpose and intuition

Copositivity of a symmetric matrix $A$ means

$$
x^T A x\geq 0
\qquad\text{for every }x\geq 0.
$$

After normalizing a nonzero vector by $\mathbf 1^T x=1$, this becomes a minimization problem on the standard simplex. A support
$I\subseteq[n]$ selects one simplex face. If the principal matrix $A_I$ is copositive, every smaller principal matrix $A_J$ with
$J\subseteq I$ is also copositive. A certificate for a large support therefore removes the complete downward interval

$$
[\varnothing,I]=\{J:J\subseteq I\}.
$$

The first nontrivial Pólya certificate is valuable because it gives such a certificate using only entries involving at most three
indices. It needs no factorization, eigenvalue computation, linear program, or floating-point arithmetic.

The search direction is consequently **top-down**: try the full support first, and if it fails this sufficient test, search for large
subsets that pass it. A failed test does not prove non-copositivity; it only identifies combinations of indices that cannot coexist in
a support certified by this particular Pólya level.

## 1. Setup

Let

$$
A=(a_{ij})\in\mathbb R^{n\times n}
$$

be symmetric, and let

$$
[n]=\{1,\ldots,n\}.
$$

For a nonempty support $I\subseteq[n]$, write $A_I$ for the corresponding principal matrix and

$$
\Delta_I=
\left\{
x\in\mathbb R^n:
x\geq0,\quad
\mathbf1^Tx=1,\quad
x_j=0\text{ for }j\notin I
\right\}
$$

for its simplex face.

The minimum on this face is

$$
\mu(I)=\min_{x\in\Delta_I}x^TAx.
$$

Thus $A_I$ is copositive exactly when $\mu(I)\geq0$, and it is strictly copositive exactly when $\mu(I)>0$.

## 2. The level-one Pólya bound

For a nonempty support $I$, define

$$
\lambda_1(I)=
\min
\left\{
\begin{array}{ll}
a_{ii},
&i\in I,\\[2mm]
\dfrac{a_{ii}+2a_{ij}}{3},
&i,j\in I,\ i\neq j,\\[3mm]
\dfrac{a_{ij}+a_{ik}+a_{jk}}{3},
&i,j,k\in I\text{ distinct}.
\end{array}
\right.
$$

The pair minimum uses ordered pairs. For the same unordered pair $\{i,j\}$, both

$$
\frac{a_{ii}+2a_{ij}}{3}
\qquad\text{and}\qquad
\frac{a_{jj}+2a_{ij}}{3}
$$

must be included. The triple term is symmetric in $i,j,k$ and therefore needs only one value per unordered triple.

### Theorem 2.1: exact lower bound

For every nonnegative vector $x$ supported in $I$,

$$
x^TAx
\geq
\lambda_1(I)(\mathbf1^Tx)^2.
$$

Consequently,

$$
\mu(I)\geq\lambda_1(I).
$$

### Intuition

Multiplying the quadratic form by the sum of the variables turns it into a cubic polynomial. The three types of cubic monomials use
one, two, or three different indices. The definition of $\lambda_1(I)$ is exactly the largest uniform lower bound that makes every
one of those coefficients nonnegative. A polynomial with nonnegative coefficients cannot be negative on the nonnegative orthant.

### Proof

Let

$$
s=\mathbf1^Tx.
$$

For a scalar $\lambda$, consider the homogeneous cubic polynomial

$$
p_\lambda(x)=s\left(x^TAx-\lambda s^2\right).
$$

Its coefficients have only three possible forms.

For $x_i^3$, the coefficient is

$$
a_{ii}-\lambda.
$$

For $x_i^2x_j$ with $i\neq j$, the coefficient is

$$
a_{ii}+2a_{ij}-3\lambda.
$$

For $x_ix_jx_k$ with distinct $i,j,k$, the coefficient is

$$
2\left(a_{ij}+a_{ik}+a_{jk}-3\lambda\right).
$$

By the definition of $\lambda_1(I)$, every coefficient is nonnegative when $\lambda=\lambda_1(I)$. Therefore

$$
p_{\lambda_1(I)}(x)\geq0
\qquad\text{for every }x\geq0\text{ supported in }I.
$$

If $x\neq0$, then $s>0$, so division by $s$ gives

$$
x^TAx-\lambda_1(I)s^2\geq0.
$$

For $x\in\Delta_I$, $s=1$, and hence $x^TAx\geq\lambda_1(I)$. This proves the claim.

This is the $r=1$ coefficient certificate in the Pólya/de Klerk--Pasechnik inner-approximation hierarchy for the copositive cone.

## 3. Copositivity and strict-copositivity implications

The sign of $\lambda_1(I)$ has three different meanings.

### 3.1 Nonnegative bound

If

$$
\lambda_1(I)\geq0,
$$

then

$$
x^TAx\geq0
\qquad\text{for every }x\in\Delta_I.
$$

Therefore $A_I$ is copositive, and every subface is safe:

$$
J\subseteq I
\quad\Longrightarrow\quad
A_J\text{ is copositive}.
$$

The whole downward interval $[\varnothing,I]$ may be removed from the ordinary-copositivity search.

### 3.2 Positive bound

If

$$
\lambda_1(I)>0,
$$

then

$$
x^TAx>0
\qquad\text{for every nonzero }x\geq0\text{ supported in }I.
$$

Thus $A_I$ and every principal submatrix $A_J$ with $J\subseteq I$ are strictly copositive.

### 3.3 Negative bound

If

$$
\lambda_1(I)<0,
$$

the test is inconclusive. It does **not** follow that $A_I$ is non-copositive, and the support must not be removed by an upward
copositivity clause.

The distinction is essential: success is an exact proof, while failure says only that this particular sufficient certificate is too
weak.

There is nevertheless one valid upward implication, but it concerns only the **Pólya-1 test**, not copositivity. If one pair or
triple inside $I$ violates a coefficient inequality, that same violating coefficient remains present in every larger support
$K\supseteq I$. Every such $K$ therefore also fails the Pólya-1 test. The implementation may skip trying this particular certificate
on those supersets, but it must leave them available for curvature, Dickinson, higher Pólya levels, or any other exact copositivity
test.

The two monotonicity statements must not be confused:

> **PÓLYA-1 SUCCESS PERSISTS DOWNWARD AND PROVES COPOSITIVITY THERE.**
>
> **PÓLYA-1 FAILURE PERSISTS UPWARD, BUT PROVES ONLY THAT PÓLYA-1 WILL FAIL THERE.**

### 3.4 Complete implication table

| Result on support $I$ | Exact conclusion | Allowed search action |
|---|---|---|
| $\lambda_1(I)>0$ | $A_I$ and every $A_J$, $J\subseteq I$, are strictly copositive | Prune $[\varnothing,I]$ for both CP and SCP |
| $\lambda_1(I)=0$ | $A_I$ and every $A_J$, $J\subseteq I$, are copositive | Prune $[\varnothing,I]$ for CP; strictness remains unresolved |
| $\lambda_1(I)<0$ | Only the lower bound $x^TAx\geq\lambda_1(I)$ is known; the true minimum may still be positive | Do not prune the copositivity search |
| A pair or triple is a Pólya-1 obstruction | Every support containing that pattern fails Pólya-1 | Exclude those supports only from the Pólya-1 certificate search |

A negative lower bound is not a negative witness. A lower bound says that the true value cannot lie below it; it does not say that
the bound is attained.

## 4. Entrywise form of the certificate

Testing whether $\lambda_1(I)\geq0$ requires no divisions. It is equivalent to all of the following conditions.

For every $i\in I$,

$$
a_{ii}\geq0.
$$

For every ordered pair $i,j\in I$ with $i\neq j$,

$$
a_{ii}+2a_{ij}\geq0.
$$

For every distinct triple $i,j,k\in I$,

$$
a_{ij}+a_{ik}+a_{jk}\geq0.
$$

The matrix parser and preprocessing already make the diagonal conditions cheap to settle. The new work is therefore the pair and
triple scan.

For strict copositivity, replace every non-strict comparison by a strict one.

## 5. The obstruction hypergraph in simple terms

The entrywise conditions can be stored as a list of small combinations that block this certificate. The formal name for such a list
is a hypergraph, but the underlying idea is simple:

- the available numbers $1,\ldots,n$ are the matrix indices;
- each stored combination contains either two or three indices;
- a candidate support passes Pólya-1 only when it does not contain all indices of any stored combination.

The word **obstruction** is used deliberately. It means “this combination obstructs the Pólya-1 proof.” It does not mean that the
pair, triple, or any matrix containing it is non-copositive.

Call an unordered pair $\{i,j\}$ a **Pólya-1 obstruction** when at least one of

$$
a_{ii}+2a_{ij}<0,
\qquad
a_{jj}+2a_{ij}<0
$$

holds. Call an unordered triple $\{i,j,k\}$ a **Pólya-1 obstruction** when

$$
a_{ij}+a_{ik}+a_{jk}<0.
$$

Let $\mathcal H_1$ be the list of all these obstructing pairs and triples. In hypergraph terminology, the matrix indices are vertices
and the obstructing combinations are hyperedges. Assuming the diagonal entries are nonnegative,

$$
\lambda_1(I)\geq0
\quad\Longleftrightarrow\quad
I\text{ contains no obstruction from }\mathcal H_1\text{ as a subset}.
$$

Thus the supports certified at Pólya level one are exactly the independent sets of a hypergraph of rank at most three.

### 5.1 How to picture it

Suppose the obstruction list is

$$
\{1,2,3\},
\qquad
\{3,4\},
\qquad
\{4,5,6\}.
$$

A support may contain some indices from each combination. It fails Pólya-1 only when it contains an entire listed combination.
For example,

$$
\{1,2,4,5\}
$$

contains part of every obstruction but contains none completely, so these three obstructions do not prevent it from passing. By
contrast,

$$
\{1,2,3,5\}
$$

contains the full obstruction $\{1,2,3\}$ and therefore fails Pólya-1.

To construct a passing support from $[n]$, cross out at least one index from every obstruction. The crossed-out indices form a
**hitting set** because they hit every obstructing combination. Everything not crossed out is a candidate Pólya-safe support.

### 5.2 What an obstruction lets us do

An obstruction gives two useful pieces of algorithmic information.

1. **Do not retest Pólya-1 on a support containing it.** The same negative coefficient is still present, so the test is guaranteed
   to fail.
2. **Remove at least one of its indices when constructing a Pólya-safe support.** SAT or a greedy hitting-set heuristic can make
   that choice.

It gives no copositivity-pruning clause. In particular, the clause

$$
\lnot s_i\lor\lnot s_j\lor\lnot s_k
$$

for an obstructing triple may be used inside a selector that searches specifically for Pólya-1-certified supports. It must not be
inserted into the main proof SAT as though it certified every support containing $\{i,j,k\}$.

### 5.3 What an obstruction does not imply

A Pólya-1 obstruction does not imply any of the following:

- the pair or triple itself is non-copositive;
- a larger support containing it is non-copositive;
- a negative vector exists;
- reduced curvature is bad;
- Dickinson will fail;
- a higher Pólya level will fail.

Two small exact examples show why upward copositivity pruning would be wrong.

#### Obstructing pair inside a strictly positive matrix

Consider

$$
A=
\begin{pmatrix}
5&-3\\
-3&5
\end{pmatrix}.
$$

Its eigenvalues are $2$ and $8$, so it is positive definite and therefore strictly copositive. Nevertheless,

$$
a_{11}+2a_{12}=5-6=-1<0.
$$

Thus $\{1,2\}$ obstructs Pólya-1 even though the complete matrix is strictly copositive.

#### Obstructing triple inside a strictly positive matrix

Consider

$$
A=
\begin{pmatrix}
5&-2&-2\\
-2&5&-2\\
-2&-2&5
\end{pmatrix}.
$$

Its eigenvalues are $1,7,7$, so it is also positive definite. Every ordered-pair coefficient passes because

$$
5+2(-2)=1>0.
$$

However, the triple coefficient fails:

$$
a_{12}+a_{13}+a_{23}=-6<0.
$$

The entire matrix is strictly copositive despite the obstructing triple. The negative coefficient is outweighed by the other positive
terms of the polynomial.

### 5.4 Intuition

A Pólya-1 obstruction is not a negative witness. It is an obstruction only to this proof system. To make the proof work on a large
support, at least one index must be removed from every obstruction. The removed coordinates therefore form a hitting set of
$\mathcal H_1$.

## 6. The top-down search

The natural workflow begins at the full support.

### Step 1: test the full matrix

Evaluate the pair and triple conditions on $I=[n]$.

If all conditions hold, then

$$
\lambda_1([n])\geq0,
$$

so the full matrix is copositive and the ordinary-copositivity problem is finished. If every condition is strict, strict
copositivity is also finished.

### Step 2: construct the obstruction hypergraph

If the full support fails, record every obstructing pair and triple. This work is done once for the complete matrix.

### Step 3: find a large safe support

Find a small hitting set $R\subseteq[n]$ that intersects every edge of $\mathcal H_1$, and set

$$
I=[n]\setminus R.
$$

Then $I$ contains no complete obstruction and therefore satisfies $\lambda_1(I)\geq0$.

The hitting set need not be minimum. A greedy construction or an existing SAT model may nominate a large independent support. Every
accepted support is verified simply by checking that none of the stored obstructions is a subset of it.

### Step 4: prune downward

Insert the downward interval

$$
[\varnothing,I].
$$

### Step 5: find additional maximal safe supports

One safe support need not cover all Pólya-certifiable supports. Request another large support that avoids the obstructions but is not
already contained in a previously certified support. Repeat only while the marginal coverage is worth the SAT and storage cost.

## 7. Example

Let $n=6$, and suppose the only Pólya-1 obstructions are the triples

$$
\{1,2,3\}
\qquad\text{and}\qquad
\{3,4,5\}.
$$

Removing coordinate $3$ intersects both triples. The remaining support is

$$
I=\{1,2,4,5,6\}.
$$

If none of the obstructing pairs or triples is a subset of it, then $\lambda_1(I)\geq0$. We may prune every subset of $I$.

Other Pólya-safe supports may contain coordinate $3$. For example, a different hitting set can remove one coordinate from
$\{1,2\}$ and one from $\{4,5\}$. A second maximal safe support can therefore cover a different part of the Boolean lattice.

## 8. Cost and exact arithmetic

The one-time scan examines

$$
\binom n2
$$

pairs and

$$
\binom n3
$$

triples. For reference,

$$
\binom{50}{3}=19{,}600,
\qquad
\binom{100}{3}=161{,}700.
$$

Every comparison uses only exact additions, multiplication by the small integer $2$, and a sign test. The lower bound itself may be
stored as a rational number with denominator $3$, but testing its sign requires no division.

Floating point is unnecessary. If it is ever used as an optional ordering heuristic, it must not create a certificate or pruning
decision; the integer inequalities above are the final verification.

## 9. Relationship to the existing search

This certificate is independent of Dickinson and reduced-Hessian curvature.

- A successful Pólya test proves that a large face is safe and prunes downward.
- A curvature-bad support cannot be the minimal carrier of a negative global minimum and prunes upward.
- Dickinson produces a generally bounded interval by extending one all-ones or optimized-right-hand-side vector.

The three methods can therefore cover different regions. A practical combined search can place minimal curvature-bad supports below
the open lattice, maximal Pólya-safe supports above it, and ask SAT only for supports lying between those two boundaries. Ordinary
Dickinson remains the completeness fallback.

## 10. Higher Pólya levels

For an integer $r\geq0$, the general coefficient certificate studies

$$
(\mathbf1^Tx)^r x^TAx.
$$

If all coefficients are nonnegative, then $A$ is copositive. At level $r$, every coefficient involves at most $r+2$ distinct
indices. Therefore:

- level $0$ is essentially the entrywise-nonnegative test;
- level $1$ uses supports of size at most three;
- level $2$ uses supports of size at most four.

Pólya's theorem implies that every strictly copositive matrix is accepted at some finite level. It gives no practically small
uniform bound on that level, especially near the boundary of the copositive cone.

The first experiment should therefore stop at level one. Level two is justified only if level-one diagnostics show that the
coefficient approach produces large safe supports but misses them narrowly.

## 11. Proposed first experiment

No new maintained model is needed to evaluate the idea.

1. Compute all Pólya-1 obstructing pairs and triples for the hard BPQY and Burer matrices.
2. Test whether the full support passes.
3. Greedily construct several inclusion-maximal supports containing no complete obstruction.
4. Record their sizes, overlap, and exact layer-by-layer marginal coverage.
5. Estimate how many exact factorizations the corresponding downward intervals would avoid in SAT B3.
6. Stop if the maximal safe supports are small or almost completely overlapping.

The experiment is valuable precisely because it is cheap. It should establish the hit rate and coverage before any new solver copy or
control-flow integration is created.

## References

- Etienne de Klerk and Dmitrii V. Pasechnik, “Approximation of the Stability Number of a Graph via Copositive Programming,” *SIAM
  Journal on Optimization* 12 (2002), 875–892. <https://doi.org/10.1137/S1052623401383248>
- Mitsuhiro Nishijima and Kazuhide Nakata, “Approximation hierarchies for copositive cone over symmetric cone and their comparison,”
  *Journal of Global Optimization* 88 (2024), 1081–1107. <https://doi.org/10.1007/s10898-023-01319-3>
- George Pólya, “Über positive Darstellung von Polynomen,” *Vierteljahrsschrift der Naturforschenden Gesellschaft in Zürich* 73
  (1928), 141–145.
