# Cardinality-BDD Dickinson

Classification: coposit-created exact strict-copositivity variant of Dickinson Final.

Public mode boundary: this experiment supports only `strictly_copositive`. A non-strict request throws `std::invalid_argument`.

## Idea In Plain Language

Dickinson processes supports in increasing cardinality. This model uses that order directly: while processing supports of size $k$,
one reduced ordered binary decision diagram (BDD) represents only the still-uncovered $k$-element supports. It never constructs or
retains a decision diagram for certificate coverage at other cardinalities.

Every Dickinson certificate remains stored as its two packed endpoints. At cardinality $k$, the model constructs a compact BDD for
the unrestricted interval and subtracts it from the active cardinality-$k$ BDD. This removes exactly the interval's $k$-element
slice without constructing that slice separately. Supports are then emitted one at a time. The complete BDD, its unique table, and
all operation caches are destroyed before cardinality $k+1$ begins.

## Name And Sources

“Cardinality-BDD” distinguishes this model from `interval_bdd_dickinson`, which retains one unrestricted BDD union across the whole
matrix. This model began as a complete copy of that experiment. Its exact principal solve, nullspace branch, sign decisions,
certificate formula, witnesses, and strict termination rules remain unchanged.

The certificate is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The reduced ordered BDD representation and
recursive Boolean operations follow Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE
Transactions on Computers* C-35(8), 1986, 677–691. The cardinality-local combination is a coposit experiment, not a claim made by
either paper.

## Dickinson Certificates

For an accepted exact vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

The certificate covers the Boolean interval

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

Only its cardinality-$k$ slice matters while stage $k$ is active:

$$
Q_k(L,U)=[L,U]\cap K_k,
\qquad
K_k=\{I\subseteq\{0,\ldots,n-1\}:|I|=k\}.
$$

The model retains `L`, `U`, $|L|$, and $|U|$. If $k<|L|$ or $k>|U|$, then $Q_k$ is empty and no diagram is built for that
certificate. Otherwise, because the active remainder satisfies $R_k\subseteq K_k$,

$$
R_k\setminus[L,U]=R_k\setminus\bigl([L,U]\cap K_k\bigr)=R_k\setminus Q_k(L,U).
$$

The implementation therefore subtracts the compact unrestricted interval and avoids constructing its cardinality slice.

## Cardinality-Local BDD

Node zero is false and node one is true. Every other node stores one matrix index and low/high children for excluding/including that
index. Variables have one fixed order. A node whose children are equal is removed, and identical `(variable, low, high)` nodes share
one canonical node through a unique table.

A memoized recurrence constructs $K_k$ directly. Its state is `(next variable, number of selected indices still needed)`, so the
graph is polynomial in $n$ and $k$ even though it denotes $\binom nk$ supports.

The unrestricted interval $[L,U]$ is constructed in one reverse pass over the variables:

- an index in $L$ must follow the high branch;
- an index outside $U$ must follow the low branch;
- an index in $U\setminus L$ is unconstrained and needs no BDD node.

The active family starts as $R_k=K_k$. Every previously retained certificate is applied sequentially as

$$
R_k\leftarrow R_k\setminus[L,U],
$$

which is equal to subtracting $Q_k(L,U)$ by the identity above. There is deliberately no global covered-union BDD. Difference uses
recursive top-variable decomposition, a per-operation pair cache, and the same unique table. A low-before-high path to true yields
one uncovered support; skipped variables are false. When a new certificate is found, its interval is immediately subtracted from
$R_k$ and its endpoints are retained for later stages.

## Unchanged Exact Dickinson Step

For an emitted support $I$, copy and factor the principal matrix $A_I$ with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness and strict copositivity fails.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero and strict copositivity fails immediately.

Every other vector creates the exact certificate above. All components of $Au$ use arbitrary-precision integers.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix and reject non-strict mode.
2. For cardinality $k=1,\ldots,n$, create a fresh BDD for $K_k$.
3. Subtract every retained certificate whose cardinality range contains $k$ using its compact unrestricted interval BDD.
4. Extract the first remaining support and run the unchanged exact Dickinson step.
5. Return `false` on a negative witness or nonnegative zero.
6. Otherwise retain the new certificate and subtract its compact interval immediately.
7. Repeat until $R_k$ is empty, then destroy the BDD and advance to $k+1$.
8. Return `true` after cardinality $n$ is exhausted.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through Python.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. BDD node identifiers and variable positions use `size_t`.

Every BDD operation is exact. Difference removes only supports covered by a valid Dickinson interval. Each accepted iteration removes
at least its current support from finite $R_k$; therefore every stage and the complete traversal terminate when sufficient resources
are available. Cooperative timeout checkpoints are present in diagram construction and difference operations.

## Known Difficult Inputs

The exact-cardinality BDD itself has only polynomial size, but repeated interval differences can still create an exponentially large
remaining-family BDD for an unfavorable certificate family or variable order. Nodes made unreachable within one cardinality remain
allocated until that stage ends; the model has no within-stage garbage collector or dynamic variable reordering.

Every retained certificate whose cardinality range contains $k$ must be reconstructed as an $O(n)$ interval BDD when stage $k$
starts. Inputs producing many broad, poorly sharing certificates can still spend substantial time on repeated differences even when
few supports need an exact matrix solve. The stage boundary limits this memory to one cardinality but does not remove the
decision-diagram worst case.
