# coposit Project Overview

Last verified: 2026-08-15

## Purpose And Contract

coposit is a standalone C++17 project for exact copositivity (CP) and strict copositivity (SCP) decisions on nonempty symmetric
matrices. Its public parsers accept exact rational input and return the denominator-cleared integer matrix together with the retained
common positive denominator. Every maintained algorithm receives only the integer matrix. The solving core stores no rational
matrices and makes no floating-point classification.

Every model implements the link-time contract
`coposit::model::solve(const matrix_integer&, copositivity_mode)` from `cpp/include/coposit/model.hpp`. A completed call returns the
Boolean for the selected predicate. A timeout, node limit, parse failure, or execution failure remains unresolved and is never
reported as `false`.

C++ clients that need the maintained exact SCP decision include `coposit/safe.hpp`, call
`coposit::safe::is_strictly_copositive()`, and link `coposit::safe`. This public composition applies the complete pre-check and
connected-component pipeline before Dickinson Final. `coposit::core` exposes only the model-independent headers and exact types.

The eight literature baselines, `adaptive_sponsel_copomatrix`, `dickinson_final`, `dense_bitset_dickinson`, all CBDD-Zed Dickinson
variants, `ceiling_pruned_dickinson`, `sat_zed_dickinson`, and `clingo_sat_zed_dickinson` support separately selected CP and SCP.
Hadeler 1983, Dickinson 2019, Dickinson Final, Dense-Bitset Dickinson, Danninger 1990, all CBDD-Zed Dickinson variants,
Ceiling-Pruned Dickinson, SAT-Zed Dickinson, and Clingo-SAT-Zed Dickinson can additionally classify both predicates in one traversal.
Other coposit-created models reject CP and combined mode explicitly, except the circular-only `fracessa_circular` experiment, which
classifies both predicates from the exact global simplex minimum.

## Repository Structure

- `cpp/` — CMake project, public launchers, native Python boundary, shared tests, and model-independent headers.
- `cpp/include/coposit/` — exact integers and matrices, parsers, packed supports, exact factorization, preprocessing, timeout state,
  open-node limit, and the model call contract.
- `models/baselines/<model>/` — literature or source baselines.
- `models/adaptive_sponsel_copomatrix/` and `models/dickinson_final/` — selected coposit-created models.
- `models/experiments/<model>/` — every other coposit-created variant or comparison.
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

## Executables And Processing Pipeline

The user-facing launcher requires a method and predicate:

```bash
cpp/build/coposit fast strict 2#1,0,1
cpp/build/coposit safe non-strict matrix.mtx
cpp/build/coposit safe both matrix.mtx
cpp/build/coposit safe strict --progress long-running-matrix.mtx
cpp/build/coposit fast strict --timeout 30 long-running-matrix.mtx
```

| Method | Linked model | Practical contract |
|---|---|---|
| `fast` | `adaptive_sponsel_copomatrix` | Often reaches a result quickly; bounded traversal can remain unresolved. |
| `safe` | `dickinson_final` | Complete finite certificate enumeration when allowed to finish. |

The expert C++ command is `coposit-analyze --model MODEL --mode strict|non-strict|both`. Its inventory is every literature baseline
except superseded `dickinson_2019`, plus `dickinson_final` and `adaptive_sponsel_copomatrix`. It exposes progress reporting and one
`--preprocessing on|off` switch for the complete fixed pipeline. The
normal `coposit` command exposes none of these choices. Both launchers dispatch to internal companions that each link exactly one
model. There is no C++ model registry, runtime factory, inheritance hierarchy, or executable containing several solvers. Python
modules follow the same one-model rule and retain all maintained experimental variants.

