# Clingo-SAT-Zed Dickinson

> **Current model boundary:** the Z-matrix stage that gave this experiment its name now runs once in shared preprocessing, not
> inside `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts
> by building the clingo support representation. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact copositivity experiment. It preserves Dickinson's exact certificate mathematics and the
rejection-only maximal-Zed stage used by the other Zed-Dickinson experiments, but delegates cardinality-ordered support enumeration
and interval blocking to clingo and its clasp solver.

The model supports separately selected copositivity (CP) and strict copositivity (SCP), and it can classify both predicates in one
traversal.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson processes principal supports in increasing cardinality. Solving one exact system on a support can certify not merely that
support but every support in a Boolean-lattice interval. The main computational problem is therefore:

> Return an uncovered support of cardinality $k$, accept one new interval that must be excluded, and continue without restarting or
> explicitly visiting any support already covered by the accumulated intervals.

This model represents matrix index $i$ by a Boolean atom `selected(i)`. A true atom means that $i$ belongs to the support. Gringo
grounds one exact-cardinality constraint for each layer $k$. Clasp enumerates completed answer sets for that layer. Whenever a
completed answer set yields a valid Dickinson certificate, one clause excludes the certificate's entire interval. Clasp installs
the clause directly into its current backtracking search and retains it for later cardinalities.

Clingo and clasp perform no matrix classification. They only maintain the finite Boolean support family. Every matrix solve,
nullspace construction, sign test, product, witness decision, and interval endpoint remains exact arbitrary-precision arithmetic.

Before the Boolean traversal, the model retains the rejection-only maximal-Zed scan. That stage may reject a matrix but never
accepts one and never contributes support clauses.

## Name, Sources, And Classification

The identifier is `clingo_sat_zed_dickinson`:

- **Clingo** identifies the public multi-shot API and ASP grounder used to describe the support family.
- **SAT** identifies the clause representation used by clasp for Dickinson intervals.
- **Zed** makes the preliminary maximal-$Z$-matrix rejection stage visible.
- **Dickinson** identifies the unchanged exact certificate mathematics.

This is an independent copy of `sat_zed_dickinson`, not a literature baseline. It replaces that experiment's hand-built Batcher
network and CaDiCaL solve loop with clingo's native cardinality grounding and clasp's backtracking answer-set enumeration. The
principal systems, singular branch, exact witnesses, interval endpoints, maximal-Zed scan, Motzkin–Straus bypass, and increasing
cardinality order are retained.

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–3. The Zed stage uses
Dickinson's Lemma 6.2 and Theorem 6.5.

The Boolean engine is clingo 5.8.2, pinned to upstream tag `v5.8.2` from <https://github.com/potassco/clingo>. That release contains
clasp 3.4.1. CMake builds the static C++ libraries without the command-line applications, examples, Python binding, Lua binding, or
upstream test suite. No clingo or clasp source is copied into this model directory.

The integration was checked against the pinned implementation, not only its public declarations. A model-callback clause passes
through `clingo_solve_control_add_clause`, `ClingoModel::addClause`, and clasp's enumeration-context `commitClause`. Disabling
clingo's enumeration assumption retains these clauses after one cardinality solve finishes. This is the maintained public route;
the model does not reproduce clasp's backtracking or clause database.

## Dickinson's Interval

Let $A\in\mathbb Z^{n\times n}$ be symmetric. For an embedded exact vector $u\in\mathbb Q^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\neq0\}
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

When Dickinson's conditions hold, the vector certifies every support in

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

For Boolean support variables $x_1,\ldots,x_n$, a support belongs to $[L,U]$ precisely when

$$
\left(\bigwedge_{i\in L}x_i\right)
\land
\left(\bigwedge_{j\notin U}\neg x_j\right)
$$

holds. Negating this conjunction gives one blocking clause:

$$
\bigvee_{i\in L}\neg x_i
\;\lor\;
\bigvee_{j\notin U}x_j.
$$

The clause is false exactly on the supports in $[L,U]$, so it removes the complete interval and nothing else. Its length is

$$
|L|+n-|U|=n-(|U|-|L|).
$$

Writing $d=|U|-|L|$, a wide certificate has large $d$ and therefore a short clause. A narrow certificate has a long clause and
removes fewer supports.

The current support always belongs to its generated interval. Consequently every completed callback adds a clause that invalidates
the current answer set, and clasp must backtrack to a genuinely uncovered support.

## Native Exact-Cardinality Layers

The base ASP program contains one independent choice atom for each matrix index:

```text
{ selected(0..n-1) }.
```

For each $k=1,\ldots,n$, the model grounds one guarded constraint:

```text
#external active(k).
:- active(k), #count { Index : selected(Index) } != k.
```

Only `active(k)` is true while layer $k$ is being enumerated. Earlier guards are false. Therefore a completed answer set in the
current solve contains exactly $k$ selected atoms. The grounded constraints remain in the same clingo control object; advancing a
layer changes only the external guard.

This deliberately uses clingo's native aggregate translation. The model does not construct a sorting network, sequential counter,
or custom cardinality propagator. Grounding all $n$ layers retains $O(n^2)$ aggregate-literal references in the worst case, but it
keeps the implementation small and lets clasp use its maintained weight/cardinality machinery.

## Why The Completed-Model Callback Is Required

Clingo offers both propagator callbacks and model callbacks. They are not interchangeable here.

A propagator's “total” check can receive clasp's compact internal assignment with some program atoms still free. Clasp can reconstruct
their truth values when it forms an answer set, but the propagator assignment itself need not identify one exact support. Treating
free atoms as false would therefore process the wrong cardinality and could omit supports.

The model instead uses `SolveEventHandler::on_model`. At that point clingo exposes the reconstructed answer set, and
`Model::is_true` gives the exact truth value of every `selected(i)` atom. The callback then:

1. extracts the ascending support-index vector;
2. verifies that its size is the active cardinality;
3. performs the exact Dickinson calculation;
4. adds the resulting interval clause through `Model::context()`; and
5. returns to the same clasp enumeration.

This is also the API specifically intended for adding clauses during model enumeration. No C++ exception is allowed to become a
classification; callback exceptions are transported by clingo's C++ wrapper and rethrown at the solve handle.

## Clause Persistence Across Cardinalities

By default, clingo treats clauses added during model enumeration as belonging to one solve step. This model explicitly disables the
enumeration assumption once, before solving:

```text
control.enable_enumeration_assumption(false)
```

The interval clauses are then retained in the same clasp context after the active cardinality changes. The model explicitly selects
clasp's `bt` enumeration mode instead of relying on clingo's current `auto` default. It neither stores a second external clause list
nor reinserts old clauses at a new layer.

This property is tested directly with an identity matrix. Its $n$ singleton certificates cover every nonempty support. The model
must therefore process exactly the $n$ singleton supports and process none at cardinalities $2,\ldots,n$. If clauses were lost at a
solve boundary, that test would immediately enumerate larger supports.

## Exact Dickinson Calculation

For a completed support $I$, copy the lower triangle of the principal matrix $A_I$ and factor it with the shared exact
fraction-free LDLT implementation.

### Nonsingular principal matrix

If $A_I$ is nonsingular, solve

$$
A_Iu=\mathbf1.
$$

The factorization returns integer numerators and one positive common denominator. Only signs and zero/nonzero support are needed, so
the denominator is not expanded into rational matrix storage.

If $u\leq0$, then the embedded vector $-u\geq0$ is a negative witness in Dickinson's decision rule, and the selected predicate
fails.

### Singular principal matrix

If $A_I$ is singular, construct one exact nullspace vector

$$
A_Iu=0
$$

and orient it so that it has a positive entry. If the oriented vector is nonnegative, it is a nonnegative zero direction. Strict
copositivity fails. Non-strict copositivity permits this equality and continues.

### Certificate construction

For every surviving vector, compute $L=\operatorname{supp}(u)$. Then compute the full product $Au$ with exact integer multiply-adds.
Every row with nonnegative product belongs to $U$; every row with negative product lies outside $U$. The implementation builds the
clasp clause directly:

- append `not selected(i)` for every $i\in L$;
- append `selected(j)` for every $j\notin U$.

It does not materialize separate packed lower and upper supports merely to translate them back into literals.

In combined mode, CP and SCP begin provisionally true. A nonnegative zero clears only SCP. A negative witness clears both and stops.
Thus the only possible combined results are `(true,true)`, `(true,false)`, and `(false,false)`.

## Rejection-Only Zed Stage

Before clingo is constructed, form the compatibility graph

$$
\{i,j\}\in E(H_A)\iff a_{ij}\leq0.
$$

Its cliques are exactly the supports whose principal submatrices are $Z$-matrices. Bron–Kerbosch enumeration returns maximal
cliques. Each maximal block is split into connected components of its strictly negative graph; entries between different components
are zero. Exact fraction-free LDLT tests each component for positive definiteness in SCP mode or positive semidefiniteness in CP
mode.

A failed component rejects the matrix. A passing block adds no clause and does not accept the matrix; complete Dickinson traversal
still supplies every positive classification.

The exact Motzkin–Straus two-value pattern bypasses maximal-clique enumeration because its Zed blocks coincide with graph cliques and
can cause an unproductive set explosion. `COPOSIT_CLINGO_SAT_ZED_SCAN=off` provides the same isolation switch explicitly; `on` is the
default and any other value is an error.

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Unless bypassed, enumerate maximal Zed blocks and reject on the relevant exact definiteness failure.
3. Construct one clingo control object and ground the independent support choices.
4. Disable the enumeration assumption so callback clauses survive solve boundaries.
5. For $k=1,\ldots,n$, ground and activate the exact-$k$ constraint.
6. Let clasp backtrack to a completed uncovered answer set.
7. Extract its exact support and run the nonsingular or singular Dickinson calculation.
8. Reject on a negative witness; in SCP mode also reject on a nonnegative zero.
9. Add the single exact interval-blocking clause through the model's solve context.
10. Continue the same clasp enumeration until layer $k$ is exhausted, deactivate its guard, and advance to $k+1$.
11. Return the positive result after every cardinality is exhausted.

The shared connected-component and matrix pre-check pipeline remains outside this model and can be selected independently by the
Python analysis runner.

## Exact Arithmetic, Progress, Timeouts, And Termination

Matrix arithmetic uses FLINT arbitrary-precision integers. Matrix supports are recovered from clingo program literals, so there is
no fixed-width support-mask limit.

Clingo solving is asynchronous only to preserve the project's cooperative timeout contract. The main caller waits on the solve
handle in short intervals and cancels clingo when the signal flag is set. Exact callback work retains the normal timeout checkpoints.
The solver itself remains configured for one clasp solving thread; this model does not introduce parallel support processing.

Progress counts a support only after clingo has produced its completed answer set. It records the same sparse $(k,d,|U|)$
certificate distribution as the related serial Dickinson experiments when progress or diagnostics collection is enabled.

The support family is finite. Every successful callback blocks at least its current support, and every failed exact witness
terminates classification. Therefore the algorithm terminates when sufficient time and memory are available. A timeout remains
unresolved and is never returned as a negative classification.

## Known Difficult Inputs

The number of valid Dickinson certificates can still be exponential. If most certificates are narrow, the model accumulates many
long clauses, exact systems, and completed-model callbacks. Clasp cannot manufacture mathematical coverage absent from Dickinson's
vectors.

All native exact-cardinality aggregates remain grounded, so their stored literal references grow quadratically with matrix order.
That cost is especially unattractive when a witness would otherwise be found after only a few supports. A measured need for larger
orders would justify replacing only this cardinality layer, not reimplementing clasp.

Highly symmetric support families can give clasp many interchangeable choices and irregular backtracking behavior. No speculative
symmetry-breaking constraints are added because they would need a separately proved relationship to matrix permutations and could
cost more than they save.

The rejection-only Zed stage has its own exponential worst case because a graph can contain exponentially many maximal cliques;
Dickinson's Example 6.7 gives this obstruction. Large Motzkin–Straus instances are bypassed automatically, but other graphs with many
maximal Zed blocks can still spend substantial time before clingo begins. Passing blocks do not prune the later traversal, so an
unproductive Zed scan is pure overhead.
