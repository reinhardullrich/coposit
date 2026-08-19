# coposit Project Overview

Last verified: 2026-08-17

## Purpose And Contract

coposit is a standalone C++17 experimentation project for exact copositivity (CP) and strict copositivity (SCP) decisions on nonempty
symmetric matrices. Its parsers accept exact rational input and return the denominator-cleared integer matrix together with the retained
common positive denominator. Every maintained algorithm receives only the integer matrix. The solving core stores no rational
matrices and makes no floating-point classification.

Every model implements the link-time contract
`coposit::model::solve(const matrix_integer&, copositivity_mode)` from `cpp/include/coposit/model.hpp`. A completed call returns the
Boolean for the selected predicate. A timeout, node limit, parse failure, or execution failure remains unresolved and is never
reported as `false`.

The eight literature baselines, `adaptive_sponsel_copomatrix`, `dense_bitset_dickinson`, all CBDD Dickinson
variants, `ceiling_pruned_dickinson`, `layered_singular_lift_dickinson`, `breadth_first_singular_lift_dickinson`, `sat_dickinson`,
`sat_halfspace_dickinson`, `sat_halfspace_rays_dickinson`, `sat_halfspace_lp_dickinson`, `sat_halfspace_milp_dickinson`,
`sat_halfspace_rays_lookahead_dickinson`,
`sat_halfspace_rays_wide_dickinson`, `cbdd_halfspace_dickinson`,
`kernel_cone_dickinson`, `affine_companion_dickinson`,
`wide_certificate_sat_dickinson`, `xxx`, `xxx_two`, `clingo_dickinson`, and `clingo_halfspace_dickinson` support
separately selected CP and SCP.
Hadeler 1983, Dickinson 2019, Dense-Bitset Dickinson, Danninger 1990, all CBDD Dickinson variants,
Ceiling-Pruned Dickinson, Kernel-Cone Dickinson, Layered Singular-Lift Dickinson, SAT Dickinson, SAT-Halfspace Dickinson,
SAT-Halfspace-Rays Dickinson, SAT-Halfspace-LP Dickinson, SAT-Halfspace-MILP Dickinson,
SAT-Halfspace-Rays Lookahead Dickinson, SAT-Halfspace-Rays Wide Dickinson,
Wide-Certificate SAT Dickinson,
XXX, XXX Two, Affine-Companion Dickinson, Clingo Dickinson, and Clingo-Halfspace Dickinson can additionally classify both predicates in one traversal.
Other coposit-created models reject CP and combined mode explicitly.

## Repository Structure

- `cpp/` — CMake project, experiment launcher, native Python boundary, shared tests, and model-independent headers.
- `cpp/include/coposit/` — exact integers and matrices, parsers, packed supports, exact factorization, preprocessing, timeout state,
  open-node limit, and the model call contract.
- `models/hadeler-based/<model>/` — maintained Hadeler-, Dickinson-, and FracESSA-derived models; the local `README.md` groups them.
- `models/zzz-old-do-not-use/<model>/` — preserved superseded or inapplicable models excluded from builds and benchmarks.
- `models/baselines/<model>/` — other literature or source baselines.
- `models/experiments/<model>/` — other coposit-created models and comparisons, including `adaptive_sponsel_copomatrix`.
- `python/pycoposit/` — thin native adapter, sequential runner, and bounded multiprocessing runner.
- `testdata/` — maintained SQLite corpus, schema, and exact external Matrix Market matrices.
- `docs/` — human-facing explanations.
- `aidocs/` — current agent documentation, routed by `aidocs/INDEX.md`.
- `research/` — papers, literature notes, and source material.
- `experiments/` — experiment implementations, raw results, and experiment-specific reports.

coposit owns the generic exact checker infrastructure. The dependency direction is `FracESSA -> coposit`; shared coposit code does
not include FracESSA headers or own ESS reasons, game normalization, candidate traversal, or logging. FracESSA-specific traversal and
forbidden-set policy remain private to the `fracessa` model.

## Input Boundary

The shared parser accepts:

- compact FracESSA matrices as `dimension#values`, using either a complete row-major upper triangle or the short circular-symmetric
  form containing the common diagonal followed by one value for each positive circular distance;
- exact integers, decimals, scientific notation, and FracESSA-only fractions written `[+|-]numerator/denominator`; and
- NIST Matrix Market `array` or `coordinate` matrices declared `symmetric`, with `integer`, `real`, `complex`, or `pattern` fields.

