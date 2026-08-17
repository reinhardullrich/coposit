# Wide-Certificate SAT Dickinson

Classification: coposit-created exact copositivity and strict-copositivity experiment. It combines SAT Dickinson's incremental
CaDiCaL support generator with the selective certificate-retention rule first tested in Wide-Certificate CBDD Dickinson.

Public mode boundary: the model supports individually selected `copositive` and `strictly_copositive` modes and combined
classification of both predicates in one traversal. Analysis and reference-run interfaces default to combined classification when
the mode is omitted. The model requires an integer percentage parameter $p$ from 0 through 100; there is no default.

## Idea In Plain Language

Dickinson's exact calculation on a support $I$ normally produces an interval of supports that can all be skipped. SAT Dickinson
stores every such interval as one permanent SAT clause. That is mathematically strong, but a narrow interval creates a long clause
that may contribute little propagation while permanently enlarging the SAT instance.

This model keeps only sufficiently wide Dickinson intervals. Width is measured against the number of indices still available above
the current cardinality. When a certificate is too narrow, the model does not discard the current support silently: it adds one
exact-support clause that blocks only that already processed support. The next SAT call must therefore return a different support,
and the finite traversal remains complete.

The percentage is a runtime parameter so 75%, 90%, 95%, or another threshold are experiments of one implementation rather than
copied model directories.

## Name, Sources, And Classification

The identifier is `wide_certificate_sat_dickinson`:

- “Dickinson” identifies Peter J. C. Dickinson's exact certificate mathematics;
- “SAT” identifies the incremental Boolean support generator; and
- “wide certificate” identifies the selective interval-retention rule.

The model is an independent copy of `models/hadeler-based/sat_dickinson`. Its exact matrix calculations, CaDiCaL cardinality network,
cardinality-first traversal, witness decisions, and combined classification are unchanged. The only mathematical search-policy
change is whether a generated Dickinson interval or only the current exact support is added to SAT.

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2.

The exact-cardinality encoding uses Kenneth E. Batcher's bitonic sorting network from “Sorting Networks and Their Applications,”
*AFIPS Spring Joint Computer Conference* 32 (1968), 307–314. Incremental SAT solving is provided by CaDiCaL 2.2.1, pinned to tag
`rel-2.2.1` from <https://github.com/arminbiere/cadical>.

The threshold rule comes from coposit's `wide_certificate_cbdd_dickinson` experiment. It is not part of Dickinson's paper or
CaDiCaL.

## Exact Certificate Geometry

Let the full index set be $N=\{1,\ldots,n\}$. For the exact vector $u$ produced from the current principal support $I$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's certificate covers the Boolean-lattice interval

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

Its number of free indices is

$$
d=|U|-|L|.
$$

The current traversal cardinality is $k=|I|$. The maximum number of new indices above a full-support current vector would be $n-k$.
For the configured percentage $p$, this model retains the full Dickinson interval exactly when

$$
d>\left\lfloor\frac{p(n-k)}{100}\right\rfloor.
$$

The strict inequality is intentional and matches Wide-Certificate CBDD Dickinson. At $p=100$, a certificate with $L=I$ cannot be
retained because $d\leq n-k$; a certificate with $L\subsetneq I$ can still qualify. At $p=0$, every certificate with at least one
free index is retained, while a zero-width certificate still blocks only its processed support.

If $L$ is smaller than $I$ because the exact vector contains zero entries, $d$ can exceed $n-k$. The implementation uses the actual
$|U|-|L|$ and does not replace it with $|U|-k$.

The threshold is calculated with quotient and remainder arithmetic rather than forming $p(n-k)$ directly, avoiding unnecessary
`size_t` overflow at very large dimensions.

## SAT Representation

Matrix index $i$ is represented by a Boolean variable $x_i$, where true means that $i$ belongs to the candidate support. Membership
in a retained Dickinson interval requires all $x_i$ for $i\in L$ to be true and all $x_i$ for $i\notin U$ to be false. Write its
single blocking clause as

$$
C(L,U)=
\bigvee_{i\in L}\neg x_i
\;\lor\;
\bigvee_{i\notin U}x_i.
$$

The cardinality-network output $y_t$ is true exactly when at least $t+1$ indices are selected. When $|U|<n$, the stored clause is

$$
C(L,U)\lor y_{|U|}.
$$

It blocks the interval through cardinality $|U|$ and is automatically satisfied afterward. Its length is $n-d+1$. A ceiling
interval with $|U|=n$ never expires and keeps the original length $n-d$. Wide certificates therefore still produce shorter clauses.

For a rejected narrow certificate, let $I$ be the exact current support. The model adds

$$
\bigvee_{i\in I}\neg x_i
\;\lor\;
\bigvee_{i\notin I}x_i,
$$

