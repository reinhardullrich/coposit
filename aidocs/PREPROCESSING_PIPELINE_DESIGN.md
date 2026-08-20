# Preprocessing Pipeline And Exact Checks

Status: current implemented workflow.

This document specifies the shared preprocessing applied after parsing a matrix. It explains the order, purpose, possible decisions,
single-switch behavior, child-matrix handling, and diagnostics meaning of every preprocessing operation. It intentionally gives only
the mathematics needed to understand the decisions; the individual model `ALGORITHM.md` files own full algorithm derivations.

The shared C++ pipeline, analysis CLI, and Python boundary implement the fixed order and single-switch contract below.

## 1. Vocabulary

The following names are fixed:

1. **Root checks** are cheap pre-component decisions made once on every matrix that enters the pipeline: the original matrix or a
   matrix created directly by Danninger or COPOMATRIX.
2. **Connected components** is an exact structural decomposition using negative off-diagonal entries. It is not a yes/no check.
3. **Ordinary checks** are component-local decisions that create no reduction children.
4. **Reduction depth** counts how many Danninger or COPOMATRIX child edges lead from the original matrix to the current matrix.
   The original matrix has depth zero. Connected-component splitting preserves depth; creating a reduction child adds one.
5. **Danninger reduction** is one bounded, exact, order-reducing proof attempt.
6. **COPOMATRIX reduction** is the following bounded Xu--Yao order-reducing proof attempt.

The fixed order is:

```text
input validation
-> matrix scan
-> root checks
-> connected components
-> ordinary checks
-> if reduction_depth reached maximum_reduction_depth: retain the component's partial result
-> Danninger reduction
-> COPOMATRIX reduction
-> component-result collection
-> return partial certificates and any unresolved component matrices
```

There is no caller configuration between these stages. Preprocessing is either disabled completely or runs this fixed sequence.
The reduction-depth bound is the fixed internal constant two. It is not exposed through a settings file, CLI option, or Python
argument. The original matrix is depth zero, children are depth one, and grandchildren are depth two.

## 2. Result State

Preprocessing must preserve two logically separate facts:

```text
copositive:          unknown / true / false
strictly copositive: unknown / true / false
```

The invariants are:

- strictly copositive `true` implies copositive `true`;
- copositive `false` implies strictly copositive `false`;
- strictly copositive `false` does not determine ordinary copositivity.

This partial state is necessary for equality cases. For example, a singular positive-semidefinite principal Z-matrix proves that
strict copositivity is false, but it need not decide ordinary copositivity of the complete matrix.

The stopping condition depends on the requested mode:

- **non-strict mode:** stop when ordinary copositivity is known;
- **strict mode:** stop when strict copositivity is known;
- **combined mode:** stop only when both facts are known.

A timeout, node limit, allocation failure, or other resource failure is never converted into either Boolean value.

## 3. Complete Control Flow

The pseudocode below is written for ordinary copositivity. It begins after mandatory input validation and assumes preprocessing is
enabled. Its inputs are a matrix $M$ and its `reduction_depth`. The fixed internal `maximum_reduction_depth` is two.
The procedure returns the root's partial certificate and one record per visited connected component. Every component is visited
unless an earlier negative certificate already decides the whole matrix. A component record contains its partial certificate and
retains its matrix only when the requested fact remains unresolved.

`combine_outcome` returns `'not_copositive'` if either input is `'not_copositive'`, returns `'copositive'` if both inputs are
`'copositive'`, and returns `'unresolved'` otherwise.

`danninger_children(C)` and `copomatrix_children(C)` denote the ordered children that the respective bounded reduction would create
from component $C$. Danninger distinguishes 0, 1, 2, and more than 2 children; COPOMATRIX always has at least one child and
distinguishes 1, 2, and more than 2. An implementation need not materialize any child after discovering that the count exceeds 2.
The recursive calls that check one or two children are written explicitly below.

### Algorithm: Complete Preprocessing