Matrix Market complex entries require zero imaginary parts. Other Matrix Market structures are rejected. The parser guarantees a
nonempty square symmetric matrix before a model is called; model entry points deliberately do not repeat those boundary checks.

The packed support representation uses `ceil(n / 64)` machine words. Shared infrastructure and maintained models must not introduce
the former dimension-63 limit.

The parser result also records whether FracESSA's short compact circular form supplied the input. Matrix Market and complete
upper-triangular inputs set this flag to false. The metadata is retained for clients such as FracESSA and ignored by coposit's
scale-invariant solving core.

## Experiment Interfaces And Processing Pipeline

The C++ experiment launcher requires an explicit model. Combined-capable models default to `both`; other models require a mode:

```bash
cpp/build/coposit --model adaptive_sponsel_copomatrix --mode strict '2#1,0,1'
cpp/build/coposit --model dickinson_2019 --mode non-strict matrix.mtx
cpp/build/coposit --model dickinson_2019 --mode both --diagnostics --timeout 30 matrix.mtx
```

`coposit --model MODEL [--mode strict|non-strict|both]` is the sole low-level model interface. It exposes every baseline and experiment,
diagnostics reporting, and one `--preprocessing on|off` switch
for the complete fixed pipeline. Its inventory is all eight literature baselines plus `adaptive_sponsel_copomatrix` and the
parameterized `wide_certificate_cbdd_dickinson` experiment. There are no
`fast`, `safe`, or implicit model aliases. The launcher dispatches to internal companions that each link exactly one model. There is
no C++ model registry, runtime factory, inheritance hierarchy, or executable containing several solvers. Python modules follow the
same one-model rule and retain all maintained experimental variants.

The optional `--diagnostics` flag starts one sleeping reporter and writes a status line to standard error every second.
Preprocessing publishes its current phase and a truthful row, vertex, center, iteration, or pivot counter where one exists. Solvers
do not query the clock in their hot loops: support models publish their small relaxed-atomic counters after every support event,
while other solvers normally publish a snapshot every 4,096 visited units. CBDD and CZDD publish diagram counters every 200
recursive diagram operations when diagnostics is enabled and read the clock once per cardinality. Their diagnostics-disabled path retains
only the existing 4,096-operation timeout checkpoint. The experimental circular bracelet traversal uses 256 because one exact
bracelet can take much longer than one raw support. Hadeler and
Dickinson report exact support-enumeration coverage; Bundfuss and Sponsel report certified simplex volume; Du Tour and
dimension-reducing models report proof-obligation coverage; the adaptive hybrid adds separate Sponsel/COPOMATRIX routing, split,
projection-child, staircase, switching, and coarse internal-phase counters; Safi reports traversal counters without inventing a
percentage. These metrics diagnose activity within a named phase or mathematical search space; they do not estimate remaining wall time.
FracESSA Circular reports cardinality, emitted bracelets, affine skips, exact KKT tests, and accepted candidates without inventing a
fixed percentage because candidate pruning changes its remaining bracelet set. CBDD and CZDD likewise avoid a false support
percentage and instead report their current phase and cardinality, time in that cardinality, emitted supports, certificates,
allocated diagram nodes, and diagram operations. Sequential Python calls can enable these lines with `diagnostics=True`.
Dickinson's support line separately counts supports skipped by existing signatures, supports sent to exact processing, and signatures
actually retained as certificates. Ceiling-Pruned Dickinson uses that support line, counts forbidden recursive branches exactly,
and records the retained ceiling certificates' sparse $(k,d)$ distribution. Kernel-Cone Dickinson uses the same support diagnostics
while replacing lifted-support search with exact root-kernel cone geometry. The two singular-lift models additionally separate outer
and lifted systems, duplicate and covered lift routes, cache and lifted-depth state; their certificate records are
$(\text{root }k,\text{lifted }k,|U|,|L|,\text{count})$. Breadth first also exposes its current and maximum FIFO frontier. A final
snapshot is printed on completion so terminating work is represented exactly.
If preprocessing remains unresolved, the selected model receives the unchanged original matrix. Its later diagnostics therefore
describes that single model call. The complete metric definitions are in `docs/DIAGNOSTICS_REPORTING.md`.

Every diagnostics line explicitly distinguishes `stage=preprocessing` from `stage=model`. Support counters are published immediately,
so expensive work after a support event is not hidden behind a batching threshold.

