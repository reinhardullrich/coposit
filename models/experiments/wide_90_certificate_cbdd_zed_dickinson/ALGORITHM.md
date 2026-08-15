# 90%-Remaining Wide-Certificate CBDD-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> with the Boolean-family traversal. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact copositivity and strict-copositivity experiment copied in full from
`wide_certificate_cbdd_zed_dickinson`. It changes only the threshold deciding which valid Dickinson certificates may suppress later
support solves.

Public mode boundary: this experiment supports individually selected `copositive` and `strictly_copositive` modes and combined
classification of both predicates in one traversal.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson's certificates cover intervals in the Boolean lattice of matrix supports. The source CBDD model immediately prunes every
such interval. That is optimal for avoiding the current exact solve, but a covered support can itself generate a much wider future
certificate. This experiment deliberately accepts more exact solves in order to retain those opportunities.

For a certificate generated while processing a support $I$ of cardinality $k$, with lower endpoint $L$, upper endpoint $U$, and
matrix order $n$, define

$$
d=|U|-|L|.
$$

The largest ordinary free-index width available above a $k$-element support is $n-k$. Only a certificate satisfying
$d>9(n-k)/10$ prunes its complete interval $[L,U]$. A certificate with $d\leq9(n-k)/10$ remains mathematically valid and is recorded
in progress diagnostics, but it removes only the exact support $I$ that was just processed. Removing $I$ is necessary bookkeeping:
extracting the first support from the CBDD does not mutate it, so leaving $I$ in the remaining family would return the same support
forever.

Before constructing any cardinality family, the model normally finds every maximal principal $Z$-matrix block. It tests the block's
strictly negative connected components for exact positive definiteness in strict mode or positive semidefiniteness in non-strict mode.
In combined mode, failure of positive semidefiniteness rejects both predicates, while a singular positive-semidefinite component
rejects only strict copositivity. An accepted block is discarded: this model deliberately does not insert its downset into the BDD.
An exact Motzkin–Straus pattern check bypasses this rejection-only stage because its maximal blocks are graph cliques and a complete
clique enumeration is pure overhead when it finds no early rejection. Dickinson's complete traversal still follows unchanged.

For each support cardinality $k$, another CBDD represents all $k$-element supports. Subtracting the suppressed-family CBDD leaves
exactly the supports that still require Dickinson's exact principal-matrix calculation. Wide certificates add their complete
interval; narrow certificates add only the already-processed singleton family $\{I\}$.

## Name And Sources

The identifier is `wide_90_certificate_cbdd_zed_dickinson`. “90” names the sole experimental rule $d>9(n-k)/10$. “Zed” spells out
the $Z$ in $Z$-matrix, and the `C` in CBDD means chain-reduced. The model is a complete copy of
`models/experiments/wide_certificate_cbdd_zed_dickinson`; its Zed prepass, principal solve, nullspace vector, sign decisions, generated Dickinson
vectors, witnesses, exact CBDD implementation, variable order, and termination decisions remain unchanged.

The certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The decision-diagram representation uses
the reduced ordered BDD rules from Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions
on Computers* C-35(8), 1986, 677–691. The chain nodes and range-aware APPLY rules follow Randal E. Bryant, “Chain Reduction for
Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`, Sections 3–6. The implementation is a small private CBDD
containing only the union and difference operations needed here; no decision-diagram dependency is added. No external CBDD source
was copied: the chain logic is a local reconstruction of Bryant's published reduction, split, cofactor, and combine rules.

The additional mathematics is Dickinson's Lemma 6.2, Algorithm 3, and the maximal-set construction in the proof of Theorem 6.5.
Dickinson does not specify how to enumerate the maximal blocks. Bron–Kerbosch enumeration is a coposit implementation choice. Using
only the negative implication of the $Z$-matrix test, and intentionally discarding positive blocks, is this model's defining policy.
The 90%-of-remaining threshold is an experiment created in coposit; it is not proposed by Dickinson or Bryant and has no claimed
optimality.

## Motzkin–Straus Bypass

The model recognizes positive integer scalings of the exact graph construction

$$
Q_\lambda=\lambda(E-B)-E.
$$

Such a matrix has one common nonnegative value on every diagonal and graph non-edge, and one common negative value on every graph
edge. The detector checks precisely that two-value structure in the upper triangle and requires at least one edge. This costs one
quadratic scan with no factorization or graph search. It neither infers the graph's clique number nor returns a copositivity result.

When the pattern is present, the model skips the maximal-Zed stage and starts ordinary CBDD Dickinson immediately. This is a
model-local routing choice: the shared pre-check pipeline remains unchanged, every later calculation is exact, and matrices not
matching the complete pattern retain the original Zed scan. The model also permits an explicit bypass for experiments that isolate
the Dickinson certificate traversal; this changes no later test or certificate.

## Zed-Block Stage

Build the compatibility graph

$$
\{i,j\}\in E(H_A)\iff a_{ij}\leq0.
$$

An index set $J$ induces a $Z$-matrix exactly when it is a clique of $H_A$. Bron–Kerbosch enumerates every maximal clique using the
current clique $R$, possible extensions $P$, and already-handled extensions $X$. With a pivot $p\in P\cup X$, it branches over
$P\setminus N(p)$; adding $v$ produces $(R\cup\{v\},P\cap N(v),X\cap N(v))$. The condition $P=X=\varnothing$ emits one maximal block.
Cardinality-one blocks are left to ordinary Dickinson because the normal cardinality-one step already decides them cheaply.

For a symmetric $Z$-matrix, strict copositivity is equivalent to positive definiteness, while non-strict copositivity is equivalent to
positive semidefiniteness. Before exact factorization, the model splits
$J$ into connected components of the strictly negative graph. Cross-component entries are zero: they are nonnegative because no
negative edge joins the components and nonpositive because $A_J$ is a $Z$-matrix. Consequently $A_J$ is PD exactly when every
component principal matrix is PD, and PSD exactly when every component principal matrix is PSD.

Each component is factorized with exact fraction-free LDLT. A component that fails the mode's definiteness requirement returns
`false`. If all components pass, the block is discarded and enumeration continues. Although accepted definiteness would also
certify every principal subset of $J$, encoding that
downset can complicate the global decision diagram more than the avoided low-cardinality work is worth. The Zed stage therefore
contributes only a negative witness and never changes the covered BDD.

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

Variables run from matrix index $n-1$ down to zero. A low edge is chosen before a high edge, preserving Dickinson Final's increasing
numeric support-mask order within each cardinality.

Union is Boolean disjunction. Difference is left conjunction with the negation of the right operand. Both use Bryant's range-aware
APPLY rule. For a pair of nodes, the common split begins at the earliest top variable and ends at the earliest chain bottom or just
before the other operand begins. A node wholly after that range is a don't-care and supplies itself as both cofactors. Splitting
inside an OR chain sends the high cofactor to its high child and represents the unconsumed suffix as one shorter chain node. Operand
pairs are memoized for each operation, and every result passes through the reductions above.

This differs deliberately from a CZDD. A CBDD level-skipping edge means don't-care, while its explicit ranges compress OR chains.
A CZDD level-skipping edge means forced zero, while its ranges compress don't-care chains.

## Dynamic Enumeration

After the rejection-only Zed stage has passed, a memoized recurrence constructs the exact-cardinality characteristic function $K_k$.
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
[L,U],& |U|-|L|>9(n-k)/10,\\
\{I\},& |U|-|L|\leq9(n-k)/10.
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

## Unchanged Dickinson Calculation

For an emitted support $I$, copy and factor $A_I$ exactly with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness, so strict copositivity fails.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero, so strict copositivity fails immediately; non-strict copositivity permits the equality and records its certificate.

Every other vector generates the exact interval above. All components of $Au$ use arbitrary-precision integers.

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Select non-strict or strict sign conditions.
3. Test the exact Motzkin–Straus two-value pattern. If it matches, skip to step 6.
4. Enumerate every maximal Zed block $J$ of size at least two.
5. Split $A_J$ by strictly negative connectivity and factor every component. Return `false` if one is not PD in strict mode or not
   PSD in non-strict mode; otherwise discard $J$ and continue the Zed scan.
6. For cardinality $k$, construct $R_k=K_k\setminus C$ and extract its first support.
7. Perform the unchanged Dickinson exact solve or nullspace branch.
8. Return `false` on a negative witness in either mode, or on a nonnegative zero in strict mode.
9. Compute $d=|U|-|L|$. If $d>9(n-k)/10$, add and subtract $[L,U]$; otherwise add and subtract only $\{I\}$. The implementation
   evaluates the equivalent integer threshold without floating-point arithmetic or overflowing a product with $n$.
10. Advance to $k+1$ when $R_k$ becomes false, and return `true` after the final cardinality is exhausted.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through the Python
analysis interface.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so there is no fixed-width support limit. CBDD node identifiers and both variable positions use `size_t`.

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

The 90%-of-remaining rule can deliberately discard nearly all pruning. If generated certificates repeatedly satisfy
$d\leq9(n-k)/10$, the
model processes supports explicitly until a later support yields a wide certificate or the complete Boolean lattice is exhausted.
This can replace CBDD node explosion with exponentially many principal solves. Conversely, a few newly exposed supports may produce
wide certificates that the source model would never generate. The threshold supplies no theorem predicting which effect wins.

The rejection-only Zed prepass also has an exponential worst case because a graph can have exponentially many maximal cliques;
Dickinson's Example 6.7 gives this obstruction. Component splitting makes zero-linked blocks cheap—for example, an identity block
becomes singleton factorizations—but a large block with a connected negative graph still requires one large exact factorization.
If every tested block is PD, the prepass provides no later pruning and its complete cost is overhead before CBDD Dickinson.
The Motzkin–Straus bypass removes that known graph-construction case, but other matrices can still induce the same expensive
maximal-clique enumeration without matching the exact two-value structure.
