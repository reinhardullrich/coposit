# Multithreaded CBDD-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> with the Boolean-family traversal. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact CP/SCP experiment. It copies `cbdd_zed_dickinson` and changes how maximal Zed blocks and
uncovered Dickinson supports are scheduled, and when certificate intervals are inserted into the chain-reduced binary decision
diagram (CBDD).

The model supports a separately selected copositivity (CP) or strict-copositivity (SCP) test and can classify both predicates in one
traversal.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson's method examines principal supports in increasing cardinality. One exact calculation on a support can certify an entire
interval of larger supports. CBDD-Zed Dickinson stores the union of those intervals as one Boolean decision diagram and skips every
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

By default, the same workers also perform the rejection-only Zed stage. The Bron–Kerbosch root is split into independent search
states before the scan begins. Workers enumerate maximal cliques below those states and factor their Zed components independently.
Thus the model does not spend an unbounded serial prepass enumerating and checking every maximal Zed block before its worker threads
exist. This model alone has a switch that can omit the entire Zed stage when its maximal-clique enumeration would be counterproductive.

Batching deliberately performs some redundant exact solves: a certificate found early in a batch may cover a later support in that
same batch, but the later solve has already been scheduled. The batch is bounded so this loss of immediate pruning cannot grow
without limit.

## Name And Sources

The identifier is `multithreaded_cbdd_zed_dickinson`. “Multithreaded” is written out because this model uses threads inside one
matrix solve. It is not Python multiprocessing: Python multiprocessing runs separate matrix calls in separate processes.

The model is a complete isolated copy of `models/experiments/cbdd_zed_dickinson`. Its mathematical source is Peter J. C. Dickinson,
“A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–3, Lemma 6.2, and Theorem 6.5.

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

## Optional Rejection-Only Zed Stage

Before support enumeration, the model performs exactly the same Zed-block check as `cbdd_zed_dickinson`.

The stage is enabled by default. Set `COPOSIT_CBDD_ZED_SCAN=off` before the matrix call to bypass it, or set it to `on` explicitly.
Any other value is rejected. The switch belongs only to `multithreaded_cbdd_zed_dickinson`; it does not alter the serial
`cbdd_zed_dickinson` model or shared preprocessing.

Disabling the stage does not weaken correctness. The Zed test is only an early rejection certificate; the complete Dickinson
cardinality traversal that follows can still prove or refute the selected predicate. Disabling it merely trades away that possible
early rejection so the model can begin Dickinson enumeration immediately.

Build the graph

$$
\{i,j\}\in E(H_A)\iff a_{ij}\leq0.
$$

A principal submatrix (A_J) is a Z-matrix exactly when (J) is a clique of this graph. A pivoted Bron–Kerbosch traversal enumerates
all maximal cliques. Singleton cliques are left to the cardinality-one Dickinson stage.

A Bron–Kerbosch search state is a triple `(R, P, X)`:

- `R` is the clique already selected;
- `P` contains vertices that may still extend `R`; and
- `X` contains vertices already handled by an earlier sibling.

At a state, the implementation chooses the lowest-index vertex in `P`, or in `X` when `P` is empty, as pivot `q`. It branches on
the vertices in (P\setminus N(q)) in increasing index order. For one branch vertex (v), the child state is

$$
(R\cup\{v\},\;P\cap N(v),\;X\cap N(v)).
$$

After constructing that child, the parent moves (v) from `P` to `X` before constructing the next sibling. This is the same pivot,
child, and sibling rule as the copied serial model.

Before the workers begin, the coordinator applies this exact expansion breadth-first until it has at least (p) independent states,
where (p) is the worker count, or until no state can be split further. One expansion may produce more than (p) states; none are
discarded. Each state retains its complete `(R, P, X)` boundary, so its subtree is disjoint from every sibling subtree and their
union contains exactly the maximal cliques of the original root. Workers obtain these states through one atomic index and continue
the ordinary depth-first Bron–Kerbosch recursion locally. The small initial frontier preserves the source traversal's depth-first
behavior while exposing enough independent work to occupy the workers.

Within one maximal Zed block, split the strictly negative graph into connected components. Entries between different components are
zero, so each component can be tested independently. Exact fraction-free LDLT determines:

- positive definiteness for SCP; or
- positive semidefiniteness for CP.

For a symmetric Z-matrix, these conditions are equivalent to SCP and CP, respectively. A failing component therefore rejects the
selected predicate. During combined classification, a non-PSD component rejects both predicates; a singular PSD component rejects
only SCP.