```cpp
maximum_reduction_depth <- 2  // Check children and grandchildren, but never create great-grandchildren.
preprocess(M, reduction_depth):
    root_outcome <- root_checks(M)
    IF root_outcome != 'unresolved':
        RETURN (root_outcome, no component records)
    overall <- 'copositive'
    component_results <- empty list
    FOR EACH C in negative_entry_components(M):
        outcome <- ordinary_checks(C)
        IF outcome == 'unresolved' AND reduction_depth >= maximum_reduction_depth:
            // The configured recursion bound is reached: retain C, but create no deeper descendants.
            outcome <- 'unresolved'
        IF outcome == 'unresolved' AND reduction_depth < maximum_reduction_depth:
            children <- danninger_children(C)
            IF number_of(children) > 2:
                // Create no child; Danninger stays unresolved and control falls through to COPOMATRIX.
                outcome <- 'unresolved'
            ELSE IF number_of(children) == 0:
                // Zero children is the direct rejection case: the parent component C is not copositive.
                outcome <- 'not_copositive'
            ELSE:
                outcome <- 'copositive'
                FOR EACH H in children:
                    // Only a reduction child increases the depth.
                    child_result <- preprocess(H, reduction_depth = reduction_depth + 1)
                    outcome <- combine_outcome(outcome, aggregate_certificate(child_result))
                    IF outcome == 'not_copositive':
                        BREAK
            IF outcome == 'unresolved':
                children <- copomatrix_children(C)
                IF number_of(children) > 2:
                    // Create no child; COPOMATRIX and preprocessing stay unresolved.
                    outcome <- 'unresolved'
                ELSE:
                    outcome <- 'copositive'
                    FOR EACH H in children:
                        // Only a reduction child increases the depth.
                        child_result <- preprocess(H, reduction_depth = reduction_depth + 1)
                        outcome <- combine_outcome(outcome, aggregate_certificate(child_result))
                        IF outcome == 'not_copositive':
                            BREAK
        IF outcome == 'unresolved':
            APPEND (matrix C, partial certificate outcome) TO component_results
        ELSE:
            APPEND (no matrix, certificate outcome) TO component_results
        overall <- combine_outcome(overall, outcome)
        IF overall == 'not_copositive':
            RETURN (root_outcome, component_results)
    RETURN (root_outcome, component_results)
```

`outcome` is the current component status. Each later method replaces it only while it remains unresolved. `overall` is the
certificate obtained by combining the component states; the records additionally preserve the matrices for components that remain
open.

The only recursive operations are the two visible calls that add one to `reduction_depth`. Every invocation scans its matrix, runs
root checks, splits connected components, and runs ordinary checks. It may enter the reduction block only while
`reduction_depth < maximum_reduction_depth`. At the bound, unresolved components remain unresolved and sibling components are still
checked because one may prove failure. With the maintained maximum of two, children may create grandchildren, but grandchildren
cannot create great-grandchildren.

Every connected component of $M$ inherits its invocation's depth. Merely copying a component into a smaller matrix does not create
a reduction descendant and does not change the depth. Only Danninger and COPOMATRIX children increment it.

One `'not_copositive'` child makes the reduction outcome `'not_copositive'`; all children must be `'copositive'` to make it
`'copositive'`; every other combination is `'unresolved'`. If Danninger's children leave the outcome unresolved, they are discarded
and COPOMATRIX receives the unchanged component $C$. If COPOMATRIX is also unresolved, its children are discarded and the unchanged
component $C$ is retained in the output work list. No copositivity model is called inside preprocessing.

Strict copositivity uses exactly the same control flow. Replace `'copositive'` by `'strictly_copositive'` and `'not_copositive'` by
`'not_strictly_copositive'`; `'unresolved'` keeps the same meaning. Combined classification follows the same sequence while carrying the
ordinary and strict facts together. The zero-child Danninger branch is relevant only in ordinary mode; strict mode has already
rejected its zero diagonal during the earlier checks.

## 4. Input Validation

Input validation is mandatory and is not a switchable pre-check. The parser requires a nonempty square symmetric matrix, constructs
both triangles, and converts supported exact input to the maintained integer representation. Model entry points assume that
contract. A direct C++ caller is responsible for providing a matrix satisfying it.

Parsing and input validation happen before preprocessing begins. An outer command timeout may cover parsing, preprocessing, and any
later caller action, but that does not make the later action part of preprocessing.

## 5. Matrix Scans

A scan gathers reusable facts; it does not by itself decide copositivity. When preprocessing is enabled, each pipeline-entry matrix
is scanned at most once for all data required by the fixed pipeline. When preprocessing is disabled, this shared scan does not run.

The scan can collect:

- diagonal signs;
- whether any negative off-diagonal entry exists;
- the negative-entry graph, with an edge when $a_{ij}<0$;
- the nonpositive-entry graph, with an edge when $a_{ij}\leq0$, when the Z-matrix test needs it;
- exact cardinality-two principal-face results;
- negative-part row sums;
- full row sums and the all-ones quadratic value;
- the maximum absolute coefficient used to scale floating Frank--Wolfe arithmetic;
- positive and negative off-diagonal counts for Danninger and COPOMATRIX pivot selection;
- exact two-value pattern facts needed for the Motzkin--Straus classifier.

