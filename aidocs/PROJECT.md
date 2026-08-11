# Coposit Project Overview

Last verified: 2026-08-11

## Purpose And Contract

Coposit is a standalone C++17 project extracted from FracESSA for exact ordinary- and strict-copositivity decisions on symmetric
integer matrices:

$$
x^T A x \geq 0
\quad\text{or}\quad
x^T A x > 0
\qquad \text{for every nonzero } x \geq 0.
$$

Every model implements the link-time contract `coposit::model::solve(const matrix_integer&, copositivity_mode)` declared in
`cpp/include/coposit/model.hpp`; the mode defaults to `strictly_copositive`. The eight literature baselines implement both modes,
as does the selected `adaptive_sponsel_copomatrix`; other Coposit-created variants reject ordinary mode explicitly. A completed
`solve` call returns the Boolean for only the selected predicate. Hadeler 1983, Dickinson 2019, and Danninger 1990 additionally
implement `classify(matrix)`, which returns ordinary and strict results from one ordinary-complete traversal. Both operations throw
`std::invalid_argument` for an empty, non-square, or asymmetric matrix. Dutour 2018, Bundfuss 2008, Sponsel 2012, Frank–Wolfe Sponsel, and Safi 2021 may instead stop
unresolved when their simultaneous unfinished-node count would exceed the fixed 50,000-node limit.

The maintained core accepts integers only. Rational input may later clear denominators at an outer boundary, but rational matrix
storage is not part of Coposit.

## Repository Map

- Maintained C++ project: `cpp/`
- Shared model-independent C++ infrastructure: `cpp/include/coposit/`
- Unchanged source and literature baselines: `models/baselines/<model-name>/`
- Coposit-created solver variants: `models/<model-name>/`
- Shared one-matrix executable protocol: `cpp/model_main.cpp`
- Maintained Python package: `python/pycoposit/`
- Canonical corpus, external Matrix Market files, and exported schema: `testdata/`
- Current and historical agent documentation: `aidocs/`
- Copied mathematical notes and papers: `research/`
- Preserved experimental implementations and results: `experiments/`
- FracESSA integration snapshots kept only as reference: `reference/fracessa/`

## Model Organization

Each solver variant owns a self-contained algorithm implementation. Unchanged source and literature baselines live under
`models/baselines/<model-name>/`; selected Coposit-created models remain directly under `models/<model-name>/`, while retained
legacy models live under `models/legacy/<model-name>/` and active, not-yet-selected variants live under
`models/experiments/<model-name>/`. The selected user-facing model is
`models/baselines/dutour_2018/`; `danninger_1990`, `hadeler_1983`, and `bundfuss_2008` are maintained historical baselines.
All eight baseline directories implement ordinary and strict copositivity. Dickinson, COPOMATRIX, Safi, Bundfuss, and Sponsel use
their published ordinary rules plus documented strict adaptations; Danninger and Hadeler have source theorems for both predicates;
Dutour uses the corresponding source boundary comparisons. These baseline directories are grouped under `models/baselines/`.
`models/frank_wolfe_sponsel/` is the maintained exact one-step Frank–Wolfe witness variant of Sponsel.
`models/fracessa/` is the maintained first-order global-minimum adaptation of FracESSA. `models/adaptive_dutour_danninger/` is the
maintained narrow-Danninger hybrid. `models/adaptive_dutour_copomatrix/` is the maintained narrow-COPOMATRIX hybrid with a forced
projection after 100 consecutive Dutour splits. `models/adaptive_sponsel_copomatrix/` uses the same forced projection after 1,000
Sponsel splits, but chooses the first local COPOMATRIX pivot attaining the exact minimum immediate-child count at every node and
supports individually selected ordinary and strict predicates.
`models/legacy/adaptive_zischg_sponsel_copomatrix/` retains the comparison that additionally decomposes every
COPOMATRIX projection child by its exact negative-entry connected components. `models/support_pruned_dickinson/` is the
Dickinson/FracESSA support-generation hybrid, and
`models/nullity_support_pruned_dickinson/` adds exact singular-vector coverage selection to that copied model.
`models/zischg_hadeler/`, `models/zischg_dickinson/`, and `models/zischg_fracessa/` are the Level 2 negative-graph variants. New
Coposit variants are made by copying the closest model directory and changing the copy.

Algorithm code is intentionally duplicated between models. Similar cone or Danninger code is not extracted into a shared helper
merely because several models use it: a hybrid must be free to interweave and change its private copy. Only stable infrastructure
that is independent of every solving strategy belongs under `cpp/include/coposit/`: exact integer and matrix storage, input parsing,
the packed support representation, the exact ordinary/strict copositivity criterion through order three, the optional exact-decision pre-check,
negative-entry connected-component decomposition, reusable exact fraction-free LDLT factorization, and the minimal model call contract.

Faithful historical or external baselines use `<first-author>_<year>` identifiers and record exact provenance in a model-local
`ALGORITHM.md`. Coposit-created variants use descriptive non-citation names. A baseline may optimize arithmetic, storage, or recomputation,
but its mathematical tests, branching, traversal, pruning, and termination stay faithful; the first mathematical change creates a
new model.

The selected model links with `cpp/model_main.cpp` into the single user-facing executable `coposit`. Model-specific benchmark targets
and native Python modules use their model identifiers. Each executable or native module links exactly one model, so no model code is
combined. The pure Python wrapper selects the corresponding module by name; there is no C++ runtime model factory, registry,
inheritance hierarchy, or selection layer.

## Optional Pre-Check

`cpp/include/coposit/pre_check.hpp` contains individually selectable ordinary- and strict-copositivity checks whose decisions are exact. Every option
defaults to on. A caller explicitly supplies the mode and may disable checks before invoking its final algorithm:

```cpp
coposit::pre_check::options checks;
checks.frank_wolfe = false;
return coposit::pre_check::check(matrix, mode, checks, final_algorithm);
```

Passing an unchanged `options{}` enables all seven checks and sets the principal-submatrix cutoff to three. `options::none()`
disables all checks and delegates directly to `final_algorithm(matrix)` without scanning or validating the matrix. No maintained
model calls the wrapper yet, so the shared boundary does not alter a baseline or variant result. One classification-aware
implementation serves ordinary, strict, and combined queries; the traversal and arithmetic are shared, while the requested query
changes the exact equality boundary:

| Quantity | Strict mode requires | Ordinary mode requires |
|---|---:|---:|
| principal singleton or accepted Qi row sum | $>0$ | $\geq0$ |
| small principal matrix | strictly copositive | copositive |
| all-ones or exactly verified Frank–Wolfe value to continue | $>0$ | $\geq0$ |

The selectable checks run as follows:

1. `small_dimension` completely decides an input of order at most three and does nothing at larger orders;
2. `principal_submatrices` enables rejection-only principal-face checking for inputs of order greater than three;
   `principal_submatrices_up_to` selects the maximum cardinality: one checks diagonal singletons, two adds every principal pair,
   and three adds every principal triple. Passing every selected face never accepts the complete matrix;
3. `nonnegative_off_diagonal` accepts a mode-valid diagonal with no negative off-diagonal entry;
4. `negative_part_diagonal_dominance` applies Qi's row-sum acceptance with the mode's strict or weak inequality;
5. `all_ones` rejects the exact all-ones quadratic value at the mode's failure boundary;
6. `frank_wolfe` starts at the simplex centre and performs at most the matrix order's number of double-precision line-minimizing
   steps. It stops early when the Frank–Wolfe gap is negligible after global input scaling, the computed step is zero or ineffective,
   the double objective no longer changes, or an exactly verified witness completes the requested mode. A promising iterate and the
   best final iterate are quantized to nonnegative integer weights; homogeneity removes their positive normalization denominator, and
   only the exact sign of the resulting integer quadratic form may reject. A floating result alone never decides copositivity;
7. `positive_definiteness` copies and factorizes the matrix exactly once; positive definiteness accepts strict mode and positive
   semidefiniteness accepts ordinary mode. For a singular positive-semidefinite matrix of nullity one, the retained factorization
   supplies one exact kernel vector: mixed signs accept strict mode, while a one-sided vector rejects it. Higher nullity and every
   non-positive-semidefinite inertia result remain inconclusive.

The principal-face option checks the selected singleton and pair cardinalities during the existing symmetric scan. At cutoff three,
a triple with a disconnected negative-entry graph is automatically mode-copositive because its components are mode-copositive and
its cross entries are nonnegative. The implementation therefore tests only connected negative triples, generating a two-edge path
once at its unique centre and a negative triangle once at its lowest vertex. This preserves the complete order-three decision while
avoiding a blind $\binom n3$ scan on sparse matrices.

`cpp/include/coposit/matrix_scan.hpp` owns that reusable exact scan. According to the selected requirements, one upper-triangle
pass validates symmetry and records diagonal signs, whether a negative off-diagonal entry exists, negative-part row sums, the
all-ones quadratic value, the negative-entry graph, and ordinary or strict principal-pair results. When Frank–Wolfe is selected, the
same pass also records full row sums and the maximum absolute entry used to scale the floating proposal. `pre_check` consumes those
facts instead of repeating its own sign and sum loops.

A selected check does not assume that an earlier disabled check passed. Nonnegative off-diagonal acceptance still requires a
factually mode-valid diagonal. Coposit uses the shared dynamically sized packed support, so principal-face enumeration has no
dimension-63 limit. The callback preserves its own timeout and resource outcomes; the wrapper adds cooperative timeout checkpoints
throughout. Each check remains independently switchable; in particular, callers comparing against FracESSA's earlier corpus
ablation can disable the exact definiteness factorization whose cost did not pay on the matrices that survived cheaper checks.

`pre_check::classify(matrix, checks, final_classifier)` supports models such as Hadeler and Dickinson that determine ordinary and
strict copositivity together. It runs every selected pre-check once and retains partial exact facts: a zero witness proves only
not-strict, weak positive semidefiniteness proves ordinary copositivity, a nullity-one kernel sign test also decides strictness, and
positive definiteness proves both. If facts remain unresolved, it calls the supplied combined classifier once on the input matrix
and merges the two Boolean results. It never obtains classification by running the pre-check once per mode.

The pre-check deliberately has no Z-matrix, FracESSA candidate/Hessian, or experimental KKT route. Floating Frank–Wolfe with exact
witness verification is only a witness search: stationarity or exhaustion of its $n$-step budget is inconclusive and falls through
to the selected final algorithm.

## Connected Components

`cpp/include/coposit/connected_components.hpp` is a separate structural transformation, not a pre-check. Its
`connected_components::visit(negative_neighbors, visitor)` function consumes the graph already produced by `matrix_scan` and
streams its ordered components through one reused `support` bitset. It does not inspect or copy the matrix and makes no yes/no
decision. Returning `false` from the visitor stops traversal immediately.

`cpp/include/coposit/component_pipeline.hpp` is the caller-selected composition of that transformation, the pre-check, and a
caller-supplied final algorithm. Its switches remain independent:

```cpp
coposit::component_pipeline::options processing;
processing.connected_components = false; // keep default-on pre-checks on the whole matrix
// processing.pre_checks_enabled = false; // independently disable all pre-checks
return coposit::component_pipeline::check(matrix, mode, processing, final_algorithm);
```

`pre_checks_enabled` and `connected_components` both default to on, and the nested seven individual pre-checks also default to on.
Either stage can be disabled without disabling the other, and each individual pre-check remains switchable. With components off,
the wrapper calls `pre_check` on the whole matrix. With components on, one root scan records the negative graph and all facts needed
by the selected cheap checks. An exact whole-matrix certificate may finish immediately. Otherwise the stored graph is traversed
without another matrix scan.

A connected input reaches the remaining exact Frank–Wolfe or definiteness check and the final algorithm by reference, with no
matrix copy. For a disconnected input, rejection-only principal faces have already been checked globally; every component matrix
is then processed immediately. One index vector is reserved once and reused only to map the current component bitset into its dense
principal matrix; no component list or `vector<vector<size_t>>` is retained. Selected row sums and scalar facts are accumulated
during that single copy. The pre-check and final algorithm run on those smaller matrices. When `small_dimension` is selected, a
singleton component is classified from its cached diagonal sign without allocating a one-by-one matrix. Ordinary and strict
results are the logical AND across components; combined classification continues after a strict-only boundary result because a
later component may still disprove ordinary copositivity.

The dynamic packed support representation imposes no dimension-63 limit. No maintained model calls this shared pipeline yet;
existing model-local Zischg variants retain their isolated implementations.

## Maintained Models

### Dutour 2018

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> mode-dependent diagonal and two-generator tests
  -> accept a cone whose exact Gram matrix is entrywise nonnegative
  -> otherwise choose the largest exact b_ij^2 / (b_ii b_jj)
  -> split through v_i + v_j and visit both child cones depth-first
  -> ordinary- or strict-copositivity Boolean