The launcher accepts an optional positive `--timeout SECONDS`. It is a hard wall-clock deadline around the isolated companion and
therefore covers parsing, component splitting, pre-checks, and the model without adding clock checks to those hot paths. Expiry kills
the companion and returns exit status `124` without a Boolean. Untimed calls retain direct process replacement and create neither a
supervisor process nor a watchdog thread.

Enabled preprocessing applies exact negative-entry connected-component decomposition and the complete exact-decision pre-check
profile. The implementation fuses them: one root scan supplies globally valid checks and the negative graph,
then each component is visited. After Frank–Wolfe, an exact Motzkin–Straus pattern takes its specialized maximum-clique path.
Otherwise preprocessing factorizes the original component. If it is already a Z-matrix, that factorization is the complete decision
and neither the negative-part nor maximal-Z factorization is repeated. Other components then factorize their exact negative part and
use maximal principal Z-matrices only when those complete-matrix certificates remain insufficient. A connected whole matrix is used
directly without copying. Last, one minimum-child Danninger reduction is attempted when it creates at most two order-reduced
children. If that remains unresolved, the same gate is applied to one COPOMATRIX reduction. Their children re-enter scan, root
checks, component splitting, and ordinary checks, then may apply both reductions again while their reduction depth is below the
internal maximum of two. Nodes at the maximum create no deeper descendants and call no model. If a reduction remains unresolved,
the next stage receives its original component unchanged.

The pre-check profile contains the complete order-at-most-three criterion, selected principal faces through cardinality three,
nonnegative off-diagonal acceptance, Qi negative-part diagonal dominance, the all-ones witness, a complete exact Motzkin–Straus
graph-matrix classifier, bounded floating Frank-Wolfe witness proposals with exact verification, exact positive-(semi)definiteness,
and the negative-only maximal-Z-matrix fallback. A recognized Motzkin–Straus matrix uses the Open-MCS-derived exact maximum-clique
branch-and-bound and skips both factorization paths. The maximal-Z fallback rejects an indefinite maximal block in both modes and a
singular positive-semidefinite block only in strict mode. Floating arithmetic may only propose a witness; an exact integer calculation
must verify it before a decision. The final Danninger and COPOMATRIX steps use exact integer rays and transformed matrices.

After whole-component exact definiteness remains inconclusive, preprocessing replaces every positive off-diagonal entry by zero and
factorizes that exact negative-part matrix. Positive semidefiniteness proves ordinary copositivity; positive definiteness proves strict
copositivity because the removed remainder is entrywise nonnegative. A singular stripped matrix does not by itself reject strict
copositivity. On a connected negative component, a positive-semidefinite negative part also makes maximal principal-Z enumeration
redundant. When the original matrix has no positive off-diagonal entry, it equals its negative part; the already available original
factorization then decides both ordinary and strict copositivity without another factorization. This auxiliary matrix is never
delegated: every unresolved model call still receives the original component matrix.

Connected components are taken from the graph whose edges are negative off-diagonal entries. Cross-component entries are
nonnegative, so CP or SCP of the whole matrix is the logical AND of the corresponding component decisions. Disconnected component
matrices are materialized once and retained with their partial preprocessing facts. The selected model receives only components whose
requested facts remain unresolved; resolved components are not solved again. A connected original matrix is borrowed without a copy.
Inconclusive Danninger and COPOMATRIX descendants remain certificate-only and are discarded, leaving their unchanged parent component
as the pending work item.

The Python analysis interface selects preprocessing `none` or `both`; all Python entry points and the corpus runner default to
`both`. `coposit --preprocessing off` and Python `preprocessing="none"` provide the direct faithful-baseline measurement
path. Internal one-stage experiments require an explicit source patch rather than another supported configuration.

## Model Inventory

Each model owns one authoritative `ALGORITHM.md` beside its implementation. That file contains its provenance, mathematics,
decision flow, exact representation, source-versus-coposit boundary, and known difficult inputs; this overview intentionally does not
duplicate those explanations.

### Literature roots and baselines

All eight baselines support individually selected CP and SCP.

