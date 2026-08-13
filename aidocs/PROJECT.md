# coposit Project Overview

Last verified: 2026-08-12

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

The eight literature baselines, `adaptive_sponsel_copomatrix`, and `dickinson_final` support separately selected CP and SCP. Hadeler
1983, Dickinson 2019, Dickinson Final, and Danninger 1990 can additionally classify both predicates in one traversal. Other
coposit-created models reject CP and combined mode explicitly.

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
except superseded `dickinson_2019`, plus `dickinson_final` and `adaptive_sponsel_copomatrix`. It independently controls connected
components, the complete pre-check stage, every individual pre-check, the principal-submatrix cutoff, and progress reporting. The
normal `coposit` command exposes none of these choices. Both launchers dispatch to internal companions that each link exactly one
model. There is no C++ model registry, runtime factory, inheritance hierarchy, or executable containing several solvers. Python
modules follow the same one-model rule and retain all maintained experimental variants.

The optional `--progress` flag starts one sleeping reporter and writes a status line to standard error every second.
Preprocessing publishes its current phase and a truthful row, vertex, center, iteration, or pivot counter where one exists. Solvers
do not query the clock: they keep local counters and publish a relaxed-atomic snapshot every 4,096 visited units. Hadeler and
Dickinson report exact support-enumeration coverage; Bundfuss and Sponsel report certified simplex volume; Du Tour and
dimension-reducing models report proof-obligation coverage; the adaptive hybrid adds separate Sponsel/COPOMATRIX routing, split,
projection-child, staircase, switching, and coarse internal-phase counters; Safi reports traversal counters without inventing a
percentage. These metrics are progress through a named phase or mathematical search space, not estimates of remaining wall time.
Public component-pipeline percentages are local to the component currently being solved. The complete definitions are in
`docs/PROGRESS_REPORTING.md`.

Every progress line explicitly distinguishes `stage=preprocessing` from `stage=model`. The first model node is published immediately
before the usual 4,096-unit batching, so expensive first-node work is not shown as zero visited nodes.

Both launchers accept an optional positive `--timeout SECONDS`. It is a hard wall-clock deadline around the isolated companion and
therefore covers parsing, component splitting, pre-checks, and the model without adding clock checks to those hot paths. Expiry kills
the companion and returns exit status `124` without a Boolean. Untimed calls retain direct process replacement and create neither a
supervisor process nor a watchdog thread.

The public `fast` and `safe` paths always enable exact negative-entry connected-component decomposition and the complete
exact-decision pre-check profile. The implementation fuses them: one root scan supplies globally valid checks and the negative graph,
then each component is visited. Frank–Wolfe and exact definiteness are deferred to the component matrices; a connected whole matrix is
used directly without copying.

The pre-check profile contains the complete order-at-most-three criterion, selected principal faces through cardinality three,
nonnegative off-diagonal acceptance, Qi negative-part diagonal dominance, the all-ones witness, bounded floating Frank-Wolfe witness
proposals with exact verification, and exact positive-(semi)definiteness. Floating arithmetic may only propose a witness; an exact
integer calculation must verify it before a decision.

Connected components are taken from the graph whose edges are negative off-diagonal entries. Cross-component entries are
nonnegative, so CP or SCP of the whole matrix is the logical AND of the corresponding component decisions. Disconnected component
matrices are materialized and processed one at a time.

The Python analysis interface selects coarse preprocessing profiles `none`, `connected_components`, `pre_checks`, or `both`.
`coposit-analyze` exposes the finer C++ controls documented in `docs/ANALYSIS_CLI.md`; disabling both preprocessing stages provides
the direct faithful-baseline measurement path.

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

Every model in this table is strict-only.

