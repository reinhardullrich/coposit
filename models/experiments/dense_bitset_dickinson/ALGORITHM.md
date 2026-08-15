# Dense-Bitset Dickinson

Classification: coposit-created exact copositivity and strict-copositivity experiment copied from `dickinson_final`.

## Idea In Plain Language

Dickinson assigns one exact certificate interval to many supports of a matrix. The ordinary implementation still constructs every
support and asks whether a retained interval covers it. This experiment instead allocates one bit for every member of the Boolean
lattice. A zero means that the support still requires Dickinson's calculation; inserting a certificate changes every newly covered
bit to one. This polarity permits a zero-initialized allocation and avoids writing the whole exponential bitmap before the first
principal calculation.

The bitmap is divided into cardinality layers. The solver finishes every support of size $k$ before entering size $k+1$, which is a
breadth-first traversal of the Boolean lattice. Within one layer it complements and scans 64-bit words and uses the first uncovered
bit, so a run of covered supports is jumped over without reconstructing or testing those supports.

## Name And Sources

“Dense-Bitset” states the only mathematical-control-flow change from Dickinson Final: the explicit $2^n$-bit representation of the
remaining support family. The exact solve, nullspace choice, sign decisions, certificate endpoints, witnesses, and mode-dependent
termination are copied from Dickinson Final.