The pipeline-entry matrix is scanned before root checks. If its negative graph is connected, that matrix and scan become the single
work unit; neither is copied or rescanned. If it is disconnected, one principal component matrix is materialized at a time, and all
statistics needed by its ordinary checks are collected during that copy.

Recording a sign bit during the root scan is not the expensive Z-matrix operation. Maximal-block enumeration and exact
factorizations occur only later, after component decomposition.

## 6. Root Checks

Root checks run once on every matrix entering `preprocess` before connected components. The original matrix and every matrix created
by Danninger or COPOMATRIX receive them. A connected component never receives root checks merely because it was copied into its own
matrix object; it inherits the current reduction depth and receives ordinary checks only.

### 6.1 Complete Small-Matrix Check

If the matrix order is at most three, the complete exact low-order criterion classifies it as one of:

- strictly copositive;
- copositive but not strictly copositive;
- not copositive.

Preprocessing returns that decision immediately. If the order is greater than three, this root check is inapplicable rather than
failed.

### 6.2 Cardinality-One Principal Faces

The cardinality-one faces are the diagonal entries:

- $a_{ii}<0$ proves that the matrix is not copositive;
- $a_{ii}=0$ proves that it is not strictly copositive;
- $a_{ii}>0$ is necessary for strict copositivity but does not accept a matrix of order greater than one.

The root scan already covers every diagonal, including every diagonal that will occur in a later component. It is not checked again
after splitting.

### 6.3 Cardinality-Two Principal Faces

Every relevant $2\times2$ principal face is checked exactly. A failing face disproves ordinary or strict copositivity, according to
the requested mode. Passing all pairs does not accept a matrix of order greater than two.

Every component face is already a principal face of the pipeline-entry matrix, so the pair check is not repeated on the components.

### 6.4 Nonnegative Off-Diagonal Check

If all off-diagonal entries are nonnegative, the diagonal signs determine the complete answer:

- nonnegative diagonal: copositive;
- positive diagonal: strictly copositive;
- nonnegative diagonal with at least one zero: copositive but not strictly copositive.

If the negative graph has a nontrivial connected component, that component contains a negative edge and cannot pass this criterion.
Singleton components are handled directly from their diagonal. The check therefore does not need to be repeated after splitting.

## 7. Connected-Component Decomposition

The decomposition graph has vertices $1,\ldots,n$ and an edge $\{i,j\}$ exactly when $a_{ij}<0$. If its connected components are
$C_1,\ldots,C_m$, then all entries between different components are nonnegative. For every nonnegative vector $x$,

$$
x^T A x
=
\sum_r x_{C_r}^T A[C_r]x_{C_r}
+2\sum_{r<s}x_{C_r}^T A[C_r,C_s]x_{C_s},
$$

and the cross-component sum is nonnegative. Consequently:

$$
A\text{ is copositive}\iff A[C_r]\text{ is copositive for every }r,
$$

and the same equivalence holds for strict copositivity.

Operationally:

- a connected graph reuses the original matrix and scan;
- a disconnected graph streams one component matrix at a time;
- a singleton component is decided directly from its diagonal;
- a negative component may stop the requested query immediately;
- otherwise component results are combined with logical AND.

Connected-component decomposition always runs when preprocessing is enabled. If the graph is connected, this stage is effectively
free after traversal: the original matrix and its scan become the single ordinary-check work unit without copying.

## 8. Ordinary Checks

Ordinary checks run on each connected work unit. When the negative-entry graph is connected, that work unit is the original matrix.
Ordinary checks create no Danninger or COPOMATRIX children.

Their maintained order is:

```text
ordinary_checks(A):
    run the small-face and cheap component checks
    run bounded Frank--Wolfe witness search
    run one heuristic KKT walk from the center of the full simplex
    if A has the exact Motzkin--Straus form:
        classify A by maximum clique and return

    factorize the original A exactly
    use positive-(semi)definiteness and, when applicable, its nullity-one kernel
    if the requested result is complete: return
    if A has no positive off-diagonal entry:
        use the same factorization as the complete symmetric-Z-matrix decision and return

    form C by replacing positive off-diagonal entries of A by zero
    factorize C exactly
    use positive-(semi)definiteness of C as a one-way positive certificate for A
    if the requested result is complete: return
    if C is positive semidefinite: return the partial result

    inspect maximal principal Z-blocks of the original A
    return the resulting partial decision; any unresolved work still refers to A
```

