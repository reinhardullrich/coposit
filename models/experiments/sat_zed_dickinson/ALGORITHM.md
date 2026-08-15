# SAT-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> by building the SAT cardinality representation. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact copositivity and strict-copositivity experiment. It preserves the mathematical decisions of
CBDD-Zed Dickinson, but replaces the decision-diagram representation of the remaining support family with one incremental SAT
instance.

Public mode boundary: the model supports individually selected `copositive` and `strictly_copositive` modes and combined
classification of both predicates in one traversal.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson examines principal supports in increasing cardinality. A successful exact calculation on one support does more than settle
that support: it proves that every support in a Boolean-lattice interval $[L,U]$ can be skipped. The earlier decision-diagram models
store the union of these intervals as a BDD, ZDD, CBDD, or CZDD.

This experiment asks a SAT solver for an uncovered support instead. Matrix index $i$ is represented by a Boolean variable $x_i$;
$x_i=true$ means that $i$ belongs to the support. Every Dickinson interval is excluded by one clause. One shared cardinality network
restricts SAT solutions to the current support size. The same solver remains alive for the whole matrix, so all interval clauses and
all clauses learned by SAT remain available at later supports and cardinalities.

SAT never decides a matrix inequality. It only enumerates a finite Boolean family. Every copositivity decision, witness, sign test,
factorization, and matrix-vector product remains exact arbitrary-precision arithmetic.

Before SAT enumeration, the model retains CBDD-Zed Dickinson's rejection-only maximal-$Z$-matrix stage. It can cheaply reject some
matrices but never accepts one or adds covered supports.

## Name, Sources, And Classification

The identifier is `sat_zed_dickinson`. “SAT” names the alternative Boolean-family representation. “Zed” makes the preliminary
$Z$-matrix rejection stage visible. “Dickinson” identifies the unchanged exact certificate mathematics.

The model was made as an independent copy of `models/experiments/cbdd_zed_dickinson`. The principal solve, singular branch, witness
tests, interval endpoints, maximal-Zed scan, Motzkin–Straus bypass, and cardinality-first traversal were retained. Only the family
representation and the order in which supports of one cardinality may be returned were changed.

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–3. The Zed stage uses
Dickinson's Lemma 6.2 and Theorem 6.5.

The exact-cardinality encoding uses Kenneth E. Batcher's bitonic sorting network from “Sorting Networks and Their Applications,”
*AFIPS Spring Joint Computer Conference* 32 (1968), 307–314. Incremental SAT solving is provided by CaDiCaL 2.2.1, pinned to upstream
tag `rel-2.2.1` from <https://github.com/arminbiere/cadical>. No CaDiCaL source is copied into this model directory; CMake fetches and
builds that exact revision.

## Dickinson Intervals As Single Clauses

For an exact vector $u$, let

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson's certificate covers precisely

$$
[L,U]=\{I:L\subseteq I\subseteq U\}.
$$

A Boolean assignment belongs to this interval exactly when every $x_i$ with $i\in L$ is true and every $x_i$ with $i\notin U$ is
false. Negating that conjunction gives the blocking clause

$$
\bigvee_{i\in L}\neg x_i
\;\lor\;
\bigvee_{i\notin U}x_i.
$$

Therefore one certificate always costs exactly one persistent SAT clause. Its length is

$$
|L|+(n-|U|)=n-(|U|-|L|)=n-d,
$$

where $d$ is the number of free indices in the interval. A wide interval gives a short, strongly propagating clause; a narrow
interval gives a long clause. The implementation stores no separate interval object and performs no later linear scan over
certificates.

The current support lies in the interval that it generates, so the new clause blocks it immediately. This guarantees that the next
SAT solution, if one exists, is a different uncovered support.

## One Cardinality Network For Every Layer

The SAT instance contains one input variable for each matrix index. It builds one Batcher bitonic sorting network at construction
time. If $n$ is not a power of two, inputs are padded to the next power of two with one shared variable constrained to false.

Each Boolean comparator receives $a,b$ and creates

$$
h=a\lor b,
\qquad
\ell=a\land b.
$$

The equivalences are encoded by six clauses, three for each output. A bitonic network sorts all outputs in descending truth order.
Consequently output $y_j$ is true exactly when at least $j+1$ original support variables are true.

For cardinality $k$, SAT is called under the temporary assumptions

$$
y_{k-1}=true,
\qquad
y_k=false\quad\text{when }k<n.
$$

These two bounds mean exactly $k$ selected indices. The network is built only once; changing cardinality adds no permanent clauses.
CaDiCaL removes the assumptions after each call but safely retains learned clauses that are valid for the persistent formula.

For padded size $p$, the network uses $O(p\log^2p)$ comparators. This is a one-time cost. It avoids an $O(n^2)$ sequential unary
counter while preserving strong propagation and constant-size cardinality selection.

## Incremental Enumeration

There is one CaDiCaL solver for one complete model call:

1. Build the cardinality network.
2. Select cardinality $k$ with assumptions. CaDiCaL's built-in `sat` configuration favors the many satisfiable intermediate calls.
   Incremental lazy backtracking retains the compatible part of the previous search trail when assumptions or a new interval clause
   force only a partial retreat.