```

`dutour_2018` is the faithful Boolean baseline for Mathieu Dutour Sikirić's Polyhedral Common implementation. Its pair decomposition
and copositivity traversal entered that repository in 2018; the maintained model was verified against commit
`d2252bc89d991fa6df9750ac9647e19b6a9aca02`. It retains the original mathematical decisions while storing only the exact Gram matrix,
using FLINT integers, comparing ratios without fractions, and updating one row and column per split. Full provenance and related
papers are in `models/baselines/dutour_2018/ALGORITHM.md`.

The former low-dimensional criteria and sign certificates are deliberately absent from this baseline because they are not part of
the source implementation. They may appear only in a separately named Coposit model. The connected-component graph reduction is
also not in the maintained path yet; its proof, benchmark, and former fixed-64 implementation are preserved for later work.

### Hadeler 1983

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> visit every nonempty principal subset in increasing cardinality and numeric-mask order
  -> use direct exact criteria through order three
  -> factor every larger principal matrix once with fraction-free LDLT
  -> positive determinant: pass; negative determinant: solve C y = -1 once
  -> zero determinant: read the LDLT rank and recover one exact kernel vector only when nullity is one
  -> ordinary- or strict-copositivity Boolean after at most 2^n - 1 subsets
```

`hadeler_1983` is the exact optimized FracESSA baseline for K. P. Hadeler's 1983 principal-submatrix criterion. It is pinned to
FracESSA commit `36902a3d`, the last optimized Hadeler revision before the cone replacement. Each negative-determinant principal
matrix performs one retained `k × 1` solve; the model never constructs a full inverse or adjugate. The former fixed-width subset mask
is replaced by a dynamic vector that preserves the old traversal order without retaining the dimension-63 limit. Its exact direct
rules through order three live in the shared low-order header and are called at the same traversal points. The model deliberately
omits connected components and every later cone or hybrid route. Combined classification follows the ordinary subset traversal,
records a strict-only singular or low-order zero, and continues until ordinary copositivity has also been decided. Full provenance
is in `models/baselines/hadeler_1983/ALGORITHM.md`.

### Dickinson 2019

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> visit every uncovered nonempty principal subset in increasing cardinality and numeric-mask order
  -> reject immediately when the shared exact order-at-most-three criterion fails
  -> skip I when a retained u has support(u) ⊆ I ⊆ support≥0(Au)
  -> solve A_I w = 1 exactly, or recover and orient one exact LDLT kernel vector when A_I is singular
  -> reject when w ≤ 0 gives Dickinson's nonnegative negative witness -w
  -> in strict mode reject immediately when the generated vector is a nonnegative zero
  -> in ordinary mode retain that zero as a valid signature and continue
  -> in combined mode retain the zero, record strict failure, and finish the ordinary certificate
  -> return true when the finite traversal completes under the selected mode
```

`dickinson_2019` implements Peter J. C. Dickinson's 2019 finite certificate construction. Fraction-free integer numerators preserve
the paper's nonsingular solves, and one exact LDLT-derived integer kernel vector preserves its singular branch for every nullity.
Ordinary mode implements the paper's certificate algorithm. Strict mode returns `false` immediately when any generated certificate
vector is a nonnegative zero. Corollary 5.3 proves the strict converse: if traversal completes, its
certificate contains every minimal zero up to positive scaling, so completing without a zero proves strict copositivity. A passing
low-order direct check still continues through normal Dickinson coverage and certificate construction. The model retains only the support and product-sign data
needed for later coverage. Both sets and the current subset use the shared packed support class, while an ordered index vector
remains beside the current subset for direct principal-matrix access. Certificate signatures are partitioned by their lowest
support index, so a subset checks only buckets that can possibly cover it. Full provenance and the strict fidelity boundary are in
`models/baselines/dickinson_2019/ALGORITHM.md`.

### Support-Pruned Dickinson

```text
integer matrix
  -> retain Dickinson's direct tests, exact principal solves, nullspace branch, and coverage signatures
  -> generate supports recursively in the same cardinality and numeric-mask order
  -> when a signature has N_A(u) equal to the full universe, forbid every strict superset of support(u)
  -> cut off those upward branches before later supports are emitted
  -> use ordinary Dickinson interval lookup for every signature with a proper upper bound
  -> strict-copositivity Boolean after the surviving traversal completes
```

`support_pruned_dickinson` is a separate Coposit-created hybrid. It applies FracESSA's recursive forbidden-support generator only to
Dickinson signatures whose product is nonnegative in every coordinate; for those signatures Dickinson's coverage interval contains
every superset. It deliberately does not copy FracESSA's KKT solver or promote bounded Dickinson intervals to unsafe upward rules.
The original `dickinson_2019` strict baseline remains unchanged. The proof, generator state, exact decision flow, source boundary,
and limitations are in `models/support_pruned_dickinson/ALGORITHM.md`.

### Nullity Support-Pruned Dickinson

```text
integer matrix
  -> retain the complete Support-Pruned Dickinson traversal and nonsingular branch
  -> recover an exact LDLT nullspace basis for every uncovered singular principal matrix
  -> nullity one: choose the better orientation of the unique direction
  -> nullity two: sweep every exact sign interval, breakpoint, and the direction at infinity
  -> larger nullity: choose the best signed exact LDLT basis vector
  -> maximize the exact number of covered supports at larger remaining cardinalities
  -> retain the selected signature and apply the ordinary/global support prunes
```

`nullity_support_pruned_dickinson` is a separate Coposit-created variant copied from `support_pruned_dickinson`. Its nullity-two
sweep is globally optimal for its documented single-certificate future-support score; its higher-nullity rule is deliberately
limited to the exact basis columns and their signs. The shared LDLT factorization exposes the complete exact basis without a second
elimination. Full formulas, guarantees, source boundaries, and limitations are in
`models/nullity_support_pruned_dickinson/ALGORITHM.md`.

### RHS Dickinson

```text
integer matrix
  -> run ordinary Dickinson coverage and factor each uncovered principal support
  -> solve A_I u_0 = 1 and retain its exact negative-witness behavior
  -> for every nonsingular support above order one, solve A_I d_k = e_k from the same factorization
  -> sweep every exact sign interval of u_0 + t d_k for t >= 0
  -> retain the single vector with the widest Dickinson coverage interval
  -> reject immediately on any generated nonnegative zero, or return true after the finite traversal
```

`rhs_dickinson` is a Coposit-created experiment using Dickinson's explicit permission to replace the all-ones right-hand side by any
strictly positive vector. It searches only the exact rays `1 + t e_k`, not the complete positive right-hand-side cone. Rational
breakpoints are converted to positively scaled integer vectors, so all decisions remain exact and the integer-only core is unchanged.
The published `dickinson_2019` model remains separate. Full derivation, traversal, scoring, source boundary, and limitations are in
`models/rhs_dickinson/ALGORITHM.md`.

### Frank–Wolfe Dickinson

```text
integer matrix
  -> search the simplex centre and up to seven vertex starts with bounded Frank–Wolfe steps
  -> reconstruct every promising proposal as a nonnegative integer vector
  -> reject only after exact verification of z^T A z <= 0
  -> otherwise run the maintained strict-only Dickinson certificate traversal