The exact Motzkin--Straus branch stays before the cubic factorizations because it is a complete specialized classifier. The ordinary
maximal principal-$Z$ search is deferred until both complete-matrix factorizations have failed. The auxiliary matrix $C$ is never
sent to a copositivity model: it supplies only a sufficient certificate for the original $A$.

### 8.1 Complete Small-Component Check

When a component has order at most three, the same exact low-order routine classifies that component completely. This is not a
repetition of the root check: the root order was greater than three, while the newly materialized principal component is itself a
small complete problem.

### 8.2 Cardinality-Three Principal Faces

Relevant $3\times3$ principal faces are checked exactly. A failing triple disproves the requested property. Passing every triple
does not accept a component of order greater than three.

This check belongs after connected components. Any useful negative interaction lies inside one negative component, so componentwise
enumeration retains the decision while reducing the active problem size and allowing early component termination.

The maintained complete profile checks cardinalities one and two at the root and cardinality three after splitting. There is no
separate cardinality cutoff in the exposed preprocessing configuration.

### 8.3 Negative-Part Diagonal Dominance

For every row, form the diagonal plus only its negative off-diagonal entries. If all resulting row sums are nonnegative, the
component is certified copositive. If they are all positive, it is certified strictly copositive. Otherwise this check is
unresolved.

This is a positive certificate. It runs componentwise because some components may pass even when a different component prevents the
whole matrix from satisfying the criterion.

### 8.4 All-Ones Witness

The check evaluates the exact quadratic value $\mathbf1^T A\mathbf1$ on the current work unit:

- a negative value disproves copositivity;
- zero disproves strict copositivity;
- a positive value gives no decision.

It runs componentwise because nonnegative cross-component terms can mask a nonpositive all-ones value on one component.

### 8.5 Bounded Frank--Wolfe Witness Search

The search starts at the center of the component simplex and performs at most one iteration per matrix coordinate. Each iteration
selects the coordinate with the smallest current matrix product and takes the best line step toward that simplex vertex using
double-precision arithmetic. It stops when any of the following occurs:

- the Frank--Wolfe gap is numerically negligible;
- the computed step length is zero;
- the floating objective no longer changes;
- an exact nonpositive witness has already been verified;
- the order-many iteration limit is reached.

Floating arithmetic may only propose a candidate. The implementation quantizes its nonnegative coordinates to integer weights and
evaluates the resulting quadratic form with exact integers. Only that exact sign can decide:

- negative: not copositive;
- zero: not strictly copositive;
- positive: unresolved.

Frank--Wolfe never accepts a component.

### 8.6 Heuristic KKT Search

This stage makes one bounded active-set walk for the simplex problem

$$
\min\{x^TAx:x\geq0,\ \mathbf1^Tx=1\}.
$$

It begins at the center $x_i=1/n$, whose active support is the full index set. On an active support $S$, the walk solves the face
stationarity equations

$$
A_{SS}x_S=\lambda\mathbf1,
\qquad
\mathbf1^Tx_S=1.
$$

A pivoted binary64 $LDL^T$ solve proposes the path. Negative active coordinates are removed first; when the face point is
nonnegative, an unused coordinate with $(Ax)_j<\lambda$ is added. The most violated coordinate is tried first, and a current-path
support is never revisited. The walk performs at most $n$ face visits and does not backtrack.

Floating arithmetic never decides copositivity. At every floating-feasible face point whose value appears negative, the same face
system is solved exactly; an exact negative value immediately disproves copositivity without waiting for a KKT point. When floating
arithmetic claims that all KKT conditions hold, the candidate is also verified exactly. If the exact signs disagree, every remaining
step in that one walk uses exact arithmetic, preventing repeated attraction to the same false floating KKT point.

The first exact KKT point ends the walk:

- a negative value disproves copositivity;
- zero disproves strict copositivity;
- a positive value gives no decision.

A KKT point exists because the simplex is compact, but this bounded heuristic is not a complete KKT enumerator. A numerically
inconclusive floating factorization, an exhausted successor path, or the $n$-visit limit therefore leaves the stage unresolved and
preprocessing continues normally.

### 8.7 Motzkin--Straus Fast Path

This stage recognizes the two-level Motzkin--Straus form:

- every diagonal entry has one common value $d\geq0$;
- every off-diagonal entry is either $d$ or one common value $q<0$;
- the value $q$ occurs at least once.

