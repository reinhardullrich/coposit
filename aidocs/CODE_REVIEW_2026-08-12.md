# Full Code Review — Open Findings

Date: 2026-08-12
Status: **current open review**
Scope: current public C++ interface, shared parsers and preprocessing, exact factorization, all maintained model targets, progress
reporting, Python wrappers and multiprocessing, CMake/package boundaries, corpus/result integrity, tests, and resource behavior.

This is a review, not a repair pass. No solver or interface source was changed. Findings are ordered by practical priority; a lower
priority does not mean that the item is mathematically unsafe.

## Executive Conclusion

No false copositivity or strict-copositivity decision was found. The current Release and ASan/UBSan suites pass, randomized exact
cross-model comparisons found no disagreement, and the stored completed result rows are internally consistent. Timeouts, node
limits, and historical execution failures remain separate from `false` classifications.

The principal open problem is performance rather than mathematics: the normal public `fast` and `safe` commands unconditionally run
two cubic exact pre-checks. The repository's own uncensored experiment already demonstrates preprocessing times of many minutes on
large matrices, including inputs for which the selected solver has not begun.

## Open Action Register

| Priority | Finding | Smallest sound action |
|---|---|---|
| P1 | Public preprocessing can dominate a run for minutes or hours | Skip optional cubic pre-checks when an explicit work budget is exceeded; keep the cheap checks and delegate to the exact model |
| P2 | The C++ launchers have no CMake install boundary | Install both launchers and their required companions as tested layouts |
| P2 | Public `fast` help/documentation states a traversal bound that does not exist | Describe practical resource incompleteness, not a bounded traversal; describe the fused preprocessing order accurately |
| P2 | Scientific exponents can request unbounded expansion during parsing | Decide and enforce an explicit public input-resource budget, returning a parse/resource error before FLINT allocation |
| P3 | The warning-clean build fails | Delete seven dead locals and two unused parameters/arguments |
| P3 | Central LDLT solve and determinant paths lack direct property tests | Add one focused solve invariant and determinant/pivot test |
| P3 | ASan/UBSan CTest is not self-contained for Python modules | Give the sanitizer Python test the required runtime preload in the test environment |
| P3 | Multiprocessing serializes every request and result twice | Put `Matrix` and result dictionaries directly on `multiprocessing.Queue` |
| P3 | Wheel builds do work that is not shipped, and package metadata is stale | Exclude standalone executables from wheel builds and describe both CP and SCP |
| P3 | Six algorithm maps and one shared comment contradict the parser/default contract | Correct the few stale lines; do not rewrite the documents |

## Findings

### P1 — Mandatory Public Preprocessing Is Not Scale-Safe

The public companions call the component pipeline with default options in
[`cpp/model_main.cpp`](../cpp/model_main.cpp), around line 84. Both stages and every individual pre-check default to enabled in
[`cpp/include/coposit/component_pipeline.hpp`](../cpp/include/coposit/component_pipeline.hpp), around line 12, and
[`cpp/include/coposit/pre_check.hpp`](../cpp/include/coposit/pre_check.hpp), around line 21.

Two enabled checks have cubic worst-case work:

- principal triples enumerate pairs of negative neighbors in
  [`pre_check.hpp`](../cpp/include/coposit/pre_check.hpp), around lines 178–201;
- exact positive-(semi)definiteness copies and fraction-free-factorizes the whole component around lines 406–433.

This is not hypothetical. The maintained uncensored experiment in
[`experiments/preprocessing_cost_2026-08-10/README.md`](../experiments/preprocessing_cost_2026-08-10/README.md) reports:

- 2,440 completed strict pre-check-only matrices consumed 9,743.951 seconds of native time;
- one completed matrix consumed 836.326 seconds in preprocessing;
- the two unfinished graph encodings of orders 3,321 and 3,361 were estimated at 16–17 minutes for triples alone;
- their exact LDLT probes were substantially worse and were stopped rather than spending multiple hours.

Connected-component discovery on the same entire corpus cost only about 3.34 seconds in total. The problem is therefore not the
fused scan or component representation; it is indiscriminate use of optional cubic checks after the cheap scan.

The smallest sound fix is a work-aware default, not a new algorithm and not a false classification. Estimate the triple work from
the already-built negative degrees and skip it when the estimate exceeds a documented budget. Likewise skip exact whole-component
LDLT above its budget. Skipping either pre-check preserves correctness because it merely delegates the undecided matrix to the
selected exact model. Explicit analysis mode can retain an uncensored `all pre-checks` choice.

### P2 — The C++ Launcher Layouts Cannot Be Installed by CMake

[`cpp/CMakeLists.txt`](../cpp/CMakeLists.txt) builds `coposit` with its two adjacent companions and `coposit-analyze` with its isolated
one-model companions, but has no install rule for either C++ layout. The only install rules install Python native modules.

