# CZDD Dickinson

Classification: coposit-created exact CP/SCP variant combining Dickinson certificates with Bryant's chain-reduced ZDD representation.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing is an external caller concern; `model::solve` starts directly with this flow.

## Idea In Plain Language

Dickinson's certificates cover many intervals in the Boolean lattice of supports. Rather than construct a support and search the
certificate list, this model stores the complete union in one chain-reduced zero-suppressed decision diagram (CZDD). A CZDD node can
span several consecutive optional variables that would otherwise form a don't-care chain.

For cardinality $k$, a second CZDD represents all $k$-element supports. Their exact set difference contains precisely the supports
that still require Dickinson's matrix calculation. The model repeatedly takes the first remaining support, performs the unchanged
exact Dickinson step, and subtracts its new certificate interval from the diagram.

## Name And Sources

The identifier is `czdd_dickinson`; the `C` in CZDD means chain-reduced. The exact Dickinson principal solve, nullspace vector,
sign decisions, witnesses, and certificate formula are unchanged.

The certificate is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The data structure follows Shin-ichi
Minato, “Zero-Suppressed BDDs for Set Manipulation in Combinatorial Problems,” DAC 1993, and “Calculation of Unate Cube Set Algebra
Using Zero-Suppressed BDDs,” DAC 1994. Its chain nodes and range-aware APPLY rules follow Randal E. Bryant, “Chain Reduction for
Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`, Sections 3–6. This model contains a small private
implementation of only the union and difference operations it needs; it adds no decision-diagram dependency. No external CZDD
source was copied: the chain logic is a local reconstruction of Bryant's published reduction, split, cofactor, and combine rules.

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
increasing numeric support-mask order used by Dickinson 2019 within each cardinality.

Union and difference use Bryant's range-aware APPLY rule plus a per-operation memo table. The split begins at the earliest top
variable and consumes the largest common chain prefix allowed by both operands. A node beginning after that range supplies itself as
the low cofactor and the empty family as the high cofactor, because a skipped ZDD variable is absent. Splitting inside a don't-care
chain supplies its unconsumed suffix as both cofactors. Outputs pass through zero suppression, chain reduction, and the unique table.

## Dynamic Enumeration

A memoized recurrence constructs the ZDD $K_k$ of all $k$-element supports. The accumulated family $C$ contains Dickinson
certificate intervals. The remaining family is

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

## Safe Cardinality Expiration

Each retained interval is tagged with $u=|U|$ and also united into an expiry bucket $E_u$. Before the model starts cardinality
$k$, it performs

$$
C\leftarrow C\setminus\bigcup_{u<k}E_u
$$

and clears those buckets. This cannot change any present or future decision: every $J\in[L,U]$ satisfies
$|J|\leq|U|=u$, so an interval with $u<k$ contains no support of cardinality $k$ or larger. Intersections with intervals that
remain live are harmless for the same reason; every removed support is below the traversal frontier.

Expiration changes only the live decision-diagram roots. The private arena and unique table retain already allocated nodes until the
matrix call ends, so this reduces later union and difference operands but is not garbage collection.

## Unchanged Dickinson Calculation

For the emitted support $I$, copy and factor $A_I$ exactly with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve $A_Iu=\mathbf1$ using integer numerators and a positive common denominator. If $u\leq0$, the embedded
$-u$ is a nonnegative negative witness and the model returns `false`.

If $A_I$ is singular, recover one exact nullspace vector and orient it to contain a positive entry. A nonnegative oriented vector is
a nonnegative zero and strict copositivity fails immediately.

Every other vector becomes the exact interval inserted above. The model computes all components of $Au$ with arbitrary-precision
integers; no floating-point value enters the ZDD.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so the model has no fixed-width support limit. CZDD node identifiers and both variable positions use `size_t`.

Every CZDD operation is exact set algebra. Difference removes only supports covered by a mathematically valid Dickinson interval.
Each accepted iteration removes at least its current support from a finite $R_k$, so every cardinality and the complete traversal
terminate when memory is sufficient.

## CP and SCP classification

The selected-predicate `solve` path and combined `classify` path use the same support traversal. A nonsingular subset whose exact
solution is componentwise nonpositive rejects CP and SCP. A singular subset with a nonzero componentwise nonnegative null vector
proves that SCP is false. Strict-only mode stops there; CP and combined mode retain the vector's ordinary Dickinson interval and
continue, because a later support may still contain a negative witness. A completed traversal proves CP; it proves SCP exactly when
no boundary vector was found. Thus `both` is one traversal, not consecutive CP and SCP calls.

## Known Difficult Inputs

Decision diagrams can be exponentially large for an unfavorable family or variable order. Chain reduction only compresses
consecutive don't-care ranges; it cannot make an arbitrary set family small. This private experimental implementation
has hash-consing and per-operation memoization but deliberately has no garbage collector or dynamic variable reordering. Nodes made
unreachable by later unions and differences remain allocated until the matrix call ends. A certificate pattern with little shared
structure can therefore consume more memory than Dickinson 2019's flat interval list.

Building an exact-cardinality family and repeatedly updating a large diagram can also cost more than a direct coverage lookup on easy
or low-order matrices. The model is intended to test whether avoided support enumeration outweighs those costs, not to assume that a
ZDD is universally smaller.