```

`frank_wolfe_dickinson` is a Coposit-created witness-first variant. A globally power-of-two-scaled floating view proposes directions,
but no floating sign is trusted. The exact verifier or the maintained Dickinson fallback makes every classification. The floating
matrix is not materialized: the search retains one product vector and converts only selected columns. Full formulas, fixed search
bounds, reconstruction, correctness boundary, provenance, and limitations are in `models/frank_wolfe_dickinson/ALGORITHM.md`.

### Exact One-Step Frank–Wolfe Dickinson

```text
integer matrix
  -> calculate exact row sums and the simplex-centre value
  -> choose the first minimum-row-sum coordinate vertex
  -> minimize exactly along that one centre-to-vertex segment
  -> scale the rational minimizer to integer weights and verify its quadratic value exactly
  -> otherwise run the maintained strict-only Dickinson certificate traversal
```

`one_step_frank_wolfe_dickinson` is a separate Coposit-created variant with no floating arithmetic, tolerance, reconstruction grid,
iteration, or restart. Its one exact rational line test can only reject; Dickinson remains the complete decision path. Full formulas,
endpoint handling, exact representation, source boundary, and limitations are in
`models/one_step_frank_wolfe_dickinson/ALGORITHM.md`.

### Pairwise Frank–Wolfe Dickinson

```text
integer matrix
  -> search the simplex centre and up to seven vertex starts
  -> choose a global toward vertex and the worst active away vertex
  -> transfer at most the away coordinate's mass using a closed-form feasible-segment line minimum
  -> reconstruct promising proposals as nonnegative integer vectors and verify them exactly
  -> otherwise run the maintained strict-only Dickinson certificate traversal
```

`pairwise_frank_wolfe_dickinson` is a separate Coposit-created variant of `frank_wolfe_dickinson`. Its floating proposal uses the
pairwise Frank–Wolfe direction `e_t - e_a`, so an unhelpful active coordinate can be removed in one step instead of shrinking every
active coordinate proportionally. The proposal cannot classify a matrix: only an exactly verified nonpositive integer witness or the
maintained Dickinson fallback decides the result. Full formulas, feasibility proof, source boundary, and limitations are in
`models/pairwise_frank_wolfe_dickinson/ALGORITHM.md`.

### Support-Polished Frank–Wolfe Dickinson

```text
integer matrix
  -> run the bounded Frank–Wolfe proposal search and exact dyadic reconstruction
  -> if reconstruction is positive, retain the proposal's pre-rounding active support
  -> test that support once by exact direct formulas, KKT solve, or one nullspace vector
  -> reject only on an exact negative vector or nonnegative zero
  -> otherwise run the maintained strict-only Dickinson certificate traversal
```

`support_polished_frank_wolfe_dickinson` is a separate Coposit-created variant of `frank_wolfe_dickinson`. It adds no support
enumeration: each promising proposal can trigger one exact test of its own active face. Full equations, decision boundaries, and
limitations are in `models/support_polished_frank_wolfe_dickinson/ALGORITHM.md`.

### Danninger 1990

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> eliminate the fixed first coordinate
  -> test B on p^T y >= 0 and aB - pp^T on p^T y <= 0
  -> triangulate mixed-sign half-cones through primitive integer pair rays
  -> recurse on every transformed child in dimension n - 1
  -> ordinary- or strict-copositivity Boolean
```

`danninger_1990` is Coposit's exact ordinary- and strict-mode reconstruction of Gabriele Danninger's 1990 proceedings reduction. The accessible
historical evidence supports the one-coordinate minimization, sign-defined half-cones, boundary rays, and dimension-reducing
recursion; no original program or complete public paper copy was available to verify concrete control flow. The fixed first pivot,
exact low-dimensional base tests, standard staircase order, and traversal are pinned to the retained FracESSA experiment. The exact
citation, reconstruction limits, combined ordinary-tree classification, and experiment checksum are in
`models/baselines/danninger_1990/ALGORITHM.md`.

### COPOMATRIX 2011

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> project the fixed first coordinate
  -> check one complete principal child
  -> normalize pivot signs conceptually to -1, 0, and 1
  -> triangulate only the negative half-simplex through Xu-Yao midpoint decomposition
  -> represent normalized midpoints by primitive integer boundary rays
  -> recurse on every order-(n - 1) child
  -> ordinary- or strict-copositivity Boolean
```

`copomatrix_2011` implements Jia Xu and Yong Yao's ordinary 2011 COPOMATRIX mathematics and its exact strict adaptation. It preserves
the source's fixed first-coordinate projection, complete principal child, negative-side Schur children, and Vmatrix midpoint
decomposition. Algorithm 2 advances complete work-set frontiers; this maintained baseline instead uses a Danninger-style
depth-first scheduler, visiting positive deletion before negative deletion, and checks every diagonal early. Positive diagonal
congruence and primitive integer boundary rays avoid the paper's rational normalization without changing a decision. The
depth-first modification is retained because it reaches rejection witnesses sooner, keeps fewer matrices live, and avoids the
frontier node-limit failures observed in the direct comparison recorded in `aidocs/CHANGES.md`. Full
provenance, strict equality rules, formulas, traversal, and fidelity boundaries are in
`models/baselines/copomatrix_2011/ALGORITHM.md`.

### Adaptive Dutour-Danninger

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> use direct exact criteria through order three
  -> choose the first Danninger pivot whose reduction has at most two children
  -> otherwise perform one maximum-ratio Dutour cone split
  -> restart the adaptive choice independently in every child
  -> strict-copositivity Boolean, or no result if same-order refinement does not terminate
```

`adaptive_dutour_danninger` is a maintained Coposit-created model. It owns complete local copies of the direct terminal criteria,
Danninger reduction, and Dutour split under `models/adaptive_dutour_danninger/`; it has no source or build dependency on the
historical experiment. A Danninger pivot is used only when its sign pattern creates one or two children. If every pivot is wider,
one Dutour split changes the current cone and both children reconsider the complete choice. Its exact routing, source boundaries,
termination limit, and difficult matrix structures are in `models/adaptive_dutour_danninger/ALGORITHM.md`.

### Adaptive Dutour–COPOMATRIX

```text
integer matrix
  -> use direct exact criteria through order three
  -> choose the first COPOMATRIX pivot whose projection has at most two children
  -> otherwise perform one maximum-ratio Dutour cone split
  -> after 100 consecutive Dutour splits on a branch, force COPOMATRIX at pivot zero
  -> reset the branch counter after every order-reducing COPOMATRIX child
  -> strict-copositivity Boolean from a finite exact recursion tree
```