which excludes exactly the assignment $I$ and no other support. Its stored form also includes $y_{|I|}$, so it blocks the current
cardinality but becomes inert immediately afterward. This clause is longer, but it is the minimum state change needed to prevent SAT
from returning the same processed support forever.

There is one CaDiCaL instance for the complete matrix call. Retained certificate clauses, exact-support clauses, and compatible
learned clauses persist across cardinalities, but the existing cardinality outputs make bounded clauses inactive after their last
relevant layer.

## Exact Cardinality Layers

At construction time the model builds one Batcher bitonic sorting network over the $n$ support variables. A non-power-of-two
dimension is padded to the next power of two with one shared variable constrained to false. Each comparator creates outputs
$h=a\lor b$ and $\ell=a\land b$ using six clauses.

After sorting in descending truth order, output $y_j$ is true exactly when at least $j+1$ input variables are true. Cardinality $k$
is selected temporarily by

$$
y_{k-1}=true,
\qquad
y_k=false\quad(k<n).
$$

These assumptions enforce exactly $k$ selected indices without rebuilding the network or adding a permanent cardinality clause.
The same outputs deactivate bounded interval clauses once $k>|U|$; no interval scan or separate activation-variable family is
needed.

## Complete Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and a configured percentage $p$.
2. Build the SAT cardinality network once.
3. For $k=1,\ldots,n$, activate the exact-$k$ assumptions.
4. Ask CaDiCaL for one support not blocked by the current permanent clauses.
5. If the cardinality layer is unsatisfiable, advance to $k+1$.
6. Otherwise copy the principal matrix $A_I$ and perform the exact Dickinson calculation below.
7. Stop with a negative predicate result if the exact vector is a decisive witness.
8. Compute $L$, $U$, and $d$ exactly.
9. If $d>\lfloor p(n-k)/100\rfloor$, add the cardinality-aware full interval clause. Otherwise add the cardinality-aware
   exact-support clause for $I$.
10. Ask SAT for the next support of the same cardinality.
11. When every cardinality layer is exhausted, return the positive result.

The order of supports inside one cardinality is chosen by CaDiCaL. The order does not affect correctness because a support is omitted
only when a valid retained interval covers it or when that exact support was already processed.

## Exact Dickinson Calculation

For a SAT support $I$, copy the principal matrix $A_I$ and factor it with the shared exact fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve

$$
A_Iu=\mathbf1
$$

as integer numerators with one positive common denominator. If $u\leq0$, the embedded vector $-u$ is a nonnegative negative
witness, so both copositivity and strict copositivity fail.

If $A_I$ is singular, construct one exact nullspace vector and orient it to have a positive entry. A nonnegative oriented vector is
a nonnegative zero. Strict copositivity then fails, while non-strict copositivity remains possible and the traversal continues.

Every surviving vector is embedded in the full dimension. The product $Au$ and all signs defining $U$ are computed with exact
arbitrary-precision integers. The wide-certificate threshold changes only what Boolean clause is retained; it never approximates a
matrix calculation or accepts an unverified certificate.

In combined mode, the traversal starts with both predicates provisionally true. A nonnegative zero clears only strict
copositivity. A negative witness clears both and stops. The only returned classifications are therefore `(true,true)`,
`(true,false)`, and `(false,false)`.

## Diagnostics, Timeouts, And Termination

Diagnostics report the generated Dickinson geometry as sparse $(k,d,|U|,count)$ bins even when a narrow certificate is represented
only by an exact-support clause. This deliberately measures what the exact calculation found, separately from what the threshold
retained.

CaDiCaL is connected to the cooperative timeout flag through its terminator interface. Cardinality-network construction, exact
factorization, and full matrix products retain the normal timeout checkpoints. A timeout is unresolved and never becomes a false
classification.

Every successful iteration adds a clause that excludes at least the current support. The support family is finite, so the model
terminates given sufficient time and memory, even at $p=100$.

Matrix arithmetic uses FLINT arbitrary-precision integers. Supports use the shared multiword packed representation and have no
63-bit dimension limit. SAT variables remain limited by CaDiCaL's signed integer literal interface; oversized constructions fail
explicitly instead of truncating indices.

## Known Difficult Inputs

A high threshold intentionally rejects most pruning intervals. The resulting exact-support clauses are length $n$, accumulate one
per processed support, and provide little propagation to future supports. Runtime and memory can therefore approach explicit
cardinality-ordered enumeration.

A low threshold approaches ordinary SAT Dickinson and can retain many long, weak interval clauses. Those clauses may enlarge the
persistent database and slow later satisfiable calls even though each is mathematically valid. The useful threshold is therefore a
performance question, not a correctness condition.

Highly symmetric matrices remain difficult because many interchangeable supports survive. A threshold may remove symmetry-related
interval clauses that would otherwise prune many assignments, while retained clauses and CaDiCaL's learned clauses can still grow
irregularly. The fixed sorting network also imposes its one-time $O(p\log^2p)$ comparator cost before any matrix witness is found.
