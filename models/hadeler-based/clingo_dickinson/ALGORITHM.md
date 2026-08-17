# Clingo Dickinson

Classification: coposit-created exact copositivity experiment. It preserves Dickinson's exact certificate mathematics but delegates
cardinality-ordered support enumeration and interval blocking to clingo and its clasp solver.

The model supports separately selected copositivity (CP) and strict copositivity (SCP), and it can classify both predicates in one
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

Dickinson processes principal supports in increasing cardinality. Solving one exact system on a support can certify not merely that
support but every support in a Boolean-lattice interval. The main computational problem is therefore:

> Return an uncovered support of cardinality $k$, accept one new interval that must be excluded, and continue without restarting or
> explicitly visiting any support already covered by the accumulated intervals.

This model represents matrix index $i$ by a Boolean atom `selected(i)`. A true atom means that $i$ belongs to the support. Gringo
grounds one exact-cardinality constraint for each layer $k$. Clasp enumerates completed answer sets for that layer. Whenever a
completed answer set yields a valid Dickinson certificate, one clause excludes the certificate's entire interval. Clasp installs
the clause directly into its current backtracking search. If the interval can cover a later cardinality, the model also keeps a copy
until that solve finishes and installs an expiration-guarded equivalent through Clingo's persistent backend.

Clingo and clasp perform no matrix classification. They only maintain the finite Boolean support family. Every matrix solve,
nullspace construction, sign test, product, witness decision, and interval endpoint remains exact arbitrary-precision arithmetic.

## Name, Sources, And Classification

The identifier is `clingo_dickinson`:

- **Clingo** identifies the public multi-shot API and ASP grounder used to describe the support family.
- **Dickinson** identifies the unchanged exact certificate mathematics.

This is an independent copy of `sat_dickinson`, not a literature baseline. It replaces that experiment's hand-built Batcher
network and CaDiCaL solve loop with clingo's native cardinality grounding and clasp's backtracking answer-set enumeration. The
principal systems, singular branch, exact witnesses, interval endpoints, and increasing cardinality order are retained.

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2.

The Boolean engine is clingo 5.8.2, pinned to upstream tag `v5.8.2` from <https://github.com/potassco/clingo>. That release contains
clasp 3.4.1. CMake builds the static C++ libraries without the command-line applications, examples, Python binding, Lua binding, or
upstream test suite. No clingo or clasp source is copied into this model directory.

The integration was checked against the pinned implementation, not only its public declarations. A model-callback clause passes
through `clingo_solve_control_add_clause`, `ClingoModel::addClause`, and clasp's enumeration-context `commitClause`, which makes it
available to the active backtracking enumeration. That callback route alone does not provide the required cross-solve lifetime.
Between cardinalities, the model therefore translates every new clause into an integrity constraint through Clingo's public backend.
It does not reproduce clasp's backtracking or clause database.

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

The clause is false exactly on the supports in $[L,U]$, so it removes the complete interval and nothing else. Its unguarded length is

$$
|L|+n-|U|=n-(|U|-|L|).
$$

Writing $d=|U|-|L|$, a wide certificate has large $d$ and therefore a short clause. A narrow certificate has a long clause and
removes fewer supports.

The interval is relevant only at cardinalities $k\leq|U|$. Clingo therefore exposes one shared external atom `expired(t)` for every
possible upper size $t$. A persistent bounded-interval constraint also contains `not expired(|U|)`. When the traversal advances to
$k=|U|+1$, it sets that one external atom to true, disabling every constraint with that upper size at once. This is one operation per
cardinality, not a scan over stored certificates. Ceiling intervals with $|U|=n$ never expire and need no guard.

The current support always belongs to its generated interval. Consequently every completed callback adds a clause that invalidates
the current answer set, and clasp must backtrack to a genuinely uncovered support.

## Native Exact-Cardinality Layers

The base ASP program contains one independent choice atom for each matrix index and the shared expiration guards:

```text
{ selected(0..n-1) }.
#external expired(0..n).
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
4. adds the resulting interval clause through `Model::context()` and, when it remains useful later, records it for guarded backend
   installation; and
5. returns to the same clasp enumeration.

This is also the API specifically intended for adding clauses during model enumeration. No C++ exception is allowed to become a
classification; callback exceptions are transported by clingo's C++ wrapper and rethrown at the solve handle.

## Clause Persistence Across Cardinalities

Clingo treats clauses added through a completed-model callback as solve-step clauses. They immediately block the current support and
guide backtracking within the active cardinality, but relying on their lifetime after that solve returns loses Dickinson coverage.

The model therefore gives each generated clause two deliberately separate lifetimes:

1. add it through `SolveControl::add_clause` so it affects the current cardinality immediately; and
2. if $|U|=k$, discard the temporary copy after the current solve because the interval cannot cover any later layer;
3. otherwise retain a temporary copy until the solve handle closes, negate its literals into a rule body, and add the persistent
   integrity constraint through `Control::backend()`; when $|U|<n$, the body also contains `not expired(|U|)`.

The backend owns each retained constraint until the matrix call ends, but an expiration guard makes it inactive after its last useful
layer. The temporary copies for one cardinality are discarded immediately after installation. The model still explicitly selects
clasp's `bt` enumeration mode rather than relying on the `auto` default.

Two focused tests cover the solve boundary. The identity test requires singleton certificates to eliminate every larger support. A
three-dimensional positive-semidefinite test has singleton intervals that eliminate exactly one of its three pairs; cardinality two
must therefore process two supports rather than rediscovering all three.

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

## Exact Arithmetic, Diagnostics, Timeouts, And Termination

Matrix arithmetic uses FLINT arbitrary-precision integers. Matrix supports are recovered from clingo program literals, so there is
no fixed-width support-mask limit.

Clingo solving is asynchronous only to preserve the project's cooperative timeout contract. The main caller waits on the solve
handle in short intervals and cancels clingo when the signal flag is set. Exact callback work retains the normal timeout checkpoints.
The solver itself remains configured for one clasp solving thread; this model does not introduce parallel support processing.

Diagnostics counts a support only after clingo has produced its completed answer set. It records the same sparse $(k,d,|U|)$
certificate distribution as the related serial Dickinson experiments when diagnostics or diagnostics collection is enabled.

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