`adaptive_dutour_copomatrix` is a Coposit-created model isolated under `models/adaptive_dutour_copomatrix/`. Its branch-local cutoff
prevents same-order Dutour refinement from continuing indefinitely merely because COPOMATRIX currently has more than two children.
The principal/Schur projection, Xu–Yao Vmatrix decomposition, exact Dutour split, cutoff semantics, termination argument, and known
difficult inputs are in `models/adaptive_dutour_copomatrix/ALGORITHM.md`.

### Adaptive Sponsel–COPOMATRIX

```text
integer matrix
  -> use mode-dependent exact criteria through order three
  -> count the exact COPOMATRIX children for every pivot and retain the first minimum
  -> use that pivot immediately when its count is at most two
  -> otherwise apply Sponsel's ordinary positive-semidefinite or strict positive-definite H certificate and exact Bundfuss split
  -> after 1,000 consecutive Sponsel splits on a branch, force COPOMATRIX at the same minimum-child pivot
  -> reset the branch counter after every order-reducing COPOMATRIX child
  -> ordinary- or strict-copositivity Boolean from a finite exact recursion tree
```

`adaptive_sponsel_copomatrix` is a Coposit-created model isolated under `models/adaptive_sponsel_copomatrix/`. It replaces the
same-order Dutour operation of `adaptive_dutour_copomatrix` with Sponsel's mode-dependent exact `H` certificate and inherited
Bundfuss rational edge split. Ordinary mode retains zero diagonals, strict edge inequalities, the zero-pivot COPOMATRIX rule, and
positive semidefiniteness; strict mode uses positive diagonals, equality rejection, and positive definiteness. Its routing, split
counter, projection, exact arithmetic, termination argument, source boundaries, and known difficult inputs are in
`models/adaptive_sponsel_copomatrix/ALGORITHM.md`.

### Adaptive Zischg–Sponsel–COPOMATRIX

```text
integer matrix
  -> retain Adaptive Sponsel–COPOMATRIX routing and its 10,000-split forced projection
  -> after every principal or Schur projection child, build its exact negative-entry graph
  -> connected child: restart the adaptive model at streak zero
  -> disconnected child: solve every component principal block independently at streak zero
  -> do not component-decompose the public root or ordinary Sponsel children
```

`adaptive_zischg_sponsel_copomatrix` is a legacy copy of `adaptive_sponsel_copomatrix` under `models/legacy/`.
Its only mathematical addition is Johannes Zischg and Immanuel M. Bomze's component theorem, extended exactly to strict copositivity
and placed at the shared gateway through which all COPOMATRIX projection children pass. Cross-component entries are nonnegative, so
a projected matrix is strictly copositive exactly when all negative-graph component blocks are strictly copositive. The complete
proof, placement boundary, traversal, source separation, and difficult inputs are in
`models/legacy/adaptive_zischg_sponsel_copomatrix/ALGORITHM.md`.

### Safi 2021

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> apply mode-dependent simplex-vertex and center rejection
  -> certify an entrywise nonnegative Gram matrix after the mode-dependent diagonal check
  -> otherwise apply the ordinary or strict simplex-center radius certificate
  -> slice maximally from the first endpoint of the first negative Gram entry
  -> partition the remainder into at most n - 1 child simplices and continue depth-first
  -> ordinary- or strict-copositivity Boolean, or no result if refinement does not terminate
```

`safi_2021` implements Mohammadreza Safi, Seyed Saeed Nabavi, and Richard J. Caron's 2021 ordinary SNC simplex-partition algorithm
and its exact strict adaptation. It preserves the paper's maximal intersection parameters and child construction, uses the
deterministic choices from the verified FracESSA experiment where the paper leaves a choice open, and changes only the terminal inequalities needed for strict
copositivity. Fraction-free integer numerators and positive denominators preserve all rational signs and bounds. Full provenance and
the strict fidelity boundary are in `models/baselines/safi_2021/ALGORITHM.md`.

### Bundfuss 2008

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> apply the mode-dependent vertex and selected-edge rejection
  -> accept an entrywise nonnegative simplex Gram matrix
  -> otherwise select its minimum edge and apply the Bundfuss lambda formula
  -> evaluate both convex-split children and traverse their descendants depth-first
  -> ordinary- or strict-copositivity Boolean, or no result if refinement does not terminate
```

`bundfuss_2008` implements Stefan Bundfuss and Mirjam Dür's 2008 ordinary-copositivity
simplicial-partition family, with concrete formulas and control flow pinned to J. M. G. Salmerón's preserved 2018 `bundfuss`
implementation. The maintained code independently implements the same minimum-edge selection, three lambda candidates, convex
split, child evaluation order, and LIFO work list; mode-dependent equality handling supplies the strict adaptation. Each node is a
content-reduced positive integer scaling of the rational Gram matrix, so no rational matrix storage or floating-point
epsilon is needed. It does not include Salmerón's separate `zbund` monotonicity route.

The exact baseline is known not to finish quickly on boundary matrix 9161; this is an unresolved run, not a negative classification.
The provenance, license boundary, formulas, and known termination limitation are in `models/baselines/bundfuss_2008/ALGORITHM.md`.

### Sponsel 2012

```text
integer matrix
  -> nonempty, square, and symmetry validation
  -> apply mode-dependent vertex and two-generator rejection
  -> accept an entrywise nonnegative simplex Gram matrix
  -> remove positive off-diagonals and test the remainder for exact positive semidefiniteness or definiteness
  -> otherwise apply the inherited Bundfuss minimum-edge lambda split
  -> traverse both children depth-first
  -> ordinary- or strict-copositivity Boolean, or no result if refinement does not terminate
```

`sponsel_2012` implements Julia Sponsel, Stefan Bundfuss, and Mirjam Dür's 2012 ordinary `H`-enhanced simplicial-partition framework
and its exact strict adaptation. The ordinary certificate accepts a stripped positive-semidefinite matrix; strict mode requires
exact positive definiteness instead. A reusable fraction-free LDLT factorization
proves that condition without eigenvalues or tolerances. Nodes not certified by `H` retain the complete minimum-edge selection,
three-lambda split, exact child construction, and LIFO traversal of `bundfuss_2008`. The model uses no SDP solver and does not
implement the paper's stronger semidefinite-feasibility certificate. Full derivation, source boundaries, formulas, and difficult
inputs are in `models/baselines/sponsel_2012/ALGORITHM.md`.

### Exact One-Step Frank–Wolfe Sponsel