| Identifier | Source and authoritative description |
|---|---|
| `dutour_2018` | Dutour Sikirić polyhedral-cone implementation; [`ALGORITHM.md`](../models/baselines/dutour_2018/ALGORITHM.md). |
| `danninger_1990` | Danninger's dimension-reducing reconstruction; [`ALGORITHM.md`](../models/baselines/danninger_1990/ALGORITHM.md). |
| `copomatrix_2011` | Xu–Yao COPOMATRIX reduction; [`ALGORITHM.md`](../models/baselines/copomatrix_2011/ALGORITHM.md). |
| `hadeler_1983` | Hadeler principal-submatrix enumeration; [`ALGORITHM.md`](../models/hadeler-based/hadeler_1983/ALGORITHM.md). |
| `dickinson_2019` | Dickinson finite certificate enumeration; [`ALGORITHM.md`](../models/hadeler-based/dickinson_2019/ALGORITHM.md). |
| `safi_2021` | Safi–Nabavi–Caron simplex partition; [`ALGORITHM.md`](../models/baselines/safi_2021/ALGORITHM.md). |
| `bundfuss_2008` | Bundfuss–Dür simplicial partition; [`ALGORITHM.md`](../models/baselines/bundfuss_2008/ALGORITHM.md). |
| `sponsel_2012` | Sponsel certificates with Bundfuss refinement; [`ALGORITHM.md`](../models/baselines/sponsel_2012/ALGORITHM.md). |

### Selected coposit-created experiment

- `adaptive_sponsel_copomatrix` supports both individually selected predicates. Its authoritative
  description is [`ALGORITHM.md`](../models/experiments/adaptive_sponsel_copomatrix/ALGORITHM.md).

### Hadeler-based and other coposit-created experiments

Every Dickinson-, Hadeler-, and FracESSA-based model in this table supports CP, SCP, and combined classification in one traversal.
When no mode is supplied through an analysis or reference-run interface, these models default to combined classification.
Their canonical source directories and compact lineage inventory are under
[`models/hadeler-based/`](../models/hadeler-based/README.md); unrelated experiments remain under `models/experiments/`.