The optional `--progress` flag starts one sleeping reporter and writes a status line to standard error every second.
Preprocessing publishes its current phase and a truthful row, vertex, center, iteration, or pivot counter where one exists. Solvers
do not query the clock in their hot loops: support models publish their small relaxed-atomic counters after every support event,
while other solvers normally publish a snapshot every 4,096 visited units. CBDD-Zed and CZDD-Zed publish diagram counters every 200
recursive diagram operations when progress is enabled and read the clock once per cardinality. Their progress-disabled path retains
only the existing 4,096-operation timeout checkpoint. The experimental circular bracelet traversal uses 256 because one exact
bracelet can take much longer than one raw support. Hadeler and
Dickinson report exact support-enumeration coverage; Bundfuss and Sponsel report certified simplex volume; Du Tour and
dimension-reducing models report proof-obligation coverage; the adaptive hybrid adds separate Sponsel/COPOMATRIX routing, split,
projection-child, staircase, switching, and coarse internal-phase counters; Safi reports traversal counters without inventing a
percentage. These metrics are progress through a named phase or mathematical search space, not estimates of remaining wall time.
FracESSA Circular reports cardinality, emitted bracelets, affine skips, exact KKT tests, and accepted candidates without inventing a
fixed percentage because candidate pruning changes its remaining bracelet set. CBDD-Zed and CZDD-Zed likewise avoid a false support
percentage and instead report their current phase and cardinality, time in that cardinality, emitted supports, certificates, tested
Zed blocks, allocated diagram nodes, and diagram operations. Sequential Python calls can enable these lines with `progress=True`.
Dickinson's support line separately counts supports skipped by existing signatures, supports sent to exact processing, and signatures
actually retained as certificates. Ceiling-Pruned Dickinson uses that support line, counts forbidden recursive branches exactly,
and records the retained ceiling certificates' sparse $(k,d)$ distribution. A final snapshot is printed on completion so a
terminating support is represented exactly.
If preprocessing remains unresolved, the selected model receives the unchanged original matrix. Its later progress therefore
describes that single model call. The complete metric definitions are in `docs/PROGRESS_REPORTING.md`.

Every progress line explicitly distinguishes `stage=preprocessing` from `stage=model`. Support counters are published immediately,
so expensive work after a support event is not hidden behind a batching threshold.

Both launchers accept an optional positive `--timeout SECONDS`. It is a hard wall-clock deadline around the isolated companion and
therefore covers parsing, component splitting, pre-checks, and the model without adding clock checks to those hot paths. Expiry kills
the companion and returns exit status `124` without a Boolean. Untimed calls retain direct process replacement and create neither a
supervisor process nor a watchdog thread.

The public `fast` and `safe` paths always enable exact negative-entry connected-component decomposition and the complete
exact-decision pre-check profile. The implementation fuses them: one root scan supplies globally valid checks and the negative graph,
then each component is visited. Frank–Wolfe and exact definiteness are deferred to the component matrices; a connected whole matrix is
used directly without copying. Last, one minimum-child Danninger reduction is attempted when it creates at most two order-reduced
children. If that remains unresolved, the same gate is applied to one COPOMATRIX reduction. Their children re-enter scan, root
checks, component splitting, and ordinary checks, then may apply both reductions again while their reduction depth is below the
internal maximum of two. Nodes at the maximum create no deeper descendants and call no model. If a reduction remains unresolved,
the next stage receives its original component unchanged.

The pre-check profile contains the complete order-at-most-three criterion, selected principal faces through cardinality three,
nonnegative off-diagonal acceptance, Qi negative-part diagonal dominance, the all-ones witness, the negative-only maximal-Z-matrix
test, bounded floating Frank-Wolfe witness proposals with exact verification, and exact positive-(semi)definiteness. The Z-matrix
test rejects an indefinite maximal block in both modes and a singular positive-semidefinite block only in strict mode; it bypasses
exact Motzkin–Straus graph-matrix patterns. Floating arithmetic may only propose a witness; an exact integer calculation must verify
it before a decision. The final Danninger and COPOMATRIX steps use exact integer rays and transformed matrices.

Connected components are taken from the graph whose edges are negative off-diagonal entries. Cross-component entries are
nonnegative, so CP or SCP of the whole matrix is the logical AND of the corresponding component decisions. Disconnected component
matrices are materialized and processed one at a time.

The Python analysis interface selects preprocessing `none` or `both`; all Python entry points and the corpus runner default to
`both`. `coposit-analyze --preprocessing off` and Python `preprocessing="none"` provide the direct faithful-baseline measurement
path. Internal one-stage experiments require an explicit source patch rather than another supported configuration.

## Model Inventory

Each model owns one authoritative `ALGORITHM.md` beside its implementation. That file contains its provenance, mathematics,
decision flow, exact representation, source-versus-coposit boundary, and known difficult inputs; this overview intentionally does not
duplicate those explanations.

### Literature baselines

All eight baselines support individually selected CP and SCP.

