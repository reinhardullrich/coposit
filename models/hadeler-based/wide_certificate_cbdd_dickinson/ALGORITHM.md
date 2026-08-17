# Wide-Certificate CBDD Dickinson

Classification: coposit-created exact copositivity and strict-copositivity experiment copied in full from `cbdd_dickinson`.
It changes only which valid Dickinson certificates may suppress later support solves.

Public mode boundary: this experiment supports individually selected `copositive` and `strictly_copositive` modes and combined
classification of both predicates in one traversal.
Analysis and reference-run interfaces default to combined classification when the mode is omitted.
The model requires one integer percentage parameter $p\in\{0,\ldots,100\}$.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix, a percentage $p$, and the selected mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing is an external caller concern; `model::solve` starts directly with this flow.

## Idea In Plain Language

Dickinson's certificates cover intervals in the Boolean lattice of matrix supports. The source CBDD model immediately prunes every
such interval. That is optimal for avoiding the current exact solve, but a covered support can itself generate a much wider future
certificate. This experiment deliberately accepts more exact solves in order to retain those opportunities.

For a certificate with lower endpoint $L$, upper endpoint $U$, and matrix order $n$, define

$$
d=|U|-|L|.
$$

At support cardinality $k$, only a certificate satisfying

$$
d>\left\lfloor\frac{p(n-k)}{100}\right\rfloor
$$

prunes its complete interval $[L,U]$. A certificate that fails this inequality remains mathematically valid and is recorded in
diagnostic output, but it removes only the exact support $I$ that was just processed. Removing $I$ is
necessary bookkeeping: extracting the first support from the CBDD does not mutate it, so leaving $I$ in the remaining family would
return the same support forever.

For each support cardinality $k$, another CBDD represents all $k$-element supports. Subtracting the suppressed-family CBDD leaves
exactly the supports that still require Dickinson's exact principal-matrix calculation. Wide certificates add their complete
interval; narrow certificates add only the already-processed singleton family $\{I\}$.

## Name And Sources

The identifier is `wide_certificate_cbdd_dickinson`. “Wide certificate” names the parameterized remaining-width rule, and the `C` in
CBDD means chain-reduced. The model is a complete copy of `models/hadeler-based/cbdd_dickinson`; its principal solve, nullspace
vector, sign decisions, generated Dickinson
vectors, witnesses, exact CBDD implementation, variable order, and termination decisions remain unchanged.

The certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The decision-diagram representation uses
the reduced ordered BDD rules from Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions
on Computers* C-35(8), 1986, 677–691. The chain nodes and range-aware APPLY rules follow Randal E. Bryant, “Chain Reduction for
Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`, Sections 3–6. The implementation is a small private CBDD
containing only the union and difference operations needed here; no decision-diagram dependency is added. No external CBDD source
was copied: the chain logic is a local reconstruction of Bryant's published reduction, split, cofactor, and combine rules.

The percentage threshold is an experiment created in coposit; it is not proposed by Dickinson or Bryant and has no claimed
optimality. The former 75%, 90%, and 95% source copies were identical except for this value and are now configurations of this one
model.

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

A memoized recurrence constructs the exact-cardinality characteristic function $K_k$.
At this point $C$ contains wide Dickinson certificate intervals and exact supports already processed under narrow certificates. The
remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path ending at true yields the next uncovered support. Variables skipped on that path are assigned false.
If an OR-chain node has no low solution, low-first traversal selects the bottom variable of the range before following its common
high child. Only the resulting support is converted to the ascending index vector used to copy the principal submatrix.

An accepted Dickinson vector first determines

$$
Q=
\begin{cases}
[L,U],& |U|-|L|>\left\lfloor p(n-k)/100\right\rfloor,\\
\{I\},& \text{otherwise}.
\end{cases}
$$

The update is then

$$
C\leftarrow C\cup Q,
\qquad
R_k\leftarrow R_k\setminus Q.
$$

The current support is removed in both branches. The next satisfying CBDD path therefore advances. Only the first branch suppresses
supports that have not yet been processed.

## Safe Cardinality Expiration

Each retained interval is tagged with $u=|U|$ and also united into an expiry bucket $E_u$. Before the model starts cardinality
$k$, it performs

$$
C\leftarrow C\setminus\bigcup_{u<k}E_u
$$

and clears those buckets. This cannot change any present or future decision: every $J\in[L,U]$ satisfies
$|J|\leq|U|=u$, so an interval with $u<k$ contains no support of cardinality $k$ or larger. Intersections with intervals that
remain live are harmless for the same reason; every removed support is below the traversal frontier. Exact narrow certificates have
$U=I$ and therefore expire immediately after their own cardinality.

Expiration changes only the live decision-diagram roots. The private arena and unique table retain already allocated nodes until the
matrix call ends, so this reduces later union and difference operands but is not garbage collection.

## Unchanged Dickinson Calculation

For an emitted support $I$, copy and factor $A_I$ exactly with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness, so strict copositivity fails.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero, so strict copositivity fails immediately; non-strict copositivity permits the equality and records its certificate.

Every other vector generates the exact interval above. All components of $Au$ use arbitrary-precision integers.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. CBDD node identifiers and both variable positions use `size_t`.
The percentage is parsed exactly as an integer from 0 through 100 and the threshold is evaluated with integer arithmetic; no
floating-point rounding enters the branching decision. Configuration is thread-local, so independent Python workers or calling
threads may use different percentages without racing.

CBDD operations are exact Boolean algebra. A full interval is removed only when it is a valid Dickinson interval; the narrow branch
removes only the support whose exact Dickinson calculation has just completed. Every accepted iteration therefore removes at least
the current support from a finite $R_k$, so the traversal terminates when sufficient time and memory are available.

## Known Difficult Inputs

A chain-reduced BDD can still require exponentially many nodes for an unfavorable Boolean function or variable order. This private
implementation has hash-consing and per-operation memoization but no garbage collector, complemented edges, or dynamic variable
reordering. Nodes made unreachable by later unions and differences remain allocated until the matrix call ends.

Ordinary BDD reduction remains favorable when intervals have many optional indices. CBDD chaining additionally helps when forced
absent indices occur in long consecutive runs, but it cannot compress arbitrary separated decisions. Exact-cardinality functions
still require state across many variables. Inputs whose certificate union shares little ordered structure can therefore consume
exponential time or memory even though every individual certificate has a short description.

Large percentages can deliberately discard nearly all pruning. If generated certificates repeatedly fail the configured threshold,
the model processes supports explicitly until a later support yields a wide certificate or the complete Boolean lattice is exhausted.
This can replace CBDD node explosion with exponentially many principal solves. Conversely, a few newly exposed supports may produce
wide certificates that the source model would never generate. The threshold supplies no theorem predicting which effect wins.
