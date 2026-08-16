# Support-Pruned Dickinson

Classification: coposit-created exact CP/SCP hybrid based on Dickinson's certificate algorithm and FracESSA's
forbidden-support generator.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson's algorithm examines nonempty coordinate supports. On every support that has not already been covered, it solves one
principal linear system or obtains one principal nullspace vector. The resulting vector may certify not only the current support
but an interval of later supports.

The maintained `dickinson_2019` model still *generates* every support in cardinality order and then asks whether a certificate covers
it. This variant notices the strongest possible Dickinson interval earlier. If a certificate covers every superset of its own
support, the recursive support generator cuts off that entire upward branch, so those supports are never constructed.

Nothing changes in Dickinson's linear algebra or certificate theorem. Certificates that cover only a bounded interval retain the
non-strict Dickinson lookup. The new pruning is therefore a change in how already-proved redundant supports are generated, not a new
copositivity test.

## Name And Sources

The descriptive identifier is `support_pruned_dickinson`. It is not a historical baseline and is not attributed to a new paper.
It combines two existing local algorithms:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025). Dickinson's Algorithms 1 and 2, Theorem 4.6,
  Lemma 5.2, and Corollary 5.3 supply the certificate construction and the strict conclusion.
- FracESSA revision `95e0ec019cf11a60c6423508e8768536a0b88860`, especially
  `cpp/include/fracessa/supports.hpp` (`NonCircularSupportGenerator`) and the support-forbidding orchestration in
  `cpp/src/fracessa.cpp`. coposit's local reconstruction is `models/hadeler-based/fracessa/solver.cpp`.

The implementation began as an independent copy of `models/hadeler-based/dickinson_2019/solver.cpp`. Only its fixed-cardinality support loop was
replaced with a private copy of the recursive FracESSA generator, and only globally covering Dickinson signatures are submitted as
forbidden supports. The original `dickinson_2019` baseline remains unchanged.

## Mathematical Problem

For a nonempty symmetric integer matrix $A\in\mathbb Z^{n\times n}$, the model decides whether

$$
x^TAx>0\qquad\text{for every nonzero }x\geq0.
$$

For an index set $I\subseteq[n]=\{0,\ldots,n-1\}$, $A_I$ denotes the principal matrix using rows and columns in $I$. A vector
calculated for $A_I$ is embedded in $\mathbb R^n$ by setting all coordinates outside $I$ to zero.

For any embedded vector $u$, define

$$
S(u)=\operatorname{supp}(u)=\{i:u_i\neq0\},
\qquad
N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson's coverage condition says that $u$ accounts for a support $I$ exactly when

$$
S(u)\subseteq I\subseteq N_A(u).
$$

The pair $(S(u),N_A(u))$ is called the certificate's **coverage signature** below.

## Non-strict Dickinson Coverage

Every accepted certificate has at least one positive component. Its signature represents the closed support interval

$$
[S(u),N_A(u)]=\{I:S(u)\subseteq I\subseteq N_A(u)\}.
$$

When a generated support $I$ belongs to any retained interval, the algorithm does not factor $A_I$. This is the coverage test from
Dickinson's Theorem 4.6.

The implementation stores the two sets as packed bit supports and discards the vector. Signatures are bucketed by the lowest index
of $S(u)$. A signature covering $I$ must have that lowest index in $I$, so only buckets named by indices of $I$ need to be searched.
Within a bucket the newest signature is tested first. These are lossless lookup choices.

## The Globally Covering Special Case

If

$$
N_A(u)=[n],
$$

then the upper containment is automatic and the Dickinson interval becomes

$$
[S(u),[n]]=\{I:S(u)\subseteq I\}.
$$

Thus **every** later support containing $S(u)$ is already covered. Coverage is upward closed: if $S(u)\subseteq I$, adding more
indices to $I$ cannot make the statement false.

This is precisely the condition used for generator pruning. During the full product calculation, the implementation checks every
row of $Au$ exactly. It submits $S(u)$ to the recursive generator only if no row is negative. Zeros count as nonnegative, exactly as
they do in Dickinson's definition of $N_A(u)$.

If even one component of $Au$ is negative, then $N_A(u)\neq[n]$. The interval is not upward closed in the full subset lattice:
adding that negative-product index can leave the interval. Such a signature is stored for non-strict coverage but is never made a
generator rule.

## Why This Is The FracESSA Support Prune

FracESSA accepts a simplex KKT point $x$ on support $S$ when

$$
A_Sx=\mu\mathbf1,
\qquad x>0,
\qquad (Ax)_k\geq\mu\quad(k\notin S)
$$

for a minimization problem. When $\mu>0$, scaling by $w=x/\mu$ gives

$$
A_Sw=\mathbf1,
\qquad w>0,
\qquad (Aw)_k\geq1\quad(k\notin S).
$$