```text
integer matrix
  -> retain Sponsel's exact vertex, edge, entrywise-nonnegative, and strict H tests
  -> after H fails, start at the current simplex centre and choose the minimum-row-sum vertex
  -> minimize exactly along that one centre-to-vertex segment
  -> reject only when the rational line point has an exact nonpositive integer quadratic value
  -> otherwise retain Sponsel's Bundfuss split and depth-first traversal unchanged
  -> strict-copositivity Boolean, or no result after a resource limit
```

`frank_wolfe_sponsel` is a Coposit-created variant of `sponsel_2012`. Its Frank–Wolfe phase has no floating arithmetic or iteration:
row sums determine one conditional-gradient direction, and the closed-form rational line minimum is evaluated by homogeneity with
FLINT integers. A positive result supplies no certificate and falls through to the unchanged Sponsel partition. Full derivation,
correctness boundary, source separation, and difficult inputs are in `models/frank_wolfe_sponsel/ALGORITHM.md`.

### FracESSA

```text
integer matrix A
  -> nonempty, square, and symmetry validation
  -> form the exact symmetric game Q = -A
  -> enumerate FracESSA first-order KKT supports in cardinality and numeric-mask order
  -> reject immediately when the shared exact order-at-most-three test fails on a generated principal face of A
  -> solve each nonsingular reduced support system exactly
  -> require positive support probabilities and the full outside-payoff inequalities
  -> return false on the first exact candidate payoff >= 0
  -> otherwise prune supersets whose KKT payoffs must remain negative
  -> return true after the complete surviving support search
```

`fracessa` is a Coposit-created adaptation of FracESSA's exact safe candidate search. It decides whether the minimum of the input
quadratic form on the simplex is positive by inspecting exact first-order KKT payoffs for `-A`. It returns false at the first
nonnegative payoff and otherwise exhausts the surviving supports. It deliberately omits every ESS/NSS,
inertia, local-maximum, Schur-reduction, and second-order stability test. Superset pruning remains valid for the global value: a
larger-support KKT candidate cannot improve the payoff of a KKT candidate whose support it contains, and every global maximizer is
KKT. Before doing KKT work on a generated support of order at most three, the model uses the shared direct strict criterion; a pass
still proceeds through the normal candidate test. Supports and forbidden supports use the shared packed representation with
`ceil(n / 64)` words, so the model has no fixed-width dimension limit. Full equations, completeness and pruning proofs, source
revision, and limitations are in `models/fracessa/ALGORITHM.md`.

### Zischg Level 2 Variants

```text
integer matrix A
  -> build the packed negative-entry adjacency once
  -> retain the base model's cardinality-ordered support traversal
  -> for every generated support above order three, test its induced negative graph
  -> disconnected: certify it from smaller connected components and skip the base model's expensive work
  -> connected: run the corresponding maintained Hadeler, Dickinson, or FracESSA support calculation
```

`zischg_hadeler`, `zischg_dickinson`, and `zischg_fracessa` implement the Level 2 strict-copositivity reduction derived locally from
Johannes Zischg and Immanuel M. Bomze's negative-sign-graph decomposition. They deliberately omit Level 1: the complete input is not
split. One exact sign scan builds packed adjacency masks, and each support is tested by packed intersection and breadth-first
frontiers. A complete negative graph and a root adjacent to the rest of a support bypass the BFS immediately.

The disconnected-support proof applies directly to Hadeler's principal subsets. In Dickinson it also preserves the minimal-zero
certificate conclusion; connectivity is checked before signature coverage so the variant can avoid both the signature scan and
factorization. In FracESSA, any nonnegative payoff on a disconnected support implies a nonnegative witness on one connected
component, so disconnected supports may be omitted from the sign decision even though a strict matrix's largest negative payoff can
itself occur on a disconnected support. Each variant owns an independent copy matching its corresponding maintained base calculation.
Full proofs, source boundaries, control flows, and known
difficult inputs are in the three model-local `ALGORITHM.md` files.

## CLI

`cpp/model_main.cpp` combined with `models/baselines/dutour_2018/solver.cpp` builds as the user-facing `coposit`; the same protocol builds
the other maintained targets `danninger_1990`, `copomatrix_2011`, `adaptive_dutour_danninger`, `adaptive_dutour_copomatrix`,
`adaptive_sponsel_copomatrix`, `adaptive_zischg_sponsel_copomatrix`, `hadeler_1983`,
`dickinson_2019`,
`support_pruned_dickinson`, `nullity_support_pruned_dickinson`, `rhs_dickinson`, `frank_wolfe_dickinson`,
`one_step_frank_wolfe_dickinson`,
`pairwise_frank_wolfe_dickinson`, `support_polished_frank_wolfe_dickinson`, `safi_2021`,
`bundfuss_2008`, `sponsel_2012`, `frank_wolfe_sponsel`, `fracessa`, `zischg_hadeler`, `zischg_dickinson`, and `zischg_fracessa`. All read
`dimension#upper-triangle-values` from a file or standard input. Values are exact base-10 integers in row-major upper-triangle order.
The CLI prints `true` or `false`; malformed input or an unresolved resource limit exits nonzero with an explanatory error.

## Python API

`python/pycoposit/` follows FracESSA's maintained wrapper structure with a one-matrix native adapter, a lazy sequential runner, and a
bounded multiprocessing runner. `compute_matrix()`, `run()`, and `run_multiprocessing()` all require an explicit model identifier.
A single `Matrix` returns one result dictionary; an iterable returns an iterator. Sequential results preserve input order, while
multiprocessing results use completion order.

All three entry points accept `preprocessing="none"`, `"connected_components"`, `"pre_checks"`, or `"both"`. The default keeps the
linked model unchanged. The other choices wrap it in the shared exact component/pre-check pipeline; the two stages remain
independently selectable and no model implementation is modified.

Each result records the algorithm, selected mode, matrix ID, status, separate ordinary and strict classification fields, native
elapsed nanoseconds, error message, and input metadata. Individually selected modes populate only their matching predicate.
`mode="both"` populates both fields through one traversal for Hadeler 1983, Dickinson 2019, and Danninger 1990; other models reject
that mode. Parse, execution, timeout, and node-limit outcomes have `None` classifications. The multiprocessing runner
bounds outstanding work, detects dead workers, and cancels workers when its iterator closes early. It intentionally has no
per-matrix timeout yet.