| Identifier | Relationship and authoritative description |
|---|---|
| `dense_bitset_dickinson` | Cardinality-layer traversal over one literal packed bit per Boolean-lattice support; [`ALGORITHM.md`](../models/hadeler-based/dense_bitset_dickinson/ALGORITHM.md). |
| `adaptive_dutour_danninger` | Adaptive Dutour/Danninger hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_danninger/ALGORITHM.md). |
| `adaptive_dutour_copomatrix` | Adaptive Dutour/COPOMATRIX hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_copomatrix/ALGORITHM.md). |
| `adaptive_zischg_sponsel_copomatrix` | Adaptive projection-component comparison; [`ALGORITHM.md`](../models/experiments/adaptive_zischg_sponsel_copomatrix/ALGORITHM.md). |
| `interval_recursive_dickinson` | Dickinson intervals used to prune a recursive support generator; [`ALGORITHM.md`](../models/hadeler-based/interval_recursive_dickinson/ALGORITHM.md). |
| `bdd_dickinson` | Dickinson interval union represented by an ordinary binary decision diagram; [`ALGORITHM.md`](../models/hadeler-based/bdd_dickinson/ALGORITHM.md). |
| `zdd_dickinson` | Dickinson interval union represented by a zero-suppressed decision diagram; [`ALGORITHM.md`](../models/hadeler-based/zdd_dickinson/ALGORITHM.md). |
| `cbdd_dickinson` | Bryant chain-reduced BDD interval algebra for Dickinson certificates; [`ALGORITHM.md`](../models/hadeler-based/cbdd_dickinson/ALGORITHM.md). |
| `cbdd_halfspace_dickinson` | CBDD Dickinson with the cumulative exact coordinate search over strictly positive right-hand sides from SAT-Halfspace Dickinson; [`ALGORITHM.md`](../models/hadeler-based/cbdd_halfspace_dickinson/ALGORITHM.md). |
| `upper_endpoint_cbdd_dickinson` | CBDD Dickinson with one nonrecursive nonsingular solve at each distinct larger certificate upper endpoint before activation; [`ALGORITHM.md`](../models/hadeler-based/upper_endpoint_cbdd_dickinson/ALGORITHM.md). |
| `cbdd_dickinson_improved_1` | CBDD Dickinson with batched affine and homogeneous singular certificates, including the complete nullity-two stacked-line family; [`ALGORITHM.md`](../models/hadeler-based/cbdd_dickinson_improved_1/ALGORITHM.md). |
| `wide_certificate_cbdd_dickinson` | Parameterized CBDD Dickinson that prunes a full interval only for $d>\lfloor p(n-k)/100\rfloor$; required $p$ replaces the former 75%, 90%, and 95% source copies; [`ALGORITHM.md`](../models/hadeler-based/wide_certificate_cbdd_dickinson/ALGORITHM.md). |
| `multithreaded_cbdd_dickinson` | Bounded parallel exact support batches with one coordinator-owned CBDD update; [`ALGORITHM.md`](../models/hadeler-based/multithreaded_cbdd_dickinson/ALGORITHM.md). |
| `ceiling_pruned_dickinson` | FracESSA forbidden-support generation retaining only Dickinson certificates whose upper endpoint is the full index set; [`ALGORITHM.md`](../models/hadeler-based/ceiling_pruned_dickinson/ALGORITHM.md). |
| `kernel_cone_dickinson` | Ceiling-pruned Dickinson with direct exact cone geometry in every high-nullity root kernel; [`ALGORITHM.md`](../models/hadeler-based/kernel_cone_dickinson/ALGORITHM.md). |
| `affine_companion_dickinson` | Kernel-Cone Dickinson plus exact consistency and affine-family certificates at singular principal supports; [`ALGORITHM.md`](../models/hadeler-based/affine_companion_dickinson/ALGORITHM.md). |
| `layered_singular_lift_dickinson` | Layer-delayed ceiling pruning plus depth-first lifting of high-nullity singular supports to nullity-one principal supersets; [`ALGORITHM.md`](../models/hadeler-based/layered_singular_lift_dickinson/ALGORITHM.md). |
| `breadth_first_singular_lift_dickinson` | The same singular-superset experiment with a FIFO frontier that finishes each lifted cardinality before the next; [`ALGORITHM.md`](../models/hadeler-based/breadth_first_singular_lift_dickinson/ALGORITHM.md). |
| `czdd_dickinson` | Bryant chain-reduced ZDD interval algebra for Dickinson certificates; [`ALGORITHM.md`](../models/hadeler-based/czdd_dickinson/ALGORITHM.md). |
| `sat_dickinson` | Incremental CaDiCaL encoding with one blocking clause per Dickinson interval and one shared exact-cardinality sorting network; [`ALGORITHM.md`](../models/hadeler-based/sat_dickinson/ALGORITHM.md). |
| `sat_halfspace_dickinson` | SAT Dickinson with cumulative exact coordinate search over strictly positive right-hand sides; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_dickinson/ALGORITHM.md). |
| `sat_halfspace_rays_dickinson` | U-first, width-second SAT-Halfspace path with an adaptive shortlist and at most two exact synthesized-ray sweeps; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_rays_dickinson/ALGORITHM.md). |
| `sat_halfspace_lp_dickinson` | SAT-Halfspace-Rays plus a tiny numerical full-ceiling LP whose candidate must pass exact reconstruction and verification; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_lp_dickinson/ALGORITHM.md). |
| `sat_halfspace_milp_dickinson` | SAT Dickinson with a bounded model-local MILP maximizing the upper endpoint; only exactly reconstructed improvements become certificates; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_milp_dickinson/ALGORITHM.md). |
| `sat_halfspace_rays_lookahead_dickinson` | SAT-Halfspace-Rays with one-cardinality child analysis, one-layer exact-result caching, and immediate insertion of every child interval not contained in its current parent interval; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_rays_lookahead_dickinson/ALGORITHM.md). |
| `sat_halfspace_rays_wide_dickinson` | Parameterized SAT-Halfspace-Rays model retaining a full interval only for $d>\lfloor p(n-k)/100\rfloor$; [`ALGORITHM.md`](../models/hadeler-based/sat_halfspace_rays_wide_dickinson/ALGORITHM.md). |
| `wide_certificate_sat_dickinson` | Parameterized SAT Dickinson that retains only intervals with $d>\lfloor p(n-k)/100\rfloor$ and otherwise blocks exactly the processed support; [`ALGORITHM.md`](../models/hadeler-based/wide_certificate_sat_dickinson/ALGORITHM.md). |
| `xxx` | SAT-Halfspace-Rays intervals plus exact KKT active-set paths; each path buffers its intervals until it reaches a KKT point or has no open proposed move, then commits the batch and resumes alternating complete layers $1,n,2,n-1,\ldots$; [`ALGORITHM.md`](../models/hadeler-based/xxx/ALGORITHM.md). |
| `xxx_two` | Binary64 KKT paths with depth-first alternative-pivot backtracking, exact verification of proposed intermediate negative witnesses, and global visited-path avoidance; exact KKT intervals and a conditional exact Halfspace-Rays seed certificate are the only SAT proofs; [`ALGORITHM.md`](../models/hadeler-based/xxx_two/ALGORITHM.md). |
| `clingo_dickinson` | Clingo/clasp backtracking enumeration with native cardinality layers and one persistent clause per Dickinson interval; [`ALGORITHM.md`](../models/hadeler-based/clingo_dickinson/ALGORITHM.md). |
| `clingo_halfspace_dickinson` | Clingo Dickinson with the cumulative exact coordinate search over strictly positive right-hand sides from SAT-Halfspace Dickinson; [`ALGORITHM.md`](../models/hadeler-based/clingo_halfspace_dickinson/ALGORITHM.md). |
| `support_pruned_dickinson` | Dickinson with upward support pruning; [`ALGORITHM.md`](../models/hadeler-based/support_pruned_dickinson/ALGORITHM.md). |
| `nullity_support_pruned_dickinson` | Support-pruned Dickinson with singular-vector selection; [`ALGORITHM.md`](../models/hadeler-based/nullity_support_pruned_dickinson/ALGORITHM.md). |
| `rhs_dickinson` | Dickinson with searched positive right-hand sides; [`ALGORITHM.md`](../models/hadeler-based/rhs_dickinson/ALGORITHM.md). |
| `frank_wolfe_dickinson` | Bounded Frank-Wolfe witness proposals before Dickinson; [`ALGORITHM.md`](../models/hadeler-based/frank_wolfe_dickinson/ALGORITHM.md). |
| `one_step_frank_wolfe_dickinson` | One exact Frank-Wolfe segment before Dickinson; [`ALGORITHM.md`](../models/hadeler-based/one_step_frank_wolfe_dickinson/ALGORITHM.md). |
| `pairwise_frank_wolfe_dickinson` | Pairwise-away proposal path before Dickinson; [`ALGORITHM.md`](../models/hadeler-based/pairwise_frank_wolfe_dickinson/ALGORITHM.md). |
| `support_polished_frank_wolfe_dickinson` | Exact active-support polish before Dickinson; [`ALGORITHM.md`](../models/hadeler-based/support_polished_frank_wolfe_dickinson/ALGORITHM.md). |
| `frank_wolfe_sponsel` | One exact Frank-Wolfe segment before Sponsel; [`ALGORITHM.md`](../models/experiments/frank_wolfe_sponsel/ALGORITHM.md). |
| `fracessa` | First-order global-simplex-minimum adaptation of FracESSA; [`ALGORITHM.md`](../models/hadeler-based/fracessa/ALGORITHM.md). |
| `zischg_hadeler` | Zischg Level 2 inside Hadeler supports; [`ALGORITHM.md`](../models/hadeler-based/zischg_hadeler/ALGORITHM.md). |
| `zischg_dickinson` | Zischg Level 2 inside Dickinson supports; [`ALGORITHM.md`](../models/hadeler-based/zischg_dickinson/ALGORITHM.md). |
| `zischg_fracessa` | Zischg Level 2 inside FracESSA supports; [`ALGORITHM.md`](../models/hadeler-based/zischg_fracessa/ALGORITHM.md). |

