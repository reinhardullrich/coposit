# Multithreaded CBDD Dickinson

Classification: coposit-created exact CP/SCP experiment. It copies `cbdd_dickinson` and changes how uncovered Dickinson supports
are scheduled and when certificate intervals are inserted into the chain-reduced binary decision diagram (CBDD).

The model supports a separately selected copositivity (CP) or strict-copositivity (SCP) test and can classify both predicates in one
traversal.
Analysis and reference-run interfaces default to combined classification when the mode is omitted.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing is an external caller concern; `model::solve` starts directly with this flow.

## Idea In Plain Language

Dickinson's method examines principal supports in increasing cardinality. One exact calculation on a support can certify an entire
interval of larger supports. CBDD Dickinson stores the union of those intervals as one Boolean decision diagram and skips every
support already covered by that union.

The serial model repeats this cycle:

1. extract one uncovered support;
2. solve its exact principal system;
3. insert its certificate into the CBDD; and
4. subtract the certificate from the current cardinality.

This experiment performs the expensive principal calculations in parallel. It extracts a small batch from one unchanged CBDD root,
lets several persistent C++ worker threads solve the supports, unions every valid certificate from the batch, and updates the CBDD
once. The CBDD itself remains single-owned by the coordinator thread. No lock is placed inside its recursive union, difference, or
unique-table operations.

Batching deliberately performs some redundant exact solves: a certificate found early in a batch may cover a later support in that
same batch, but the later solve has already been scheduled. The batch is bounded so this loss of immediate pruning cannot grow
without limit.

## Name And Sources

The identifier is `multithreaded_cbdd_dickinson`. “Multithreaded” is written out because this model uses threads inside one
matrix solve. It is not Python multiprocessing: Python multiprocessing runs separate matrix calls in separate processes.

The model is a complete isolated copy of `models/hadeler-based/cbdd_dickinson`. Its mathematical source is Peter J. C. Dickinson,
“A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2.

The decision-diagram representation follows:

- Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions on Computers* C-35(8), 1986,
  677–691; and
- Randal E. Bryant, “Chain Reduction for Binary and Zero-Suppressed Decision Diagrams,” 2017, arXiv `1710.06500`.

The local CBDD is a reconstruction of the published reduction, cofactor, union, and difference rules. No external decision-diagram
code is copied and no new dependency is used. The scheduler uses standard C++ threads and synchronization; POSIX affinity is used
only to place those threads on explicitly selected CPUs.

The batching and parallel schedule are coposit changes. Dickinson's systems, sign tests, witnesses, and certificate formula are not
changed.

## Mathematical Problem

For a symmetric matrix (A\in\mathbb{Q}^{n\times n}), CP means

$$
x^T A x\geq0\qquad\text{for every }x\geq0,
$$

and SCP means

$$
x^T A x>0\qquad\text{for every nonzero }x\geq0.
$$

The parser clears one common positive denominator, so the model receives an integer matrix. Positive scaling does not change either
predicate.

A support is an index set (I\subseteq\{1,\ldots,n\}). The model visits supports by increasing cardinality
(1,2,\ldots,n).

## Dickinson Calculation On One Support

For one uncovered support (I), copy the lower triangle of (A_I) and factor it exactly.

If (A_I) is nonsingular, solve

$$
A_Iu=\mathbf1.
$$

The fraction-free solver stores integer numerators for (u) and a positive common denominator. Multiplying by that denominator does
not change any sign and is therefore omitted from the certificate calculation.

If (A_I) is singular, obtain one exact nonzero nullspace vector and orient it so that it has a positive component.

Then apply the unchanged Dickinson decisions:

- if (u\leq0), the embedded vector (-u\geq0) is a negative witness, so both CP and SCP fail;
- if the singular vector satisfies (u\geq0), it is a nonnegative zero, so SCP fails but CP may continue; and
- otherwise construct a Dickinson interval.

Embed (u) in the full coordinate space and define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

The exact certificate covers

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

Every component of (Au) is accumulated with arbitrary-precision integer arithmetic.

## CBDD State

The coordinator owns two Boolean families:

- (C), the union of all accepted Dickinson intervals; and
- (R_k=K_k\setminus C), where (K_k) is the family of every support of cardinality (k).

Node zero is false and node one is true. Every nonterminal node stores `(top, bottom, low, high)`. A range from `top` through
`bottom` compresses consecutive BDD nodes whose high edges are identical. Nodes are canonicalized by a unique table. Union and
difference use memoized range-aware APPLY recursions.

A skipped BDD variable is a genuine don't-care. A satisfying-assignment enumerator must therefore branch over both values of a
skipped variable. Inside a chain, taking low advances to the next variable, while taking high follows the common high child. This
experiment uses that direct read-only traversal to enumerate distinct supports. It does not remove each selected support from
(R_k), and it does not build temporary singleton certificates merely to advance the iterator.

“Read-only” here means the selected root and the represented Boolean function do not change during enumeration. The node table is
not touched by the enumerator.

## Batch Size

Let (p\geq1) be the worker count and (n) the matrix order. The target batch size is the largest whole worker wave not exceeding
(5n):

$$
B=
\begin{cases}
5n, & 5n<p,\\
5n-(5n\bmod p), & 5n\geq p.
\end{cases}
$$

The actual batch contains

$$
\min(B,\text{supports reached before }R_k\text{ is exhausted})
$$

supports. The model does not count (|R_k|) first; enumeration simply stops at false or at (B). Rounding avoids a deliberately
partial final worker wave when at least (B) supports remain. It cannot equalize exact factorization times, so workers take new
supports dynamically from one atomic index rather than receiving fixed equal chunks.

