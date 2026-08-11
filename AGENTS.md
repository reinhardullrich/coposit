**Use the Ponytail skill for every code analysis, plan, modification, review, and summary.**

# AGENTS.md

**Only Coposit-specific instructions and explicit Coposit overrides belong here.**

## Startup

1. Before extraction work, read `aidocs/handoffs/COPOSITIVITY_CHECKER_EXTRACTION_HANDOFF.md`.
2. When a task refers to old behavior, work from before the extraction, or anything historically unclear, search the local FracESSA
   folders first, including its source, `aidocs/`, `research/`, and `experiments/`. Prefer this local evidence over remote lookup or
   assumptions.

## Worktrees And Source Material

1. Work in the main worktree at `/home/reinhard/projects/coposit` unless Reinhard explicitly approves another worktree first.
2. `/home/reinhard/projects/fracessa` and everything under it are permanently read-only for this project. Never create, modify,
   move, or delete anything there.
3. Copy needed source material into Coposit.
4. Do not copy generated builds, binaries, object files, caches, or nested Git metadata.
5. FracESSA's ignored `research/` and `experiments/` material is not present in a fresh clone. Preserve the selected files manually
   before the source worktree is removed.

## Priorities

1. Exact mathematical correctness is absolute. Performance is second. Other concerns are secondary.
2. Keep public input validation explicit and keep proven numerical hot paths small.
3. Optimize for exact arbitrary-precision work on many small matrices without imposing a false small-dimension limit.
4. Keep ordinary-copositivity results, strict-copositivity results, and unresolved resource limits distinct. Never turn a timeout
   or resource exhaustion into `false`.

## Extraction Boundary

1. Coposit owns its exact integer wrappers, exact matrix storage, and generic copositivity API. The dependency direction is
   `FracESSA -> Coposit`, never the reverse.
2. Shared Coposit infrastructure must not include FracESSA headers or own FracESSA concepts such as candidate traversal, ESS reasons,
   circular game normalization, or logging. It may own the generic packed support representation used consistently by models, while
   each model keeps its traversal and pruning policy private.
3. Do not inherit FracESSA's dimension-63 limit. Remove or omit fixed-width sign-scan, graph, parser, and runner limits before
   claiming unrestricted dimensions, including in the `fracessa` model.
4. Require symmetry explicitly at the generic boundary. Do not silently replace an input with its symmetric part.
5. Keep `reference/fracessa/testdata/Copos_testdata.original.sqlite3.xz` as the immutable byte-exact source snapshot for every
   corpus migration.
6. Keep the maintained core integer-only. Do not add rational matrix storage; a later input wrapper may clear denominators before
   calling the core.
7. Do not add connected-component decomposition until the base checker is established and Reinhard explicitly asks for that
   optimization.
8. Do not publish a full-file checksum for the mutable maintained database. Checksums remain appropriate for the immutable source
   snapshot and model binaries stored with result rows.

## Model Structure

1. Put unchanged source and literature baselines in `models/baselines/<model-name>/` and Coposit-created variants in
   `models/<model-name>/`. Each model directory owns its complete algorithm implementation, including copied cone, Danninger, or
   hybrid code it changes or interweaves.
   Keep the selected `adaptive_sponsel_copomatrix` model directly under `models/`; put its retired comparison variants under
   `models/legacy/<model-name>/`. Legacy models remain only for reproducibility and are not used or developed further unless
   Reinhard explicitly asks for them. Put active, not-yet-selected variants under `models/experiments/<model-name>/`.
2. Create a new model by copying the closest existing model and changing that copy independently. Duplication between models is deliberate
   isolation, not Ponytail cleanup debt; share it only when Reinhard explicitly decides that it is stable project-wide infrastructure.
3. Shared code under `cpp/include/coposit/` is limited to genuinely model-independent infrastructure such as exact integer and
   matrix storage, packed support storage, reusable exact factorization, input parsing, and the minimal model call contract.
4. Every model implements the same `coposit::model::solve(const matrix_integer&, copositivity_mode)` link-time contract, with strict
   mode as the default. Literature baselines implement both modes; Coposit-created variants reject ordinary mode until explicitly
   extended. `adaptive_sponsel_copomatrix` is explicitly extended to both individually selected modes but does not implement combined
   classification. The selected user-facing model
   builds as the single `coposit` binary with `cpp/model_main.cpp`; model-specific benchmark targets and Python native modules use the
   model identifier. Every executable or native module links exactly one model. Python may select among those modules by name; do not
   link model implementations together or add C++ runtime factories, registries, inheritance, or model-selection plumbing.
