# BDD Negative-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> with the Boolean-family traversal. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact strict-copositivity variant combining a rejection-only use of Dickinson's Section 6
$Z$-matrix blocks with the interval-BDD Dickinson support representation.

Public mode boundary: this experiment supports only `strictly_copositive`. A non-strict request throws `std::invalid_argument`.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson's certificates cover intervals in the Boolean lattice of matrix supports. This model represents the complete union of
those covered intervals as one reduced ordered binary decision diagram (BDD), rather than constructing every support and searching
a flat certificate list.

Before constructing any cardinality family, the model finds every maximal principal $Z$-matrix block. It tests the block's strictly
negative connected components for exact positive definiteness. Failure rejects strict copositivity. A positive-definite block is
discarded: this model deliberately does not insert its downset into the BDD.

For each support cardinality $k$, another BDD represents all $k$-element supports. Subtracting the covered BDD leaves exactly the
supports that still require Dickinson's exact principal-matrix calculation. Each new certificate is added to the covered diagram
and removed from the active cardinality diagram.

## Name And Sources

The identifier is `bdd_zed_dickinson`. “Zed” spells out the $Z$ in $Z$-matrix so the extra certificate family remains visible. The
model began as a complete copy of `models/experiments/interval_bdd_dickinson`; its BDD algebra, principal solve, nullspace vector,
sign decisions, certificate formula, witnesses, and strict termination rules remain unchanged.

The certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The decision-diagram representation uses
the reduced ordered BDD rules from Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions
on Computers* C-35(8), 1986, 677–691. The implementation is a small private BDD containing only the operations needed here; no
decision-diagram dependency is added.

The additional mathematics is Dickinson's Lemma 6.2, Algorithm 3, and the maximal-set construction in the proof of Theorem 6.5.
Dickinson does not specify how to enumerate the maximal blocks. Bron–Kerbosch enumeration is a coposit implementation choice. Using
only the negative implication of the $Z$-matrix test, and intentionally discarding positive blocks, is this model's defining policy.

## Zed-Block Stage

Build the compatibility graph

$$
\{i,j\}\in E(H_A)\iff a_{ij}\leq0.
$$

An index set $J$ induces a $Z$-matrix exactly when it is a clique of $H_A$. Bron–Kerbosch enumerates every maximal clique using the
current clique $R$, possible extensions $P$, and already-handled extensions $X$. With a pivot $p\in P\cup X$, it branches over
$P\setminus N(p)$; adding $v$ produces $(R\cup\{v\},P\cap N(v),X\cap N(v))$. The condition $P=X=\varnothing$ emits one maximal block.
Cardinality-one blocks are left to ordinary Dickinson because the normal cardinality-one step already decides them cheaply.

For a symmetric $Z$-matrix, strict copositivity is equivalent to positive definiteness. Before exact factorization, the model splits
$J$ into connected components of the strictly negative graph. Cross-component entries are zero: they are nonnegative because no
negative edge joins the components and nonpositive because $A_J$ is a $Z$-matrix. Consequently $A_J$ is PD exactly when every
component principal matrix is PD.

Each component is factorized with exact fraction-free LDLT. A non-PD component returns `false`. If all components pass, the block is
discarded and enumeration continues. Although positive definiteness would also certify every principal subset of $J$, encoding that
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

After the rejection-only Zed stage has passed, a memoized recurrence constructs the exact-cardinality characteristic function $K_k$.
At this point $C$ contains only ordinary Dickinson certificate intervals. The remaining family is

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

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Reject a non-strict request.
3. Enumerate every maximal Zed block $J$ of size at least two.
4. Split $A_J$ by strictly negative connectivity and factor every component. Return `false` if one is not PD; otherwise discard
   $J$ and continue the Zed scan.
5. For cardinality $k$, construct $R_k=K_k\setminus C$ and extract its first support.
6. Perform the unchanged Dickinson exact solve or nullspace branch.
7. Return `false` on a negative witness or nonnegative zero.
8. Otherwise add $[L,U]$ to $C$ and subtract it from $R_k$.
9. Advance to $k+1$ when $R_k$ becomes false, and return `true` after the final cardinality is exhausted.

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

The rejection-only Zed prepass also has an exponential worst case because a graph can have exponentially many maximal cliques;
Dickinson's Example 6.7 gives this obstruction. Component splitting makes zero-linked blocks cheap—for example, an identity block
becomes singleton factorizations—but a large block with a connected negative graph still requires one large exact factorization.
If every tested block is PD, the prepass provides no later pruning and its complete cost is overhead before ordinary BDD Dickinson.