On this machine, the default is seven worker threads pinned consecutively to CPUs 3–9. `COPOSIT_CBDD_WORKERS` may select another
positive count, and `COPOSIT_CBDD_FIRST_CPU` may select the first consecutive CPU. Invalid values or unavailable CPUs fail
explicitly. These model-specific settings exist because the appropriate placement depends on whether an outer Python process pool
is also being used.

## Persistent Worker Stage

The worker threads are created once before the first cardinality and remain alive through every cardinality. Each worker owns:

- one fraction-free LDLT object;
- one principal-matrix scratch buffer;
- one solution scratch buffer;
- one full (Au) product buffer; and
- one index-vector scratch buffer.

Nothing in this mutable arithmetic state is shared. The original matrix and the batch supports are read-only.

For each support, a worker returns one of three exact outcomes:

1. a negative witness;
2. a nonnegative zero, together with its valid Dickinson interval; or
3. another valid Dickinson interval.

In SCP mode, either witness stops the batch. In CP mode, only a negative witness stops it. In combined mode, a nonnegative zero
clears the SCP result while processing continues for CP.

An exception in any worker is caught, recorded once, and rethrown by the coordinator after all active workers leave the batch. This
includes cooperative timeouts. An exception is never converted to a negative classification.

## Batched CBDD Update

If no rejecting witness was found, the coordinator forms the Boolean union

$$
Q=\bigcup_{u\text{ returned by the batch}}[L(u),U(u)].
$$

It then performs exactly one state update:

$$
C\leftarrow C\cup Q,
\qquad
R_k\leftarrow R_k\setminus Q.
$$

This is mathematically equivalent to inserting those certificates one at a time because Boolean union is associative and
commutative, and

$$
R\setminus(Q_1\cup\cdots\cup Q_m)
=(((R\setminus Q_1)\setminus Q_2)\cdots)\setminus Q_m.
$$

The difference is only scheduling. A support solved redundantly inside a batch still yields a valid certificate, so it cannot remove
an uncertified support.

Waiting for an entire cardinality would be unsafe for performance: a certificate calculated from a (k)-element support may have
(|L(u)|<k) and can therefore cover other (k)-element supports. The bounded (5n) batch retains most of that same-cardinality
feedback while exposing enough independent exact calculations to the workers.

## Safe Cardinality Expiration

Each returned interval is tagged with $u=|U|$ and also united into its expiry bucket $E_u$ before the batch union is installed.
Before the model starts cardinality $k$, it performs

$$
C\leftarrow C\setminus\bigcup_{u<k}E_u
$$

and clears those buckets. This cannot change any present or future decision: every $J\in[L,U]$ satisfies
$|J|\leq|U|=u$, so an interval with $u<k$ contains no support of cardinality $k$ or larger. Intersections with intervals that
remain live are harmless for the same reason; every removed support is below the traversal frontier. Bucketing happens per returned
certificate rather than per batch because one batch may contain several different upper cardinalities.

Expiration changes only the live decision-diagram roots. The private arena and unique table retain already allocated nodes until the
matrix call ends, so this reduces later union and difference operands but is not garbage collection.

## Exactness, Termination, And Limits

All mathematical decisions use FLINT arbitrary-precision integers. Supports and interval endpoints use dynamically sized packed
storage, so the model has no fixed 64-bit support limit.

The coordinator is the only thread that modifies CBDD nodes, caches, roots, or the diagnostics tracker. The worker queue shares only
indices, cancellation/completion state, and immutable inputs. This avoids a concurrent unique table and preserves the copied CBDD's
deterministic Boolean semantics.

Every successful batch removes at least its originally selected supports because each accepted Dickinson certificate covers the
support from which it was constructed. The finite traversal therefore terminates when sufficient time and memory are available.

## Known Difficult Inputs

The worst case still contains exponentially many supports. Parallel workers reduce elapsed time per batch but do not change that
combinatorial bound.

Small or easily pruned matrices can be slower than the serial model because thread creation, wake-up, and certificate collection are
fixed overheads. A batch can also waste up to its bounded number of exact solves when its first few certificates cover most of the
remaining batch.

Exact support costs are uneven. Singular nullspace work and integer coefficient growth can make one support much slower than its
neighbors, so rounding the batch to a worker multiple does not guarantee equal finishing times.

The CBDD itself remains serial. Inputs whose interval union has poor structure can spend most of their time or memory in node
construction, hashing, union, or difference; worker parallelism cannot accelerate that stage. Unreachable nodes are retained until
the matrix call ends because the private CBDD has no garbage collector or dynamic variable reordering.

Matrix 9651 is $Q_{125}$ for a graph with clique number $126$. On a clique $J$ of cardinality $k$, its principal matrix is

$$
(Q_{125})_J=125I-J,
$$

whose all-ones eigenvalue is $125-k$. A clique of size $125$ therefore gives a nonnegative zero and disproves SCP, while a clique of
size $126$ gives a negative vector and disproves CP. These witnesses are mathematically simple but combinatorially difficult to
reach. Dickinson visits supports from small to large, so absent an unusually wide earlier certificate it cannot encounter these
direct witnesses before cardinalities 125 and 126. Long before then, the union of low-cardinality certificate intervals can have
poor sharing under the fixed matrix index order. The canonical CBDD can create far more internal nodes than emitted supports. The
single coordinator's unique-table, union, and difference work then becomes the time and memory bottleneck, so adding exact-solve
workers does not cure the set-representation explosion.

Finally, combining this model's internal threads with several outer Python worker processes multiplies the runnable thread count and
the per-worker exact scratch memory. Reference runs must budget both levels explicitly.