3. Ask SAT for one satisfying assignment.
4. Convert the first $n$ truth values into the ascending support-index vector.
5. Run the exact Dickinson calculation.
6. Add its interval-blocking clause.
7. Repeat the SAT call with the same cardinality assumptions.
8. When SAT returns unsatisfiable, advance to $k+1$.

An unsatisfiable result means every support of that cardinality is covered by the accumulated exact certificates. When the final
cardinality is unsatisfiable, the complete Dickinson search has finished.

The solver may return supports of the same cardinality in a different order from Dickinson Final or the decision-diagram variants.
This does not change correctness: all smaller cardinalities have already been exhausted, and every omitted same-cardinality support
is omitted only because a valid accumulated interval clause covers it.

## Rejection-Only Zed Stage

Before constructing the SAT network, form the compatibility graph

$$
\{i,j\}\in E(H_A)\iff a_{ij}\leq0.
$$

Its cliques are exactly the principal $Z$-matrix supports. A Bron–Kerbosch search enumerates maximal cliques. Each maximal block is
split into connected components of its strictly negative graph; cross-component entries are zero. Exact fraction-free LDLT then
tests every component for positive definiteness in strict mode or positive semidefiniteness in ordinary mode.

A component failing the required condition rejects the matrix. A passing block is discarded and contributes no SAT clause. This is
deliberately only an early negative test: complete Dickinson enumeration still supplies every positive result.

The exact Motzkin–Straus two-value graph pattern bypasses the maximal-Zed search because its maximal blocks are graph cliques and the
enumeration can be pure overhead. `COPOSIT_SAT_ZED_SCAN=off` also disables the stage for isolation experiments; `on` is the default.
Neither bypass changes the later SAT or Dickinson rules.

## Exact Dickinson Calculation

For a SAT support $I$, copy the principal matrix $A_I$ and factor it with the shared exact fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve

$$
A_Iu=\mathbf1
$$

using integer numerators and one positive common denominator. If $u\leq0$, the embedded vector $-u$ is a nonnegative negative
witness and the selected copositivity predicate fails.

If $A_I$ is singular, construct one exact nullspace vector and orient it to contain a positive entry. If the oriented vector is
nonnegative, it is a nonnegative zero. Strict copositivity therefore fails; non-strict copositivity permits the equality and continues.

Every surviving vector generates $L(u)$ and $U(u)$. The full product $Au$ is computed with arbitrary-precision integers, and the
corresponding single blocking clause is added to SAT.

In combined mode, the traversal begins with both predicates provisionally true. A nonnegative zero clears only strict copositivity;
a negative witness clears both and stops. Thus one traversal returns one of the only possible pairs: `(true,true)`, `(true,false)`,
or `(false,false)`.

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Unless bypassed, enumerate maximal Zed blocks and reject on an exact definiteness failure.
3. Build the one-time SAT cardinality network only after the rejection stage passes.
4. For $k=1,\ldots,n$, impose exact-cardinality assumptions on the persistent SAT solver.
5. If the assumed formula is unsatisfiable, advance to $k+1$.
6. Otherwise extract the SAT assignment's support and run Dickinson's exact nonsingular or singular calculation.
7. Reject on a negative witness, and in strict mode also reject on a nonnegative zero.
8. Add the valid interval's single blocking clause and return to step 4 without rebuilding SAT.
9. Return the positive result after all cardinalities are unsatisfiable.

The shared connected-component and pre-check pipeline remains outside the model and is selected independently through the Python
analysis interface.

## Representation, Timeouts, And Termination

Matrix arithmetic uses FLINT arbitrary-precision integers. Support endpoints use the shared multiword packed support, so the model
has no 63-bit dimension limit. SAT literals use CaDiCaL's signed `int` interface; construction checks its variable limit and reports
overflow rather than truncating an index.

CaDiCaL is connected to the project's cooperative timeout flag through its terminator interface. Cardinality-network construction,
Zed enumeration, exact factorization, and matrix products retain the normal project timeout checkpoints. A timeout remains
unresolved and is never converted into a negative classification.

SAT clauses are exact Boolean logic. Every completed iteration blocks at least the emitted support. The finite support family
therefore terminates when sufficient time and memory are available.

## Known Difficult Inputs

The representation is compact per certificate but the number of certificates can still be exponential. A run that emits many
supports performs one incremental SAT solve and one exact Dickinson system per support. Narrow intervals produce long clauses and
little pruning, so the persistent clause database can become large.

Highly symmetric support families can also be difficult for SAT: many assignments remain interchangeable, while this experiment
does not add symmetry-breaking clauses or dynamically permute matrix coordinates. CaDiCaL's learned clauses may help substantially,
but its runtime can be irregular and depends on the accumulated clause structure. The fixed sorting network adds a nontrivial
$O(p\log^2p)$ startup and memory cost even when an early exact witness is found.

The rejection-only Zed stage has its own exponential worst case because a graph can contain exponentially many maximal cliques;
Dickinson's Example 6.7 gives this obstruction. A large connected Zed block also requires a large exact factorization. Passing blocks
do not prune SAT, so an unproductive Zed scan is pure overhead before the complete traversal.