Algorithm code is intentionally duplicated between model directories. A new mathematical variant is copied from its nearest model
and then changed independently. Shared code is restricted to infrastructure whose behavior is genuinely independent of traversal,
branching, pruning, and termination policy.

## Python And Reference Runs

The Python analysis package keeps three layers:

- `compute_matrix()` — one native call;
- `run()` — sequential execution; and
- `run_multiprocessing()` — bounded process execution yielding completion-ordered results.

Every call requires an explicit model identifier. Omitting the mode selects combined classification for every Dickinson-, Hadeler-,
or FracESSA-based model; a model without that capability requires an explicit predicate. The analysis API accepts
the preprocessing selections. `Matrix(matrix, matrix_id=None)` puts the required matrix text or direct relative/absolute file
path first; the optional ID is only a result-correlation label for corpus and batch work. Relative paths use the process working
directory. There is no metadata or base-directory argument. `python/README.md` is the authoritative Python interface and status
reference.

`python/run_results.py` is the maintained timed corpus runner. Each persistent single-threaded Python worker executes one matrix at a
time through `coposit`, while the parent dispatcher and serialized SQLite writer remain pinned separately. Each result stores the exact
selected model-companion SHA-256. At a deadline, the parent sends `SIGUSR1`; the worker forwards it to the companion, whose signal-safe
flag makes cooperative model checkpoints return `TIMEOUT`. A worker that does not return within the grace period is replaced. Ctrl-C
stops new assignments and drains assigned work to its result or timeout. The optional `--without-results` selector restricts a run to
matrices with no prior result row from any model.