| Identifier | Source and authoritative description |
|---|---|
| `dutour_2018` | Dutour Sikirić polyhedral-cone implementation; [`ALGORITHM.md`](../models/baselines/dutour_2018/ALGORITHM.md). |
| `danninger_1990` | Danninger's dimension-reducing reconstruction; [`ALGORITHM.md`](../models/baselines/danninger_1990/ALGORITHM.md). |
| `copomatrix_2011` | Xu–Yao COPOMATRIX reduction; [`ALGORITHM.md`](../models/baselines/copomatrix_2011/ALGORITHM.md). |
| `hadeler_1983` | Hadeler principal-submatrix enumeration; [`ALGORITHM.md`](../models/baselines/hadeler_1983/ALGORITHM.md). |
| `dickinson_2019` | Dickinson finite certificate enumeration; [`ALGORITHM.md`](../models/baselines/dickinson_2019/ALGORITHM.md). |
| `safi_2021` | Safi–Nabavi–Caron simplex partition; [`ALGORITHM.md`](../models/baselines/safi_2021/ALGORITHM.md). |
| `bundfuss_2008` | Bundfuss–Dür simplicial partition; [`ALGORITHM.md`](../models/baselines/bundfuss_2008/ALGORITHM.md). |
| `sponsel_2012` | Sponsel certificates with Bundfuss refinement; [`ALGORITHM.md`](../models/baselines/sponsel_2012/ALGORITHM.md). |

### Selected coposit-created models

- `adaptive_sponsel_copomatrix` is the selected fast model and supports both individually selected predicates. Its authoritative
  description is [`ALGORITHM.md`](../models/adaptive_sponsel_copomatrix/ALGORITHM.md).
- `dickinson_final` is the selected safe model and supports individual and combined classification. It starts as an independent,
  algorithmically identical copy of the Dickinson baseline; the public composition applies the shared prechecks before it. Its
  authoritative description is [`ALGORITHM.md`](../models/dickinson_final/ALGORITHM.md).

### coposit-created experiments

Every model in this table is strict-only except `dense_bitset_dickinson`, the CBDD-Zed Dickinson variants,
`ceiling_pruned_dickinson`, `sat_zed_dickinson`, `clingo_sat_zed_dickinson`, and `fracessa_circular`, which support CP, SCP, and
combined classification.

