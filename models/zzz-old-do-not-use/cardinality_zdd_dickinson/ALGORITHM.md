# Cardinality-ZDD Dickinson

Classification: coposit-created exact CP/SCP classification variant of Dickinson 2019.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson processes supports in increasing cardinality. This model gives each cardinality its own zero-suppressed decision diagram
(ZDD): while processing size $k$, the ZDD represents only the still-uncovered $k$-element supports. It never constructs or retains a
decision diagram for certificate coverage at other support sizes.

Every Dickinson certificate remains stored as two packed endpoint sets. At cardinality $k$, the model constructs a compact ZDD for
the unrestricted interval and subtracts it from the active cardinality-$k$ ZDD. This removes exactly the interval's $k$-element slice
without constructing that slice separately. Supports are emitted one at a time from the remaining family. The complete ZDD, unique
table, and operation cache are discarded before cardinality $k+1$ begins.

## Name And Sources

“Cardinality-ZDD” distinguishes this model from `zdd_dickinson`, which retains one unrestricted ZDD union across the whole
matrix. This model began as a complete copy of that experiment. Its exact principal solve, nullspace branch, sign decisions,
certificate formula, witnesses, and strict termination rules remain unchanged.

The certificate is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The set-family representation follows
Shin-ichi Minato, “Zero-Suppressed BDDs for Set Manipulation in Combinatorial Problems,” DAC 1993, and “Calculation of Unate Cube Set
Algebra Using Zero-Suppressed BDDs,” DAC 1994. The cardinality-local combination is a coposit experiment, not a claim made by those
papers.

## Dickinson Certificates

For an accepted exact vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

The certificate covers

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

Only the current cardinality slice is represented:

$$
Q_k(L,U)=[L,U]\cap K_k,
\qquad
K_k=\{I\subseteq\{0,\ldots,n-1\}:|I|=k\}.
$$

The model retains `L`, `U`, $|L|$, and $|U|$. If $k<|L|$ or $k>|U|$, the slice is empty and creates no ZDD nodes. Otherwise, because
the active remainder satisfies $R_k\subseteq K_k$,

$$
R_k\setminus[L,U]=R_k\setminus\bigl([L,U]\cap K_k\bigr)=R_k\setminus Q_k(L,U).
$$

The implementation therefore subtracts the compact unrestricted interval and avoids constructing its cardinality slice.

## Cardinality-Local ZDD

Node zero represents the empty family. Node one represents the family containing only the empty support. Every other node stores one
matrix index and low/high children for omitting/including that index. A node whose high child is empty is replaced by its low child;
identical `(variable, low, high)` nodes share one canonical node through a unique table.

A memoized recurrence constructs $K_k$ directly. Its state is `(next variable, number of selected indices still needed)`, so a compact
graph denotes all $\binom nk$ supports without listing them.

The unrestricted interval $[L,U]$ is constructed in one reverse pass over the variables:

- an index in $L$ must use the high branch;
- an index outside $U$ is omitted and therefore absent;
- an index in $U\setminus L$ uses a node with identical low and high families, making it optional.

The active family begins as $R_k=K_k$. Each old certificate is applied sequentially as

$$
R_k\leftarrow R_k\setminus[L,U],
$$

which is equal to subtracting $Q_k(L,U)$ by the identity above. There is deliberately no global covered-union ZDD. Difference uses
recursive top-variable decomposition, a per-operation pair cache, and the unique table. A low-before-high path yields one uncovered
support. A newly accepted Dickinson certificate is subtracted from the current $R_k$ immediately and its endpoints are retained for
later cardinalities.

## Unchanged Exact Dickinson Step

For an emitted support $I$, copy and factor the principal matrix $A_I$ with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness and strict copositivity fails.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero and strict copositivity fails immediately.

Every other vector creates the exact certificate above. All components of $Au$ use arbitrary-precision integers.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix and reject non-strict mode.
2. For cardinality $k=1,\ldots,n$, create a fresh ZDD for $K_k$.
3. Subtract every retained certificate whose cardinality range contains $k$ using its compact unrestricted interval ZDD.
4. Extract the first remaining support and run the unchanged exact Dickinson step.
5. Return `false` on a negative witness or nonnegative zero.
6. Otherwise retain the new certificate and subtract its compact interval immediately.
7. Repeat until $R_k$ is empty, then destroy the ZDD and advance to $k+1$.
8. Return `true` after cardinality $n$ is exhausted.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through Python.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. ZDD node identifiers and variable positions use `size_t`.

Every ZDD operation is exact. Difference removes only supports covered by a valid Dickinson interval. Each accepted iteration removes
at least its current support from finite $R_k$; therefore every stage and the complete traversal terminate when sufficient resources
are available. Cooperative timeout checkpoints are present in diagram construction and difference operations.

## CP and SCP classification

The selected-predicate `solve` path and combined `classify` path use the same support traversal. A nonsingular subset whose exact
solution is componentwise nonpositive rejects CP and SCP. A singular subset with a nonzero componentwise nonnegative null vector
proves that SCP is false. Strict-only mode stops there; CP and combined mode retain the vector's ordinary Dickinson interval and
continue, because a later support may still contain a negative witness. A completed traversal proves CP; it proves SCP exactly when
no boundary vector was found. Thus `both` is one traversal, not consecutive CP and SCP calls.

## Known Difficult Inputs

The exact-cardinality ZDD itself is compact, but repeated interval differences can still create an exponentially large remaining
family for an unfavorable certificate family or variable order. Nodes made unreachable within one cardinality remain allocated until
that stage ends; the model has no within-stage garbage collector or dynamic variable reordering.

Every retained certificate whose cardinality range contains $k$ must be reconstructed as an $O(n)$ interval ZDD when stage $k$
starts. Inputs producing many broad, poorly sharing certificates can still spend substantial time on repeated differences even when
few supports need an exact matrix solve. Zero suppression favors families in which most indices are absent, but it does not remove
the worst case for dense middle-cardinality families.