Consequently $N_A(w)=[n]$, and Dickinson covers every superset of $S$. FracESSA calls $S$ forbidden because a later KKT support
cannot contain it; Dickinson calls the same later supports covered. The generator operation is identical even though the two
algorithms reach the rule through different descriptions.

This model does not run FracESSA's reduced KKT solve. Dickinson's already-computed vector and full product provide the needed fact,
so adding a second solver would only duplicate exact work.

## Recursive Support Generator

The generator visits supports by increasing cardinality $k=1,2,\ldots,n$. Within one cardinality it produces increasing numeric
bit masks, matching the deterministic order of `dickinson_2019`.

Its state is:

- `partial_support`: the high-index bits chosen on the current recursion path;
- `target_cardinality`: the number of bits required by the current outer pass;
- `pending_forbidden`: globally covered supports found during the current cardinality;
- `forbidden_by_lowest[i]`: active forbidden supports whose lowest set bit is $i$.

For one target cardinality the recursion considers bit positions from high to low. At each bit it first explores the branch that
does not set the bit, then the branch that sets it. This exclusion-before-inclusion order produces increasing numeric masks.

Suppose the recursion has just set bit $i$. Because bits are chosen from high to low, $i$ is now the lowest selected bit. Only a
forbidden set with lowest bit $i$ can become completely contained for the first time at this step. The generator therefore tests
only `forbidden_by_lowest[i]`. If any such set is a subset of `partial_support`, the entire recursion branch already contains a
globally covering Dickinson support and is cut off immediately.

New rules remain pending until the next cardinality. This has two purposes:

1. it avoids modifying active pruning buckets while their recursion is running;
2. it preserves the current cardinality's non-strict Dickinson order and behavior.

A signature may have fewer nonzero coordinates than the support on which it was found. In that case it can cover later supports of
the same cardinality. Those supports may still be generated in the current pass, but the normal Dickinson coverage lookup skips
their factorization. Starting with the next cardinality, the generator removes their branches before emission.

If one complete cardinality emits no support, every support of that size contains an active forbidden set. Every larger support
contains some support of that size and therefore also contains an active forbidden set. The generator can terminate safely without
attempting larger cardinalities.

## Shared Direct Test Through Order Three

Before coverage or factorization, every emitted support $I$ of size at most three is checked by
`cpp/include/coposit/small_copositivity.hpp`:

- order one requires a positive diagonal entry;
- order two requires positive diagonals and, when the off-diagonal is negative, a positive determinant;
- order three checks all order-two faces and then the exact determinant-and-adjugate criterion.

A failure supplies a nonzero nonnegative vector on that principal face with quadratic value at most zero, so the full matrix is not
strictly copositive. A passing check does not create a certificate. The model still performs non-strict Dickinson coverage and, when
uncovered, the normal solve or nullspace branch. For input order at most three, the shared criterion returns the final answer without
starting the support traversal.

## Processing An Uncovered Support

Let $C=A_I$.

### Nonsingular branch

Solve exactly

$$
Cw=\mathbf1.
$$

The fraction-free LDLT solver returns integer numerators and a positive common denominator. Positive rescaling does not change any
sign, support, coverage set, or witness conclusion, so the denominator is discarded after its sign is checked.

If $w\leq0$, let $z=-w\geq0$. Then

$$
z^TCz=w^TCw=w^T\mathbf1<0,
$$

so the embedded $z$ is a strict negative witness and the model returns `false`. Otherwise $w$ has a positive component and becomes
a Dickinson certificate.

### Singular branch

Recover one nonzero exact integer vector satisfying

$$
Cw=0.
$$

The fraction-free LDLT factorization exposes the exact rank. The solver fixes one free transformed coordinate, solves backward, and
reverses the factorization's coordinate operations. It constructs one null vector, not a basis, for any positive nullity. The sign
is reversed when necessary so that the vector has a positive component.

If the oriented vector is nonnegative, it is a nonnegative zero and strict copositivity fails immediately. Otherwise it becomes a
Dickinson certificate.

### Retained information

For every accepted vector the model:

1. records the coordinates where $w$ is nonzero as $S(w)$;
2. calculates every component of the full product $Aw$ exactly;
3. records its nonnegative components as $N_A(w)$;
4. if $N_A(w)=[n]$, queues $S(w)$ as a generator pruning rule;
5. stores $(S(w),N_A(w))$ for later Dickinson interval lookup.

The full vector and product values are then discarded.

## CP/SCP Decision

Dickinson's published Algorithms 1 and 2 decide non-strict copositivity. coposit tracks strict copositivity in the same traversal.