| Identifier | Relationship and authoritative description |
|---|---|
| `dense_bitset_dickinson` | Cardinality-layer traversal over one literal packed bit per Boolean-lattice support; [`ALGORITHM.md`](../models/experiments/dense_bitset_dickinson/ALGORITHM.md). |
| `adaptive_dutour_danninger` | Adaptive Dutour/Danninger hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_danninger/ALGORITHM.md). |
| `adaptive_dutour_copomatrix` | Adaptive Dutour/COPOMATRIX hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_copomatrix/ALGORITHM.md). |
| `adaptive_zischg_sponsel_copomatrix` | Adaptive projection-component comparison; [`ALGORITHM.md`](../models/experiments/adaptive_zischg_sponsel_copomatrix/ALGORITHM.md). |
| `interval_recursive_dickinson` | Dickinson intervals used to prune a recursive support generator; [`ALGORITHM.md`](../models/experiments/interval_recursive_dickinson/ALGORITHM.md). |
| `interval_bdd_dickinson` | Dickinson interval union represented by an ordinary binary decision diagram; [`ALGORITHM.md`](../models/experiments/interval_bdd_dickinson/ALGORITHM.md). |
| `interval_zdd_dickinson` | Dickinson interval union represented by a zero-suppressed decision diagram; [`ALGORITHM.md`](../models/experiments/interval_zdd_dickinson/ALGORITHM.md). |
| `cardinality_bdd_dickinson` | Cardinality-local BDD subtraction of Dickinson intervals; [`ALGORITHM.md`](../models/experiments/cardinality_bdd_dickinson/ALGORITHM.md). |
| `cardinality_zdd_dickinson` | Cardinality-local ZDD subtraction of Dickinson intervals; [`ALGORITHM.md`](../models/experiments/cardinality_zdd_dickinson/ALGORITHM.md). |
| `dickinson_zed` | Flat Dickinson signatures plus Section 6 maximal-Zed downsets; [`ALGORITHM.md`](../models/experiments/dickinson_zed/ALGORITHM.md). |
| `bdd_zed_dickinson` | Rejection-only maximal-Zed check before ordinary-BDD Dickinson; [`ALGORITHM.md`](../models/experiments/bdd_zed_dickinson/ALGORITHM.md). |
| `zdd_zed_dickinson` | Rejection-only maximal-Zed check before ZDD Dickinson; [`ALGORITHM.md`](../models/experiments/zdd_zed_dickinson/ALGORITHM.md). |
| `cbdd_zed_dickinson` | Bryant chain-reduced BDD interval algebra after the rejection-only maximal-Zed check; [`ALGORITHM.md`](../models/experiments/cbdd_zed_dickinson/ALGORITHM.md). |
| `wide_certificate_cbdd_zed_dickinson` | CBDD-Zed Dickinson that prunes a full interval only when its free-index count exceeds half the matrix order; [`ALGORITHM.md`](../models/experiments/wide_certificate_cbdd_zed_dickinson/ALGORITHM.md). |
| `wide_75_certificate_cbdd_zed_dickinson` | Full-interval pruning only for $d>3(n-k)/4$; [`ALGORITHM.md`](../models/experiments/wide_75_certificate_cbdd_zed_dickinson/ALGORITHM.md). |
| `wide_90_certificate_cbdd_zed_dickinson` | Full-interval pruning only for $d>9(n-k)/10$; [`ALGORITHM.md`](../models/experiments/wide_90_certificate_cbdd_zed_dickinson/ALGORITHM.md). |
| `wide_95_certificate_cbdd_zed_dickinson` | Full-interval pruning only for $d>19(n-k)/20$; [`ALGORITHM.md`](../models/experiments/wide_95_certificate_cbdd_zed_dickinson/ALGORITHM.md). |
| `multithreaded_cbdd_zed_dickinson` | Optional parallel maximal-Zed subtrees and bounded parallel exact support batches with one coordinator-owned CBDD update; [`ALGORITHM.md`](../models/experiments/multithreaded_cbdd_zed_dickinson/ALGORITHM.md). |
| `ceiling_pruned_dickinson` | FracESSA forbidden-support generation retaining only Dickinson certificates whose upper endpoint is the full index set; [`ALGORITHM.md`](../models/experiments/ceiling_pruned_dickinson/ALGORITHM.md). |
| `czdd_zed_dickinson` | Bryant chain-reduced ZDD interval algebra after the rejection-only maximal-Zed check; [`ALGORITHM.md`](../models/experiments/czdd_zed_dickinson/ALGORITHM.md). |
| `sat_zed_dickinson` | Incremental CaDiCaL encoding with one blocking clause per Dickinson interval and one shared exact-cardinality sorting network; [`ALGORITHM.md`](../models/experiments/sat_zed_dickinson/ALGORITHM.md). |
| `clingo_sat_zed_dickinson` | Clingo/clasp backtracking enumeration with native cardinality layers and one persistent clause per Dickinson interval; [`ALGORITHM.md`](../models/experiments/clingo_sat_zed_dickinson/ALGORITHM.md). |
| `support_pruned_dickinson` | Dickinson with upward support pruning; [`ALGORITHM.md`](../models/experiments/support_pruned_dickinson/ALGORITHM.md). |
| `nullity_support_pruned_dickinson` | Support-pruned Dickinson with singular-vector selection; [`ALGORITHM.md`](../models/experiments/nullity_support_pruned_dickinson/ALGORITHM.md). |
| `rhs_dickinson` | Dickinson with searched positive right-hand sides; [`ALGORITHM.md`](../models/experiments/rhs_dickinson/ALGORITHM.md). |
| `frank_wolfe_dickinson` | Bounded Frank-Wolfe witness proposals before Dickinson; [`ALGORITHM.md`](../models/experiments/frank_wolfe_dickinson/ALGORITHM.md). |
| `one_step_frank_wolfe_dickinson` | One exact Frank-Wolfe segment before Dickinson; [`ALGORITHM.md`](../models/experiments/one_step_frank_wolfe_dickinson/ALGORITHM.md). |
| `pairwise_frank_wolfe_dickinson` | Pairwise-away proposal path before Dickinson; [`ALGORITHM.md`](../models/experiments/pairwise_frank_wolfe_dickinson/ALGORITHM.md). |
| `support_polished_frank_wolfe_dickinson` | Exact active-support polish before Dickinson; [`ALGORITHM.md`](../models/experiments/support_polished_frank_wolfe_dickinson/ALGORITHM.md). |
| `frank_wolfe_sponsel` | One exact Frank-Wolfe segment before Sponsel; [`ALGORITHM.md`](../models/experiments/frank_wolfe_sponsel/ALGORITHM.md). |
| `fracessa` | First-order global-simplex-minimum adaptation of FracESSA; [`ALGORITHM.md`](../models/experiments/fracessa/ALGORITHM.md). |
| `fracessa_circular` | FracESSA global-minimum adaptation with exact circular support-orbit reductions; [`ALGORITHM.md`](../models/experiments/fracessa_circular/ALGORITHM.md). |
| `zischg_hadeler` | Zischg Level 2 inside Hadeler supports; [`ALGORITHM.md`](../models/experiments/zischg_hadeler/ALGORITHM.md). |
| `zischg_dickinson` | Zischg Level 2 inside Dickinson supports; [`ALGORITHM.md`](../models/experiments/zischg_dickinson/ALGORITHM.md). |
| `zischg_fracessa` | Zischg Level 2 inside FracESSA supports; [`ALGORITHM.md`](../models/experiments/zischg_fracessa/ALGORITHM.md). |

