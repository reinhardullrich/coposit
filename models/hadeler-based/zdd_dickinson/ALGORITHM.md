# ZDD Dickinson

Classification: coposit-created exact CP/SCP classification variant of Dickinson 2019.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson's certificates cover many intervals in the Boolean lattice of supports. Rather than construct a support and search the
certificate list, this model stores the complete union of covered intervals in one zero-suppressed decision diagram (ZDD).

For cardinality $k$, a second ZDD represents all $k$-element supports. Their exact set difference contains precisely the supports
that still require Dickinson's matrix calculation. The model repeatedly takes the first remaining support, performs the unchanged
exact Dickinson step, and subtracts its new certificate interval from the diagram.

## Name And Sources

“ZDD” identifies the sole algorithmic change: Dickinson intervals are compiled into a ZDD set-family representation. The
model began as a complete copy of `models/hadeler-based/dickinson_2019`; its principal solve, nullspace vector, sign decisions, witnesses, and
certificate formula are unchanged.

The certificate is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569
(2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The data structure follows Shin-ichi
Minato, “Zero-Suppressed BDDs for Set Manipulation in Combinatorial Problems,” DAC 1993, and “Calculation of Unate Cube Set Algebra
Using Zero-Suppressed BDDs,” DAC 1994. This model contains a small private implementation of only the operations it needs; it adds no
decision-diagram dependency.

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

## ZDD Representation

Node zero represents the empty family. Node one represents the family containing only the empty support. Every other node stores a
variable and two children: the low child omits that index; the high child includes it. The standard zero-suppression rule replaces a
node whose high child is empty by its low child. A unique table shares identical `(variable, low, high)` nodes.

Variables are ordered from matrix index $n-1$ down to index zero. Traversing a low edge before a high edge therefore preserves the
increasing numeric support-mask order used by Dickinson 2019 within each cardinality.

Union and difference use recursive top-variable decomposition plus a per-operation memo table. Their outputs pass through the unique
table, so equivalent suffixes from different certificate cubes share the same nodes.

## Dynamic Enumeration

For cardinality $k$, a memoized recurrence constructs the ZDD $K_k$ of all $k$-element supports. The remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path in $R_k$ gives the next uncovered support. Only then is its packed mask converted to the ascending
index vector required for principal-matrix access.

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

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Reject a non-strict mode request.
3. Maintain one ZDD $C$ for the union of all retained certificate intervals.
4. For cardinality $k$, construct $R_k=K_k\setminus C$.
5. Extract the first support from $R_k$ and materialize its index vector.
6. Run Dickinson 2019's exact solve or nullspace branch.
7. Return `false` on a negative witness or nonnegative zero.
8. Otherwise compute $[L,U]$, union it into $C$, and subtract it from the active $R_k$.
9. Advance to $k+1$ when $R_k$ is empty.
10. Return `true` after $R_n$ is empty.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through the normal
Python analysis interface.

## Exact Representation And Termination

All matrix arithmetic uses FLINT arbitrary-precision integers. Certificate endpoints and extracted supports use the shared dynamic
packed support, so the model has no fixed-width support limit. ZDD node identifiers and variable positions use `size_t`.

Every ZDD operation is exact set algebra. Difference removes only supports covered by a mathematically valid Dickinson interval.
Each accepted iteration removes at least its current support from a finite $R_k$, so every cardinality and the complete traversal
terminate when memory is sufficient.

## CP and SCP classification

The selected-predicate `solve` path and combined `classify` path use the same support traversal. A nonsingular subset whose exact
solution is componentwise nonpositive rejects CP and SCP. A singular subset with a nonzero componentwise nonnegative null vector
proves that SCP is false. Strict-only mode stops there; CP and combined mode retain the vector's ordinary Dickinson interval and
continue, because a later support may still contain a negative witness. A completed traversal proves CP; it proves SCP exactly when
no boundary vector was found. Thus `both` is one traversal, not consecutive CP and SCP calls.

## Known Difficult Inputs

Decision diagrams can be exponentially large for an unfavorable family or variable order. This private experimental implementation
has hash-consing and per-operation memoization but deliberately has no garbage collector or dynamic variable reordering. Nodes made
unreachable by later unions and differences remain allocated until the matrix call ends. A certificate pattern with little shared
structure can therefore consume more memory than Dickinson 2019's flat interval list.

Building an exact-cardinality family and repeatedly updating a large diagram can also cost more than a direct coverage lookup on easy
or low-order matrices. The model is intended to test whether avoided support enumeration outweighs those costs, not to assume that a
ZDD is universally smaller.