Both launchers deliberately resolve companions beside their own path, falling back to `PATH`. That design is simple and correct only
when each launcher and its required companions are deployed together. At present `cmake --install` cannot create either layout.

The smallest fix is explicit install rules for the two complete layouts and one test per installed launcher from outside the build
tree. The internal analysis companions need not become separately documented commands.

### P2 — The Public `fast` Contract Is Described Incorrectly

The public help in [`cpp/coposit_main.cpp`](../cpp/coposit_main.cpp), around lines 18–24, says:

- `fast` has a “bounded traversal” that may stop unresolved;
- connected components are followed by all pre-checks.

The maintained adaptive model intentionally has no open-node limit. Its 1,000-Sponsel streak cutoff guarantees a finite mathematical
tree; it is not a practical runtime or memory bound. The model can remain unresolved only through timeout, allocation/process
failure, or an external resource limit. The same inaccurate “bounded traversal” wording occurs in [`README.md`](../README.md) and
[`aidocs/PROJECT.md`](PROJECT.md).

The second sentence also hides the actual fused order: cheap root checks happen during the root scan, components are visited, and
Frank–Wolfe plus LDLT are deferred to each component. The implementation and longer documentation already describe that correctly.

Replace only these short contract lines. `fast` should be described as exact but more resource-sensitive, especially in memory; it
should not be described as mathematically bounded when it is not.

### P2 — Exact Scientific Notation Has No Expansion Budget

The exact parser accepts an exponent up to `slong` range in
[`cpp/include/coposit/parsers/exact_number_parser.hpp`](../cpp/include/coposit/parsers/exact_number_parser.hpp), around lines 47–67.
It later forms `10^exponent` with `fmpz_pow_ui`, around lines 141–166. A tiny textual input such as a nonzero number with an enormous
scientific exponent can therefore request enormous CPU and memory before a matrix reaches preprocessing or a solver.

Arbitrary precision is required and ordinary large exact values must remain supported. The missing part is a public resource
boundary, not a small-integer restriction. A documented maximum expanded bit/digit budget, preferably configurable at the analysis
boundary, would let the parser return a controlled resource error instead of relying on OOM or operator interruption. Zero values
should continue to normalize without expansion, as they do now.

### P3 — The Repository Is Not Warning-Clean

A separate build with `-Wall -Wextra -Wpedantic -Werror` fails on eight unique source issues:

- COPOMATRIX's `make_schur_block` does not use its `matrix` parameter;
- seven `solve` functions retain an unused `dimension` local: Du Tour, Safi, Bundfuss, Sponsel, Frank–Wolfe Sponsel, Adaptive
  Du Tour–Danninger, and Adaptive Du Tour–COPOMATRIX.

These are dead remnants, not algorithm defects. Delete them rather than suppressing the warnings. No permanent warning framework is
needed unless warning-clean builds become a maintained CI requirement.

### P3 — Exact LDLT Solve and Determinant Need Direct Tests

[`cpp/tests/test_fraction_free_ldlt.cpp`](../cpp/tests/test_fraction_free_ldlt.cpp) directly covers rank, inertia, one nullspace
vector, complete nullspace bases, symmetric swapping, and an addition pivot. It does not directly call `solve_inplace()` or verify
`determinant()`.

Hadeler and Dickinson exercise solves indirectly, and the randomized model agreement is strong evidence, but this shared exact
primitive deserves one small property test:

```text
A * X == denominator * B
```

Use one multiple-right-hand-side case containing a symmetric swap and one addition pivot, then compare the stored determinant with
FLINT. This also corrects the prior review's overly broad statement that direct factorization tests already covered solve operations.

### P3 — Sanitizer CTest Requires an Undocumented External Preload

The current ASan/UBSan native and Python suite passes when the runtimes are preloaded. Running the sanitizer build's Python test with
plain CTest fails immediately with:

```text
ASan runtime does not come first in initial library list
```

This is a harness problem, not a detected memory error. Set the Python test's sanitizer environment when sanitizer flags are active,
or record one authoritative sanitizer invocation. The former makes `ctest` self-contained and is preferable.

### P3 — Python Multiprocessing Pickles Every Payload Twice

[`python/pycoposit/mp.py`](../python/pycoposit/mp.py), around lines 45–51 and 82–97, manually serializes each `Matrix` and result to
`bytes`, then puts those bytes on `multiprocessing.Queue`. The queue already pickles its objects, so requests and results are each
serialized and copied twice.

The pending-window and result-queue bounds are otherwise correct. Put the `Matrix` and result dictionary directly on the queues and
remove `ForkingPickler`, `pickle.loads`, and their byte wrappers. File-backed corpus jobs make this a small cost today; large inline
Python matrices are the case that benefits.

### P3 — Wheel Builds Include Unshipped Executables