5. Name a faithful historical or external baseline `<first-author>_<year>`, using one surname in the identifier, for example
   `dutour_2018`. Give every such model a local `ALGORITHM.md` that identifies the exact paper or source revision.
6. A baseline must preserve the source model's mathematical tests, split choice and construction, traversal, pruning, and termination
   rules. Arithmetic, storage, and recomputation may be optimized only when those mathematical decisions remain unchanged. Do not
   add another model's shortcut or reduction to a baseline.
7. Give Coposit-created models descriptive non-citation names. The first mathematical change to a baseline requires a copied model
   directory and a new name; never let an original baseline silently become a Coposit variant.
8. Every maintained algorithm must have one authoritative, human-facing `ALGORITHM.md` beside its implementation. This file is only
   about the algorithm: do not put build instructions, binary usage, benchmarks, experiment results, or development history in it.
   Write for a reader who understands basic programming and mathematics but does not already know the source: begin with the idea
   in plain language, define necessary terms and notation, and only then give exact formulas and implementation details. Explain
   why the algorithm has its name; classify it as a faithful baseline, strict adaptation, or Coposit-created variant; cite the
   primary paper, repository, revision, and local reconstruction or source implementation from which it came; and describe the
   mathematics and complete decision flow in enough detail for a human to reproduce and verify it. Document node state, acceptance
   and rejection tests, witnesses, formulas, reductions or subdivisions, pivot or split selection, child construction and traversal
   order, exact-arithmetic representation, termination behavior, and mathematical limits. Clearly separate source behavior,
   Coposit changes, and representation-only optimizations. Update `ALGORITHM.md` in the same task as every mathematical or
   control-flow change. Include a `Known Difficult Inputs` section that explains the matrix structures on which the algorithm's own
   branching, coverage, arithmetic, or termination behavior becomes problematic. This section is about weaknesses, not benefits;
   it may identify a stable corpus matrix as a reproducible example, but must not contain timings, benchmark tables, success rates,
   rankings, or experiment-result summaries. A `README.md`, if useful, is operational, links to `ALGORITHM.md`, and does not
   duplicate its explanation.

## Python Structure

1. Keep the maintained package under `python/pycoposit/` with the same thin `core.py`, sequential `single.py`, and bounded process
   runner `mp.py` split used by FracESSA.
2. Require an explicit maintained model identifier in `compute_matrix()`, `run()`, and `run_multiprocessing()`.
3. Keep multiprocessing completion-ordered and bound submitted-but-not-yielded work by both the worker prefetch window and result
   queue capacity. Never report a worker crash, timeout, or resource limit as a negative classification.
4. Use `python/run_results.py` for timed corpus reference runs. Keep model IDs lowercase, hash the exact native module used, keep one
   persistent single-threaded worker pinned to each requested CPU, request cooperative native timeouts with `SIGUSR1`, and keep every
   SQLite write in the parent process. Pin the parent dispatcher and its bounded database-writer queue to a separate explicit CPU;
   for standard local reference runs use CPU 3 for that parent work and CPUs 4 through 7 for the four native solver workers.

## Matrix Test Sets

Use the four overlapping Boolean flags in the `matrices` table instead of inventing an ad hoc matrix list. See
`aidocs/BENCHMARK_SETS.md` for their composition and selection evidence.

1. Use `smoke_set` first for fast correctness, build, wrapper, runner, and integration checks after an implementation change.
2. Use `representative_core` for the normal comparison between algorithms, routine performance measurements, and parameter choices.
3. Use `stress_test` when testing difficult branching, equality and boundary cases, exact-arithmetic growth, timeouts, or node limits.
4. Use `scale_set` for large matrices and questions about dimension growth, density, storage, memory use, and scalability.
5. A matrix may belong to several sets. Run the smallest applicable set first; use the complete corpus only for final reference results
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
4. Check the copied corpus with `sqlite3 testdata/Copos_testdata.sqlite3 'PRAGMA integrity_check;'`.
