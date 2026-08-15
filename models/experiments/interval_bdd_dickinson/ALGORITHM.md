# Interval-BDD Dickinson

Classification: coposit-created exact strict-copositivity variant of Dickinson Final.

Public mode boundary: this experiment supports only `strictly_copositive`. A non-strict request throws `std::invalid_argument`.

## Idea In Plain Language

Dickinson's certificates cover intervals in the Boolean lattice of matrix supports. This model represents the complete union of
those covered intervals as one reduced ordered binary decision diagram (BDD), rather than constructing every support and searching
a flat certificate list.

For each support cardinality $k$, another BDD represents all $k$-element supports. Subtracting the covered BDD leaves exactly the
supports that still require Dickinson's exact principal-matrix calculation. Each new certificate is added to the covered diagram
and removed from the active cardinality diagram.

## Name And Sources

“Interval-BDD” identifies the sole mathematical change from Dickinson Final: Dickinson intervals are compiled into a BDD
set-family representation. The model began as a complete copy of `models/experiments/interval_zdd_dickinson`; its principal solve,
nullspace vector, sign decisions, certificate formula, witnesses, and strict termination rules remain unchanged.

The certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The decision-diagram representation uses
the reduced ordered BDD rules from Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions
on Computers* C-35(8), 1986, 677–691. The implementation is a small private BDD containing only the operations needed here; no
decision-diagram dependency is added.

## Dickinson Intervals As Boolean Functions

For a computed vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson covers exactly

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

Each matrix index is a Boolean variable whose true value means that the index belongs to the support. The characteristic function
of $[L,U]$ therefore requires:

- true for every index in $L$;
- false for every index outside $U$; and
- either value for every index in $U\setminus L$.

An optional variable disappears under ordinary BDD reduction because its low and high continuations are identical. Before sharing
with other certificates, one interval consequently needs at most $|L|+n-|U|$ nonterminal BDD nodes.

## Reduced Ordered BDD

Node zero is constant false and node one is constant true. Every other node stores a variable and two children: the low child assigns
the variable false and the high child assigns it true. Variables have one fixed order. Two reductions make the diagram canonical for
that order:

1. a node whose low and high children are equal is replaced by that child; and
2. nodes with the same `(variable, low, high)` triple are shared through a unique table.

Variables run from matrix index $n-1$ down to zero. A low edge is chosen before a high edge, preserving Dickinson Final's increasing
numeric support-mask order within each cardinality.

Union is Boolean disjunction. Difference is left conjunction with the negation of the right operand. Both operations recursively
split on the earliest variable present in either operand, use the unchanged operand as both cofactors when that variable is absent,
memoize operand pairs for the duration of the operation, and pass every result through the unique table.

This differs deliberately from a zero-suppressed decision diagram. In a BDD, a skipped variable is irrelevant and may be either
false or true. In a ZDD, a skipped variable is absent from every represented set.

## Dynamic Enumeration

For cardinality $k$, a memoized recurrence constructs the exact-cardinality characteristic function $K_k$. If $C$ is the accumulated
covered family, the remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path ending at true yields the next uncovered support. Variables skipped on that path are assigned false.
Only this support is converted to the ascending index vector used to copy the principal submatrix.

An accepted Dickinson vector creates $Q=[L,U]$, followed by

$$
C\leftarrow C\cup Q,
\qquad
R_k\leftarrow R_k\setminus Q.
$$

The current support belongs to its own certificate and is removed. The next satisfying BDD path therefore advances without a
separate scan through retained certificates.

## Unchanged Dickinson Calculation

For an emitted support $I$, copy and factor $A_I$ exactly with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness, so strict copositivity fails.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero, so strict copositivity fails immediately.

Every other vector generates the exact interval above. All components of $Au$ use arbitrary-precision integers.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Reject a non-strict request.
3. Maintain one BDD $C$ for all covered certificate intervals.
4. For cardinality $k$, construct $R_k=K_k\setminus C$.
5. Extract the first satisfying support from $R_k$.
6. Perform Dickinson Final's exact solve or nullspace branch.
7. Return `false` on a negative witness or nonnegative zero.
8. Otherwise add $[L,U]$ to $C$ and subtract it from $R_k$.
9. Advance to $k+1$ when $R_k$ becomes false.
10. Return `true` after the final cardinality is exhausted.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through the Python
analysis interface.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. BDD node identifiers and variable positions use `size_t`.

BDD operations are exact Boolean algebra. Difference removes only supports covered by a valid Dickinson interval. Every accepted
iteration removes at least the current support from a finite $R_k$, so the traversal terminates when sufficient memory is available.

## Known Difficult Inputs

A reduced ordered BDD can still require exponentially many nodes for an unfavorable Boolean function or variable order. This private
implementation has hash-consing and per-operation memoization but no garbage collector, complemented edges, or dynamic variable
reordering. Nodes made unreachable by later unions and differences remain allocated until the matrix call ends.

BDD reduction is particularly favorable when certificate intervals have many optional indices, because those variables disappear.
It can be unfavorable when most indices are forced absent: unlike a ZDD, a BDD must retain the corresponding false decisions.
Exact-cardinality functions also require state across many variables. Inputs whose certificate union shares little structure can
therefore consume exponential time or memory even though every individual certificate has a short description.