All standalone executables are unconditional CMake `ALL` targets. A scikit-build wheel disables tests but still builds/install-drives
the default target, so solver sources are compiled both for native modules and for standalone executables that are not installed in
the wheel. Keep the source-tree build unchanged and exclude standalone executables only for the wheel build.

The package description in [`pyproject.toml`](../pyproject.toml), line 8, also says only “strict-copositivity” although the maintained
package supports both CP and SCP. Correcting that line is enough; no packaging redesign is needed.

### P3 — Small Documentation Drift

Six algorithm implementation maps still say their `solve` function validates public shape or symmetry even though the parser owns
that contract and model entry points deliberately do not rescan:

- COPOMATRIX 2011;
- Sponsel 2012;
- Adaptive Sponsel–COPOMATRIX;
- Adaptive Zischg Sponsel–COPOMATRIX;
- Adaptive Du Tour–COPOMATRIX;
- Adaptive Du Tour–Danninger.

In addition, `component_pipeline.hpp` calls the pipeline “opt-in” while both pipeline stages default to enabled. These are isolated
line edits. The mathematical descriptions do not need rewriting.

## Improvement Opportunity — Reduce Adaptive Peak Memory Without Changing the Algorithm

This is not a correctness defect and should follow the P1/P2 work. In
[`models/adaptive_sponsel_copomatrix/solver.cpp`](../models/adaptive_sponsel_copomatrix/solver.cpp), around lines 370–383,
`sponsel_split` constructs both full dense children before the first child is checked. The parent and both children therefore coexist,
and the second child is constructed even when the first rejects immediately.

The Bundfuss and Sponsel baselines already use the simpler pattern to copy/construct one child, inspect it, and only then construct
the sibling. Reusing that local pattern in the adaptive model would:

- remove one full dense sibling from the immediate peak;
- avoid sibling construction on early rejection;
- preserve split choice, exact arithmetic, child order, and mathematical coverage.

It does not eliminate the model's deeper `O(depth * n^2)` ancestor storage. In-place mutation/undo or compact deferred-sibling
recipes could address that later, but they are materially more complex and should wait until the one-child-at-a-time change is
measured.

## What Passed

### Current Build And Runtime Checks

- Release build completed successfully.
- All 52 current CTest checks passed, including model, parser, preprocessing, public CLI, progress, and Python wrapper tests.
- ASan/UBSan build completed successfully.
- All 52 ASan/UBSan checks passed with the sanitizer runtimes preloaded; no sanitizer finding was emitted.
- The progress-only source changes that landed during this review were included in both final builds.

### Exact Cross-Model Checks

Random symmetric integer matrices were compared against Dickinson Final:

- 300 matrices, orders 1–6, entries in `[-4,4]`: every maintained model in strict mode and every CP-capable model in non-strict
  mode, 10,500 native calls, zero disagreement;
- 100 matrices, orders 1–7, entries in `[-5,5]`: every CP-capable model with both preprocessing stages in both modes, 2,000 calls,
  zero disagreement;
- 200 matrices, orders 1–7: all four combined-capable models compared combined classification with separate CP/SCP calls, 2,400
  calls, zero inconsistency.

Progress weights and counters are gated telemetry and do not participate in the exact decisions.

### Corpus And Result Integrity

- `PRAGMA integrity_check`: `ok`.
- `PRAGMA foreign_key_check`: zero rows.
- Current result rows: 138,065 `ok`, 25,913 `timeout`, 211 `node_limit`, and 3 historical `error` rows.
- Zero completed truth-field shape violations.
- Zero conflicting completed answers across stored binary hashes.
- Zero impossible completed `CP=false, SCP=true` pairs.
- Zero impossible matrix truth labels.

The three `error` rows are old `-11` executions retained as history, not current negative classifications. Current Danninger code
uses iterative staircase traversal and reports its configured node limit instead of reproducing the former recursive stack failure.

## Acknowledged Limits That Should Not Be Reopened As Bugs

- Adaptive Sponsel–COPOMATRIX can consume substantial memory on deep large-order work; it is documented and is not a leak.
- Dickinson has not been memory-bound in the maintained workloads, but can still be computationally exponential.
- The 3,184 empty Hadeler binary hashes are an explicit legacy schema exception; newer rows are hashed.
- Historical timeout, node-limit, and error rows are evidence, not classifications.
- Algorithm duplication between model directories is intentional isolation under the project rules.
- Progress coverage is a named search-space metric, not an ETA.

## Recommended Order

1. Make the public pre-check profile work-aware.
2. Install the three public binaries together, add the missing Du Tour target, and correct the short public contract text.
3. Decide the exact-number expansion budget.
4. Apply the warning, direct-LDLT-test, sanitizer-environment, double-pickling, wheel, and documentation cleanups as one low-risk pass.
5. Measure one-child-at-a-time Sponsel construction before considering any deeper adaptive memory redesign.
