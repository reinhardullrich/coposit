# CBDD Dickinson

Classification: coposit-created exact copositivity and strict-copositivity variant combining Dickinson certificates with Bryant's
chain-reduced BDD representation.

Public mode boundary: this experiment supports individually selected `copositive` and `strictly_copositive` modes and combined
classification of both predicates in one traversal.
Analysis and reference-run interfaces default to combined classification when the mode is omitted.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing is an external caller concern; `model::solve` starts directly with this flow.

## Idea In Plain Language

Dickinson's certificates cover intervals in the Boolean lattice of matrix supports. This model represents their complete union as
one chain-reduced ordered binary decision diagram (CBDD), rather than constructing every support and searching a flat certificate
list. A CBDD node can span several consecutive variables when they form one OR chain, including the common forced-zero case.

For each support cardinality $k$, another CBDD represents all $k$-element supports. Subtracting the covered CBDD leaves exactly the
supports that still require Dickinson's exact principal-matrix calculation. Each new certificate is added to the covered diagram
and removed from the active cardinality diagram.

## Name And Sources

The identifier is `cbdd_dickinson`; the `C` in CBDD means chain-reduced. The exact Dickinson principal solve, nullspace vector,
sign decisions, certificate formula, witnesses, and termination rules are unchanged.

The certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The decision-diagram representation uses
the reduced ordered BDD rules from Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions
on Computers* C-35(8), 1986, 677–691. The chain nodes and range-aware APPLY rules follow Randal E. Bryant, “Chain Reduction for
Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`, Sections 3–6. The implementation is a small private CBDD
containing only the union and difference operations needed here; no decision-diagram dependency is added. No external CBDD source
was copied: the chain logic is a local reconstruction of Bryant's published reduction, split, cofactor, and combine rules.

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

An optional variable disappears under ordinary BDD reduction because its low and high continuations are identical. Consecutive
forced-absent variables form a zero chain and are compressed into one CBDD node. Forced-present variables remain explicit.

## Chain-Reduced BDD Representation

Node zero is constant false and node one is constant true. Every other node stores a top variable $t$, a bottom variable $b$, a low
child $f$, and a high child $g$. The notation $⟨t:b\to g,f⟩$ represents the OR chain from $t$ through $b$: every high edge
goes directly to $g$, while successive low edges reach $f$. The case $t=b$ is an ordinary BDD node.

The implementation applies three canonical construction rules:

1. if low and high are equal, discard the node as an ordinary BDD don't-care;
2. if the low child starts at $b+1$ and has the same high child, absorb that adjacent OR chain; and
3. share nodes with the same `(top, bottom, low, high)` tuple through the unique table.

Variables run from matrix index $n-1$ down to zero. A low edge is chosen before a high edge, preserving Dickinson 2019's increasing
numeric support-mask order within each cardinality.

Union is Boolean disjunction. Difference is left conjunction with the negation of the right operand. Both use Bryant's range-aware
APPLY rule. For a pair of nodes, the common split begins at the earliest top variable and ends at the earliest chain bottom or just
before the other operand begins. A node wholly after that range is a don't-care and supplies itself as both cofactors. Splitting
inside an OR chain sends the high cofactor to its high child and represents the unconsumed suffix as one shorter chain node. Operand
pairs are memoized for each operation, and every result passes through the reductions above.

This differs deliberately from a CZDD. A CBDD level-skipping edge means don't-care, while its explicit ranges compress OR chains.
A CZDD level-skipping edge means forced zero, while its ranges compress don't-care chains.

## Dynamic Enumeration

A memoized recurrence a memoized recurrence constructs the exact-cardinality characteristic function $K_k$.
The accumulated family $C$ contains Dickinson certificate intervals. The remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path ending at true yields the next uncovered support. Variables skipped on that path are assigned false.
If an OR-chain node has no low solution, low-first traversal selects the bottom variable of the range before following its common
high child. Only the resulting support is converted to the ascending index vector used to copy the principal submatrix.

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
a nonnegative zero, so strict copositivity fails immediately; non-strict copositivity permits the equality and records its interval.

Every other vector generates the exact interval above. All components of $Au$ use arbitrary-precision integers.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. CBDD node identifiers and both variable positions use `size_t`.

CBDD operations are exact Boolean algebra. Difference removes only supports covered by a valid Dickinson interval. Every accepted
iteration removes at least the current support from a finite $R_k$, so the traversal terminates when sufficient memory is available.

## Known Difficult Inputs

A chain-reduced BDD can still require exponentially many nodes for an unfavorable Boolean function or variable order. This private
implementation has hash-consing and per-operation memoization but no garbage collector, complemented edges, or dynamic variable
reordering. Nodes made unreachable by later unions and differences remain allocated until the matrix call ends.

Ordinary BDD reduction remains favorable when intervals have many optional indices. CBDD chaining additionally helps when forced
absent indices occur in long consecutive runs, but it cannot compress arbitrary separated decisions. Exact-cardinality functions
still require state across many variables. Inputs whose certificate union shares little ordered structure can therefore consume
exponential time or memory even though every individual certificate has a short description.