`python/run_results.py` is the timed reference-results boundary. It selects one maintained model, optional benchmark-set flags and
dimension or matrix-ID intervals, a cooperative per-matrix timeout, an explicit parent CPU, and explicit non-overlapping worker CPUs.
Its preprocessing selector forwards the same four choices and records each non-default choice canonically in `parameters`.
Multiple set flags select their union. The dispatcher and its bounded SQLite-writer
thread are pinned to the parent CPU. The writer drains the currently queued results into one transaction; its queue holds at most
twice the worker count and applies backpressure when full, so a batch is bounded by the same limit. One persistent single-threaded
native worker is pinned to each selected worker CPU and loads its model once. Standard local reference runs use CPU 3 for the parent
and CPUs 4 through 7 for the workers. At the deadline, the parent sends
`SIGUSR1`; the native handler sets only a signal-safe flag, and safe model checkpoints return a distinct timeout outcome without
terminating the worker. If a native call has not returned one second after that signal, the parent records the same unresolved timeout,
terminates and replaces that worker, and thereby bounds one long native operation. Ctrl-C stops new assignments and drains active
matrices. Every result has an optional free-text parameter description. Hadeler 1983 is the fixed
baseline and stores an empty binary hash, so
its existing matrix/parameter rows are skipped across rebuilds. Every other model uses the native-extension SHA-256. Rows with the same
identity are skipped unless `--rerun` is requested; `--retry-timeouts` selects and replaces only timeout rows with that identity.
The five simplex-refinement models record `node_limit` if they would exceed 50,000 simultaneously unfinished nodes.

## Build And Dependencies

The maintained project needs CMake 3.18 or newer, a C++17 compiler, Python 3.11 or newer, FLINT, MPFR, and GMP. CMake fetches
pybind11 3.0.4 for the wrapper and GoogleTest 1.14 for tests.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

## Corpus And Evidence