Algorithm code is intentionally duplicated between model directories. A new mathematical variant is copied from its nearest model
and then changed independently. Shared code is restricted to infrastructure whose behavior is genuinely independent of traversal,
branching, pruning, and termination policy.

## Python And Reference Runs

The Python analysis package keeps three layers:

- `compute_matrix()` — one native call;
- `run()` — sequential execution; and
- `run_multiprocessing()` — bounded process execution yielding completion-ordered results.

Every call requires an explicit model identifier. The analysis API also requires an explicit predicate or combined mode and accepts
the four preprocessing selections. `Matrix(matrix, matrix_id=None)` puts the required matrix text or direct relative/absolute file
path first; the optional ID is only a result-correlation label for corpus and batch work. Relative paths use the process working
directory. There is no metadata or base-directory argument. `python/README.md` is the authoritative Python interface and status
reference.

`python/run_results.py` is the maintained timed corpus runner. It loads one native module once in each persistent single-threaded
worker, pins the parent dispatcher and serialized SQLite writer separately from solver workers, and stores the exact native-module
SHA-256 with each result. At a deadline, the parent sends `SIGUSR1`; the native handler sets a signal-safe flag and cooperative model
checkpoints return `TIMEOUT`. A worker that does not return within the grace period is replaced. Ctrl-C stops new assignments and
drains assigned work to its result or timeout. The optional `--without-results` selector restricts a run to matrices with no prior
result row from any model.

SQLite writes remain in the parent. Its bounded queue is drained into batch transactions, so workers never write the database.
Standard local reference runs use parent CPU 3 and solver CPUs 4 through 7.

## Corpus And Evidence

`testdata/copos_testdata.sqlite3` contains 3,669 exact matrices of orders 1 through 5,000 and uses SQLite `auto_vacuum=FULL` so
deleted or replaced result rows do not leave persistent free pages. The `matrices` table stores nullable strict and non-strict truth,
free-form occurrence provenance and family text, an earliest-known primary `source_id`, an `additional_source_ids` JSON bibliography,
a `references_solved` JSON array of source-linked literature solution claims, a parallel `references_unsolved` array of explicit
method-specific failure claims,
and seven overlapping benchmark flags: `smoke_set`, `representative_core`, `stress_test`, `scale_set`, `timeout_5s_strict_set`,
generated `n_le_100`, and generated `n_gt_100_solved`. The small `sources` table stores only authors, title,
the earliest documented public year, bibliographic reference, and a provenance comment. It contains 98 literature, collection,
repository, and local-generator source records. Every current corpus matrix points to its
earliest located source or exact local generator. Another 513 matrices carry 837 chronologically ordered secondary source links
obtained from explicit catalog matches, stored occurrence provenance, exact positive-scale duplicates, and audited named or
family-level reuse statements. These links are
best-effort literature evidence, not a claim that a class-level paper prints every member's coefficients.
Separately, 462 matrices have 678 `references_solved` entries from 23 papers that report a completed direct copositivity test or an
equivalent global-StQP decision. Each entry is an object with required `source_id` and an optional qualification comment; an empty list
means only that no identifiable completed claim was located. `aidocs/LITERATURE_SOLVED_REFERENCES.md` records the audit boundary.
Another 175 matrices have 234 `references_unsolved` claims from 11 papers. Each comment names the algorithm and whether it timed out,
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
one known truth value. The imported rows initially had all five benchmark flags off; a survivor retains any memberships already held
by an older equivalent corpus row.
Their definitions and guarded selection evidence are in [`BENCHMARK_SETS.md`](BENCHMARK_SETS.md).

