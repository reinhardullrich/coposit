# CZDD Negative-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> with the Boolean-family traversal. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact strict-copositivity variant combining a rejection-only use of Dickinson's Section 6
$Z$-matrix blocks with Bryant's chain-reduced ZDD representation of Dickinson intervals.

Public mode boundary: this experiment supports only `strictly_copositive`. A non-strict request throws `std::invalid_argument`.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson's certificates cover many intervals in the Boolean lattice of supports. Rather than construct a support and search the
certificate list, this model stores the complete union in one chain-reduced zero-suppressed decision diagram (CZDD). A CZDD node can
span several consecutive optional variables that would otherwise form a don't-care chain.

Before constructing any cardinality family, the model finds every maximal principal $Z$-matrix block. It tests the block's strictly
negative connected components for exact positive definiteness. Failure rejects strict copositivity. A positive-definite block is
discarded: this model deliberately does not insert its downset into the ZDD.

For cardinality $k$, a second CZDD represents all $k$-element supports. Their exact set difference contains precisely the supports
that still require Dickinson's matrix calculation. The model repeatedly takes the first remaining support, performs the unchanged
exact Dickinson step, and subtracts its new certificate interval from the diagram.

## Name And Sources

The identifier is `czdd_zed_dickinson`. “Zed” spells out the $Z$ in $Z$-matrix so the extra certificate family remains visible. The
`C` in CZDD means chain-reduced. The model began as a complete copy of `models/experiments/zdd_zed_dickinson`; its Zed prepass,
principal solve, nullspace vector, sign decisions, witnesses, and certificate formula remain unchanged.

The certificate is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The data structure follows Shin-ichi
Minato, “Zero-Suppressed BDDs for Set Manipulation in Combinatorial Problems,” DAC 1993, and “Calculation of Unate Cube Set Algebra
Using Zero-Suppressed BDDs,” DAC 1994. Its chain nodes and range-aware APPLY rules follow Randal E. Bryant, “Chain Reduction for
Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`, Sections 3–6. This model contains a small private
implementation of only the union and difference operations it needs; it adds no decision-diagram dependency. No external CZDD
source was copied: the chain logic is a local reconstruction of Bryant's published reduction, split, cofactor, and combine rules.

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
contributes only a negative witness and never changes the covered ZDD.

## Dickinson Intervals As Set Families

For a computed vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson covers exactly the interval

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

The ZDD interprets every support as a selected-index set. The interval is built as one chain:

- an index in $L$ has only an inclusion edge;
- an index in $U\setminus L$ has exclusion and inclusion edges to the same continuation;
- an index outside $U$ is omitted from the chain and is therefore absent.

Union inserts this family into the accumulated covered family $C$:

$$
C\leftarrow C\cup[L,U].
$$

## Chain-Reduced ZDD Representation

Node zero represents the empty family. Node one represents the family containing only the empty support. Every other node stores a
top variable $t$, a bottom variable $b$, a low child $f$, and a high child $g$. The notation $⟨t:b\to g,f⟩$ represents a
don't-care chain: variables $t$ through $b-1$ may be absent or present and continue to the next level; variable $b$ has the ordinary
low child $f$ and high child $g$. The case $t=b$ is an ordinary ZDD node.

Construction first applies standard zero suppression. A one-level node with empty high child becomes its low child; for a longer
range, its final forced-zero level is removed while the preceding range remains one don't-care chain. If both children are the same
adjacent chain node, that child is absorbed. A unique table shares identical `(top, bottom, low, high)` tuples.

Variables are ordered from matrix index $n-1$ down to index zero. Traversing a low edge before a high edge therefore preserves the
increasing numeric support-mask order used by Dickinson Final within each cardinality.

Union and difference use Bryant's range-aware APPLY rule plus a per-operation memo table. The split begins at the earliest top
variable and consumes the largest common chain prefix allowed by both operands. A node beginning after that range supplies itself as
the low cofactor and the empty family as the high cofactor, because a skipped ZDD variable is absent. Splitting inside a don't-care
chain supplies its unconsumed suffix as both cofactors. Outputs pass through zero suppression, chain reduction, and the unique table.

## Dynamic Enumeration

After the rejection-only Zed stage has passed, a memoized recurrence constructs the ZDD $K_k$ of all $k$-element supports. At this
point $C$ contains only ordinary Dickinson certificate intervals. The remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path in $R_k$ gives the next uncovered support. Within a chain, the optional prefix is omitted first; if the
bottom low child is empty, the bottom variable is selected before the high child is followed. Only then is the packed mask converted
to the ascending index vector required for principal-matrix access.

An accepted Dickinson vector creates interval $Q=[L,U]$. The update is

$$
C\leftarrow C\cup Q,
\qquad
R_k\leftarrow R_k\setminus Q.
$$

The current support always lies in its own interval and disappears in this subtraction. The next path therefore advances without a
separate certificate scan. At the next cardinality, one difference against the accumulated $C$ removes all old intervals together.

## Unchanged Dickinson Calculation

For the emitted support $I$, copy and factor $A_I$ exactly with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness and the model returns `false`.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero and strict copositivity fails immediately.

Every other vector becomes the exact interval inserted above. The model computes all components of $Au$ with arbitrary-precision
integers; no floating-point value enters the ZDD.

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Reject a non-strict mode request.
3. Enumerate every maximal Zed block $J$ of size at least two.
4. Split $A_J$ by strictly negative connectivity and factor every component. Return `false` if one is not PD; otherwise discard
   $J$ and continue the Zed scan.
5. For cardinality $k$, construct $R_k=K_k\setminus C$ and extract its first support.
6. Run the unchanged Dickinson exact solve or nullspace branch.
7. Return `false` on a negative witness or nonnegative zero.
8. Otherwise compute $[L,U]$, union it into $C$, and subtract it from the active $R_k$.
9. Advance to $k+1$ when $R_k$ is empty, and return `true` after $R_n$ is empty.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through the normal
Python analysis interface.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so the model has no fixed-width support limit. CZDD node identifiers and both variable positions use `size_t`.

Every CZDD operation is exact set algebra. Difference removes only supports covered by a mathematically valid Dickinson interval.
Each accepted iteration removes at least its current support from a finite $R_k$, so every cardinality and the complete traversal
terminate when memory is sufficient.

## Known Difficult Inputs

Decision diagrams can be exponentially large for an unfavorable family or variable order. Chain reduction only compresses
consecutive don't-care ranges; it cannot make an arbitrary set family small. This private experimental implementation
has hash-consing and per-operation memoization but deliberately has no garbage collector or dynamic variable reordering. Nodes made
unreachable by later unions and differences remain allocated until the matrix call ends. A certificate pattern with little shared
structure can therefore consume more memory than Dickinson Final's flat interval list.

Building an exact-cardinality family and repeatedly updating a large diagram can also cost more than a direct coverage lookup on easy
or low-order matrices. The model is intended to test whether avoided support enumeration outweighs those costs, not to assume that a
ZDD is universally smaller.

The rejection-only Zed prepass also has an exponential worst case because a graph can have exponentially many maximal cliques;
Dickinson's Example 6.7 gives this obstruction. Component splitting makes zero-linked blocks cheap—for example, an identity block
becomes singleton factorizations—but a large block with a connected negative graph still requires one large exact factorization.
If every tested block is PD, the prepass provides no later pruning and its complete cost is overhead before CZDD Dickinson.
