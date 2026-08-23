**Use the Ponytail skill for every code analysis, plan, modification, review, and summary.**

# AGENTS.md

**Only coposit-specific instructions and explicit coposit overrides belong here.**

## Historical Context

1. The standalone extraction is complete. For inherited extraction decisions or the original file map, consult the historical
   `aidocs/history/COPOSITIVITY_CHECKER_EXTRACTION_HANDOFF.md` only when relevant.
2. When a task refers to old behavior, work from before the extraction, or anything historically unclear, search the local FracESSA
   folders first, including its source, `aidocs/`, `research/`, and `experiments/`. Prefer this local evidence over remote lookup or
   assumptions.

## Worktrees And Source Material

1. Work in the main worktree at `/home/reinhard/projects/coposit` unless Reinhard explicitly approves another worktree first.
2. `/home/reinhard/projects/fracessa` and everything under it are permanently read-only for this project. Never create, modify,
   move, or delete anything there.
3. Copy needed source material into coposit.
4. Do not copy generated builds, binaries, object files, caches, or nested Git metadata.
5. FracESSA's ignored `research/` and `experiments/` material is not present in a fresh clone. Preserve the selected files manually
   before the source worktree is removed.

## Priorities

1. Exact mathematical correctness is absolute. Performance is second. Other concerns are secondary.
2. Keep public input validation explicit and keep proven numerical hot paths small.
3. Optimize for exact arbitrary-precision work on many small matrices without imposing a false small-dimension limit.
4. Keep non-strict-copositivity results, strict-copositivity results, and unresolved resource limits distinct. Never turn a timeout
   or resource exhaustion into `false`.

## Extraction Boundary

1. coposit owns its exact integer wrappers, exact matrix storage, and generic copositivity API. The dependency direction is
   `FracESSA -> coposit`, never the reverse.
2. Shared coposit infrastructure must not include FracESSA headers or own FracESSA concepts such as candidate traversal, ESS reasons,
   circular game normalization, or logging. It may own the generic packed support representation used consistently by models, while
   each model keeps its traversal and pruning policy private.
3. Do not inherit FracESSA's dimension-63 limit. Remove or omit fixed-width sign-scan, graph, parser, and runner limits before
   claiming unrestricted dimensions, including in the `fracessa` model.
4. Require a nonempty square symmetric matrix at the text parser boundary. The parsers construct both triangles, and model entry
   points assume that contract without rescanning shape or symmetry; direct C++ callers are responsible for satisfying it.
5. Keep `testdata/archive/copos_testdata.original.sqlite3.xz` as the immutable byte-exact source snapshot for every
   corpus migration.
6. Keep the maintained core integer-only. Do not add rational matrix storage; a later input wrapper may clear denominators before
   calling the core.
7. Do not add connected-component decomposition until the base checker is established and Reinhard explicitly asks for that
   optimization.
8. Do not publish a full-file checksum for the mutable maintained database. Checksums remain appropriate for the immutable source
   snapshot and model binaries stored with result rows.

## Model Structure

1. Put every model in the Hadeler, Dickinson, or FracESSA inheritance line under `models/hadeler-based/<model-name>/`. Put other
   unchanged source and literature baselines in `models/baselines/<model-name>/` and other coposit-created models under
   `models/experiments/<model-name>/`. Do not put model directories directly under `models/` or create a legacy-model category.
   Each model directory owns its complete algorithm implementation, including copied cone, Danninger, or hybrid code it changes or
   interweaves.
2. Create a new model by copying the closest existing model and changing that copy independently. Duplication between models is deliberate
   isolation, not Ponytail cleanup debt; share it only when Reinhard explicitly decides that it is stable project-wide infrastructure.
3. Shared code under `cpp/include/coposit/` is limited to genuinely model-independent infrastructure such as exact integer and
   matrix storage, packed support storage, reusable exact factorization, input parsing, and the minimal model call contract.
4. The maximal-Z-matrix check belongs only to shared preprocessing. Model implementations must not repeat it or expose a model-local
   switch for it.
5. Every model implements the same `coposit::model::solve(const matrix_integer&, copositivity_mode)` link-time contract and assumes
   its matrix is nonempty, square, and symmetric. Every Dickinson-, Hadeler-, and FracESSA-based model also implements
   `coposit::model::classify(const matrix_integer&)`, supports `both` as one traversal, and defaults to `both` when an analysis or
   reference-run interface omits the mode. Do not implement that classification as two hidden predicate runs. Other models require
   an explicit mode unless they independently implement combined classification. `adaptive_sponsel_copomatrix` supports both
   individually selected predicates but not combined classification. There is no public `fast`, `safe`, or implicit model-selection
   interface during the experimentation phase.
   `coposit --model MODEL --mode strict|non-strict|both` is the sole low-level model-execution interface and owns the single
   complete-preprocessing on/off control. Its inventory includes every literature baseline and every experiment. Python must invoke
   models through `coposit`; it must not import, link, or expose a separate model-specific native execution path. Every internal
   companion links exactly one model and the launcher links none. Do not add method aliases, public model-named
   executables, C++ runtime factories, registries, inheritance, or executables that link model implementations together until
   Reinhard explicitly promotes a public interface.
6. Name a faithful historical or external baseline `<first-author>_<year>`, using one surname in the identifier, for example
   `dutour_2018`. Give every such model a local `ALGORITHM.md` that identifies the exact paper or source revision.
7. A baseline must preserve the source model's mathematical tests, split choice and construction, traversal, pruning, and termination
   rules. Arithmetic, storage, and recomputation may be optimized only when those mathematical decisions remain unchanged. Do not
   add another model's shortcut or reduction to a baseline.
8. Give coposit-created models descriptive non-citation names. The first mathematical change to a baseline requires a copied model
   directory and a new name; never let an original baseline silently become a coposit variant.