| Identifier | Relationship and authoritative description |
|---|---|
| `adaptive_dutour_danninger` | Adaptive Dutour/Danninger hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_danninger/ALGORITHM.md). |
| `adaptive_dutour_copomatrix` | Adaptive Dutour/COPOMATRIX hybrid; [`ALGORITHM.md`](../models/experiments/adaptive_dutour_copomatrix/ALGORITHM.md). |
| `adaptive_zischg_sponsel_copomatrix` | Adaptive projection-component comparison; [`ALGORITHM.md`](../models/experiments/adaptive_zischg_sponsel_copomatrix/ALGORITHM.md). |
| `support_pruned_dickinson` | Dickinson with upward support pruning; [`ALGORITHM.md`](../models/experiments/support_pruned_dickinson/ALGORITHM.md). |
| `nullity_support_pruned_dickinson` | Support-pruned Dickinson with singular-vector selection; [`ALGORITHM.md`](../models/experiments/nullity_support_pruned_dickinson/ALGORITHM.md). |
| `rhs_dickinson` | Dickinson with searched positive right-hand sides; [`ALGORITHM.md`](../models/experiments/rhs_dickinson/ALGORITHM.md). |
| `frank_wolfe_dickinson` | Bounded Frank-Wolfe witness proposals before Dickinson; [`ALGORITHM.md`](../models/experiments/frank_wolfe_dickinson/ALGORITHM.md). |
| `one_step_frank_wolfe_dickinson` | One exact Frank-Wolfe segment before Dickinson; [`ALGORITHM.md`](../models/experiments/one_step_frank_wolfe_dickinson/ALGORITHM.md). |
| `pairwise_frank_wolfe_dickinson` | Pairwise-away proposal path before Dickinson; [`ALGORITHM.md`](../models/experiments/pairwise_frank_wolfe_dickinson/ALGORITHM.md). |
| `support_polished_frank_wolfe_dickinson` | Exact active-support polish before Dickinson; [`ALGORITHM.md`](../models/experiments/support_polished_frank_wolfe_dickinson/ALGORITHM.md). |
| `frank_wolfe_sponsel` | One exact Frank-Wolfe segment before Sponsel; [`ALGORITHM.md`](../models/experiments/frank_wolfe_sponsel/ALGORITHM.md). |
| `fracessa` | First-order global-simplex-minimum adaptation of FracESSA; [`ALGORITHM.md`](../models/experiments/fracessa/ALGORITHM.md). |
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
drains assigned work to its result or timeout.

SQLite writes remain in the parent. Its bounded queue is drained into batch transactions, so workers never write the database.
Standard local reference runs use parent CPU 3 and solver CPUs 4 through 7.

## Corpus And Evidence

`testdata/copos_testdata.sqlite3` contains 2,442 exact matrices of orders 1 through 3,361 and uses SQLite `auto_vacuum=FULL` so
deleted or replaced result rows do not leave persistent free pages. The `matrices` table stores exact truth,
source and family text, and five overlapping benchmark flags: `smoke_set`, `representative_core`, `stress_test`, `scale_set`, and
`timeout_5s_strict_set`.
Their definitions and guarded selection evidence are in [`BENCHMARK_SETS.md`](BENCHMARK_SETS.md).

Small matrices remain inline. The 201 rows larger than 500 KB use `file:matrices/<matrix_id>.mtx` and exact symmetric integer Matrix
Market files under `testdata/matrices/`; each row retains the file SHA-256 for an explicit integrity audit. Normal runs neither
recompute that hash nor use a mutable whole-database checksum.

The `results` table distinguishes `ok`, `parse_error`, `timeout`, `node_limit`, and `error`. Only `ok` contains a Boolean and elapsed
time. Result identity includes matrix, lowercase model ID, requested mode, constrained preprocessing choice, and exact native-module
hash. The separate
`preprocessing_results` table records preprocessing-only positive, negative, or unresolved outcomes without pretending that
delegation is a completed copositivity decision.

The immutable source snapshot is `testdata/archive/copos_testdata.original.sqlite3.xz`; its decompressed SHA-256 is
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`. Historical migration utilities and generators are retained
under `testdata/archive/`. The maintained database has no published whole-file checksum because it is intentionally mutable.

Reference-result reports remain under `aidocs/`. Experiment-specific reports live beside their raw data, including
[`experiments/preprocessing_cost_2026-08-10/README.md`](../experiments/preprocessing_cost_2026-08-10/README.md). Literature-family
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
resource behavior, `scale_set` for dimension, storage, and memory questions, and `timeout_5s_strict_set` for the frozen common
five-second strict timeouts of the two final pre-checked algorithms. Use the complete corpus only for final reference results or an
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