Small matrices remain inline. The 290 rows larger than 500 KB use `file:matrices/<matrix_id>.mtx` and exact symmetric integer Matrix
Market files under `testdata/matrices/`; each row retains the file SHA-256 for an explicit integrity audit. Normal runs neither
recompute that hash nor use a mutable whole-database checksum.

The tracked corpus database contains only `sources` and `matrices`. Each matrix may cache the nanosecond time and exact composite key
of its fastest eligible completed local result. Mutable measurements live in the ignored local
`experiments/diagnostics.sqlite3` database, reproducibly created from `testdata/diagnostics_schema.sql`. Its `results` table keeps
classification, stop status, elapsed time, cutoff, native-module hash, optional full progress diagnostics, and optional sparse
certificate joint frequencies. During diagnostic campaigns, a `running` row is updated once per second and replaced by the final
`ok`, `parse_error`, `timeout`, `node_limit`, or `error` row, so a hard timeout retains its latest state. Its separate
`preprocessing_results` table records preprocessing-only positive, negative, or unresolved outcomes without pretending that
delegation is a completed copositivity decision. The serialized result writer refreshes only the affected matrix cache rows; eligibility
requires `ok` status and agreement with all known corpus truth values.

The immutable source snapshot is `testdata/archive/copos_testdata.original.sqlite3.xz`; its decompressed SHA-256 is
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`. Historical migration utilities and generators are retained
under `testdata/archive/`. The maintained database has no published whole-file checksum because it is intentionally mutable.

Reference-result reports remain under `aidocs/`. Experiment-specific reports live beside their raw data, including
[`experiments/preprocessing_cost_2026-08-10/README.md`](../experiments/preprocessing_cost_2026-08-10/README.md) and the complete
child-versus-grandchild comparison in
[`experiments/preprocessing_depth_2026-08-15/README.md`](../experiments/preprocessing_depth_2026-08-15/README.md). Literature-family
construction evidence is in [`LITERATURE_MATRIX_FAMILIES.md`](LITERATURE_MATRIX_FAMILIES.md).

## Build And Verification

Required dependencies are CMake 3.18 or newer, a C++17 compiler, FLINT, MPFR, and GMP. Python 3.11 or newer and pybind11 are needed
only for Python modules; GoogleTest is needed only for the test suite. Standalone builds enable applications, Python modules, and
tests by default. When added with `add_subdirectory()`, coposit builds only `coposit::core` and `coposit::safe` unless the embedding
project explicitly enables `COPOSIT_BUILD_APPS`, `COPOSIT_BUILD_PYTHON`, or `COPOSIT_BUILD_TESTS`.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
sqlite3 testdata/copos_testdata.sqlite3 'PRAGMA integrity_check;'
```

Use `smoke_set` first for integration checks, `representative_core` for normal comparisons, `stress_test` for difficult exact and
resource behavior, `scale_set` for dimension, storage, and memory questions, `timeout_5s_strict_set` for the frozen common
five-second strict timeouts of the two final pre-checked algorithms, `n_le_100` for comprehensive dimension-bounded comparisons, and
`n_gt_100_solved` for higher-order matrices that literature reports as solved.
Use the complete corpus only for final reference results or an
explicitly exhaustive question.

## Non-Obvious Constraints

- Exact mathematical correctness has priority over performance.
- CP, SCP, timeout, node limit, and execution failure are distinct outcomes.
- Symmetry is enforced at the public parser boundary rather than silently repaired.
- The core remains integer-only; rational input is normalized once before it enters a model.
- Each executable and native module links exactly one model.
- Baselines preserve their source mathematics; the first mathematical change creates a separately named copied model.
- Model-local `ALGORITHM.md` files, not this overview, are authoritative for algorithm details.
- Dickinson has not been memory-bound in the maintained workloads. Adaptive Sponsel–COPOMATRIX retains full dense pending siblings;
  memory can become limiting around order 1,000 and above, or earlier on an unusually deep branch.
- FracESSA is read-only source material for coposit and is never a dependency of shared code.