The certificate is Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019),
15–37, [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The fixed-cardinality representation uses the standard colexicographic ranking formula documented in Frank Ruskey and Aaron
Williams, “Generating Combinations by Prefix Shifts,” COCOON 2005, and its journal successor “The Coolest Way to Generate
Combinations,” *Discrete Mathematics* 309 (2009), 5305–5320. The bitmap traversal and its use for Dickinson intervals are coposit
implementation choices; no matching published Dickinson implementation was found.

## Dense Boolean-Lattice Layout

There are $2^n$ subsets of $[n]$, including the empty set, and therefore exactly $2^n$ bitmap bits. Bit zero for the empty support is
marked covered during construction because Dickinson uses only nonempty supports. The remaining bitmap starts at zero. Its logical
size is fixed immediately, while operating systems that provide lazily backed zero allocations need not create every physical page
until the solver writes that page.

The bitmap stores the cardinality layers consecutively. Layer $k$ has

$$
\binom nk
$$

bits and begins at

$$
o_k=\sum_{j=0}^{k-1}\binom nj.
$$

For a support

$$
I=\{c_1<c_2<\cdots<c_k\},
$$

using zero-based matrix indices, its zero-based colexicographic rank is

$$
r(I)=\sum_{j=1}^{k}\binom{c_j}{j}.
$$

Its unique bitmap position is $o_k+r(I)$. Pascal's triangle is constructed once with exact `size_t` additions. The same table
unranks a selected bitmap position into the ascending index vector required for the principal matrix.

## Breadth-First Traversal

For $k=1,2,\ldots,n$, the solver scans only the contiguous range for layer $k$. A word containing only covered bits is skipped in one
operation. After complementing a word, its least significant one identifies the next surviving support. Colexicographic order
agrees with increasing numeric support-mask order, so the exact calculations have the same deterministic order as Dickinson Final
among supports of equal cardinality.

No queue of supports is stored. The bitmap itself is the complete frontier: bits in earlier layers are finished, one bits in the
current or later layers are covered, and zero bits remain eligible.

## Clearing A Dickinson Interval

For a computed vector $u$, define

$$
L=\operatorname{supp}(u),
\qquad
U=N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson covers exactly the Boolean interval

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

Indices in $L$ are fixed present, indices outside $U$ are fixed absent, and the $d=|U|-|L|$ remaining indices are free. A recursive
binary enumeration visits the resulting $2^d$ supports. It computes each support's cardinality and colex rank incrementally and
marks the corresponding bitmap bit covered. Interval endpoints are single 64-bit masks: the bitmap itself is addressable only when
$n$ is smaller than the number of bits in `size_t`, so this introduces no additional order restriction. Indices outside $U$ are
absent by definition and are skipped by the recursion. Already-covered bits remain covered and are not counted twice.

Traversal never moves backward. When a certificate is inserted, the current support and every earlier bitmap position are therefore
irrelevant and are not written. A recursive branch is discarded immediately when its largest possible cardinality is smaller than
the current layer. Exact progress reporting counts the processed support once and every newly covered future bit as a skipped
support.

## Exact Dickinson Calculation

For the emitted support $I$, form the principal matrix $C=A_I$.

If $C$ is nonsingular, solve

$$
Cw=\mathbf1
$$

with the shared exact fraction-free LDLT implementation. If $w\leq0$, then the embedded vector $-w\geq0$ has negative quadratic
value, so the matrix is not copositive and traversal stops.

If $C$ is singular, recover one exact nullspace vector and orient it to contain a positive component. A nonnegative oriented vector
is a nonnegative zero. Strict mode stops with `false`; ordinary or combined mode records strict failure, retains the valid
Dickinson interval, and continues the non-strict certificate.

Every other vector produces the exact $L$ and $U$ above. Products, signs, factorization, solves, and nullspace recovery use
arbitrary-precision integers. For every index in the processed support $I$, the principal calculation already proves that $(Au)_i$
is either the positive solve denominator or zero. Thus $I\subseteq U$ is inserted directly. The implementation evaluates $(Au)_i$
only for $i\notin I$ and skips zero coefficients of $u$; these are representation-only reductions of exact arithmetic.
The solve denominator and row-product accumulator are retained as reusable exact scalar scratch instead of allocating them for every
processed support.

## Decision Modes

`solve(A, strictly_copositive)` returns `false` on the first negative witness or nonnegative zero. Completing all bitmap layers proves
strict copositivity.

`solve(A, copositive)` returns `false` only on a negative witness. It retains nonnegative-zero certificates and completes the
copositivity certificate.

`classify(A)` performs the ordinary traversal once, remembers whether a nonnegative zero occurred, and returns both predicates. Its
only possible results are `{false,false}`, `{true,false}`, and `{true,true}`.

## Memory Limit

The packed allocation uses

$$
8\left\lceil\frac{2^n}{64}\right\rceil
$$

bytes. Before allocation the model accepts exactly one of these environment variables:

- `COPOSIT_DENSE_BITSET_MAX_N`: largest permitted matrix order;
- `COPOSIT_DENSE_BITSET_MAX_GIB`: largest permitted bitmap allocation in binary GiB.

If neither is set, the default is one GiB. Setting both, using a nonpositive or nonintegral value, exceeding the configured limit,
or requesting a dimension whose $2^n$ bits cannot be addressed by `size_t` raises an explicit error rather than returning a
copositivity decision. A max-order setting is an intentional authorization to attempt that allocation; operating-system allocation
failure remains possible.

## Complete Decision Flow

1. Validate the configured dense-bitmap limit and allocate exactly one packed Boolean-lattice bitmap.
2. Build binomial coefficients and cardinality-layer offsets.
3. For cardinalities $k=1,\ldots,n$, find the next uncovered bit in layer $k$.
4. Unrank that position into the support indices and perform Dickinson's exact principal calculation.
5. Return a negative decision immediately on the mode's witness condition.
6. Otherwise construct $[L,U]$ and mark every future represented support in that interval covered; do not write the current or any
   earlier bitmap position.
7. Continue scanning after the current position; covered runs require no support construction or certificate search.
8. Return the positive decision after the final layer contains no uncovered bits.

The independent connected-component and pre-check pipeline remains outside the model and is selected through the Python analysis
interface.

## Termination And Fidelity

There are finitely many bitmap positions. The traversal cursor advances after every processed support and never moves backward;
certificates additionally cover future positions. Every rejected matrix terminates with an exact witness. Hence the algorithm
terminates when sufficient memory and time are available.

The bitmap is a lossless representation of Dickinson coverage. Marking a bit changes neither the certificate theorem nor any exact
principal calculation; it replaces repeated interval-containment queries with explicit set-family membership. Cooperative timeout
checks occur between supports, matrix rows, factorization work, and batches of 4,096 interval bits.

## Known Difficult Inputs

Memory doubles whenever $n$ increases by one. The representation is therefore useful only at modest order, regardless of how few
supports ultimately require exact processing.

Inserting a certificate costs $2^d$ bit addresses for $d=|U|-|L|$. Wide certificates are excellent for later traversal but expensive
to materialize; a certificate covering nearly the whole lattice can spend substantial time clearing bits that a symbolic BDD or ZDD
would describe compactly. Conversely, narrow certificates are cheap to insert but leave many supports for exact factorization.

The bitmap reserves its complete logical address range before the first principal solve, but it does not fill the complete range.
As certificates accumulate, touched pages can still approach the full configured allocation. This model is therefore an experiment
in direct membership and predictable packed storage, not a replacement for the symbolic decision-diagram models at larger order.