SQLite writes remain in the parent. Its bounded queue is drained into batch transactions, so workers never write the database.
Standard local reference runs use parent CPU 3 and solver CPUs 4 through 7.

## Corpus And Evidence

`testdata/copos_testdata.sqlite3` contains 3,523 exact matrices of orders 1 through 5,000 and uses SQLite `auto_vacuum=FULL` so
deleted or replaced result rows do not leave persistent free pages. The `matrices` table stores nullable strict and non-strict truth,
free-form occurrence provenance and family text, an earliest-known primary `source_id`, an `additional_source_ids` JSON bibliography,
a `references_solved` JSON array of source-linked literature solution claims, a parallel `references_unsolved` array of explicit
method-specific failure claims,
and four overlapping benchmark flags: stored `smoke_set` and `core_and_stress_test`, plus generated `n_le_100` and
`n_gt_100_solved`. The small `sources` table stores only authors, title,
the earliest documented public year, bibliographic reference, and a provenance comment. It contains 96 literature, collection,
repository, and local-generator source records. Every current corpus matrix points to its
earliest located source or exact local generator. Another 513 matrices carry 837 chronologically ordered secondary source links
obtained from explicit catalog matches, stored occurrence provenance, exact positive-scale duplicates, and audited named or
family-level reuse statements. These links are
best-effort literature evidence, not a claim that a class-level paper prints every member's coefficients.
The `preprocessing_solved` metadata flag identifies all 2,765 retained matrices completely classified by the current maintained
depth-2 combined preprocessing workflow in the five-second corpus run, its sixty-second timeout continuation, the focused ten-minute
Motzkin--Straus follow-up, or a stored combined diagnostic result with zero model delegations. They comprise 744 strictly copositive,
1,032 copositive-boundary, and 989 non-copositive matrices. Partial facts are excluded, and shorter later diagnostics can add evidence
but never remove a flag established by a longer run.
Every named benchmark set excludes these rows; the current sets therefore measure matrices that reach the selected model.
Separately, 430 matrices have 629 `references_solved` entries from 27 sources whose reported result establishes the matrix's complete
stored copositivity classification. A negative witness is decisive, but a nonnegative heuristic screen, a stationary point, or merely
`not strictly copositive` is not. Each entry is an object with required `source_id` and an optional qualification comment; an empty list
means only that no identifiable completed claim was located. `aidocs/LITERATURE_SOLVED_REFERENCES.md` records the audit boundary.
Another 197 matrices have 256 `references_unsolved` claims from 14 sources. Each comment names the algorithm and whether it timed out,
exhausted memory, failed numerically, remained inconclusive, or returned a wrong result. A paper can occur in both fields when its
methods differ; `aidocs/LITERATURE_UNSOLVED_REFERENCES.md` records the conservative matrix-level inclusion boundary.
The reference runner accepts `--matrix-set references_unsolved` as a derived selector rather than storing a redundant Boolean flag;
`aidocs/REFERENCE_RESULTS_LITERATURE_UNSOLVED.md` records the current ten-second comparison of the three decision-diagram models.
The 1,048 retained catalog occurrences imported on 2026-08-14 are represented by 864 distinct matrix rows after direct positive-scale
deduplication. Nontrivial simultaneous row-and-column permutations remain separate literature matrices because coordinate order can
change solver traversal and runtime. Every occurrence's source text and bibliography remain attached. Another 790 raw
QP objectives with a negative diagonal remain cataloged but are excluded from the solver corpus because a coordinate witness makes each
one trivially non-copositive. Paper/repository labels and later exact, row-commented certificates establish 336 retained occurrences as
strictly copositive, 269 as copositive boundary, and 420 as non-copositive; 23 retain both truth fields as `NULL`, and no row has only
one known truth value. The imported rows initially had all then-current stored benchmark flags off; a survivor retains any memberships already held
by an older equivalent corpus row.
Their definitions and guarded selection evidence are in [`BENCHMARK_SETS.md`](BENCHMARK_SETS.md).