`testdata/Copos_testdata.sqlite3` is the slim maintained corpus. Its eleven-column `matrices` table contains 2,442 exact integer
matrices with dimensions 1 through 3,361: stable ID, dimension, exact matrix data, expected strict result, nullable
ordinary-copositivity result, optional source, optional family, and the independent `smoke_set`, `representative_core`, `stress_test`,
and `scale_set` Boolean flags. Matrices up to 500 KB keep their packed upper triangle inline. The 201 larger rows use
`file:matrices/<matrix_id>.mtx` references relative to the database directory; these files are exact symmetric integer Matrix Market
arrays and are resolved by the Python corpus readers. Ordinary classifications comprise 674 strict matrices, 1,179 copositive
boundary matrices, and 589 non-copositive matrices. No ordinary classification remains unknown. The nullable field still reserves
`NULL` for “not established,” never false.
The guarded dated benchmark assignment and exact composition are documented in `BENCHMARK_SETS.md`. The `results` table stores canonical
matrix/model/parameter-text/native-binary outcomes with `ok`, `timeout`, `node_limit`, and
`error` kept distinct. Hadeler 1983 deliberately has one hash-independent baseline result per matrix and parameter text. There are no
provenance tables or manually created indexes. The byte-exact FracESSA source database is preserved at
`reference/fracessa/testdata/Copos_testdata.original.sqlite3.xz`; its decompressed SHA-256 remains
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`.

The maintained corpus keeps only the lowest-ID representative under whole-matrix positive projective permutation
`B=cP^TAP`, with `c>0` and one simultaneous row-and-column permutation `P`. The 2026-08-09 exact audit removed 155 redundant
matrices from 58 classes and merged their provenance into the retained records. Positive diagonal congruences `DAD` with nonconstant
`D` remain deliberate distinct inputs because they test different exact coefficient scales.

IDs 9657 through 9681 are exceptional extremal order-6 equality cases from Hildebrand's case-34 parametrization, selected after
positive diagonal congruences made the Dutour baseline exceed a 250 ms screen. IDs 9682 through 9710 are Väliaho-style almost
strictly copositive matrices `D*(nI-J)*D` through order 12. They are positive semidefinite boundary cases, not exceptional matrices.
Horn and Hoffman-Pereira remain IDs 9162 and 9163; their previously blank provenance and family fields are now populated.

IDs 9711 through 9737 add permutation/projective-new rational half-angle instances from Cases 13.1 and 18 of the complete
Afonin-Hildebrand-Dickinson order-6 classification. IDs 9738 through 9756 are positive diagonal congruences of the explicit
Kostyukova-Tchemisova 9-by-9 extremal extension `B(A,b)` from their 2026 Example 5. All 46 exact boundary matrices exceeded 200 ms
in an initial selected-model screen; 23 exceeded one second, and 15 exceeded a five-second confirmation cutoff. Each row has an
exact checked nonnegative zero and paper-backed copositivity, so timed runs remain unresolved rather than being recorded as false.

IDs 9757 through 9955 add 199 non-isomorphic Hoffman-Pereira exceptional boundary classes generated from Brendan McKay's connected
graph catalogs: every new class through order 9 and the first 130 qualifying order-10 classes. IDs 9956 through 9959 add the four
remaining exact integer matrices from Kostyukova-Tchemisova Examples 1 and 5. ID 9960 is Štrekelj-Zalar's rational strict exceptional
5-by-5 matrix `C`. The stdlib-only importer and retained graph6 catalogs reproduce the exact selection; broader order-10 generation
and hardness screening is deferred in the Coposit TODO.

The source batch originally stored at IDs 9961 through 10160 contributed 200 literature occurrences: 24 rational Hildebrand `COP(5)`
extreme forms, Baston's 54
all-orders and 18 cyclic basic extreme forms through order 64, 73 Johnson-Reams generalized Horn matrices at every odd order from 7
through 151, and 31 Dickinson-de Zeeuw Table-2 cop-irreducible matrices with stability number 3 or 4. ID 10132 duplicated ID 9957
under simultaneous row-and-column permutation and was removed during projective deduplication; its provenance is retained on ID 9957,
leaving 199 distinct representatives in the original batch range. These are all exact boundary
matrices and do not extend the Hoffman-Pereira graph-catalog sample. The Johnson-Reams construction works at every odd order; 151 is
only the finite endpoint that brings this batch to the approved limit of 200 new matrices.

IDs 10161 through 10244 append 84 more Johnson-Reams generalized Horn boundary matrices while leaving every odd order from 7 through
151 unchanged. The added odd orders start 163, 175, 181, and 199, then use an irregular roughly-ten-dimension spacing through 999.

IDs 10245 through 10304 add three exact Dickinson Case-9 order-6 extreme forms, three Hildebrand-Afonin order-6 forms outside
Parrilo's first sum-of-squares level, seven Laurent-Vargas direct sums outside every level of that hierarchy, and 47 Hildebrand
circulant extreme forms of orders 7 through 25. The circulant forms have exact rational parameters and minimal zero supports of
cardinality `n-2`; all 47 exceeded 250 ms in the Dutour screen, and six representative orders exceeded five seconds.

IDs 10305 through 10504 add the two strict perfect copositive seeds printed by Dannenberg-Schürmann and 198 consecutive applications
of their exact lifting lemma. The indefinite SPN seed `I` covers orders 3 through 102; the exceptional certificate `E` covers orders
5 through 104. Every lift is singular through duplicated trailing rows and columns yet remains strictly copositive, perfect, and in
the seed's component. Dutour exceeded five seconds on representative orders 20 and 10 respectively. The reproducible importer
checks the printed minimal-vector value and propagates a qualifying minimal vector across all 200 exact rows.

IDs 10505 through 10594 add three deterministic sparse small-integer stress matrices at each of 30 irregular dimensions from 51
through 2,997. For signed edge vectors $b_e=e_i\pm e_j$, let $G=\sum_e b_e b_e^T$, using two seeded pseudo-random Hamiltonian
cycles. The strict case is $I+G$. The boundary case is a direct sum of `[[1,-1],[-1,1]]` and an independent $I+G$ block, with
nonnegative zero `e1+e2`. The non-copositive case starts from $I+G$ and replaces one off-diagonal entry by
$a_{pq}=-(a_{pp}+a_{qq})$, making `(ep+eq)^T A (ep+eq)=-(a_pp+a_qq)<0`. Every entry has absolute value at most 10. The exact
generator is `testdata/import_high_order_small_integer_stress_2026_08_09.py`.

IDs 10595 through 10684 repeat the same 30 dimensions and three mathematical classes with dense pseudo-random data. Each column has
a unique seeded eight-coordinate fingerprint in `{-2,-1,1,2}^8`; their Gram products make at least 94.194% of upper-triangle entries
nonzero. A rank-`n-1` PSD regularizer preserves the boundary witness, identity regularization proves the strict case, and one seeded
off-diagonal replacement gives the non-copositive case an exact negative witness. The maximum observed absolute entry is 60, below
the requested bound of 100. `testdata/import_high_order_dense_randomized_stress_2026_08_09.py` is the exact NumPy-based generator.

The primary papers used in these research passes are retained under `research/papers/`, including the full order-6 classification,
the 2026 higher-dimensional extension, Hildebrand's 2012 and 2016 constructions, Baston 1969, Baumert 1966 and 1967, Johnson-Reams
2008, Dickinson 2019, Dickinson-de Zeeuw 2021, Dannenberg-Schürmann 2023, Laurent-Vargas 2023, Hildebrand-Afonin 2024, and
Štrekelj-Zalar 2025. McKay's
CC BY 4.0 connected graph catalogs for orders 5 through 10 are retained under
`research/data/mckay_connected_graphs/` with source checksums.

The Dutour 2018 model's focused suite retains the extracted exact regressions, adds input-boundary checks, checks a dimension above
the former 63-limit, and includes stress matrix 9161. Shared parser tests remain outside the model directory. The copied handoff
records the complete evidence, file map, remaining stress matrices, and research status.

The Danninger 1990 suite retains its extracted exact self-checks, input-boundary checks, a dimension above the former limit, and the
positive-definite order-15 tridiagonal matrix that is easy for direct Danninger recursion and pathological for the cone baseline.

The COPOMATRIX 2011 suite checks strict equality boundaries, its complete principal child, its all-negative Schur child, the exact
Xu-Yao negative staircase, arbitrary-precision scaling, a dimension above the former limit, and public input validation.

The Adaptive Dutour-Danninger suite exercises every direct terminal order, both one-child Danninger cases, the narrow two-child
reduction, the Dutour fallback, arbitrary-precision scaling, a dimension above the former limit, and public input validation.

The shared fraction-free LDLT suite verifies exact rank and one nonzero kernel vector across every rank below dimensions 2 through
12, including symmetric swaps, zero-diagonal coordinate additions, nullity one, and higher nullity.

The shared small-copositivity suite verifies exact ordinary, strict, and boundary decisions at orders one through three and indexed
principal-matrix access without constructing a temporary matrix. The Hadeler, Dickinson, and FracESSA model suites retain
higher-dimensional cases whose decisions exercise that shared path.

The Hadeler 1983 suite exercises both one-system negative-determinant outcomes, the one-sign, mixed-sign, and higher-nullity singular
branches, arbitrary-precision scaling, corpus stress matrix 9161, input validation, and an early rejection above the former limit.

The Dickinson 2019 suite exercises strict certificates, nonnegative zeros, negative witnesses, singular kernel vectors,
arbitrary-precision scaling, corpus stress matrix 9161, input validation, and a two-coordinate negative witness spanning the first
and second packed support words. A read-only native-module comparison classified all 1,412 corpus matrices through dimension 10
exactly as recorded.

The Safi 2021 suite checks strict boundary rejection, published slicing branches, arbitrary-precision scaling, dimensions above the
former limit, and input validation. Corpus equivalence is checked against the preserved fraction-free SNC experiment.

The Bundfuss 2008 suite checks strict boundary handling, a fractional `5/12` split, a partition-discovered negative direction,
arbitrary-precision scaling, a strict corpus branch, dimensions above the former limit, and input validation. Matrix 9161 is
excluded because faithful Bundfuss refinement may not terminate; it belongs in an externally timed corpus run.

The Sponsel 2012 suite checks the exact strict `H` certificate on stripped matrices, an all-negative positive-definite matrix that
would otherwise require extensive Bundfuss refinement, the inherited fractional split, strict rejection, arbitrary-precision
scaling, dimensions above the former limit, and public input validation.

The exact one-step Frank–Wolfe Sponsel suite checks an interior rational line witness missed by the vertex and selected-edge tests,
the unchanged strict `H` certificate and fractional split, arbitrary-precision scaling, dimensions above the former limit, and
public input validation.

The FracESSA suite checks strict interior minima, exact zero boundaries, a negative minimum, candidate-payoff signs without
second-order filtering, arbitrary-precision scaling, invalid public inputs, and strict and non-strict order-70 cases spanning
multiple support words. A direct native-module comparison classified all 1,412 corpus matrices of dimensions 1 through 10 exactly
as recorded.

## Ownership Boundary

Coposit owns its exact integer wrapper, exact matrix storage, exact low-order ordinary/strict copositivity criterion, reusable fraction-free
LDLT factorization, parser, shared model contract, packed support representation, self-contained solver models, and generic Python
execution wrapper. FracESSA may later consume a selected Coposit model directly. The dependency direction is `FracESSA -> Coposit`;
Coposit does not include FracESSA headers. FracESSA candidate traversal and forbidden-set policy remain private to the `fracessa`
model and do not enter shared infrastructure or the generic API; ESS reasons, game normalization, and logging remain outside Coposit.