9. Every maintained algorithm must have one authoritative, human-facing `ALGORITHM.md` beside its implementation. This file is only
   about the algorithm: do not put build instructions, binary usage, benchmarks, experiment results, or development history in it.
   Write for a reader who understands basic programming and mathematics but does not already know the source: begin with the idea
   in plain language, define necessary terms and notation, and only then give exact formulas and implementation details. Explain
   why the algorithm has its name; classify it as a faithful baseline, strict adaptation, or coposit-created variant; cite the
   primary paper, repository, revision, and local reconstruction or source implementation from which it came; and describe the
   mathematics and complete decision flow in enough detail for a human to reproduce and verify it. Document node state, acceptance
   and rejection tests, witnesses, formulas, reductions or subdivisions, pivot or split selection, child construction and traversal
   order, exact-arithmetic representation, termination behavior, and mathematical limits. Clearly separate source behavior,
   coposit changes, and representation-only optimizations. Update `ALGORITHM.md` in the same task as every mathematical or
   control-flow change. Include a `Known Difficult Inputs` section that explains the matrix structures on which the algorithm's own
   branching, coverage, arithmetic, or termination behavior becomes problematic. This section is about weaknesses, not benefits;
   it may identify a stable corpus matrix as a reproducible example, but must not contain timings, benchmark tables, success rates,
   rankings, or experiment-result summaries. A `README.md`, if useful, is operational, links to `ALGORITHM.md`, and does not
   duplicate its explanation.

## Python Structure

1. Keep the maintained package under `python/pycoposit/` with the thin `core.py`, sequential `single.py`, and bounded process runner
   `mp.py` split. `core.py` adapts Python objects to `coposit`; it does not load model-specific extension modules.
2. Require an explicit maintained model identifier in `compute_matrix()`, `run()`, and `run_multiprocessing()`.
3. Keep multiprocessing completion-ordered and bound submitted-but-not-yielded work by both the worker prefetch window and result
   queue capacity. Never report a worker crash, timeout, or resource limit as a negative classification.
4. Use `python/run_results.py` for timed corpus reference runs. Keep model IDs lowercase, hash the exact model companion used, keep
   one persistent single-threaded Python worker pinned to each requested CPU, and keep every SQLite write in the parent process. The
   worker must execute the selected model only through `coposit` and must report a timeout or terminated companion as
   unresolved. Pin the parent dispatcher and its bounded database-writer queue to a separate explicit CPU; for standard local
   reference runs use CPU 3 for that parent work and CPUs 4 through 7 for the four solver workers.
5. Run every benchmark, corpus test, comparison, and reference run of a Hadeler-, Dickinson-, or FracESSA-based model in `both` mode
   so the result is a full CP/SCP classification. A single-predicate run is allowed only when explicitly studying that predicate or
   its early-exit cost; label it as a predicate run and never count it as a fully solved matrix run.
6. Call all optional runtime activity reporting and stored telemetry **diagnostics**, never progress. The C++ flag is `--diagnostics`,
   the Python argument is `diagnostics=True`, and test-only baseline event recording is source diagnostics.
7. Until Reinhard explicitly changes this policy, enable and persist diagnostics for every benchmark, comparison, corpus, reference,
   and diagnostic run. `python/run_results.py` does this automatically; do not bypass it with an alternate benchmark path that omits
   diagnostics.
8. Keep runtime-tunable variants as one model when their implementations differ only by a scalar parameter. Python forwards the
   required value as `--model-parameter VALUE` to `coposit`; do not create copied model directories for parameter values.

## Matrix Test Sets

Use the six overlapping Boolean flags in the `matrices` table instead of inventing an ad hoc matrix list. See
`aidocs/BENCHMARK_SETS.md` for their composition and selection evidence.

1. Use `smoke_set` first for fast correctness, build, wrapper, runner, and integration checks after an implementation change.
2. Use `core_and_stress_test` for normal algorithm comparisons, parameter choices, difficult exact cases, and timeout pressure.
3. Use `n_le_100` for comprehensive comparisons over every current and future matrix of order at most 100.
4. Use `n_gt_100_solved` for higher-order matrices with at least one paper-reported completed solve in `references_solved`.
5. Use `bpqy_benchmark` for BPQY COP constructions whose exact lift is strictly copositive or still unclassified.
6. Use `bpqy_quick_test` for the fixed six-matrix, medium-duration BPQY development panel.
7. A matrix may belong to several sets. Run the smallest applicable set first; use the complete corpus only for final reference results
   or when the question explicitly requires exhaustive coverage.

## Style

1. Use 140 columns as a soft line-width target for C++, Python, comments, and Markdown.
2. Do not wrap lines merely to satisfy the traditional 80-column limit.
3. Exceed 140 columns when splitting a formula, command, URL, matrix, or readable expression would make it harder to understand.

## Build And Verification

Build and run the maintained Release checks from the repository root:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

1. Every non-trivial model must retain at least one focused runnable correctness check in its own model directory.
2. Before replacing FracESSA's implementation, verify exact classifications against the preserved corpus and include the documented
   stress matrices.
3. Bound parallel corpus runs by memory as well as CPU count, and serialize SQLite writes.
4. Check the copied corpus with `sqlite3 testdata/copos_testdata.sqlite3 'PRAGMA integrity_check;'`.
5. After any change to shared preprocessing or any other shared code executed before model delegation, rebuild and relink every model
   companion and every test target before running or comparing results. This includes parsing and input
   preparation, matrix scans, pre-checks, reductions, connected components, and preprocessing routing. Never mix benchmark rows from
   binaries built against different versions of that pre-model pipeline; run the complete build and test commands above rather than
   building only the model currently under investigation.