Small matrices remain inline. The 149 rows whose inline encoding would be large use `file:matrices/<matrix_id>.mtx` and exact
symmetric integer Matrix Market files under `testdata/matrices/`; each row retains the file SHA-256 for an explicit integrity audit.
Normal runs neither recompute that hash nor use a mutable whole-database checksum.

The tracked corpus database contains only `sources` and `matrices`. Each matrix may cache the nanosecond time and exact composite key
of its fastest eligible completed local classification. Mutable measurements live in the ignored local
`experiments/diagnostics.sqlite3` database, reproducibly created from `testdata/diagnostics_schema.sql`. Its `results` table keeps
classification, stop status, elapsed time, cutoff, model-companion hash, optional full diagnostic text, and optional sparse
certificate joint frequencies. Every `run_results.py` campaign enables diagnostics automatically; a `running` row is updated once per
second and replaced by the final
`ok`, `parse_error`, `timeout`, `node_limit`, or `error` row, so a hard timeout retains its latest state. Its separate
`preprocessing_results` table records preprocessing-only positive, negative, or unresolved outcomes without pretending that
delegation is a completed copositivity decision. The serialized result writer refreshes only the affected matrix cache rows; eligibility
requires `ok` status and agreement with all known corpus truth values. Combined results are eligible; an SCP-only result must be
positive, while a CP-only result must be negative or positively confirm a known copositive-boundary matrix. A negative SCP-only result
cannot stand in for a non-copositivity classification. For `preprocessing_solved` matrices, the two cache fields deliberately retain
the shortest complete shared-preprocessing decision instead of a later model traversal; the 36 older complete depth-2 CSV outcomes
are registered as exact `preprocessing_depth_2` diagnostic rows so these references remain concrete.

The immutable source snapshot is `testdata/archive/copos_testdata.original.sqlite3.xz`; its decompressed SHA-256 is
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`. Historical migration utilities and generators are retained
under `testdata/archive/`. The maintained database has no published whole-file checksum because it is intentionally mutable.

Reference-result reports remain under `aidocs/`. Experiment-specific reports live beside their raw data, including
[`experiments/preprocessing_cost_2026-08-10/README.md`](../experiments/preprocessing_cost_2026-08-10/README.md) and the complete
depth-1/2/3 reduction comparison in
[`experiments/preprocessing_depth_2026-08-15/README.md`](../experiments/preprocessing_depth_2026-08-15/README.md). Literature-family
construction evidence is in [`LITERATURE_MATRIX_FAMILIES.md`](LITERATURE_MATRIX_FAMILIES.md).

## Build And Verification

Required dependencies are CMake 3.18 or newer, a C++17 compiler, FLINT, MPFR, and GMP. Python 3.11 or newer is needed for the Python
adapter; GoogleTest is needed only for the test suite. Standalone builds enable applications, Python support, and
tests by default. When added with `add_subdirectory()`, coposit builds only `coposit::core` unless the embedding
project explicitly enables `COPOSIT_BUILD_APPS`, `COPOSIT_BUILD_PYTHON`, or `COPOSIT_BUILD_TESTS`.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
sqlite3 testdata/copos_testdata.sqlite3 'PRAGMA integrity_check;'
```

Use `smoke_set` first for integration checks, `core_and_stress_test` for normal comparisons and difficult exact/resource behavior,
`n_le_100` for comprehensive dimension-bounded comparisons among matrices not solved by preprocessing, and
`n_gt_100_solved` for higher-order matrices that literature reports as solved.
Use the complete corpus only for final reference results or an
explicitly exhaustive question.

## Non-Obvious Constraints

- Exact mathematical correctness has priority over performance.
- CP, SCP, timeout, node limit, and execution failure are distinct outcomes.
- Symmetry is enforced at the parser boundary rather than silently repaired.
- The core remains integer-only; rational input is normalized once before it enters a model.
- Each internal model companion links exactly one model; `coposit` and Python link none.
- Baselines preserve their source mathematics; the first mathematical change creates a separately named copied model.
- Model-local `ALGORITHM.md` files, not this overview, are authoritative for algorithm details.
- Dickinson has not been memory-bound in the maintained workloads. Adaptive Sponsel–COPOMATRIX retains full dense pending siblings;
  memory can become limiting around order 1,000 and above, or earlier on an unusually deep branch.
- FracESSA is read-only source material for coposit and is never a dependency of shared code.