Each worker owns its own fraction-free LDLT and matrix/component scratch storage. A failed block requests cancellation of the other
subtrees. A worker already inside exact arithmetic may finish its current checkpointed operation, but no failure is turned into a
positive result. In combined mode, workers record whether any accepted block is PSD but singular; the coordinator then clears the
global SCP result. An accepted Zed block is not inserted into the CBDD. This is intentionally the rejection-only policy of the
copied model.

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

The worker threads are created once before the Zed scan and remain alive through every cardinality. Each worker owns:

- one Zed-stage fraction-free LDLT object and one Dickinson-stage fraction-free LDLT object;
- separate principal-matrix scratch buffers for the two stages;
- reusable Zed index, reachability, queue, and negative-component buffers;
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

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-validated nonempty square symmetric matrix.
2. Select CP, SCP, or combined classification semantics.
3. Create the persistent exact worker threads.
4. If the Zed switch is on, build the nonpositive-entry graph and partition its Bron–Kerbosch root into at least one independent
   state per worker when the graph permits it.
5. If enabled, let the workers enumerate and check maximal Zed blocks; reject on a failed exact definiteness condition.
6. For cardinality (k=1,\ldots,n), construct (R_k=K_k\setminus C).
7. Read at most (B) distinct supports from the unchanged (R_k) root.
8. Let the workers evaluate those supports through a dynamic queue.
9. Return `false` for the selected predicate when a decisive witness is found.
10. Otherwise union the batch certificates, update (C), and subtract the union from (R_k) once.
11. Repeat until (R_k) is empty, then advance to (k+1).
12. Return `true` after cardinality (n) is exhausted.

Shared connected-component splitting and public pre-checks remain outside this model and are selected independently by the analysis
wrapper.

## Exactness, Termination, And Limits

All mathematical decisions use FLINT arbitrary-precision integers. Supports and interval endpoints use dynamically sized packed
storage, so the model has no fixed 64-bit support limit.

The coordinator is the only thread that modifies CBDD nodes, caches, roots, or the progress tracker. Zed workers increment one
atomic completed-block counter, which the waiting coordinator periodically transfers to progress output. The worker queue otherwise
shares only indices, cancellation/completion state, and immutable inputs. This avoids a concurrent unique table and preserves the
copied CBDD's deterministic Boolean semantics.

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

Maximal-clique enumeration remains exponential. Its independent frontier cannot exceed the actual branching exposed by the graph,
so a graph whose Bron–Kerbosch search stays on one long chain cannot use all workers. Conversely, a dense near-Zed graph can expose
many large maximal blocks whose exact component factorizations dominate the run even though every CPU is busy. Corpus matrix 9651,
the order-378 Motzkin–Straus matrix derived from the MANN_a27 Steiner-system graph, is a reproducible example of a heavy Zed scan.
For this structure, `COPOSIT_CBDD_ZED_SCAN=off` avoids enumerating maximal graph cliques that can only be rejection certificates.

Matrix 9651 is $Q_{125}$ for a graph with clique number $126$. On a clique $J$ of cardinality $k$, its principal matrix is

$$
(Q_{125})_J=125I-J,
$$

whose all-ones eigenvalue is $125-k$. A clique of size $125$ therefore gives a nonnegative zero and disproves SCP, while a clique of
size $126$ gives a negative vector and disproves CP. These witnesses are mathematically simple but combinatorially difficult to
reach. The shared whole-matrix pre-checks do not decide this indefinite connected matrix. With the optional Zed stage enabled, the
nonpositive-entry graph is the original MANN graph, and maximal Zed blocks are exactly its maximal cliques. Bron–Kerbosch may have to
enumerate an enormous number of other maximal cliques before it reaches a size-125 singular block or a size-126 indefinite block.

Disabling the Zed stage exposes a separate failure mode. Dickinson still visits supports from small to large, so absent an unusually
wide earlier certificate it cannot encounter the direct zero and negative witnesses before cardinalities 125 and 126. Long before
then, the union of low-cardinality certificate intervals can have poor sharing under the fixed matrix index order. The canonical CBDD
then creates far more internal nodes than there are emitted supports. The exact solves at these small cardinalities are cheap; the
single coordinator's unique-table, union, and difference work becomes the time and memory bottleneck, so adding exact-solve workers
does not cure this set-representation explosion. Graph symmetry does not guarantee CBDD compression because symmetric supports can
still occupy different ordered Boolean paths.

Finally, combining this model's internal threads with several outer Python worker processes multiplies the runnable thread count and
the per-worker exact scratch memory. Reference runs must budget both levels explicitly.