Let $G$ join $i$ and $j$ exactly when $a_{ij}=q$, and let $\omega$ be the maximum number of pairwise joined vertices. The
Motzkin--Straus identity gives the exact simplex minimum

$$
\min_{x\geq0,\;\mathbf1^Tx=1}x^TAx=q+\frac{d-q}{\omega}.
$$

This is the scaled matrix form of Motzkin and Straus, *Maxima for Graphs and a New Proof of a Theorem of Turán* (1965),
[doi:10.4153/CJM-1965-053-6](https://doi.org/10.4153/CJM-1965-053-6).

The implementation therefore compares the integers $(-q)\omega$ and $d-q$ without division:

- $(-q)\omega<d-q$: strictly copositive;
- $(-q)\omega=d-q$: copositive but not strictly copositive;
- $(-q)\omega>d-q$: not copositive.

An exact maximum-clique branch-and-bound computes $\omega$. The search is adapted from Darren Strash's Open MCS implementation at
commit `735788af066fc8589f577036af521f22f45c2731`. It retains the MCR minimum-degree initial ordering with neighborhood-degree
tie-breaking, the static vertex order, a greedy coloring upper bound at every search node, and the MCS recoloring repair that can
reduce that bound. Graph adjacency remains in coposit's packed multiword support type; search orders and color classes use lazily
grown retained vectors. The integration uses `size_t` indices, the shared cooperative timeout, and shared diagnostics rather than
Open MCS's executable and legacy clock timeout.

A strict-only query stops as soon as a proved clique reaches the strict boundary; ordinary or combined classification stops early
only after a clique passes the ordinary boundary. Equality in combined mode remains open until the coloring bounds prove that no
larger clique exists. The outer timeout remains authoritative, so an interrupted exponential search produces no Boolean result.

Only an exact pattern match enters this branch. Its exact result classifies the whole component, so neither complete-matrix
factorization nor maximal principal-$Z$ enumeration follows. A matrix with any third coefficient value continues with the ordinary
factorization flow.

### 8.8 Exact Positive-(Semi)definiteness

The current work unit is copied once and factorized exactly with fraction-free LDLT:

- positive definite accepts both ordinary and strict copositivity;
- positive semidefinite accepts ordinary copositivity;
- a singular PSD matrix with nullity one and a one-signed kernel vector rejects strict copositivity;
- a singular PSD matrix with nullity one and a mixed-sign kernel vector accepts strict copositivity;
- higher-nullity PSD accepts ordinary copositivity but may leave strict copositivity unresolved;
- a matrix that is not PSD gives no copositivity decision.

This check is deliberately component-local because exact factorization is one of the most expensive preprocessing operations and can
be substantially cheaper on separated principal components.

### 8.9 Exact Negative-Part Positive-(Semi)definiteness

This check runs only when the preceding factorization leaves the requested result unresolved. Define the exact symmetric matrix
$C$ by retaining the diagonal and every negative off-diagonal entry of the current work unit $A$, while replacing each positive
off-diagonal entry by zero:

$$
C_{ii}=A_{ii},
\qquad
C_{ij}=\min\{A_{ij},0\}\quad(i\ne j).
$$

Then $A=C+N$ for an entrywise-nonnegative matrix $N$. For every $x\geq0$,

$$
x^TAx=x^TCx+x^TNx,
\qquad x^TNx\geq0.
$$

Therefore:

- positive semidefiniteness of $C$ accepts ordinary copositivity of $A$;
- positive definiteness of $C$ accepts strict copositivity of $A$;
- singular positive semidefiniteness of $C$ does not reject strict copositivity, because $N$ may be positive on every nonnegative
  null vector of $C$;
- failure of positive semidefiniteness gives no decision.

The implementation constructs $C$ and reuses the existing exact fraction-free LDLT object for a second factorization. When $A$ has
no positive off-diagonal entry, $C=A$ is itself a symmetric Z-matrix. The preceding factorization then gives the complete decision:
PSD is equivalent to copositivity, positive definiteness is equivalent to strict copositivity, and singular PSD lies on the strict
boundary. This avoids both constructing $C$ and refactorizing $A$ as a maximal Z-block. This is the fixed, easily checked SPN
certificate $A=C+N$; it does not solve a semidefinite feasibility problem.

### 8.10 Maximal Principal Z-Matrix Fallback

A principal Z-matrix is a principal block of the original $A$ whose off-diagonal entries are all nonpositive. The fallback searches
maximal such blocks, which are maximal cliques in the graph with edges $a_{ij}\leq0$. It then:

1. splits each maximal block into components of its strictly negative graph;
2. copies each resulting principal block of $A$;
3. factorizes it exactly with fraction-free LDLT.

The decisions are:

- a block that is not positive semidefinite disproves ordinary and strict copositivity;
- a singular positive-semidefinite block disproves strict copositivity but leaves ordinary copositivity open;
- a positive-definite block gives no whole-component decision.

This fallback runs only when a separately constructed negative-part matrix $C$ was not positive semidefinite. The skip is exact, not
heuristic. If $C=A$, the first exact factorization already completed the symmetric-Z-matrix decision. Each ordinary work unit has a
connected negative graph. If its $C$ is positive semidefinite, then $C$ is an irreducible symmetric positive-semidefinite Z-matrix.
Every proper principal submatrix of $C$ is positive definite. Any principal Z-block of $A$ is identical to the corresponding block of
$C$; when $A$ has a positive off-diagonal entry, such a Z-block must be proper. Therefore maximal principal-Z enumeration cannot add
a decision after this certificate.

The fallback can only disprove a requested property. Its internal block decomposition is not the general connected-component
decomposition used by the preprocessing pipeline. The two graphs are related but different:

- connected components use $a_{ij}<0$;
- Z blocks use $a_{ij}\leq0$.

The root sign scan supplies both graphs. A Z block spanning separate negative components has only zero cross entries and is block
diagonal there, so processing its negative components separately loses no decision.

## 9. Bounded Danninger Reduction

This is a reduction, not an ordinary check, because it creates lower-order matrices. It runs only after every ordinary check
has remained unresolved.

The gate:

1. counts the immediate Danninger children for every possible pivot;
2. chooses the first pivot attaining the minimum count;
3. remains unresolved when that minimum exceeds two;
4. otherwise performs exactly one order-reducing Danninger step;
5. returns its ordered children to the main preprocessing flow, which explicitly sends each child through the certificate-only
   child pipeline described below.

Depending on the pivot-row sign pattern, the exact child is a principal block, the division-free Schur block
$a_{pp}B-pp^T$, or one of two exact ray-transformed half-cone blocks. The complete Danninger model may generate a broad recursive
staircase; this shared gate deliberately does not.

Child results combine as follows:

- if any child disproves the requested property, the parent component also fails it;
- if every child proves the requested property, the parent component satisfies it;
- otherwise the Danninger gate is unresolved.

An unresolved Danninger gate discards its generated proof attempt and passes the unchanged original component to COPOMATRIX.

The isolated strict Core/Stress experiment found that this bounded nonrecursive gate proved strict copositivity for 51 of 268
matrices left unresolved by all earlier checks. Every success used the one-child case. This evidence is why the bounded gate is
retained without expanding it into recursive shared preprocessing.

## 10. Bounded COPOMATRIX Reduction

COPOMATRIX runs only when Danninger is unresolved. It is likewise one exact order-reducing proof attempt rather than a
recursive invocation of the complete Xu--Yao algorithm.

The gate:

1. counts immediate COPOMATRIX children for every pivot;
2. chooses the first pivot attaining the minimum count;
3. remains unresolved when the minimum exceeds two;
4. otherwise constructs the exact principal child and, when required, one transformed Schur child;
5. returns its ordered children to the main preprocessing flow, which explicitly sends each child through the same certificate-only
   child pipeline.

Child outcomes combine in the same way as Danninger. An unresolved COPOMATRIX attempt leaves the preprocessing result unresolved.

In the same isolated experiment, bounded COPOMATRIX proved strict copositivity for three of the 268 former-precheck leftovers. Two
overlapped Danninger, so it added one unique decision. Together the two gates proved strict copositivity for 52 matrices and left 216
unresolved. Neither gate disproved strict copositivity in that experiment.

## 11. Depth-Bounded Reduction Pipeline

A reduction child is a genuinely new transformed matrix. It enters the same pipeline at its parent's reduction depth plus one.
Root facts about its parent do not carry over, so every descendant receives its own scan and root checks. Every connected component
split from that descendant preserves the same depth. For ordinary copositivity, the descendant returns exactly one of three states:

- **`COPOSITIVE`:** the child checks proved that the child is copositive;
- **`NOT_COPOSITIVE`:** the child checks proved that the child is not copositive;
- **`UNRESOLVED`:** the checks proved neither statement.

For strict copositivity, the corresponding states are `STRICTLY_COPOSITIVE`, `NOT_STRICTLY_COPOSITIVE`, and `UNRESOLVED`.

Preprocessing invokes no model for a reduction descendant. An unresolved descendant therefore stays unresolved. The parent
reduction combines all child states and decides what happens next. The complete call and stopping rule are shown in Section 3.

There are two nested aggregations:

1. **Inside one child:** one `NOT_COPOSITIVE` component makes the child `NOT_COPOSITIVE`; all components must be `COPOSITIVE` to make
   the child `COPOSITIVE`; every other combination is `UNRESOLVED`.
2. **Across a reduction's children:** one `NOT_COPOSITIVE` child makes the parent `NOT_COPOSITIVE`; all children must be `COPOSITIVE`
   to make the parent `COPOSITIVE`; every other combination is `UNRESOLVED`.

The strict version uses the same two rules with the strict state names. An unresolved component or child does not stop traversal,
because a later one may still prove that the parent fails the requested property. A proven failure stops immediately; a positive
conclusion requires every required component or child to satisfy the property.

The visit order is fixed:

- a two-child Danninger step visits the positive-side transformed child first and the negative-side child second;
- a two-child COPOMATRIX step visits the principal child first and the transformed Schur-side child second.

If the first child proves that the requested property fails, the second child is never passed through the child pipeline. If the
first is merely unresolved, the second is still checked because it may prove failure. Generated child matrices and their intermediate
results are discarded when the reduction remains unresolved; the next stage always receives the unchanged original component.

Combined classification invokes this ternary reduction flow per property. Strict copositivity is tried first. When that does not
complete the classification, ordinary copositivity is tried next. `STRICTLY_COPOSITIVE` proves both properties;
`NOT_COPOSITIVE` disproves both; and `NOT_STRICTLY_COPOSITIVE` together with `COPOSITIVE` proves the boundary case. Any remaining
unknown state is returned as unresolved.

A descendant whose depth is below two may call Danninger and COPOMATRIX again. A grandchild at depth two calls neither reduction and
never calls a copositivity model. It returns the facts proved by its scan, root checks, component split, and ordinary checks. This
keeps the reduction tree finite without a runtime setting. Connected-component copying never changes the depth.

## 12. What Is Repeated

| Operation | Top-level path | Reduction-descendant path |
|---|---:|---:|
| Root checks | Once on the original matrix before splitting | Once on each new reduction child before splitting |
| Connected-component decomposition | Once on the original matrix | Once per new reduction child |
| Ordinary checks | Once per top-level component | Once per reduction-child component |
| Danninger reduction | At most once per unresolved top-level component | At most once per unresolved component while depth is below two |
| COPOMATRIX reduction | At most once per unresolved top-level component | At most once per unresolved component while depth is below two |

The small-component ordinary check may call the same low-order mathematical routine as the root small-matrix check. That is not a
repeat on the same problem: the original matrix was too large for the root criterion, while the principal component is a new complete
low-order problem.

## 13. Single-Switch Semantics

The agreed target has one preprocessing switch and no individual check controls:

```cpp
bool preprocessing_enabled = true;
```

The two states are exhaustive:

| Preprocessing | Effective behavior |
|---|---|
| off | Bypass preprocessing. Equivalently, preprocessing contributes no decision. |
| on | Run every stage in the fixed order documented here. No individual stage can be skipped or reordered. |

The C++ and Python analysis interfaces expose the single on/off switch for measurements. A narrower experiment that removes one internal check is a
temporary source-level experiment, not another supported configuration, and its exact patch must be recorded with its results.

## 14. Preprocessing Output And Aggregation

Preprocessing returns a root partial certificate plus one result record per connected component. Each record carries the component's
known ordinary and strict facts. A fully resolved record needs no matrix. An unresolved record retains the exact matrix on which the
missing facts must be solved. Preprocessing never invokes a copositivity model.

The dispatcher calls the selected model only for unresolved records, fills their missing facts, and combines all component results
with logical AND. If the negative-entry graph is connected, the pending record borrows the original matrix. A proper principal
component is materialized once because the maintained models require a dense `matrix_integer`; that owned matrix is then moved into
the result record and is never copied again.

Danninger and COPOMATRIX remain certificate-only proof attempts. Their generated descendants are not returned to the dispatcher.
When such an attempt remains unresolved, its descendants are discarded and its unchanged parent component is the pending work item.

For multiple original components, ordinary and strict results are combined independently with logical AND. Processing may stop as
soon as the requested result becomes false. Positive completion requires the corresponding property from every component.

## 15. Diagnostics And Timeouts

The fixed flow gives each visible preprocessing phase one unambiguous name:

```text
matrix scan
root checks
connected components
component scan
ordinary: principal triples
ordinary: negative-part diagonal dominance
ordinary: all-ones
ordinary: Frank-Wolfe
ordinary: heuristic KKT search
ordinary: Motzkin-Straus fast path
ordinary: exact factorization
ordinary: negative-part factorization
ordinary: maximal Z-matrix fallback
Danninger reduction
COPOMATRIX reduction
```

Diagnostics percentages, when a truthful denominator exists, are local to the current matrix or component. They are activity indicators,
not estimates of remaining wall time. Expiration during a root check, component operation, ordinary check, or reduction child leaves
preprocessing unresolved.

The maintained diagnostics retain one summary across preprocessing and every later model-stage line:

- `preprocessing_outcome=running|resolved|pending` distinguishes an interrupted/in-progress pass, a completed preprocessing decision,
  and completed preprocessing with work left for the model;
- `component_split`, `components_seen`, and `largest_component` record the top-level connected-component decomposition actually visited;
- `pending_components` and `largest_pending_component` record the unresolved matrices returned by preprocessing;
- `reduction_child_checks`, `maximum_reduction_depth`, and `reduction_decisions` record bounded certificate-only child work; and
- `model_delegations` counts actual selected-model calls.

Thus a large matrix can be identified directly as fully resolved, partly reduced to smaller pending blocks, or still running in a
particular preprocessing phase. A `running` row contains only work observed before interruption and is not a completed unresolved result.

When preprocessing is off, none of the preprocessing phase names is emitted.

## 16. Cost And Failure Characteristics

The ordering deliberately places cheap decisions and structural decomposition before expensive work:

- the mandatory root scan is quadratic in the matrix order but reads each relevant entry only once;
- negative-component traversal uses the packed multiword support representation and is cheap after the scan;
- principal-triple work can grow rapidly with negative degrees, so it is component-local;
- bounded Frank--Wolfe uses only order-many floating iterations, followed by exact verification of proposed witnesses;
- heuristic KKT search visits at most the component order in supports; ordinary steps are floating, while a proposed negative value
  or KKT endpoint pays for exact verification and one floating/exact disagreement makes the remainder of that walk exact;
- the Motzkin--Straus maximum-clique branch-and-bound and maximal-Z search are both exponential in the worst case; the exact pattern
  takes its complete specialized path, while maximal-Z runs only after both complete-matrix factorizations remain insufficient;
- exact fraction-free LDLT has bounded pivot count but can suffer severe arbitrary-precision coefficient growth;
- the two reduction gates refuse pivots with more than two immediate children;
- preprocessing retains a pending component record whenever these stages establish neither the requested fact nor its negation.

Disabling preprocessing removes all shared shortcuts and contributes no classification. It must never turn a timeout or unresolved
state into a negative classification.

## 17. Implementation Invariants

The maintained implementation preserves these invariants:

1. retain one master preprocessing on/off state and remove the individual supported configuration switches;
2. make root checks and ordinary checks separate return-value stages rather than nested delegation callbacks;
3. run cardinality-three faces only after component decomposition;
4. avoid repeating negative-part diagonal dominance and all-ones decisions at root and component level;
5. run Frank--Wolfe, one bounded heuristic KKT walk, and the exact Motzkin--Straus fast path before factorizing the original component
   and its negative part, then run maximal-Z enumeration only when those complete-matrix checks leave it useful;
6. reuse each matrix scan and collect Z/reduction sign data without an extra full-matrix sign pass;
7. fix the internal maximum reduction depth at two, with no settings-file, CLI, or Python control;
8. make the enabled profile execute connected components, every check, Danninger, and COPOMATRIX in the documented fixed order;
9. preserve combined-mode equality states and the logical implications between strict and ordinary copositivity;
10. keep the C++, Python, diagnostics, and project documentation synchronized with this specification;
11. preserve one result record per connected component and delegate only its missing facts to the selected model;
12. borrow the unchanged original matrix, own each materialized principal component once, and perform no later matrix copies;
13. keep Danninger and COPOMATRIX descendants certificate-only and retain their parent component after an inconclusive attempt;
14. relink every model companion against the reconciled shared preprocessing before comparing results.

Focused tests cover the master switch, complete low-order decisions, shared scan data, two-child reduction, the fixed depth-two
bound, child recursion, grandchild stopping, combined equality, disconnected aggregation, resolved-component skipping, delegation of only
unresolved component matrices, connected-matrix borrowing, early model failure, and unchanged-parent reduction fallback.