A negative witness from the nonsingular solve proves failure directly. A nonnegative singular null vector is a zero and also proves
strict failure directly. Conversely, Dickinson's Lemma 5.2 and Corollary 5.3 show that a completed certificate contains every
minimal nonnegative zero up to positive scaling. Therefore a traversal that completes without encountering a negative witness or a
nonnegative zero proves that no such zero exists. In CP or combined mode, a nonnegative zero is retained as a certificate and the CP
proof continues; only an SCP-only query stops at that boundary.

Generator pruning does not weaken this conclusion: it removes only supports already covered by a retained Dickinson vector, exactly
the same supports that the baseline would generate and then skip.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric matrix.
2. For order at most three, return the shared exact direct result.
3. Start the recursive generator at cardinality one.
4. Activate globally covered supports discovered in the previous cardinality.
5. Recursively construct the next support in numeric-mask order, cutting off a branch as soon as it contains an active forbidden
   support.
6. Apply the shared direct classifier to emitted supports of size at most three; update CP and SCP independently.
7. Search the retained Dickinson signatures; skip the support if any $S(u)\subseteq I\subseteq N_A(u)$.
8. Otherwise factor $A_I$ and perform the nonsingular or singular Dickinson branch.
9. Return failure for both predicates on a nonpositive nonsingular solution. On a nonnegative singular null vector, clear only SCP;
   stop only for an SCP-only query.
10. Calculate and retain the certificate signature. Queue its nonzero support for recursive pruning exactly when
    $N_A(u)=[n]$.
11. Continue until every surviving support has been processed, or stop early when a cardinality emits none.
12. Return `true` after a zero-free completed traversal.

## Exact Arithmetic And Representation

All classifications use FLINT arbitrary-precision integers. The lower triangle of each principal matrix is copied into reusable
storage and factorized once. One reusable column stores either the solve numerators or the null vector, and one reusable length-$n$
array stores $Aw$ during signature construction.

Current supports, certificate supports, product-sign sets, and forbidden sets use the shared dynamic packed representation with
$\lceil n/64\rceil$ 64-bit words. The generator has no fixed-width dimension limit. It retains an ordered index vector beside the
packed current support only because principal-matrix access needs explicit indices.

Timed Python modules check the cooperative timeout flag at support-recursion, coverage, factorization, and full-product boundaries.
A timeout is unresolved and is never returned as `false`.

## Source Behavior, coposit Changes, And Non-Changes

From Dickinson 2019:

- the coverage interval $S(u)\subseteq I\subseteq N_A(u)$;
- the solve $A_Iw=\mathbf1$ for nonsingular principal matrices;
- the admissible oriented null vector for singular principal matrices;
- the negative-witness conclusion and completed-certificate theorem.

From FracESSA:

- the cardinality-first recursive generator;
- increasing numeric-mask order;
- pending activation between cardinalities;
- pruning grouped by the forbidden support's lowest index;
- termination after a cardinality emits no surviving support.

coposit-specific classification adaptations:

- exact direct CP/SCP classification through order three;
- immediate termination on a generated nonnegative zero only for an SCP-only query;
- dynamic packed supports and reusable exact storage.

The model does **not** add FracESSA's KKT equations, candidate payoffs, ESS tests, circular-game normalization, or logging. It does not
turn a bounded interval into an upward rule, add Frank–Wolfe witnesses, use the negative-entry graph, subdivide a cone, or perform
connected-component decomposition.

## CP and SCP classification

The selected-predicate `solve` path and combined `classify` path use the same support traversal. A nonsingular subset whose exact
solution is componentwise nonpositive rejects CP and SCP. A singular subset with a nonzero componentwise nonnegative null vector
proves that SCP is false. Strict-only mode stops there; CP and combined mode retain the vector's Dickinson certificate and any valid
ceiling-support pruning, then continues because a later support may still contain a negative witness. A completed traversal proves
CP; it proves SCP exactly when no boundary vector was found. Thus `both` is one traversal, not consecutive CP and SCP calls.

## Known Difficult Inputs

The recursive prune helps only when a generated vector satisfies $Au\geq0$ on every coordinate. If most certificates have one or
more negative product components, their intervals have proper upper bounds $N_A(u)\neq[n]$. The generator cannot safely use them,
so the model approaches the same $2^n-1$ emitted-support traversal and exact-factorization workload as `dickinson_2019`.

Boundary matrices whose first nonnegative zero has large support can still force the enumeration of many smaller supports before the
decisive singular vector appears. Globally covering rules found late cannot recover work already performed at lower cardinalities.

Matrices with many singular principal submatrices remain expensive. Each uncovered singular support still requires exact
factorization and may yield a mixed-sign null vector with narrow bounded coverage. Large integer entries can also make the exact
factorizations and full products expensive even when the number of surviving supports is modest.

Corpus matrix 9161 is a small reproducible boundary example protected by the focused model test. Its zero is found exactly; the
example guards strict-zero behavior but does not imply that support pruning helps on that structure.
