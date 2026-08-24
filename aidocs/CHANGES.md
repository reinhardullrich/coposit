# Project Changes

This append-only file records meaningful decisions, results, and evidence that are not clear from Git. Do not log routine edits here.

## 2026-08-24 — Combined results refresh corpus truth and fastest timings

- Promoted 13 previously unknown matrices to strictly copositive from unanimous successful exact `both` results: 12695, 13249,
  13250, 13267, 13269, 13299, 13310, 13375, 13380, 13382, 13384, 13389, and 15392. Evidence came from completed Improved NBC-B7
  and Improved NBC-G2 rows; no model was rerun. The corpus now has 1,497 strict, 1,526 boundary, 2,718 non-copositive, and 93 unknown
  matrices.
- Recomputed the fastest native nanosecond and exact result-reference fields from all successful `both` rows agreeing with current
  truth. The cache covers 5,700 matrices and changed 43 rows. Every reference resolves to its diagnostic result; foreign keys pass
  and SQLite integrity is `ok`. The guarded migration is
  `testdata/archive/refresh_truth_and_fastest_from_combined_results_2026_08_24.sql`.

## 2026-08-24 — Improved NBC-G2 adds a cheap closed-cone extension

- Added `improved_nbc_g2` as an independent copy of Improved NBC-B7. After its exact Halfspace-Rays search, it tests every omitted
  index for a nonnegative right-hand side that preserves the current upper endpoint and adds that index. Floating point only
  proposes a point; integer reconstruction and every certificate or witness check remain exact.
- Removed the preliminary two-millisecond internal deadline. Each target feasibility problem now runs to completion; termination is
  deterministic because a failed full target sweep stops and every successful sweep strictly enlarges the finite upper endpoint.
- A focused boundary case proves that the new stage finds an endpoint unavailable to strictly positive right-hand sides. The full
  108-test Release suite passes.
- On the six-matrix BPQY Quick Test with combined classification, complete preprocessing, diagnostics, and a 120-second cutoff, both
  models solved all six correctly. G2 won five matrices and lost one: its median per-matrix relative time change was -5.90%, and its
  summed native time fell from 104.290 to 97.589 seconds (-6.43%). Processed supports fell from 137,426 to 130,940 (-4.72%). The
  experiment deliberately uses a bounded numerical proposer, not an exact global optimization over right-hand sides.
- A fresh five-second Core-and-Stress comparison found no general gain. B7 and G2 each completed the same 436 of 469 matrices and
  timed out on the same 33, with no mismatch or execution error. Their four-worker campaign wall times were 71.839 and 71.783
  seconds. Across the 436 paired completions, G2's median relative time change was +0.20%, while summed completed native time changed
  from 110.426 to 110.029 seconds (-0.36%). It changed the traversal on only 23 inputs and reduced aggregate processed supports from
  72,676 to 72,611 (-0.09%). The strong six-matrix BPQY effect therefore does not generalize to this mixed corpus.
- On the complete 404-matrix BPQY Benchmark with a 180-second cutoff, combined classification, preprocessing, and diagnostics, G2
  completed 308 matrices and timed out on 96 in 2,511.582 seconds wall time using eight workers. It completed all 298 matrices with
  existing truth and matched every one. It also exactly classified ten previously unclassified matrices as strictly copositive:
  13249, 13269, 13299, 13310, 13375, 13380, 13382, 13389, 12695, and 15392. There were no errors or mismatches.
- Compared with the stored complete 180-second Improved NBC-B7 campaign, G2 gained matrices 13382 and 13389 but lost 13250, 13267,
  and 13384: 308 versus 309 completions, with 306 common completions and 93 common timeouts. Compared with the stored SAT-B3
  campaign, G2 gained 14 completions and lost none: 308 versus 294 completions, with 294 common completions and 96 common timeouts.

## 2026-08-24 — Improved NBC-B9 backoff now measures KKT endpoints

- A walk now resets its scheduling gap to $n$ only when it ends at an exactly verified KKT point. Exact curvature certificates from
  a non-KKT walk remain active, but the next gap still doubles. The seed that triggered a walk is no longer counted toward the next
  gap. This corrects the 180-second BPQY traces in which thousands of dead-end walks continually reset the nominal exponential
  backoff merely because they contributed another overlapping interval.

## 2026-08-24 — Improved NBC-B9 adds exact no-hiding active-set walks

- Added `improved_nbc_b9` as an independent Improved NBC-B7 copy with bounded, reproducibly jittered KKT walks and exponential
  backoff. Floating point chooses candidate moves only; a false floating KKT endpoint switches the remainder of that walk to exact
  arithmetic.
- Walk certificates are buffered until the walk stops. They are admitted only by the exact no-hiding table: a negative eigenvalue
  of the reduced Hessian gives a full upward closure; a positive-definite reduced Hessian plus a feasible nonnegative face minimum,
  or a positive-definite principal block at the endpoint, gives a full downward closure. Flat reduced curvature adds no walk closure.
- The selected B7 seed is processed once before its walk. Its ordinary certificate is deferred until the walk ends, so it cannot
  block its own path and the solver no longer returns to process the same seed a second time.
- Diagnostics now retain every heuristic support and transition, including factorization outcomes, exact rank, nullity and inertia,
  jitter draws, path and coverage rejections, the chosen successor, stopping reason, and the final principal-matrix check.
- The focused B9 checks and the complete 107-test Release suite passed. Fresh five-second combined runs with preprocessing solved all
  46 smoke matrices and 436 of 469 core-and-stress matrices; the remaining 33 timed out, with no wrong classification or execution error.

## 2026-08-23 — Improved NBC-B8 isolates the upward-only B7 path

- Added `improved_nbc_b8` as an independent Improved NBC-B7 copy with the high-cardinality traversal, floating high-frontier filter,
  and all downward pruning removed. It retains ascending exact curvature pruning, Halfspace-Rays Dickinson intervals, resumable
  one-support NBC enumeration, cardinality-boundary compaction, and combined CP/SCP classification.
- The focused regression test requires an entirely ascending event history with no high-frontier visit and no downward certificate.

## 2026-08-23 — BPQY benchmark owns its preprocessing exclusion

- Moved `preprocessing_solved = 0` into the generated `bpqy_benchmark` definition instead of relying only on the named-set runner.
  The nine order-10 BPQY matrices already decided by preprocessing are no longer benchmark members; membership is now exactly the
  existing 404 model inputs, so stored benchmark results and comparisons do not change.

## 2026-08-23 — Improved NBC isolates a sound repeated-call boundary

- Kept the existing `nbc_b7` model and NBC wrapper unchanged for comparison. Added `improved_nbc_b7`, an otherwise identical B7
  model linked to a separate `improved_nbc_upward_supports` module and copied NBC MiniSat All derivative.
- Exhaustive dynamic interval checks exposed a remaining repeated-call defect in the original engine: after certificates made the
  permanent Boolean formula inconsistent, a later prefix call could return an invalid model because the root conflict was not
  retained and the propagation queue had already advanced.
- Improved NBC resets assumptions and enumeration scratch state after every stopped or exhausted call, retains permanent and
  logically valid learned clauses, and latches inconsistency only when it is proved without cardinality or prefix assumptions.
- Focused checks cover repeated one-model enumeration, the permanent-root-conflict regression, every single interval through order
  five, intervals inserted between calls through order three, and the copied exact B7 classification checks.

## 2026-08-23 — NBC-B7 restores support-by-support low/high alternation

- Corrected NBC-B7's traversal after the BPQY Quick Test exposed that its layer-draining callback processed an entire low
  cardinality before the corresponding high cardinality. That batching was not part of SAT-B3 and delayed useful downward
  certificates; on matrix 15436 it processed 198,141 supports without finishing in 180 seconds, while saved SAT-B3 finished after
  11,721 supports in 20.073 seconds.
- NBC-B7 now asks for one low support and one high support alternately. Every exact certificate affects both live NBC solvers
  immediately and is also retained for boundary compaction.
- Added independent low and high prefix-cube cursors. They partition each exact-cardinality layer into disjoint unexplored cubes, so
  NBC can return one support at a time without inserting an exact-support blocker. Rebuilding from compacted certificates preserves
  those cursors and cannot repeat an emitted support.
- Focused tests cover alternating cardinalities, immediate cross-frontier pruning, cursor preservation through compaction, and the
  existing exact small-matrix classifications.
- On the 469-matrix Core-and-Stress set with combined classification, preprocessing, diagnostics, and a five-second cutoff, the
  corrected model completed 434 matrices and timed out on 35 in 47.383 seconds wall time. Every completion matched corpus truth.
  SAT-B3's saved result completed 436, and the former layer-draining NBC-B7 completed 437; over paired completions, corrected NBC-B7's
  median time was 119.38% of SAT-B3. The three completions lost relative to the former NBC-B7 were matrices 10503, 10504, and 13025.
- On the six-matrix BPQY Quick Test with a 180-second cutoff, corrected NBC-B7 completed all six in 38.505 seconds wall time. Its
  per-matrix times were 5.680, 14.343, 20.579, 28.206, 38.215, and 13.861 seconds for matrices 13173, 13226, 13318, 13377, 13387,
  and 15436. The former NBC-B7 completed only matrix 13173 and timed out on the other five; corrected NBC-B7's median paired time was
  41.61% of SAT-B3, a 58.39% median reduction.

## 2026-08-23 — NBC-B7 buffers and compacts certificates by cardinality

- Replaced NBC-B7's stop/restart first-model loop and immediate clause insertion. Review found that the persistent NBC state could
  retain an assumption-derived root assignment across changed cardinality assumptions; one order-five regression then revisited a
  two-element support millions of times although the complete nonempty lattice has only 31 supports.
- NBC now streams one support at a time through a single callback enumeration for the current cardinality. The model processes that
  support immediately but leaves every exact certificate pending. It inserts no exact-support blocker merely to request the next
  support.
- At each cardinality boundary, the active and pending certificate families are combined, expired bounded intervals are removed,
  contained intervals are discarded, and complete full-upward sibling families are recursively replaced by their already-checked
  parent root. A fresh NBC instance then receives only the compacted survivors for the next layer.
- The optional high scan still uses floating point only as a screen. A floating rejection stores nothing and remains available to
  the exact low traversal, which continues through cardinality `n` even after the two frontiers cross.
- Focused compaction tests, exhaustive order-two and order-three classifications, and the former order-five repetition case pass. The
  regression now finishes after nine processed supports with the same strict-copositive classification as SAT-B3.

## 2026-08-23 — NBC-B6 defers certificate activation to cardinality boundaries

- Added lowercase `nbc_b6` as SAT-B3's exact low/upward curvature and Halfspace-Rays path without the high frontier, downward
  pruning, or Johnson--Reams Schur reduction.
- Added a reusable NBC MiniSat All cardinality enumerator. It enumerates a complete layer without temporary per-model blockers,
  stores new exact intervals as pending, expires bounded intervals after their final possible layer, and activates a compacted
  interval family only at the layer boundary. A singular certificate keeps its exact lower endpoint even when `|L| < k`; it does
  not alter the current layer.
- The vendored NBC adaptation exposes model and timeout callbacks, fixes its Boolean representation to explicit signed bytes for
  AArch64, and restores the value returned by its chronological-backtrack helper.
- The focused NBC-B6 checks, exhaustive exact order-three comparison, three stored smoke examples, complete Release build, and the
  maintained C++ suite passed. The full suite initially exposed one stale Python expectation for the already-present `clasp_b3`
  combined model; after correcting that inventory assertion, both affected test groups passed.

## 2026-08-23 — NBC-B7 restores SAT-B3's downward frontier

- Added lowercase `nbc_b7` as an isolated SAT-B3 copy whose only algorithm-engine change is replacing CaDiCaL with a persistent NBC
  MiniSat All instance. It restores alternating individual low/high supports, the floating high-frontier screen, and exact
  positive-definite or consistent singular-PSD downward closures omitted by NBC-B6.
- Every selected support installs its upward, downward, exact-support, or Dickinson clause immediately. NBC keeps its learned state
  across first-model callback calls; high-only floating rejections remain disabled on the exact low frontier.
- The implementation review fixed two incremental-solver boundary defects: pending root clauses are now propagated before assumptions,
  and returning to decision level zero no longer reads before the trail. Small clause and assumption conversions also avoid heap
  allocation. The NBC-B7 sanitizer run, complete Release build, and all 104 maintained checks passed.

## 2026-08-23 — F2 splits B3 coverage between a support generator and exact intervals

- Added lowercase `f2` as an isolated copy of SAT-B3. Its curvature tests, exact Halfspace-Rays Dickinson construction, alternating
  individual low/high traversal, downward rules, witnesses, and combined CP/SCP classification remain unchanged.
- Full upward curvature closures are stored as forbidden roots in a FracESSA-style recursive support generator. Every Dickinson
  interval, downward closure, and exact-support exclusion is stored separately in an exact interval index and checked after support
  generation. Floating high-frontier rejections advance only the resumable high cursor and never affect the exact low proof.
- F2 has no CaDiCaL dependency. The complete Release build and all 101 checks passed. A five-second combined/preprocessing Smoke
  run completed all 46 selected matrices with matching CP/SCP classifications and no timeout.

## 2026-08-23 — BDD-B3 isolates B3's Boolean backend

- Added lowercase `bdd_b3` as an isolated copy of SAT-B3. Its curvature rules, alternating low/high traversal, exact verification,
  Halfspace-Rays fallback, witnesses, and stopping conditions are unchanged; only the incremental SAT coverage backend is replaced.
- One private reduced ordered BDD manager stores the global covered family, cached exact-cardinality families, and separate live low-
  and high-frontier remainder roots. Floating high-layer rejections affect only the high root and disappear when that layer changes.
  The BDD has no CaDiCaL dependency.
- The complete Release build and all 100 checks passed. A five-second combined/preprocessing Smoke run completed all 46 selected
  matrices with matching CP/SCP classifications and no timeout.
- On the 469-matrix Core-and-Stress set with combined classification, preprocessing, diagnostics, and a five-second cutoff, BDD-B3
  completed 436 matrices and timed out on 33 in 28.780 seconds wall time. Its completion and timeout sets exactly matched the saved
  SAT-B3 reference, with no classification mismatch. BDD-B3's completed-case median was 0.703 ms versus SAT-B3's 1.476 ms; over the
  436 paired completions, the median per-matrix time was 47.98% of SAT-B3, and BDD-B3 was faster on 435 cases.
- On the 404-matrix BPQY benchmark with a ten-second cutoff, BDD-B3 completed 232 matrices and timed out on 172 in 304.216 seconds,
  versus SAT-B3's durable result of 257 completions, 147 timeouts, and 254.068 seconds. BDD-B3 gained 13 completions at orders 30–35
  but lost 38 at orders 40–60; its faster 47.145 ms successful-case median did not compensate for 25 additional hard-case timeouts.

## 2026-08-23 — SAT-B3 BPQY benchmark at 180 seconds

- Reran SAT-B3 in combined mode on all 404 effective `bpqy_benchmark` inputs with complete preprocessing, diagnostics, a 180-second
  per-matrix cutoff, CPU 2 for dispatch, and workers on CPUs 3 through 9. The campaign completed in 3,210.422 seconds wall time.
- SAT-B3 completed 294 matrices and timed out on 110, versus 257 completions and 147 timeouts at ten seconds. The longer cutoff
  recovered 37 former timeouts and raised the completion rate from 63.61% to 72.77%.
- Every completion was a known strictly copositive matrix and matched corpus truth. None of the 106 truth-unknown inputs completed;
  four known-strict inputs still timed out. Full per-order and timing evidence is in `aidocs/REFERENCE_RESULTS_BPQY_BENCHMARK.md`.

## 2026-08-22 — BPQY baseline and SAT-B3 benchmark

- Ran all eight maintained literature baselines, Adaptive Sponsel/Copomatrix, and SAT-B3 on the 404 effective `bpqy_benchmark`
  matrices with complete preprocessing, diagnostics, a ten-second cutoff, CPU 2 for dispatch, and workers on CPUs 3 through 9.
  Models with a combined classifier ran in `both` mode; the other six models ran the strict predicate only.
- SAT-B3 completed 257 full classifications. Dickinson 2019 completed 98, Hadeler 1983 completed 69, and Danninger 1990 completed
  10. Among strict-only models, Adaptive Sponsel/Copomatrix completed 50, Copomatrix 2011 completed 24, Bundfuss 2008 and Sponsel
  2012 completed four each, Safi 2021 completed one, and Dutour 2018 completed none.
- Every one of the 517 completed rows matched known corpus truth. None of the 106 truth-unknown matrices completed, and every
  non-completion was a timeout. Full settings, hashes, per-order counts, timing summaries, and mode qualifications are recorded in
  `aidocs/REFERENCE_RESULTS_BPQY_BENCHMARK.md`.

## 2026-08-22 — Generated BPQY COP benchmark

- Reconciled 57 BPQY COP rows whose corpus truth remained unknown despite unanimous saved exact combined results. All 57 are strictly
  copositive; no solver was rerun. The corpus now has 1,484 strict, 1,526 boundary, 2,718 non-copositive, and 106 unknown matrices.
- Added the generated `bpqy_benchmark` corpus flag for source-51 `BPQY COP ...` matrices whose exact lift is either strictly
  copositive or still has both truth fields unknown. Known non-copositive and boundary lifts are excluded, and later truth updates
  change membership automatically.
- The raw flag contains 413 of 825 COP constructions: 307 strict and 106 unknown. The standard named-set preprocessing exclusion
  removes nine strict cases, so `python/run_results.py --matrix-set bpqy_benchmark` selects 404 matrices.
- Added the selector to the reference runner and a focused generated-membership regression. The guarded migration verifies corpus
  composition, timing references, SQLite integrity, and foreign keys.

## 2026-08-22 — Remove BPQY SPN materializations

- Removed all 150 BPQY SPN Float64 materializations, matrix IDs 12874 through 13023, from the maintained corpus. All 150 were
  non-copositive exact lifts decided by root preprocessing, so none exercised a model. The 525 BPQY PSD and 825 BPQY COP matrices
  remain unchanged.
- Removed the corresponding 1,050 model-result rows and 600 preprocessing-result rows from the diagnostics database. The guarded,
  idempotent removal script preserves the original 450-row TSV artifact; both SQLite integrity checks pass.

## 2026-08-22 — BPQY COP extensions at orders 10, 15, 55, and 60

- Applied the unmodified BPQY Julia 1.8.5 Float64 COP construction at orders 10, 15, 55, and 60. Each order has three designated
  support sizes and seeds 1 through 25: respectively $(2,4,5)$, $(4,8,10)$, $(14,28,41)$, and $(15,30,45)$.
- Imported 300 unique primitive dyadic integer materializations as matrix IDs 15206 through 15505. Their intended construction is
  copositive boundary, but both exact truth fields remain `NULL` because Float64 materialization can perturb the boundary.
- The reproducible TSV artifact has SHA-256 `8594d83c46f6c6b803ffa846858d14e0d5e4a2abb75b25d365e9082c7d85dac9`. The guarded
  importer found no identical existing matrix under direct positive scaling, its idempotent replay retained exactly 300 rows, and
  SQLite integrity passed.
- Ran exact SAT-B3 in combined mode with complete preprocessing, diagnostics, and a 30-second per-matrix cutoff. The 300 attempts
  finished in 413.409 seconds wall time: 252 completed, comprising 101 strictly copositive and 151 non-copositive exact integer
  materializations, while 48 timed out and remain unknown. Preprocessing alone decided 160 completed cases with zero model
  delegations.
- Copied only those 252 completed classifications into corpus truth, marked the 160 preprocessing decisions additively, and refreshed
  each completed row's fastest eligible nanosecond result. The 48 timeouts received neither truth nor timing. Replaying the guarded
  merge made zero changes; database integrity and foreign-key checks passed.

## 2026-08-22 — G1 separates bounded guidance from full-upward coverage

- Added lowercase `g1` as an isolated F1-derived experiment. Exact curvature roots and Dickinson intervals reaching the full ceiling
  remain in the FracESSA-style upward generator. Proper Dickinson intervals live in a separate expiring index: covered supports skip
  repeated exact Dickinson work but may still nominate a full-upward curvature root through a floating screen followed by exact
  verification.
- At each completed cardinality, G1 may greedily shrink the active full-upward roots through an exact counterexample search. All
  completed layers are don't-cares; every accepted replacement preserves the covered/uncovered partition on every unprocessed layer.
  The deterministic work budget may forgo compaction but cannot create pruning without proof.
- The focused test exhaustively checks every nonempty root family on three indices at both possible layer boundaries. The complete
  Release build and all 99 checks passed; a five-second combined/preprocessing Smoke run completed all 46 matrices with matching
  CP/SCP classifications and no timeout.
- On the maintained 469-matrix Core-and-Stress set with combined classification, preprocessing, diagnostics, and a five-second
  cutoff, G1 completed 436 matrices and timed out on 33, with no mismatch. Its completed-case median was 0.569 ms. The current stored
  SAT-B3 binary completed the same 436 matrices with a 1.476 ms median; on those paired cases G1's median relative time was 60.86%
  lower. Current F1 completed 428 matrices with a 0.751 ms median; G1 was 26.69% lower on their 428 paired completions and completed
  eight additional matrices.
- The fixed nine-matrix BPQY/Chen--Burer development panel exposed the opposite behavior under a 180-second cutoff. G1 completed only
  the three order-25 cases in 14.895, 50.159, and 46.627 seconds, versus SAT-B3's 0.050, 0.094, and 0.008 seconds. G1 timed out on
  all four order-30/40 BPQY cases that B3 solved in 1.006--2.498 seconds; both models timed out on the two order-50 Chen--Burer cases.
- To remove needless covered-support screening exposed by that panel, every proper Dickinson interval now stores an exact
  reduced-curvature decision for its final optimized upper endpoint. Covered supports receive the floating screen and conditional
  exact check only when at least one covering interval has a curvature-bad upper endpoint; otherwise they are skipped immediately.
- The full-upward root compactor now works breadth-first, allowing at most one accepted deletion per root and round. Its deterministic
  search and normalization budgets scale as $\max(4096,16n|\mathcal F|)$ and charge both recursion states and root comparisons;
  large activation batches no longer perform quadratic pairwise redundancy checks. On synthetic compressible antichains it reduced
  6,435 roots to 3,318 in 0.021 seconds, 24,310 to 13,891 in 0.047 seconds, and 92,378 to 56,473 in 0.173 seconds while exhaustive
  small-family tests preserved every unfinished-layer coverage decision.
- The resulting G1 binary retained the Core-and-Stress result of 436 completions and 33 timeouts over 469 matrices, with no mismatch
  and a 0.579 ms completed-case median. On the fixed nine-matrix panel it again completed only the three order-25 cases, now in
  17.037, 56.208, and 52.543 seconds; these were 12--14% slower than the preceding G1 binary. The four order-30/40 BPQY cases and two
  order-50 Chen--Burer cases all timed out at 180 seconds, whereas the stored SAT-B3 binary completes the four BPQY cases in
  1.006--2.498 seconds.

## 2026-08-21 — SAT-B5 ascending-only block-reduction experiment

- Added `sat_b5` as an isolated SAT-B4 experiment without the high-frontier scan or downward SAT clauses. It retains ascending
  upward-curvature pruning, Halfspace-Rays Dickinson intervals, and exact Johnson--Reams Schur-complement restarts.
- On the fixed nine-matrix Bomze/Chen--Burer panel with preprocessing, combined classification, diagnostics, and a 180-second
  timeout, SAT-B5 completed all nine. Its total time was 228.634 seconds versus 296.993 seconds for stored compatible
  Halfspace-Rays results. SAT-B3 and SAT-B4 were much faster on the seven smaller completing cases but timed out on both order-50
  Chen--Burer matrices; SAT-B5 completed those in 81.527 and 81.569 seconds.

## 2026-08-21 — SAT-B4 exact iterative block reduction

- Added `sat_b4` as an isolated copy of SAT-B3. Whenever either frontier has already factorized a positive-definite principal block
  $B$, B4 exactly tests the Johnson--Reams condition $-B^{-1}C\geq0$. On success it constructs a positive integer multiple of the
  Schur complement, removes its common content, destroys the old SAT state, and iteratively restarts on that strictly smaller
  equivalent CP/SCP problem. Before solving every column of $-B^{-1}C$, an exact all-ones solve rejects any outside column $c$ with
  $c^TB^{-1}\mathbf1>0$; the low-frontier fallback reuses that solve. A miss otherwise falls back unchanged to SAT-B3's
  Halfspace-Rays or downward-closure action.
- An exact density probe on the maintained seven BPQY and two Chen--Burer development matrices exhaustively checked cardinalities one
  through three and sampled up to 500 supports at every higher cardinality. Of 203,410 inspected blocks, 116,610 were positive
  definite but only five satisfied the sign condition. All five were BPQY supports of cardinality at most three; none of 44,234
  sampled positive-definite blocks at cardinality at least four and none of 67,899 Chen--Burer positive-definite blocks qualified.
- Chained low reductions nevertheless compounded on BPQY matrix 13279: the final active problem had order 33 rather than 40. Matrix
  13145 reduced from order 30 to 29. The seven previously completing panel cases retained matching classifications and essentially
  unchanged one-run wall times. Both Chen--Burer cases still timed out after 30 seconds; at second 29 B4 had processed 1.61% and
  0.35% fewer supports than the matching stored B3 diagnostics, measuring the miss-path overhead as small but nonzero.
- The initial complete Release build and all 95 checks passed. Focused tests now also cover a nontrivial two-index eliminated block,
  all three CP/SCP outcomes, exact prefilter rejection, iterative reduction chains, and preservation of exhaustive small-matrix
  classifications.

## 2026-08-20 — Current preprocessing and unresolved BPQY corpus refresh

- Imported the exact primitive-integer negation of every current FracESSA game matrix not already present up to positive scaling.
  Sixteen of 1,411 source matrices were already covered; IDs 13811 through 15205 add the other 1,395, with both truth fields left
  unknown. A full replay found no positive-scale duplicate in either source or the resulting 5,684-row corpus, all 1,411 source
  matrices are covered, the two external payload hashes match, and both SQLite integrity checks pass.
- Ran the current combined preprocessing pipeline over all 4,289 matrices with a 10-second cutoff. It completed 3,426 matrices,
  produced partial classifications for 270, and left 514 unresolved. The maintained corpus now marks 3,482 matrices as solved by
  preprocessing and records complete CP/SCP truth for 4,127 matrices from this run and earlier evidence.
- Repeated that full-corpus run with the same binary and cutoff as a conservative audit. It completed 3,427 matrices, produced 270
  partial classifications and 514 unresolved outcomes, and ended with 71 native and 7 parent hard timeouts. All 55 flags supported
  only by older or longer successful evidence remained set, so the additive total stayed 3,482. The merge lowered 770 cached fastest
  preprocessing times, changed four equal-time references to their canonical row, and increased or removed none. Every flag has a
  matching complete zero-delegation diagnostic result, no such result lacks its flag, and both SQLite integrity checks passed.
- The new heuristic KKT step decided order-50 BPQY matrix 12693 as non-copositive in 5.284 milliseconds; its previous fastest
  complete XXX2 decision took 42.439 milliseconds.
- Ran SAT-Halfspace-Rays for 180 seconds on the 162 remaining BPQY matrices without a completed timing, smallest orders first. It
  completed 47 strictly copositive cases: all 17 order-30 cases, 26 of 37 order-35 cases, and 4 of 38 order-40 cases. The remaining
  115 timed out, including every tested order-45 and order-50 case.
- After importing those exact decisions and timings, the corpus contains 4,174 complete truth classifications and 4,133 fastest
  completion references. SQLite integrity and cache-consistency checks passed.

## 2026-08-19 — SAT-A5 best-improvement monotone Dickinson

- Added lowercase `sat_a5` as an isolated copy of SAT-A3. Halfspace, synthesized Rays, exact reconstruction, SAT traversal, and the
  shared 20 millisecond monotone deadline remain unchanged.
- Every monotone pass now evaluates all targets reached before that deadline against the same incumbent, retains the strongest exact
  $(|U|,d)$ improvement, and installs only that one endpoint. If the deadline expires after an exact improvement was found, the best
  result reached so far is installed before the optional search stops.
- A focused fixed-support regression has a first exact target reaching $|U|=7$ and a later target reaching $|U|=8$, proving that A5
  selects the latter rather than retaining A3's target-index-order result.
- The single-job Release build and all 88 checks passed. A five-second combined/preprocessing Smoke run classified all 46 matrices,
  matched every stored truth value, and had no timeout; the maintained corpus integrity check returned `ok`.

## 2026-08-19 — SAT-A4 monotone-only Dickinson

- Added lowercase `sat_a4` as an isolated SAT-Dickinson model. It starts from the ordinary exact all-ones right-hand side and applies
  the A3 monotone feasibility extension directly, without coordinate sweeps, synthesized rays, shortlists, objective LPs, or MILP.
- Every numerical proposal must preserve the incumbent upper endpoint and add an omitted index; exact integer reconstruction and exact
  sign checks are the only operations allowed to enlarge the certificate or decide the matrix.
- A focused regression proves strict enlargement of an all-ones endpoint while retaining all incumbent indices. The complete
  single-job Release build and all 87 checks passed. A five-second combined/preprocessing Smoke run classified all 46 selected
  matrices correctly with no timeout, and both SQLite integrity checks returned `ok`.
- On the three previously selected order-25 BPQY COP cases, A4 completed in 2.378, 23.323, and 13.071 seconds. Its paired median was
  28.65% slower than A3 and 8.24% slower than SAT-Halfspace-Rays; it processed a paired median 37.82% more supports than A3.
- On the matching order-30 panel, A4 completed in 2.612, 5.053, and 14.777 seconds. Its paired median was 0.64% slower than A3 but
  9.66% faster than SAT-Halfspace-Rays. One case was 19.01% faster than A3, while another was 94.50% slower.
- All three matching order-50 cases timed out after 180 seconds at the same cardinalities as A3. A4 processed 13.48% to 45.50% more
  exact supports without reaching a higher cardinality. The evidence is therefore mixed and does not support replacing the Rays
  incumbent with monotone-only enlargement.

## 2026-08-19 — SAT-A3 monotone enlargement of the Rays endpoint

- Added lowercase `sat_a3` as an isolated copy of `sat_halfspace_rays_dickinson`. It first completes the unchanged Halfspace-Rays
  search and sets $F=U(u_{\rm rays})$.
- For each omitted outside index, A3 uses a bounded feasibility LP to seek a strictly positive right-hand side that preserves every
  index of $F$ and adds the target. Only exact integer reconstruction and sign checks may enlarge $F$ or create a certificate;
  numerical failure is not treated as a proof of infeasibility.
- The focused regression raises a Rays endpoint from five to more than five elements and verifies that every original endpoint index
  remains present. The LP uses no binaries, objective, big-$M$ constants, or new exact factorization.
- The complete Release build and all 86 tests passed. A five-second combined/preprocessing Smoke run classified all 46 selected
  matrices correctly with no timeout, and the maintained corpus integrity check returned `ok`.

## 2026-08-19 — SAT-A2 LP-relaxation-guided endpoint

- Added lowercase `sat_a2` as an isolated copy of `sat_halfspace_rays_dickinson`. After the exact ray incumbent, it solves one bounded
  binary64 relaxation of the maximum-halfspace model, exactly reconstructs and verifies the proposed right-hand side, and makes one
  LP-guided monotone extension only when the estimated upper-endpoint gap exceeds one. It performs no MILP branching and claims no
  exact optimality from the floating objective.
- Focused exact regressions exhibit both improvement paths: the root LP candidate raises one fixed-support upper endpoint from 10 to
  11, while the monotone follow-up raises another from 5 to 6 under an estimated ceiling of 7.
- A disposable five-second combined/preprocessing Smoke probe classified all 46 delegated matrices correctly. Against the latest
  saved SAT-Halfspace-Rays rows, SAT-A2 processed 1,418 rather than 1,419 supports: 45 matrices were unchanged, while matrix 9561 fell
  from 36 to 35 after replacing one `(k,d,|U|)=(3,3,6)` certificate by `(3,4,7)`. The paired median elapsed-time ratio was 1.037, so
  this small set shows real but sparse extra pruning and a small median overhead, not a general speed advantage.
- On the three order-25 BPQY COP `rho0=19` representatives previously used for the A1 comparison (IDs 12625, 12628, and 12631), a
  180-second SAT-A2 run completed all three in 2.850, 11.778, and 11.703 seconds. Relative to saved SAT-Halfspace-Rays rows, SAT-A2
  processed a paired median 6.16% fewer supports but was only 0.39% faster, so its LP cost consumed almost the entire pruning gain.
  Relative to the latest A1 rows, SAT-A2 was 42.04% faster even though it processed a paired median 8.45% more supports; A1's multiple
  retained intervals raised the corresponding certificate counts to 25,285, 97,148, and 79,684.
- A simultaneous three-core-per-model comparison on strictly copositive order-30 BPQY COP `rho0=15` representatives 13145, 13147,
  and 13152 completed every case. SAT-A2 processed 4.04%, 4.87%, and 4.50% fewer supports than SAT-Halfspace-Rays, but took 3.73%,
  5.48%, and 8.01% longer. At this order the paired median pruning gain was 4.50%, while the paired median runtime loss was 5.48%.
- The complete single-job Release build and all 85 tests passed; the maintained corpus integrity check returned `ok`.

## 2026-08-19 — SAT-A1 upper-endpoint antichain

- Added the lowercase `sat_a1` model as an isolated copy of `sat_halfspace_rays_dickinson`.
- Each existing exact coordinate or synthesized-ray sweep now retains every inclusion-maximal upper endpoint it encounters. The model
  inserts the corresponding anchored Dickinson intervals `[I,U]` while preserving the original interval when it contributes distinct
  downward coverage. It adds no factorization, sweep, LP, or MILP call.
- A focused regression verifies both antichain reduction and insertion of multiple incomparable intervals. The 46-matrix Smoke set
  completed with 46 matching classifications and no timeouts; all 84 Release checks passed, and the corpus integrity check returned
  `ok`.

## 2026-08-19 — KKT endpoints no longer receive redundant Dickinson certificates

- Removed the exact Halfspace-Rays calculation at every verified KKT endpoint in `xxx_two`. Its exact KKT calculation already supplies
  the maximal upward interval and, for a positive-semidefinite face, the maximal downward interval.
- A distinct non-KKT seed still receives its exact Halfspace-Rays certificate. When the seed itself is KKT, its KKT intervals cover it
  and no ordinary seed certificate is calculated.
- Added focused checks for both cases: a distinct seed remains certified, while neither a distinct KKT endpoint nor a KKT seed emits
  an ordinary path-certificate event.

## 2026-08-19 — KKT-gated XXX2 terminal certificates

- Changed `xxx_two` so every path still receives an exact SAT-Halfspace-Rays certificate at its SAT-selected seed, but its distinct
  terminal support receives that certificate only after exact arithmetic confirms a floating KKT claim. Step-limit, blocked,
  negative-candidate, and numerically inconclusive endpoints are no longer factorized merely because the floating walk stopped there.
- Preserved the critical-point rule: when floating arithmetic claims KKT and exact arithmetic rejects that claim, the path switches
  to exact arithmetic and continues until an exact KKT or exact stopping condition. A focused regression proves that a non-KKT
  endpoint emits no terminal-certificate event; all 83 Release checks passed.
- Repeated the 30-second combined, complete-preprocessing order-15--30 independent-angle Hildebrand run with ascending seeds, parent
  CPU 2, and worker CPUs 3 through 9. Binary SHA-256 was
  `a2f965f26d334304abf71b26b182552e35754b9d45c948b6e1ff7da0cb9653ee`.
- The changed model again completed orders 15--19 and timed out from order 20 onward. It was faster than the preceding XXX2 build on
  all five common completions, with a median 25.811% reduction in elapsed time, but still took a median 1.367 times as long as
  SAT-Halfspace-Rays. SAT-Halfspace-Rays remains the cached fastest model and uniquely completed order 20.

## 2026-08-19 — Exact independent-angle Hildebrand panel

- Added one exact Hildebrand circulant boundary matrix at every order from 15 through 30 while retaining all original representatives
  and their diagnostics. The new rows are IDs 13795--13810 and use source ID 20.
- Replaced the repeated rational rotation only within the new construction: each admissible angle has its own rational
  $\tan(\zeta_j/4)$. Exact checks enforce angle order, Hildebrand's alternating-angle condition, positive zero-polynomial
  coefficients, and the resulting zero equation. No rounding, perturbation, or near-Hildebrand family was introduced.
- A bounded search approximated the theorem's equally spaced target angles with every denominator limit from 5 through 100, followed
  by deterministic 20- and 30-second low-denominator refinements per order. A separate 60-second order-19 pass tested 18,447 further
  exact tuples without improving its 118-digit result. These are documented best-found values, not claimed global minima.
- Maximum primitive entry length ranges from 52 digits at order 15 to 360 digits at order 29. The existing order-15--25 panel ranges
  from 294 to 2,423 digits, so the new panel isolates the same exact support-$n-2$ mathematics with substantially lower arithmetic
  height. All new matrices remain inline; SQLite integrity and foreign-key checks passed.
- Ran both `sat_halfspace_rays_dickinson` and `xxx_two@ascending` in combined mode with complete preprocessing and diagnostics, a
  30-second matrix cutoff, parent CPU 2, and worker CPUs 3 through 9. SAT-Halfspace-Rays completed orders 15--20; ascending XXX2
  completed orders 15--19. All completed decisions agreed with the construction: copositive but not strictly copositive.
- On the five matrices completed by both models, SAT-Halfspace-Rays was faster in every case; the median paired time ratio was
  2.043 for ascending XXX2 over SAT-Halfspace-Rays. SAT-Halfspace-Rays also uniquely completed order 20 in 23,343,422,080 ns.
- The successful SAT-Halfspace-Rays rows now supply `fastest_elapsed_ns` and `fastest_result_ref` for orders 15--20. Orders 21--30
  timed out in both runs and retain no fabricated completion time.

## 2026-08-19 — Reproducible BPQY COP/PSD order extension

- Applied the unmodified BPQY Julia 1.8.5 Float64 generation formulas to orders 20, 30, 35, 40, and 45, using the nearest-quarter,
  half, and nearest-three-quarter designated support sizes and the 25 manuscript-reported seeds 1 through 25.
- Generated and imported 75 COP and 75 PSD matrices at each order, for 750 new primitive dyadic integer materializations. The SPN
  family was excluded because all 150 earlier SPN materializations have a negative diagonal and are immediate negative witnesses.
- Retained the point of the experiment: the paper proves the ideal floating construction, but exact integer lifting can perturb its
  boundary. Both truth fields therefore remain `NULL`; no copositivity claim was inferred from the intended class.
- Reproduction is pinned by the Julia script and the 9.1 MiB TSV artifact with SHA-256
  `cd5c4eb3aaf0bad236b375b934d23a142234008546ae98c6c3f25c644466a81f`. Direct positive-scaling duplicates were absent, all 750
  rows have source ID 51, and SQLite integrity passed.
- The older 450-row BPQY reconstruction remains unchanged and uses seeds 0 through 24. The upstream wrappers run 0 through 25 but
  state that only seeds 1 onward are reported; the new extension follows the reported 1-through-25 set.
- Ran exact `sat_halfspace_rays_dickinson` in combined mode with complete preprocessing and diagnostics, a ten-second matrix cutoff,
  parent CPU 2, and worker CPUs 3 through 9. Binary SHA-256 was
  `485b736c5a747fb998f85c7272099b5e657e64645c774c2b540f56d421affb16`; all 750 assignments finished in 546.577 seconds wall time.
- Completed 404 classifications: 255 exact integer materializations are strictly copositive and 149 are non-copositive; none were
  classified as copositive boundary. The other 346 timed out and retain both truth fields as `NULL`.
- The intended COP family completed 123 of 375 rows (62 strict, 61 non-copositive), while intended PSD completed 281 of 375
  (193 strict, 88 non-copositive). This is classification of the exact lifted integers, not validation of the ideal BPQY labels.
- Copied only the 404 completed combined answers into corpus truth. Their `fastest_elapsed_ns` and `fastest_result_ref` fields exactly
  match the stored diagnostic rows; timeout rows have neither cached completion nor inferred truth. Both databases passed integrity
  and cross-database consistency checks.

## 2026-08-19 — Binary64 KKT-path experiment (`xxx_two`)

- Added `xxx_two`, an isolated copy of XXX whose active-set walk uses a one-time normalized binary64 matrix and a pivoted symmetric
  Bunch--Kaufman solve. Floating point chooses only the next support; an exact fraction-free KKT solve must verify the terminal point
  before any upward or downward interval enters the proof SAT instance.
- Added explicit depth-first backtracking over each support's ordered preferred successors. A blocked branch, current-path cycle,
  earlier-path collision, numerically inconclusive floating solve, or failed exact terminal proposal pops to the previous support and
  tries its next candidate. Every support visited on successful and dead-end branches joins separate non-proof path memory.
- After exact KKT intervals are installed, SAT now checks the original path seed. Only when that seed remains open, the retained exact
  Halfspace-Rays engine computes and installs one Dickinson interval for it. This closes the seed without charging every floating
  path support for exact arithmetic.
- A floating intermediate point that appears nonnegative with negative payoff now triggers the existing exact face solve immediately.
  Only an exactly nonnegative negative-payoff vector terminates; a failed proposal adds no interval and leaves the path unchanged.
- Added twelve focused checks for exact classifications, the order-two criterion, alternative-pivot selection, complete backtracking,
  repeated-path seed closure, conditional seed closure, alternating cardinalities, exact SAT intervals, diagnostics, and timeout
  propagation. The complete Release build passed all 82 checks.
- The final 46-row Smoke integration run completed every matrix with the exact expected combined CP/SCP classification, zero timeout,
  and zero execution error. This supersedes the two earlier diagnostic binaries whose one-way walks terminated on path collisions or
  floating singularities.

## 2026-08-18 — Preprocessing decision times are now canonical for preprocessing-complete matrices

- Registered the 14 exact complete depth-2 60-second continuation results and 22 exact complete ten-minute Motzkin--Straus results
  that existed only in retained CSVs. Each is now an exact, referencable `preprocessing_depth_2` combined diagnostic row; no solver
  or preprocessing work was rerun.
- For all 2,765 `preprocessing_solved` matrices, `fastest_elapsed_ns` and `fastest_result_ref` now identify the shortest complete
  shared-preprocessing decision. Non-preprocessing-complete rows retain their normal shortest eligible combined model result.

## 2026-08-18 — Truth and fastest-time refresh after XXX completions

- Added the unanimous exact combined classifications for matrices 12649 and 12693. Both are non-copositive, reducing the number of
  matrices without a CP/SCP classification from 105 to 103.
- Refreshed only `fastest_elapsed_ns` and `fastest_result_ref` from completed combined diagnostic rows agreeing with the now-current
  truth. The 3,351 eligible timings are current; no solver or preprocessing run was repeated.
- Verified all 2,729 complete preprocessing decisions retained in the central diagnostics database against corpus truth and
  `preprocessing_solved`. No flag was missing or removed, and the additive total remains 2,765.

## 2026-08-18 — XXX KKT paths confined to uncovered supports

- Corrected XXX's local-walk policy: every proposed successor is now tested under a complete assignment against the existing SAT
  proof clauses. Covered proposals are skipped, alternative pivots are tried in deterministic order, and the local walk stops when
  no uncovered successor remains; the global SAT traversal then supplies another uncovered seed.
- Reused the one persistent SAT instance. No second proof state, visited-support database, or new search abstraction was added.
- Removed the now-redundant path-local visited set: every processed support's mandatory Dickinson interval already makes an exact
  revisit unsatisfiable.
- Removed the arbitrary $2n$ path cap. A local walk now ends exactly when all deterministic successor candidates are empty or already
  covered; finiteness still follows because each entered support is newly open and becomes covered immediately.
- Added an exhaustive dimension-four regression over every $L\subseteq U$ interval and every nonempty candidate, proving that
  interval-covered KKT proposals are rejected and uncovered alternatives remain admissible.
- The final changed binary passed all 19 focused XXX checks, classified all 46 Smoke matrices exactly in combined mode with both
  preprocessing stages and no timeout or error, and passed all 81 Release checks.

## 2026-08-18 — XXX proof-complete interval integration

- Superseded XXX's KKT-only stalled-search design. Every visited support now runs the retained SAT-Halfspace-Rays certificate engine
  and immediately installs its ordinary Dickinson interval; KKT downward and full-ceiling intervals are additional proofs.
- Replaced guarded tried-support clauses and separate proof/exploration SAT views with one persistent SAT instance containing only
  mathematical intervals. A KKT path may pass through globally covered supports but rejects path-local repetition.
- Restored explicit cardinality-layer exhaustion in the order $1,n,2,n-1,\ldots$. Cardinality-aware clauses become satisfied outside
  their $[|L|,|U|]$ range, so no explicit certificate deletion or SAT rebuild is needed after a low or high layer finishes.
- Added focused regressions for the full inherited halfspace-rays engine, alternating layer order, the ordinary interval produced by
  a previously stalled six-dimensional support, and complete CP/SCP classification of that matrix.
- The new-binary 46-row Smoke run in combined mode with both preprocessing stages completed all 46 with exact reference matches,
  zero timeout, and zero execution error. The earlier KKT-only binary had completed one row and stalled on 45.

## 2026-08-18 — Exact SAT-guided KKT experiment

- Replaced the copied `xxx` placeholder with exact SAT-selected KKT active-set paths. The model reuses the shared fraction-free
  symmetric LDLT factorization, alternates minimum- and maximum-cardinality uncovered seeds, and adds exact downward convex-face
  blocks and full-ceiling Dickinson coverage clauses.
- Kept heuristic tried-support clauses behind one selector so they cannot prove completion. If those clauses exhaust the bounded
  search while proof SAT still contains a support, the model raises `xxx KKT search stalled`; the existing companion reports
  execution-error status 4 with no Boolean fields. There is no ordinary Dickinson fallback or model-specific public status.
- The first 46-row Smoke run with both predicates and preprocessing completed one matrix and explicitly stalled on 45. It produced
  no conflicting Boolean result and no timeout, showing that the first bounded policy is mathematically guarded but not a generally
  complete practical classifier.

## 2026-08-18 — Additive truth, preprocessing, and timing refresh

- Added 17 previously missing exact truth classifications from unanimous completed combined diagnostic rows: 16 strictly copositive
  matrices and one non-copositive matrix. The corpus now has 105 matrices without a completed CP/SCP classification.
- Added 33 `preprocessing_solved` flags from stored combined diagnostics reporting a resolved preprocessing outcome and zero model
  delegations. No existing flag was cleared, preserving the earlier long-timeout evidence; the total is now 2,765.
- Recomputed and verified the fastest-result cache against every eligible exact combined diagnostic result. All 3,349 existing timing
  entries were already current, including the eight decimal-rounding matrices, so no timing value changed.
- Preserved the stored 49-row Smoke and 512-row Core-and-Stress assignments. Because selectors exclude preprocessing-complete rows,
  their effective sizes are now 46 and 469; comprehensive N ≤ 100 contains 731 matrices.

## 2026-08-18 — Exact negative-part SPN preprocessing certificate

- Added one component-local exact certificate after whole-matrix positive-(semi)definiteness remains inconclusive. It retains the
  diagonal and negative off-diagonal entries, replaces positive off-diagonal entries by zero, and factorizes the resulting matrix
  with the existing fraction-free LDLT implementation.
- Kept exact Motzkin--Straus recognition ahead of both cubic factorizations. For every other connected negative component, the fixed
  order is the original matrix, its negative part, and then maximal principal-Z enumeration only when still useful. A
  positive-semidefinite negative part makes every proper principal Z-block positive definite, so that final enumeration is skipped.
- A positive-semidefinite negative part proves ordinary copositivity; a positive-definite negative part proves strict copositivity.
  Singular positive semidefiniteness deliberately makes no negative strict claim unless the original matrix is already a Z-matrix.
  In that case the first factorization is the complete Z-matrix decision, avoiding both the duplicate negative-part factorization and
  maximal-Z refactorization. The auxiliary negative-part matrix is never passed to a model; unresolved work retains the original
  component matrix.
- Focused checks cover a connected indefinite matrix whose negative part is positive definite and a singular negative part for which
  strict copositivity remains delegated.

## 2026-08-18 — Bounded MILP halfspace model

- Added `sat_halfspace_milp_dickinson`, an isolated SAT Dickinson experiment that replaces ray search with a small bounded
  branch-and-bound MILP maximizing the number of nonnegative full-product entries. Floating candidates are reconstructed and checked
  in exact integer arithmetic; every limit or rejected proposal falls back to the ordinary all-ones Dickinson certificate.
- Kept the solver model-local and dependency-free. It uses dense two-phase simplex relaxations, a 20 ms and 10,000-node per-support
  limit, bounded optional workspace, and focused classification, SAT-interval, singular-orientation, and MILP checks.

## 2026-08-18 — Tiny full-ceiling LP experiment

- Added `sat_halfspace_lp_dickinson`, a copied SAT-Halfspace-Rays experiment that probes full-ceiling feasibility with a tiny dense
  simplex and accepts only exactly reconstructed candidates. Numerical failure falls back to the unchanged exact ray search.

## 2026-08-17 — SAT-Halfspace-Rays one-cardinality look-ahead

- Added `sat_halfspace_rays_lookahead_dickinson` as an isolated SAT-Halfspace-Rays experiment. At a cardinality-$k$ support it
  analyzes every immediate cardinality-$k+1$ child and compares complete Dickinson intervals rather than only $d=|U|-|L|$.
- It immediately inserts every child interval that is not contained in the current interval. It omits the current interval only when
  one inserted child contains it completely; equal intervals retain the current copy.
- A packed-support cache ensures each examined child is factored once even though it has several parents. Every examined child is
  covered immediately, so the cache is discarded with the active parent cardinality instead of being carried into the next layer.

## 2026-08-17 — Corrected Anstreicher G17 max-clique transforms

- Corrected generated corpus rows 13035 and 13036 after the initially imported orientation made both matrices exactly non-copositive:
  the all-ones simplex point was a direct negative witness. The source pattern in matrix 11794 is the adjacency input for the paper's
  complement-graph construction, so the correct transform is \(A_k=k(E-B)-E\), not \(k(I+B)-E\).
- The repaired files have 4,667 lower-triangle \(-1\) entries, one for each raw G17 edge, and 315,733 entries of \(k-1\). Their
  SHA-256 values are `89720ed80566a21a1650fda9b7cb62a491f112d077b42307d4d40352e7a51891` for \(A_5\) and
  `cd01380014bdd7ab13f6644f1b42d32aa079e8a4e95376a124bc29f1db83d771` for \(A_6\).
- The complete preprocessing pipeline classifies both immediately in its exact Motzkin--Straus phase, with no model delegation:
  \(A_5\) is non-copositive and \(A_6\) is copositive but not strictly copositive, agreeing with Anstreicher Section 3.3.
- Marked both rows `preprocessing_solved`; the maintained corpus now has 2,732 such rows and no remaining
  `n_gt_100_solved` member, because every literature-solved higher-order row is resolved before model delegation.

## 2026-08-17 — Ordinary Dickinson singular-support diagnostics

- Added diagnostics-only $(k,q,\mathrm{count})$ histograms to SAT Dickinson, SAT-Halfspace Dickinson, and SAT-Halfspace-Rays
  Dickinson. A count is added only after an exact principal LDLT factorization has found nullity $q>0$; SAT-excluded supports are
  never refactored for measurement.
- A mixed no-preprocessing, 60-second combined-mode sample covered Zischg--Bomze, Motzkin--Straus/Johnson, BPQY, Burer, and
  MANN/Steiner matrices. No run reached nullity two or higher. Only the Motzkin--Straus/Johnson matrix 9628 reached singular
  supports, all at nullity one. The half-space reduction in processed supports therefore came from nonsingular certificates in this
  first sample, not from avoiding observed high-nullity systems.
- Reconfigured the complete Release build conservatively with two compilation jobs and passed all 76 maintained checks.

## 2026-08-17 — Wider singular certificates in every halfspace model

- Changed all four Halfspace Dickinson variants to compare both orientations of their one exact singular nullspace vector and retain
  the orientation with larger Dickinson upper set. A nonnegative orientation remains the copositive-zero case and still decides SCP.
- Reused the single exact full product: positive and negative product counts determine both upper-set sizes, and selecting the negative
  orientation only negates the vector and product. Higher-nullity supports still use one deterministic kernel vector.

## 2026-08-17 — SAT-Halfspace-Rays wide-certificate experiment

- Added `sat_halfspace_rays_wide_dickinson` as an isolated parameterized copy of SAT-Halfspace-Rays Dickinson. It keeps the exact
  U-first coordinate and synthesized-ray search, but retains the resulting full interval only when
  $d>\lfloor p(n-k)/100\rfloor$; a rejected interval blocks only the exact processed support.
- The initial experiment uses $p=50$. The percentage remains a required runtime model parameter so later comparisons do not require
  copied source variants.
- Diagnostics now distinguish every exact interval found from the complete intervals accepted by the width gate. Rejected intervals'
  exact-support fallback clauses are not counted as accepted certificates.
- The complete Release build and all 77 maintained checks pass. On the 490 preprocessing-unsolved Core/Stress matrices in combined
  mode with a five-second cutoff, $p=50$ completed 455 and timed out on 35, with no mismatch or error. Ordinary SAT-Halfspace-Rays
  completed 457 and timed out on 33; the selective rule gained no completion and lost matrices 10285 and 13024. Across the 455
  common completions, its median paired runtime was 0.527% higher.
- Caution: the matching ordinary-model `--rerun` replaced 93 pre-existing longer-cutoff rows for the current binary hash because the
  diagnostics primary key does not include the cutoff. Dedicated long-run files and reports under `experiments/` and `aidocs/`
  remain intact, but those 93 database rows are not recoverable from a current backup or WAL. Do not use destructive reruns to mix
  cutoff campaigns in this table.

## 2026-08-17 — Clingo model naming and halfspace variant

- Renamed `clingo_sat_dickinson` to `clingo_dickinson`; Clingo/clasp still stores Dickinson intervals as clauses, but SAT is an
  implementation mechanism rather than a distinguishing model name.
- Added `clingo_halfspace_dickinson` as an isolated Clingo Dickinson copy using SAT-Halfspace Dickinson's cumulative exact
  coordinate search over strictly positive right-hand sides.

## 2026-08-17 — SAT-Halfspace synthesized-ray experiment

- Added `sat_halfspace_rays_dickinson` as an isolated copy of SAT-Halfspace Dickinson. Its coordinate-ascent path maximizes
  $(|U|,d)$ lexicographically—$|U|$ first and $d=|U|-|L|$ only as the tie-breaker—and, on the final stalled pass, keeps a
  dimension-dependent shortlist of complementary coordinate-ray points.
- It ranks the shortlisted pairs by their gained and lost upper indices, synthesizes the best two exact combined directions, and
  performs at most two extra breakpoint sweeps. The shortlist is bounded by
  $\min\{k,64,\lceil3\sqrt n\rceil\}$, so pair selection cannot become an unbounded quadratic cost on large matrices.
- The complete Release build and all 76 maintained checks pass. A five-second combined-mode Smoke run with complete preprocessing
  classified all 49 matrices, with zero timeouts and zero disagreements with the stored exact expectations.

## 2026-08-17 — CBDD-Halfspace Dickinson experiment

- Added `cbdd_halfspace_dickinson` as an isolated model combining the exact cumulative positive-right-hand-side coordinate search from
  SAT-Halfspace Dickinson with CBDD Dickinson's chain-reduced interval union and exact upper-cardinality expiry.
- Kept the halfspace certificate engine mathematically identical to the SAT variant while removing the SAT dependency. Focused tests
  cover an actual exact halfspace improvement, combined CP/SCP classification, CBDD interval expiry, diagnostics, and timeout handling.
- All 74 maintained C++/Python tests pass. The rebuilt model completed all 49 Smoke matrices in combined mode with diagnostics and
  complete preprocessing, with zero disagreements or unresolved cases.

## 2026-08-17 — Decision-diagram certificates expire below the cardinality frontier

- Added exact upper-cardinality expiry buckets to all eight maintained BDD-, ZDD-, CBDD-, and CZDD-backed Dickinson models. Before
  cardinality $k$, each model removes intervals with $|U|<k$ from its live covered root; such an interval contains no support of
  cardinality $k$ or greater, including when it overlaps a longer-lived interval.
- Kept the change model-local, including per-certificate bucketing inside the multithreaded model's batch union. The decision-diagram
  arenas still retain allocated nodes until the matrix call ends; this is logical root reduction, not garbage collection.
- Added a focused overlapping-live-interval check to every affected model. All 73 C++/Python tests pass, and all eight rebuilt models
  completed the 49-matrix Smoke set in combined CP/SCP mode with zero disagreements or unresolved cases.

## 2026-08-17 — Ten-minute preprocessing results marked

- Marked the 22 Motzkin--Straus matrices completely classified in the focused ten-minute depth-2 continuation as
  `preprocessing_solved`; the 23 timeouts remain unmarked. The corpus now has 2,730 preprocessing-complete matrices.
- Preserved the exact 22 matrix IDs and truth guards in
  `testdata/archive/mark_ten_minute_preprocessing_solved_2026_08_17.sql`. The 22 rows remain stored Core/Stress members, but the
  runner's preprocessing gate excludes them; curated replacement is intentionally deferred rather than silently selecting a new
  benchmark composition.

## 2026-08-16 — Ordinary-copositivity cone comparison completed

- Completed the five-second ordinary-copositivity campaign for 11 literature and experimental cone models on the 49-matrix Smoke
  set and the 512 preprocessing-unresolved Core/Stress set, with complete preprocessing and diagnostics enabled.
- `adaptive_zischg_sponsel_copomatrix` completed 405 Core/Stress cases, followed by `adaptive_sponsel_copomatrix` with 403. The
  strongest literature baselines were `copomatrix_2011` with 349 and `danninger_1990` with 344.
- Verified exactly 512 current-hash rows per model, complete diagnostics, a uniform five-second limit, and no disagreement with
  known corpus truth. Removed 16,589 obsolete-hash rows for the four rebuilt experiments; both SQLite databases passed integrity
  checks.

## 2026-08-16 — Circular FracESSA archived

- Moved `fracessa_circular` to `models/zzz-old-do-not-use/`. Its circulant-only contract makes a mixed-corpus result table
  inapplicable to most matrices, so it is preserved for inspection but excluded from maintained builds, inventories, tests,
  current result summaries, and stored diagnostic benchmark rows.

## 2026-08-16 — Cardinality-local BDD and ZDD models archived

- Moved `cardinality_bdd_dickinson` and `cardinality_zdd_dickinson` to `models/zzz-old-do-not-use/` after their inferior
  Core/Stress outcomes. Their source, focused tests, and algorithm descriptions remain available for historical inspection.
- Removed both identifiers from maintained builds, public analysis inventories, current result summaries, and stored diagnostic
  benchmark rows. They are no longer benchmark candidates.

## 2026-08-16 — Hadeler-based current-preprocessing comparison completed

- Rebuilt and tested every maintained target, then ran all 34 finished Hadeler-, Dickinson-, and FracESSA-derived model identities on
  the 49-matrix Smoke and 512-matrix Core/Stress sets in combined mode with complete preprocessing, diagnostics, and five-second
  per-matrix timeouts. Every model identity has exactly 512 current-hash rows and every completed classification agrees with corpus
  truth.
- The strongest completion group solved 457 of 512 matrices: BDD, CBDD, CBDD Improved 1, ZDD, CZDD, SAT, Clingo-SAT, and
  Multithreaded CBDD. Upper-Endpoint CBDD solved 456. Dense-Bitset reported 71 explicit size-limit errors, while circular FracESSA
  reported 133 out-of-contract non-circulant inputs; neither was counted as a negative classification.
- Removed 80,739 Hadeler-based result rows tied to obsolete companion hashes. The diagnostics database passed its integrity check.

## 2026-08-16 — Upper-endpoint CBDD Dickinson experiment

- Added `upper_endpoint_cbdd_dickinson` as an isolated copy of CBDD Dickinson. Before activating a new interval `[L,U]`, it probes
  each distinct strict upper endpoint `U` once, solves `A_Uv=1` only when `A_U` is nonsingular, and activates the resulting exact
  Dickinson interval before the root interval. The lookahead never recurses; singular endpoints retain the ordinary traversal.
- Kept exact CP/SCP combined classification and added separate probe-certificate diagnostics plus a focused positive-definite
  example where the covered upper endpoint yields new ceiling coverage.

## 2026-08-16 — Benchmark sets refreshed after Motzkin--Straus preprocessing

- Removed all 21 newly preprocessing-complete Smoke members and all 226 newly preprocessing-complete Core members, then restored the
  curated sizes to 49 and 512 using unresolved matrices with established truth.
- Smoke now contains 23 strict and 26 boundary matrices. No known non-copositive matrix of order at most 100 survives preprocessing,
  so a model-reaching Smoke set cannot retain that outcome. Core contains 242 strict, 251 boundary, and all 19 known non-copositive
  matrices that still reach a model; family-diversified selection avoids letting the Hoffman-Pereira catalog dominate the refill.
- Kept N ≤ 100 comprehensive rather than padding it: it now contains all 754 unresolved matrices of orders at most 100. The generated
  higher-order literature-solved set is empty because preprocessing resolves all 75 higher-order rows carrying a completed-solve
  claim. Recorded the guarded exact assignment in
  `testdata/archive/refresh_benchmark_sets_after_motzkin_straus_2026_08_16.sql`.

## 2026-08-16 — Current preprocessing flag refreshed on the complete corpus

- Ran only the current depth-2 combined preprocessing pipeline on all 3,513 retained matrices, using a five-second pass followed by a
  60-second retry of its 61 timeouts. The passes took 62.343 and 480.238 seconds respectively.
- Completely classified 2,708 matrices: 705 strictly copositive, 1,025 copositive-boundary, and 978 non-copositive. Another 758
  returned only partial facts or no decision, and 47 timed out. Every exact fact agreed with available corpus truth.
- Replaced the 2,115-row `preprocessing_solved` assignment by the resulting 2,708-row set, adding 593 matrices and removing none.
  Stored Smoke/Core memberships are unchanged in this step; their effective selections now contain 28 and 286 unresolved matrices
  until the curated sets are refreshed.

## 2026-08-16 — Open MCS replaces the initial Motzkin--Straus clique engine

- Replaced the locally written maximum-clique search in the exact Motzkin--Straus classifier with a focused adaptation of Darren
  Strash's Open MCS commit `735788af066fc8589f577036af521f22f45c2731`. The integration retains MCR initial ordering, static-order
  traversal, greedy coloring bounds, and MCS recoloring while using coposit's packed adjacency, `size_t` indices, cooperative timeout,
  diagnostics, and exact CP/SCP threshold termination.
- Audited the unmodified upstream implementation against brute force on every graph through order six and 6,000 deterministic random
  graphs of orders 7 through 12: all 39,867 maximum-clique sizes and returned clique witnesses matched. The maintained regression
  test independently exhausts all 32,768 graphs of order six and tests incomplete threshold termination separately.
- The exact negative-entry graph of corpus matrix 9651 is byte-for-byte the bundled Open MCS `MANN_a27` graph. Unmodified Open MCS
  found its order-126 maximum clique in 0.204 seconds on one core. The integrated threshold search resolved matrix 9651 after 248
  branch nodes; boundary matrix 9647 and strict matrix 9630 retained their exact classifications after 46 and 126 nodes respectively.

## 2026-08-16 — Exact Motzkin--Straus preprocessing classifier

- Replaced the former Motzkin--Straus bypass in the shared maximal-Z stage with a complete exact classifier for matrices whose
  diagonal and non-edge value is one common $d\geq0$ and whose remaining off-diagonal entries are one common $q<0$.
- The classifier searches the negative-entry graph for its maximum clique using packed multiword supports, greedy-color upper bounds,
  lazily allocated retained per-depth workspaces, exact integer threshold comparisons, and query-specific early termination.
  Recognized matrices skip maximal-Z enumeration; every other matrix follows the unchanged Z-matrix path.
- Exhaustive tests over every nonempty graph on five vertices and every integral threshold from two through five match brute-force
  clique enumeration. Focused cases cover a nonintegral exact threshold, combined equality classification, and order 65 without a
  fixed-width support limit.
- The motivating order-45 MANN/Steiner matrix 9647 now completes in preprocessing as copositive but not strictly copositive after
  2,338 branch nodes and with no model delegation. The order-70 strict Motzkin--Straus matrix 9630 completes after 126 branch nodes.

## 2026-08-16 — Hadeler-based model grouping

- Moved all 33 models inheriting the Hadeler, Dickinson, or FracESSA principal-support approach into the canonical
  `models/hadeler-based/` directory. The move changes only source organization: model identifiers, command-line and Python names,
  mathematical implementations, and stored result identities remain unchanged.
- Added a compact lineage inventory in `models/hadeler-based/README.md` and taught CMake discovery, application targets, and focused
  tests to use the new location. Other literature baselines remain in `models/baselines/`; unrelated coposit-created models remain in
  `models/experiments/`.

## 2026-08-16 — One low-level execution interface

- `coposit --model MODEL --mode strict|non-strict|both` is now the sole low-level execution interface and exposes every literature
  baseline and experiment through an isolated one-model companion.
- Python now invokes the same `coposit` interface instead of importing one pybind11 extension per model. Reference rows hash the exact
  selected companion, retain cooperative timeouts and diagnostics, and do not mix model implementations in one executable.
- Removed the pybind11 build dependency and the duplicate native-extension execution path. There are still no `fast`, `safe`, or
  implicit model-selection aliases during the experimentation phase.

## 2026-08-16 — Affine-companion Dickinson experiment

- Added `affine_companion_dickinson` as an isolated copy of Kernel-Cone Dickinson. At every singular root it decides exact
  consistency of $A_Ix=\mathbf1$ from a complete nullspace basis and reuses the retained singular fraction-free LDLT factorization
  to recover one particular solution without refactoring.
- For nullity one, the model intersects all outside inequalities into one exact rational interval and selects a feasible coordinate
  breakpoint with the most simultaneous zeros, which is a minimum-support member of that affine line. For higher nullity it tests
  only the deterministic particular solution, then retains the ordinary homogeneous and complete Dickinson fallback.
- Kernel-Cone Dickinson now removes zero and positively proportional projected inequalities before active-set enumeration. Its
  nullity-two path uses an exact angular sweep and scans full feasibility only at the true planar cone boundaries while preserving
  every strict-zero test performed by the former generic path.
- For higher nullity, both models retain opposite inequalities for feasibility but enumerate their shared equality hyperplane only
  once. Affine-Companion also defers the outside-by-nullspace product until a consistent affine line or the later homogeneous search
  actually needs it.
- Focused tests cover singular consistent solves, inconsistency, factorization coordinate operations, affine support reduction,
  higher-nullity particular solutions, lazy projected-product construction, antipodal active-hyperplane reduction, planar boundary
  search, and combined CP/SCP outcomes.

## 2026-08-16 — Direct kernel-cone Dickinson experiment

- Added `kernel_cone_dickinson` as an isolated copy of Ceiling-Pruned Dickinson. At a singular root with nullity greater than one it
  constructs an exact basis $Z$ of $\ker A_I$ and the projected outside matrix $G=A_{[n]\setminus I,I}Z$ instead of traversing a graph
  of singular principal supersets.
- If $\ker G\ne\{0\}$, the model extracts a small dependent full-column circuit and emits its exact full-kernel ceiling certificate.
  If $\ker G=\{0\}$, it enumerates independent rank-$(q-1)$ active outside-row sets, normalizes and deduplicates their projective rays,
  and verifies both orientations exactly in $\{y:Gy\geq0\}$.
- A failed cone search falls back to the unchanged Dickinson traversal. New certificates remain pending until the next root
  cardinality, combined CP/SCP classification stays one-pass, and no empirical relation between $d$, $n$, and $k$ is assumed.
- Focused tests cover a persistent kernel of dimension two, pointed-cone ray discovery, an empty nonzero cone, projective ray
  deduplication for $q=2$, and all three possible combined-classification outcomes.

## 2026-08-16 — Breadth-first singular-lift Dickinson experiment

- Added `breadth_first_singular_lift_dickinson` as an isolated traversal variant of Layered Singular-Lift Dickinson. Its FIFO queue
  processes every reachable singular support at one lifted cardinality before the next cardinality, while preserving the outer
  cardinality barrier, exact certificate tests, first-nullity-one stop, and call-wide support deduplication.
- Added current and maximum lift-frontier diagnostics. The separate model and native-module identity ensure stored analyses cannot
  mix breadth-first and depth-first results.
- The focused traversal test proves nondecreasing lifted cardinalities on an all-singular support graph. Exhaustive combined,
  no-preprocessing classification over all 59,808 symmetric matrices of orders one through four with entries in
  $\{-1,0,1\}$ matched the depth-first model exactly.
- On matrix 9647, a 30-second no-preprocessing probe found six ceiling certificates with
  $(\text{root }k,\text{lifted }k,|U|,|L|)=(3,10,45,6)$. It stayed far shallower than depth first, but accumulated about
  15.07 million seen supports and a 10.52-million-support FIFO frontier, confirming the expected memory cost.

## 2026-08-16 — Layered singular-lift Dickinson experiment

- Added `layered_singular_lift_dickinson` as an isolated copy of Ceiling-Pruned Dickinson. A high-nullity singular outer support now
  starts a depth-first search through singular principal supersets; a first-reached nullity-one state tests both signs of its unique
  exact kernel ray and retains only full-upper-endpoint Dickinson certificates.
- The stored forbidden set is the final vector support $L=\operatorname{supp}(u)$, not the root or lifted principal support. Pending
  lowers are minimized as an antichain and become active only after every root of the current outer cardinality has been processed.
- A call-wide packed-support cache expands each lifted state and all of its one-index children exactly once, independent of the route
  used to reach it. An earlier layer's active lower also suppresses covered lifted states; pending lowers remain frozen until their
  layer ends. The focused tests cover the high-nullity-to-nullity-one lift, the final-vector lower cardinality, layer-delayed activation,
  CP/SCP classification, and supports above 64 indices.
- A first 60-second combined, no-preprocessing diagnostic run on order-45 MANN/Steiner matrix 9647 timed out inside the lift started
  from outer cardinality three: 1,255 outer supports had been visited, 859,696 exact outer-or-lifted systems had been processed, and
  no ceiling certificate had yet been retained. This is the intended unbounded experiment, but the initial evidence shows that full
  singular-graph exploration can dominate the outer Dickinson traversal before reaching the motivating order-eight certificates.
- Split the lift telemetry into outer and lifted systems, duplicate and covered lift routes, cache state, and current and maximum
  lifted cardinality and depth. Certificate diagnostics now store
  `(root_k,lifted_k,|U|,|L|,count)` instead of conflating the root cardinality with the support where the vector was obtained.
- A corrected rerun found its first certificate at about 72 seconds with $(\text{root }k,\text{lifted }k,|U|,|L|)=(3,25,45,6)$.
  The search had nevertheless reached lifted $k=45$ in its first second, proving that the previous maximum-lift field described a
  different explored branch rather than the successful certificate.

## 2026-08-16 — Clingo Dickinson certificates persist across cardinality solves

- Fixed `clingo_sat_dickinson`: callback clauses still prune the active clasp backtracking search immediately, while each generated
  Dickinson interval is now also installed through Clingo's backend before the next cardinality solve.
- The previous enumeration-assumption setting did not provide the claimed global interval family. A focused partial-interval test now
  proves that a singleton certificate suppresses its covered pair across the solve boundary without suppressing uncovered pairs.

## 2026-08-16 — Fastest-result cache restricted to the full maintained workflow

- Reset `matrices.fastest_elapsed_ns` and `matrices.fastest_result_ref` from the diagnostics database. Eligible timings now require
  one-pass combined `both` classification, `ok` status, and agreement with every known corpus truth; preprocessing is optional.
- Predicate-only measurements can no longer populate or replace the cache. The serialized result writer enforces the same rule for
  future runs, and the dated SQL migration verifies every reconstructed cache row.

## 2026-08-16 — One diversified low-digit Hildebrand representative per higher order

- Replaced all 17 Hildebrand circulant parameter points at orders 15–25 by one exact representative at each order. The choices stay
  near the low-digit frontier of the searched rational grid but deliberately vary both half-angle parameters; maximum entry length is
  294 digits at order 15 and 2,423 digits at order 25.
- Removed 2,833 obsolete diagnostic results, 68 preprocessing results, and four old Matrix Market payloads. The eleven new matrices
  have new IDs 13024--13034 and no inherited timing; orders 23--25 use three exact symmetric Matrix Market payloads.
- Preserved Core and Stress at 512 rows and its 256/175/81 classification split by adding six existing boundary matrices at the same
  duplicate orders. The corpus now has 3,513 matrices, N ≤ 100 has 1,162, and all 11 orders 15–25 occur exactly once in this family.
- Added the guarded, idempotent migration `testdata/archive/replace_hildebrand_circulants_2026_08_16.py`; its exact generation,
  copied-database rehearsal, payload counts, and both SQLite integrity checks passed.

## 2026-08-16 — Benchmark sets now require model traversal

- Excluded all 2,115 `preprocessing_solved` matrices from every named benchmark selector. Generated N ≤ 100 and higher-order
  literature-solved membership now includes `preprocessing_solved = 0`; the runner applies the same gate to stored and derived sets.
- Kept Smoke at 49 and Core and Stress at 512 by replacing their 29 and 201 preprocessing-complete members with classified matrices
  of the closest available order and similar provenance. Smoke retains its 23/14/12 strict/boundary/non-copositive split. Core and
  Stress is 256/175/81 because only 63 eligible known non-copositive replacements existed for 64 removed slots.
- The generated sets now contain 1,168 N ≤ 100 matrices and 17 higher-order literature-solved matrices. Added the guarded migration
  `testdata/archive/exclude_preprocessing_solved_from_benchmarks_2026_08_16.py` and a runner regression check.

## 2026-08-16 — Current-preprocessing solved flag

- Added `matrices.preprocessing_solved`, default false, and marked all 2,115 retained matrices completely classified by the new
  maintained depth-2 combined preprocessing workflow in the five-second corpus run or its sixty-second timeout continuation.
- The flagged rows comprise 615 strictly copositive, 584 copositive-boundary, and 916 non-copositive classifications. Excluded every
  partial fact, unresolved return, and timeout.

## 2026-08-16 — Persistent preprocessing reduction diagnostics

- Added a persistent diagnostics summary for the original matrix: completion state, connected-component split and sizes, unresolved
  component count and largest size, bounded reduction-child checks and decisions, maximum reduction depth, and actual model delegations.
- The summary survives delegation into model diagnostics. Interrupted preprocessing remains explicitly `running`; only a completed pass
  is reported as `resolved` or `pending`, so partial timeout telemetry cannot be mistaken for a preprocessing decision.

## 2026-08-16 — Depth-2 preprocessing timeout diagnostics

- Reran the 107 still-retained matrices from the original depth-2 five-second timeout cohort for up to 60 seconds each, with
  one-second preprocessing diagnostics preserved per matrix. The other 13 original timeout rows had been removed with the
  precheck-trivial generated stress panel.
- Preprocessing returned on 49 matrices: three complete non-copositivity classifications, 25 exact `not strictly copositive` partial
  facts, and 21 unresolved results. The other 58 timed out again: 41 in exact factorization, 15 in principal-submatrix checks, and
  two in the maximal-Z-matrix scan.
- Retained the raw merged CSV, a compact 107-row diagnostic index, and all individual logs under
  `experiments/preprocessing_depth_2026-08-15/results/`; full interpretation is in the experiment README.

## 2026-08-16 — Retired Scale and Timeout benchmark flags

- Removed `scale_set` and `timeout_5s_strict_set` from the maintained corpus schema and reference runner. Their frozen definitions and
  historical result reports remain under the dated archive and reference documents.
- The maintained matrix selectors are now Smoke, Core and Stress Test, N ≤ 100, and higher-order literature-solved. The first two are
  stored curated flags; the last two are generated from current matrix data.
- Added `testdata/archive/retire_scale_and_timeout_sets_2026_08_16.sql`, guarded against the final 184-row Scale membership, 105-row
  Timeout membership, and 3,519-row corpus before dropping the columns.

## 2026-08-16 — Removed the precheck-trivial generated stress panel

- Removed all 150 deterministic project-generated matrices: 75 sparse and 75 dense, evenly divided among strict, boundary, and
  non-copositive constructions. Exact definiteness or an explicit two-coordinate witness classified every row in preprocessing, so
  normal runs never exercised the selected model.
- A ten-second no-preprocessing `cbdd_dickinson` check solved only four rows and timed out on 146. This confirmed that the panel can
  create artificial raw-model work, but certifying known positive-definite or positive-semidefinite constructions is not a useful
  target for the maintained hard-case corpus.
- Removed 21 external matrix files, 1,456 dependent benchmark rows, 240 preprocessing-result rows, and the two unreferenced local
  generator sources. The corpus now contains 3,519 matrices and 96 sources; Scale has 184 rows and Timeout 5s Strict has 105.
- Preserved the dated generators and migrations as historical material and added the guarded removal migration
  `testdata/archive/remove_generated_stress_2026_08_16.py`.

## 2026-08-16 — Benchmark diagnostics enabled by default

- Made diagnostics capture unconditional in `python/run_results.py` for every benchmark, comparison, corpus, reference, and diagnostic
  campaign. Each active row is snapshotted once per second, and final diagnostics plus any model-supplied certificate distribution are
  persisted for completed and unresolved outcomes.
- Removed the runner's opt-in `--certificate-joint-distribution` flag and its model/preprocessing restrictions. Direct C++ and Python
  calls remain opt-in because they are not necessarily benchmark runs.
- Recorded the temporary project-wide policy in `AGENTS.md` so every coposit session uses the same default until Reinhard explicitly
  changes it.

## 2026-08-16 — Representative Core and Stress fused into one benchmark flag

- Replaced the separate `representative_core` and `stress_test` columns with `core_and_stress_test`. Their post-reshape union had 518
  rows; excluding six precheck-trivial generated rows already in the former Core gives the current 512-row set.
- Kept all 150 controlled sparse/dense matrices from sources 93 and 94 exclusively in Scale. They exercise parsing, matrix scanning,
  exact LDLT, dimension, and density rather than model search: preprocessing proves the strict and boundary constructions by exact
  definiteness and finds every non-copositive construction's two-coordinate negative witness.
- Recorded the guarded schema/data change in `testdata/archive/fuse_core_and_stress_test_2026_08_16.sql`. The former 524-matrix
  reference report remains a historical snapshot and is not presented as a result for the new set.

## 2026-08-16 — Preprocessing depth-3 comparison

- Extended the existing 3,669-matrix preprocessing-only experiment with maximum reduction depth 3, using the same Release binary,
  five-second cutoff, CPU 2 coordinator, and workers on CPUs 3–9 as the depth-1 and depth-2 passes.
- Depth 3 completed 2,263 full CP/SCP classifications in 116.747 seconds wall time: 34 more than depth 1 and 14 more than depth 2.
  Its material additional gain was eleven order-25 BPQY decisions. Three near-cutoff cases at orders 331, 725, and 769 timed out.
- Relative to depth 1, depth 3 used 2.02% more wall time and had a +0.85% paired median time difference over the 3,546 matrices that
  completed at all three depths. No exact results conflict, and all known corpus truth agrees. The maintained depth remains fixed at
  two; depth 3 is experiment-only, with no settings file or supported runtime option.

## 2026-08-16 — Literature solve evidence now requires a complete classification

- Tightened `references_solved`: a paper must establish the matrix's full stored CP/SCP classification. A negative witness remains a
  solve, but a nonnegative heuristic screen, an undecidable stationary point, or merely `not strictly copositive` does not.
- Removed 49 partial or heuristic claims: 32 positive screens from Keys, Ferreira, and Aragón-Artacho, plus 17 Brás claims that did
  not establish ordinary copositivity status. The 30 decisive negative-witness claims from the three one-sided methods remain.
- Moved only the explicitly inconclusive positive runs to `references_unsolved`: five Ferreira rows described as guesses and 17
  Aragón-Artacho boundary rows explicitly declared undecidable. Keys's ten Horn screens and non-failing partial Brás outputs are in
  neither array; explicit Brás failures were already recorded as unsolved.
- The audited corpus now has 629 solved claims on 430 matrices and 256 unsolved claims on 197 matrices.

## 2026-08-16 — Full-corpus CBDD Dickinson classification reference

- Ran `cbdd_dickinson` on all 3,669 matrices in combined classification mode with both preprocessing stages, a five-second cutoff,
  dispatcher and database writer on CPU 2, and seven persistent workers on CPUs 3–9. Native SHA-256 is
  `c08c26b6165356360b51f78876a0125374b8cdf6082b043bc42a4f385be2d729`.
- The run completed 3,195 classifications and timed out on 474 matrices in 393.911 seconds observed wall time. Completed results were
  967 strictly copositive, 1,222 copositive but not strictly copositive, and 1,006 not copositive. All 2,944 completed rows with known
  corpus truth agree; there are no mismatches, errors, or node limits.
- Completion-time distribution was 2,569 at or below 1 ms, 516 above 1 ms through 100 ms, 110 above 100 ms through 5 s, and 474
  unresolved after the cutoff. Five-second-substituted one-core work is 2,489.874230 seconds; completed native-time median is
  0.049166 ms.
- Restricted the matrix timing cache to results that establish the matrix's relevant truth category. A combined result is eligible;
  an SCP-only result must be positive; and a CP-only result must be negative or positively confirm a known copositive-boundary matrix.
  In particular, an SCP-negative result cannot stand in for a non-copositivity classification.
- Rebuilding both cache fields from all stored rows populated 3,271 matrices and left 398 without an eligible classification. The
  selected minima comprise 855 combined results, 894 SCP-positive results, 854 CP-positive boundary results, and 668 CP-negative
  results. Every reference resolves to an `ok` row, agrees with all known truth, and satisfies the eligibility rule.

## 2026-08-16 — Wide-certificate thresholds consolidated into one runtime-parameterized model

- Replaced the copied 75%, 90%, and 95% wide-certificate CBDD Dickinson models with one
  `wide_certificate_cbdd_dickinson` implementation. Its integer percentage parameter applies exactly to the remaining width $n-k$
  and is required.
- Added `--model-parameter VALUE` to `coposit-analyze` and the final `model_parameter` argument to the Python analysis paths. Reference
  rows store the configured run as `wide_certificate_cbdd_dickinson@VALUE`, keeping parameter values distinct without separate model
  sources or binaries.

## 2026-08-16 — Z-matrix checking centralized in preprocessing

- Removed the maximal-Z-matrix scan, its switches, and its diagnostics counters from every model. The one shared preprocessing stage
  is now the sole owner of that check.
- Renamed the remaining affected experiments by removing `zed`: `cbdd_dickinson`, `czdd_dickinson`, `sat_dickinson`,
  `clingo_sat_dickinson`, `multithreaded_cbdd_dickinson`, and the four `wide*_certificate_cbdd_dickinson` variants.
- Deleted three models that became exact duplicates after extraction: `dickinson_zed` duplicates the strict path of
  `dickinson_2019`, `bdd_zed_dickinson` duplicates `bdd_dickinson`, and `zdd_zed_dickinson` duplicates `zdd_dickinson`.
  Historical benchmark rows retain their original model identifiers and binary hashes.

## 2026-08-15 — Returned to explicit experiment interfaces and diagnostics terminology

- Removed the duplicate `dickinson_final` model and the `coposit` `fast`/`safe` launcher, their internal companions, and the
  `coposit::safe` convenience library. The underlying `dickinson_2019` baseline and `adaptive_sponsel_copomatrix` experiment remain.
- Retained `coposit-analyze` as the C++ experiment command. It requires explicit model and mode selection; its Dickinson entry is now
  `dickinson_2019`. Python and reference runs continue to require an explicit model identifier.
- Renamed optional runtime activity reporting from progress to diagnostics throughout current C++, Python, tests, flags, and
  documentation. The only CLI flag is now `--diagnostics`, the Python argument is `diagnostics=True`, and test-only source events use
  source-diagnostics names. No compatibility aliases were retained.

## 2026-08-15 — Adaptive Sponsel–COPOMATRIX moved under experiments

- Moved the unchanged `adaptive_sponsel_copomatrix` directory to `models/experiments/`. Its model identifier, selected `fast`
  integration, Python module, tests, and algorithm remain unchanged.

## 2026-08-15 — Unresolved connected components preserved for model delegation

- Replaced preprocessing's lossy aggregate-only return with component records carrying partial ordinary/strict facts and retaining a
  matrix only while a requested fact remains unresolved. The dispatcher now calls the selected model only for those pending component
  matrices and combines the completed records with logical AND.
- A connected input is borrowed directly. Each proper principal component is materialized once, moved into owned storage, and never
  copied afterward. Resolved components carry no matrix storage.
- Danninger and COPOMATRIX remain bounded certificate-only attempts: inconclusive generated descendants are discarded and their
  unchanged parent component becomes the pending work item. Focused CP, strict, combined, ownership, early-stop, and reduction-fallback
  tests pass; all 85 Release tests pass after relinking every companion.

## 2026-08-15 — Generated stress corpus shifted below order 1,000

- Removed the 120 project-generated sparse/dense stress matrices above order 1,000, their 120 external payloads, and 1,973 dependent
  diagnostic rows. The retained two-family panel now ends at order 952.
- Added 90 matrices at 15 new irregular dimensions from 43 through 199, using the same sparse/dense constructions and the same three
  exact truth classes at every dimension. Each family now contains 75 matrices at 25 shared dimensions.
- The corpus now contains 3,669 matrices. The generated rows retain Scale membership; generated `N <= 100` membership follows their
  dimensions automatically. SQLite integrity checks passed for both corpus and diagnostics databases.

## 2026-08-15 — Preprocessing child-versus-grandchild comparison

- Ran the combined preprocessing-only pipeline on all 3,669 matrices with a five-second cutoff and seven workers on CPUs 3–9.
  Depth 1 took 114.439 seconds wall time and completed 2,229 classifications; depth 2 took 116.397 seconds and completed 2,249.
- Grandchildren added 21 exact facts without a conflict, all at orders 5–25. One order-331 generated strict matrix completed just
  below the cutoff at depth 1 and timed out at depth 2, leaving a net gain of 20 classifications and one extra timeout.
- Full outcomes, timings, method, binary hash, and per-matrix CSVs are retained in
  `experiments/preprocessing_depth_2026-08-15/`.

## 2026-08-15 — Fixed single-switch preprocessing workflow implemented

- Replaced the independently configurable component/pre-check flow with one fixed pipeline and one master on/off switch. The order
  is scan, root checks, negative-entry components, ordinary checks, bounded Danninger, then bounded COPOMATRIX.
- Reduction descendants repeat scan, root, split, and ordinary checks. Danninger and COPOMATRIX may create children while the
  current reduction depth is below one internal maximum, now two; grandchildren stop before further reductions and never call a
  model. Unresolved preprocessing delegates only the unchanged original matrix to the selected model.
- The shared scan now collects both sign graphs, pair facts, row aggregates, reduction pivot counts, and Motzkin–Straus pattern data
  in one pass. Public and analysis companions and every Python native module were relinked against this shared implementation.

## 2026-08-15 — Fastest-result cache restored across the split databases

- Restored `matrices.fastest_elapsed_ns` and `matrices.fastest_result_ref` while keeping the full mutable result rows in the ignored
  diagnostics database. The cache uses every eligible completed diagnostic run and retains the existing exact composite-key JSON.
- The existing serialized batch writer now refreshes only affected matrix rows after result upserts. Eligibility remains `ok` status
  plus agreement with every known corpus truth value; deterministic ties use model, mode, preprocessing, and binary hash.
- The all-history backfill populated 3,463 of 3,699 matrices; 236 have no eligible completed diagnostic row. Both databases passed
  integrity checks, the corpus passed its foreign-key check, and every populated cache row equals the deterministic minimum candidate.

## 2026-08-15 — Dense Boolean-lattice bitmap Dickinson experiment

- Added `dense_bitset_dickinson`, an isolated Dickinson Final copy that represents all $2^n$ supports with one packed bit each,
  scans surviving bits in cardinality order, and explicitly clears every support in each exact Dickinson interval.
- The bitmap has either a user-authorized maximum matrix order or maximum binary-GiB allocation; the default limit is one GiB.
  Python reference runs expose the same mutually exclusive limit choices.
- Added exact interval/layout tests, CP/SCP/combined classification support, cooperative timeouts, and exact support-coverage progress.

## 2026-08-15 — Incremental SAT representation of Dickinson intervals

### Clingo/clasp backtracking comparison

- Added the isolated `clingo_sat_zed_dickinson` experiment. It retains the exact Dickinson and rejection-only maximal-Zed
  mathematics while using clingo 5.8.2 and clasp 3.4.1 for native exact-cardinality answer-set enumeration.
- Completed supports are handled through clingo's model callback, where one exact interval clause is added directly to the running
  clasp enumeration. Disabling the enumeration assumption retains those clauses across later cardinalities without an external
  clause replay list.
- Added CP, SCP, combined classification, cooperative timeout handling, sparse certificate diagnostics, and a focused persistence
  test proving that singleton clauses continue to cover later cardinalities.

- Added the isolated `sat_zed_dickinson` experiment. It keeps the exact Dickinson and rejection-only maximal-Zed mathematics of
  CBDD-Zed Dickinson, but stores each covered Boolean-lattice interval as one incremental SAT blocking clause.
- A single Batcher sorting network supplies exact-cardinality assumptions for every support layer. One persistent CaDiCaL 2.2.1
  instance retains all interval clauses and learned clauses across the complete traversal. CaDiCaL's satisfiable-instance profile
  and incremental lazy backtracking retain the compatible part of the previous trail between supports.
- Added CP, SCP, combined classification, cooperative timeout, sparse $(k,d,|U|)$ diagnostics, exhaustive interval-encoding tests,
  and Python/reference-run integration.
- The 49-matrix smoke set matched all known combined CP/SCP classifications. On the 28 copositive order-25 BPQY matrices with no
  shared preprocessing and a 30-second cutoff, SAT and CBDD each completed 27. In the final run, SAT completed 19 of their 26 common
  finishes faster, with an 8.476-second completed median versus 10.866 seconds for CBDD. Their single timeouts differed: SAT timed
  out on 12625, while CBDD timed out on 12582 and SAT completed that matrix in 23.758 seconds on an isolated repeat.
- On all 75 order-50 BPQY COP materializations, non-strict copositivity without shared preprocessing and a 30-second cutoff, every SAT
  run timed out at cardinality five. The mean processed-support counts were 93,055 for designated support size $\rho_0=12$, 82,614
  for $\rho_0=25$, and 81,511 for $\rho_0=38$; the seven-worker campaign took 330.424 seconds wall time. Complete per-matrix progress
  and $(k,d,|U|)$ certificate distributions remain in `experiments/diagnostics.sqlite3`.
- On the standard 524-matrix representative-core/stress union, strict mode, both preprocessing stages, and a five-second cutoff,
  SAT-Zed completed 488 matrices correctly and timed out on 36. It completed all 379 Interval-Recursive successes plus 109 more;
  cutoff-substituted work fell from 738.007 to 189.238 seconds, and four-worker wall time fell from 187.387 to 48.202 seconds.
- Retrying each model's own five-second timeout cohort for 60 seconds rescued the same five order-16/17 Hildebrand matrices for
  SAT-Zed and CBDD-Zed. The combined two-stage results are 493/31 for SAT and 498/26 for CBDD; CBDD has six unique completions and
  SAT one. Counting both attempts, substituted work is 2,109.312 seconds for SAT and 1,775.753 seconds for CBDD.

## 2026-08-15 — CBDD-Zed and CZDD-Zed diagnostics record upper-endpoint cardinality

- Serial `cbdd_zed_dickinson` and `czdd_zed_dickinson` certificate diagnostics now store sparse
  `(generating cardinality, free indices, upper-set cardinality, count)` quadruples. The added $|U|$ value distinguishes how high an
  interval reaches from its width $d=|U|-|L|$.
- Existing diagnostic JSON and historical triples remain valid; no database migration or result rewrite was performed. The Python
  runner accepts both shapes, while new runs of these two models emit the quadruple form.
- The complete Release build and all 82 CTest checks passed, including direct native/Python checks for both models and a persisted
  timeout-distribution check.

## 2026-08-15 — Ceiling-only Dickinson pruning without a decision diagram

- Added the isolated `ceiling_pruned_dickinson` experiment. It preserves the rejection-only maximal-Zed precheck and exact Dickinson
  principal systems, replaces CBDD storage with FracESSA's cardinality-first forbidden-support generator, and retains a certificate
  only when its upper endpoint is the full index set. The retained lower endpoint then forbids every future support containing it;
  bounded Dickinson intervals are discarded.
- Added exact support progress and standard diagnostics capture. `visited` includes emitted supports and exact binomial counts for
  skipped recursive branches, `covered` is the skipped part, `processed` counts exact principal systems, `certificates` counts only
  retained ceiling certificates, and `certificate_k_d_counts` stores their sparse generating-cardinality/free-index distribution.
  The preceding maximal-Zed scan reports its completed-block count on the same progress line.
- The focused seven-test model suite and shared progress tests pass. A final combined, no-preprocessing smoke campaign completed all
  49 matrices, matched every known CP/SCP result, and stored diagnostics plus a valid certificate distribution for every row. The
  native module SHA-256 was `264826cb35978f3773342e247ea3c88b676ef9f447f8acc9a12b088084ef01c5`.

## 2026-08-15 — Wide-certificate CBDD experiment exposes larger future cuts

- Added the isolated `wide_certificate_cbdd_zed_dickinson` experiment. It keeps a Dickinson interval only when its free-index count
  $d=|U|-|L|$ exceeds half the full matrix order. A narrower certificate removes only the exact support already processed; removing
  nothing would make the read-only CBDD `take_first()` operation return that support forever.
- Ran strict matrix 9630 ($n=70$, Johnson/Motzkin–Straus) for 60 seconds with preprocessing disabled. The model timed out at
  cardinality two after emitting 224 supports and retaining 223 certificates. Their distribution was $(1,16)$: 70, $(2,28)$: 72,
  $(2,32)$: 9, and $(2,59)$: 72. The original traversal had never exposed the $d=59$ family because its narrower certificates had
  already covered those supports, so the experiment confirms that early valid pruning can hide much wider later certificates.
- The wider certificates did not cure the decision-diagram bottleneck. At the cutoff, the model had allocated 55,765,075 CBDD nodes
  and performed 118,288,200 CBDD operations, remained at cardinality two, and did not return during the one-second cooperative grace.
  The threshold therefore trades one poorly shared interval union for another rather than supplying a practical improvement on this
  matrix.
- A parallel 60-second CP comparison on order-50 BPQY COP instance 12649 ($\rho_0=12$, seed 0), again without shared preprocessing,
  also timed out in both models at cardinality three. Normal CBDD emitted 1,020 supports, retained 1,019 certificates, allocated
  49,969,809 nodes, and performed 182,055,800 diagram operations. The half-order experiment emitted 1,185 supports, retained 1,184
  certificates, allocated 49,969,753 nodes, and performed 178,690,400 operations. Singleton and pair counts were identical; the
  experiment exposed 720 rather than 555 cardinality-three certificates, including a net 14 additional certificates with $d>25$,
  but still supplied no classification. Both diagnostic rows and their complete distributions remain in the local results database.
- Three further exact-integer threshold copies compare $d>75\%(n-k)$, $d>90\%(n-k)$, and $d>95\%(n-k)$ on the same matrix and
  60-second CP setup. All timed out. The 75% copy reached cardinality four with 12,584 certificates, of which 327 inserted full
  intervals. The 90% copy reached cardinality five with 370,428 certificates and 1,527 full intervals. The 95% copy reached
  cardinality five with 843,669 certificates but inserted only seven full intervals. Their final allocated-node counts were
  49,969,782, 36,382,795, and 40,086,111 respectively. A failed threshold was logged but inserted only the exact processed support;
  the complete $(k,d,count)$ distributions remain with the three result rows in `experiments/diagnostics.sqlite3` and are preserved
  in `aidocs/WIDE_CERTIFICATE_THRESHOLD_EXPERIMENT.md`.

## 2026-08-15 — Benchmark and diagnostics data separated from the public corpus

- Rebuilt `testdata/copos_testdata.sqlite3` as a corpus-only database containing 98 sources and 3,699 matrices. Mutable benchmark
  rows, preprocessing experiments, timing caches, and their triggers are no longer part of the tracked corpus. The compacted corpus
  changed from 119 MiB to 60 MiB.
- Created the ignored local `experiments/diagnostics.sqlite3` from the tracked `testdata/diagnostics_schema.sql`. Before removing the
  old tables, copied all 187,570 result rows and all 9,761 preprocessing rows; row counts and aggregate elapsed, cutoff, delegate,
  message, and stored-distribution data matched exactly, and both databases passed `PRAGMA integrity_check`.
- The reference runner now reads matrices from the corpus and writes results to the separate diagnostics database. A custom
  `--results-database` remains available; disposable custom corpus databases retain the old single-file convenience by default.
- CBDD diagnostic campaigns store two independent fields: the complete one-second progress history and the sparse
  `(support cardinality, free indices, count)` certificate distribution. Each active row is updated once per second. The first full
  run exposed native operations that could exceed the one-second cooperative grace; the parent now finalizes such hard timeouts with
  the latest stored diagnostics and distribution instead of losing them.
- Completed the full strict, no-preprocessing CBDD-Zed diagnostic campaign with a 60-second cutoff, CPUs 2–9 for eight workers,
  CPU 1 for dispatch, and a 4 GiB virtual-memory limit per worker. Native SHA-256
  `15504711ea82a7e42e5170d0115ae7338f99f02e9a5e076a05856ce92044f11f` produced one final row for every one of the 3,699 matrices:
  3,237 completed, 280 timed out, and 182 ended with `std::bad_alloc`; every row retained diagnostics and its certificate
  distribution. The 2,977 completed rows with known strict truth had zero mismatches. Summed native/resource elapsed time was
  28,226.594 seconds.

## 2026-08-15 — Dickinson progress reports joint certificate distributions

- Serial `cbdd_zed_dickinson` progress now reports sparse `(k,d,count)` entries, where $k$ is the support cardinality that generated
  a certificate and $d=|U|-|L|$ is its number of free indices, including the possible $d=0$ case. Collection is compiled out of the
  ordinary no-progress path. The existing explicit CBDD Zed switch is also available in this serial model so an experiment can
  isolate the complete Dickinson traversal without changing its later tests.
- With Zed and preprocessing disabled, strictly copositive matrix 11672 ($n=50$) completed in 1.159 ms with 50 certificates, all at
  $(k,d)=(1,49)$. Strictly copositive matrix 11694 ($n=100$) completed in 8.672 ms with 100 certificates, all at $(1,99)$.
- Under the same isolation, hard strictly copositive Motzkin–Straus matrix 9630 ($n=70$) timed out after the requested 30-second
  window while still at support cardinality two. Its 750 certificates had the distribution $(k,d)=(1,16)$: 70, $(2,28)$: 492, and
  $(2,32)$: 188. It had emitted 751 supports and allocated 25,064,613 CBDD nodes, showing that many comparatively narrow, poorly
  shared certificate intervals—not exact system solving or the Zed scan—dominated this run.

## 2026-08-15 — CBDD-Zed bypasses Motzkin–Straus maximal-clique scans

- `cbdd_zed_dickinson` now recognizes the exact two-value Motzkin–Straus graph-matrix pattern: one common nonnegative value on
  every diagonal and graph non-edge, and one common negative value on every graph edge. Matching matrices bypass only the optional
  rejection-only maximal-Zed scan; the complete CBDD Dickinson classification remains unchanged.
- Added focused checks that a matching graph matrix reaches no Zed event and that a superficially similar matrix with two different
  negative edge values retains the Zed stage.
- Re-ran all 70 currently unresolved Motzkin–Straus matrices in combined mode with both preprocessing stages, a 60-second cutoff,
  CPU 2 for dispatch, and CPUs 3–9 for seven persistent single-core workers. All 70 matrices, of orders 45 through 3,361, still
  timed out; no error or resource-limit result occurred. Wall time was 608.934 seconds and the native SHA-256 was
  `0c48440858ff5ad977faf21b79daf56fecfb508ddfd76782c91e38adeea73406`.

## 2026-08-15 — Matrix 9651 exposes both Zed-scan and CBDD set explosion

- Matrix 9651 is the order-378 Motzkin–Straus matrix $Q_{125}$ derived from the MANN_a27 Steiner-triple-system graph, whose clique
  number is 126. On a clique support of size $k$, the principal matrix is $125I-J$: size 125 supplies a nonnegative zero that
  disproves SCP, and size 126 supplies a negative vector that disproves CP. The shared whole-matrix pre-checks do not decide this
  connected indefinite input.
- This is the only one of the current 270 matrices without an eligible completed local result for which the literature audit records
  a completed external solve. Brás–Eichfelder–Júdice (source 35) report an LCP-based copositivity-test result, and
  Júdice–Sessa–Fukushima (source 49) report an equivalent completed global standard-quadratic-program solve. Every stored local
  attempt on matrix 9651 has timed out; the literature claims are recorded evidence, not local reproductions.
- The optional rejection-only Zed stage is itself pathological here. Its nonpositive-entry graph is the MANN graph, so its maximal
  Zed blocks are the graph's maximal cliques. An interrupted seven-thread scan had already tested 201,301 maximal Zed blocks after
  15:22 without reaching Dickinson or finding a decisive size-125 singular or size-126 indefinite block. This is why the
  multithreaded experiment now permits `COPOSIT_CBDD_ZED_SCAN=off`; the switch bypasses only this model-local stage, not the shared
  pre-checks.
- Turning the Zed scan off revealed a second, independent explosion in the support-set representation. The strict run was stopped
  manually after 6:49, in the support-solve phase at cardinality 3 of 378; cardinality 3 alone had consumed 6:13. It had emitted and
  certified 484,839 supports, was emitting about 1,889.8 supports per second in the final reporting interval, had allocated
  385,642,726 CBDD nodes, and had performed 645,855,000 CBDD operations. No Zed blocks were tested and no final classification or
  resumable checkpoint was produced.
- One CBDD node contains four 64-bit `size_t` fields, so the node vector alone occupied 12,340,567,232 bytes (about 11.49 GiB).
  Unique-table and operation-cache hash tables, support and certificate storage, exact-arithmetic scratch space, allocator overhead,
  and thread state raised the real memory use substantially above that lower bound. The process exited by interrupt with status 130,
  released its memory, and wrote no result row.
- The cardinality-three exact systems are cheap. Runtime and memory were instead dominated by the coordinator-owned CBDD union,
  difference, canonicalization, and hashing. The 484,839 certificate intervals shared poorly under the fixed variable order and
  expanded to roughly 795 CBDD nodes per emitted support. Consequently the seven exact-solve workers could not remain fully occupied:
  multithreading the factorizations does not address this serial decision-diagram/set explosion. The full mechanism and its
  implications are documented in the model's `ALGORITHM.md`.

## 2026-08-14 — First result for every corpus matrix

- Added the exact `--without-results` runner selector, which requires the complete absence of a `results` row regardless of model or
  status. This avoids a temporary corpus flag or hand-maintained matrix-ID list.
- Ran strict CBDD-Zed Dickinson with both preprocessing stages and a 30-second cutoff on all 212 previously unmeasured matrices,
  using CPU 2 for dispatch and CPUs 3–9 for seven persistent workers. The run completed 198 matrices, timed out on 14, produced no
  known-truth mismatch, and took 70.464 seconds wall time. Native SHA-256 was
  `73a11c9c5a045136a3364541558ec2ee7b65396dc7affadcdb880f3affaf3dd6`.
- Every matrix now has at least one result row. The fastest-result cache is populated on 3,429 matrices; the remaining 270 have only
  unresolved results. SQLite integrity passes.

## 2026-08-14 — Literature permutations retained; bulk FracESSA orderings excluded

- Changed the maintained corpus identity rule to collapse only direct positive whole-matrix scalings in the same coordinate order.
  Nontrivial simultaneous row-and-column permutations remain separate literature inputs because traversal and runtime can depend on
  coordinate order.
- Restored 41 literature matrices previously merged through a permutation: Dickinson-de Zeeuw matrix 10132 and 40 rows from the
  literature-catalog audit. The 293 catalog rows that are only direct positive scalings remain merged.
- Removed the temporary recovery of 1,396 old FracESSA reduced-B coordinate orderings and their 1,320 dependent historical result
  rows. These are bulk extraction occurrences rather than useful literature test inputs. The guarded removal reruns with zero pending
  rows and confirms all 41 literature matrices remain.
- The final corpus has 3,699 matrices, 187,288 result rows, and 9,761 preprocessing rows. Truth constraints, foreign keys, and SQLite
  integrity pass; no curated benchmark membership changed.

## 2026-08-14 — Multithreaded CBDD-Zed support batches

- Added the isolated `multithreaded_cbdd_zed_dickinson` experiment. It enumerates at most the largest complete worker wave below
  `5n` supports from one unchanged CBDD root, evaluates the exact Dickinson systems through persistent dynamically scheduled C++
  workers, unions the valid certificates, and performs one coordinator-owned CBDD subtraction per batch. The serial
  `cbdd_zed_dickinson` source remains unchanged.
- Parallelized the model's formerly serial maximal-Zed stage. The coordinator expands the exact Bron–Kerbosch root until it has at
  least one independent subtree per worker, then the same persistent workers enumerate and exactly check those subtrees. On matrix
  9651, `/proc` showed all seven pinned worker threads at approximately 98–100% CPU during the Zed scan; the live progress counter
  advanced to 6,585 completed maximal blocks after 14 seconds in the model phase.
- Added the model-local `COPOSIT_CBDD_ZED_SCAN=on|off` switch, defaulting to `on`. `off` bypasses only the rejection-only maximal-Zed
  stage and goes directly to the complete parallel Dickinson traversal. This is intended for Motzkin–Straus graph matrices such as
  MANN_a27-derived matrix 9651, whose interrupted scan had tested 201,301 maximal Zed blocks after 15:22 without reaching Dickinson.
- The machine default is seven internal workers pinned to CPUs 3–9. `COPOSIT_CBDD_WORKERS` and `COPOSIT_CBDD_FIRST_CPU` are validated
  count and first-CPU overrides. The focused 17-test model suite passes, including exhaustive four-variable interval checks and
  every five-vertex nonpositive-entry graph under three partition targets, and 960
  deterministic random comparisons against serial CBDD agree in CP, SCP, and combined mode. The 49-matrix smoke set also agrees in
  all three modes with the seven-worker default. `/proc` reported the seven live worker affinity masks as exactly `3`, `4`, …, `9`.
- On one no-precheck CP run of support-heavy order-25 matrix 12580, native times were 10.130 s for serial CBDD, 4.244 s for one
  batched worker, and 2.981 s for the final seven-worker CPU-3–9 default. This single case shows that the larger gain came from batching
  CBDD updates; parallel exact solves supplied a further gain. These exploratory timings were not inserted into the reference tables.

## 2026-08-14 — Fastest completed result cache

- Added nullable `matrices.fastest_elapsed_ns` and `matrices.fastest_result_ref`. The sortable integer stores the shortest eligible
  completed native time; the JSON object stores the exact `model_id`, `mode`, `preprocessing`, and `binary_sha256` that complete the
  owning matrix's composite result key. No unstable SQLite row ID or redundant result ID was introduced.
- Eligibility requires `status='ok'` and agreement with every matrix truth value that is already known. Unknown truth remains
  eligible, while timeout, resource-limit, parse, execution, and known contradictory results do not. A view centralizes this rule;
  four SQLite triggers refresh the cache after result insertion, update, deletion, or truth changes.
- Immediately after the permutation cleanup, the cache was populated on 3,231 of 3,699 matrices from 187,288 result rows; the other
  468 had no eligible completion and retained two
  `NULL` cache fields. There are no known contradictory `ok` rows. Integrity and foreign-key checks passed, every cached reference
  matches the deterministic fastest candidate, and all pre-migration matrix payloads, sources, results, and preprocessing rows are
  unchanged.

## 2026-08-14 — New-import combined CBDD-Zed classification

- Extended `cbdd_zed_dickinson` with Dickinson's one-traversal combined classification: a nonnegative nullspace witness or singular
  positive-semidefinite Z-block clears only strict copositivity, while a negative witness clears both predicates. Focused C++ and
  Python checks cover strict, boundary, and non-copositive outcomes.
- The generated corpus selectors automatically incorporated the imported rows. `n_le_100` now contains 3,125 matrices,
  `n_gt_100_solved` contains 94, and the derived `references_unsolved` selector contains 175.
- Ran combined mode with both preprocessing stages and a 30-second cutoff on the 465 newly imported order-at-most-100 matrices,
  using CPU 2 for dispatch and CPUs 3–9 for seven workers. CBDD-Zed classified 306 matrices (97 strict and 209 non-copositive) and
  timed out on 159; no boundary case completed. All 15 rows with existing truth matched, and the other completions classify 291 of
  the 450 truth-unknown imports. Native SHA-256 is
  `73a11c9c5a045136a3364541558ec2ee7b65396dc7affadcdb880f3affaf3dd6`; wall time was 804.119 seconds.

## 2026-08-14 — Complete Kuzmanović preprocessing-archive screen

- Reconstructed all 100,000 exact order-5–20 matrices from Kuzmanović's unseeded preprocessing-study archive and ran exact ordinary
  CBDD-Zed Dickinson without preprocessing. Four persistent workers classified every matrix in 8.255 seconds wall time with no
  timeout, node limit, parse failure, or execution error.
- The exact model resolved all 2,850 published `no_answer` rows and disagreed with 36 of the remaining 97,150 preprocessing labels.
  Exact Dickinson Final independently reproduced all 36 disagreements, confirming that the published outputs must not be treated as
  maintained truth.
- Removed the separate 100,000-row staging table because the low-order, predominantly cheap-negative archive adds no unresolved case
  or substantially different benchmark structure. The six printed exact examples and source record remain in the main corpus, and
  `aidocs/KUZMANOVIC_100000_MATRIX_SCREEN.md` preserves the complete evidence and timing summary.

## 2026-08-14 — Literature-reported failures and higher-order retry

- Added `matrices.references_unsolved`, a required JSON array of source-linked objects whose comments name an explicit timeout,
  memory exhaustion, numerical failure, inconclusive result, or wrong answer. The conservative audit records 232 claims from 11
  papers on 173 matrices; 149 also have a solved claim because different methods in one paper can have different outcomes.
- No failure was inferred from absence. Unidentified partial-group failures and unseeded random instances remain unassigned, and
  failure claims are not propagated to other retained scalar multiples because numerical behavior can depend on scaling.
- The guarded migration reruns with zero pending rows. Object shape, source existence, duplicate pairs, foreign keys, and SQLite
  integrity pass. `aidocs/LITERATURE_UNSOLVED_REFERENCES.md` records the evidence boundary.
- Retried the two ten-second CBDD-Zed timeouts with a 600-second cutoff. Matrix 9575 completed correctly as not strictly copositive in
  14.207 seconds; matrix 9651 timed out again at 600 seconds. Both results remain keyed by the rebuilt native-module hash.
- Added the derived `--matrix-set references_unsolved` runner selector and ran strict ZDD-Zed, CBDD-Zed, and CZDD-Zed with both
  preprocessing stages and a ten-second cutoff on all 173 claimed failures. Every model solved the same 154 matrices and timed out on
  the same 19 DIMACS clique encodings; all completed classifications match stored truth. The dedicated reference report records
  hashes, timings, and the common timeout families.

## 2026-08-14 — Higher-order literature-solved decision-diagram campaign

- Added generated Boolean membership `n_gt_100_solved = (dimension > 100 AND json_array_length(references_solved) > 0)`. It selects
  all 58 current order-120–1,000 matrices for which at least one literature source reports a completed solve and automatically includes
  future qualifying rows.
- Ran strict ZDD-Zed, CBDD-Zed, and CZDD-Zed Dickinson with both preprocessing stages, a ten-second cutoff, CPU 2 for dispatch and
  database writes, and CPUs 3–9 for seven persistent native workers. Every model solved 56 matrices and timed out on the same two
  known non-strict cases, matrix 9575 (order 200) and matrix 9651 (order 378), with no mismatch, node limit, parse error, or execution
  error.
- ZDD, CBDD, and CZDD used 39.059, 39.048, and 39.586 seconds of cutoff-substituted work. Their coverage is identical at this cutoff;
  `aidocs/REFERENCE_RESULTS_N_GT_100_SOLVED.md` records the hashes, exact settings, medians, and wall times.

## 2026-08-14 — Complete N ≤ 100 decision-diagram campaign

- Added generated Boolean membership `n_le_100 = (dimension <= 100)`, which selects all 2,619 current order-at-most-100 matrices and
  automatically includes future qualifying rows. The standard runner accepts it alongside the five curated matrix flags.
- Ran strict ZDD-Zed, CBDD-Zed, and CZDD-Zed Dickinson with both preprocessing stages, a ten-second cutoff, CPU 2 for dispatch and
  database writes, and CPUs 3–9 for seven persistent native workers. Every model completed all 2,619 calls in 44.0–44.7 seconds wall
  time, solved 2,599, and timed out on the same 20 matrices with no known-truth mismatch, node limit, parse error, or execution error.
- ZDD, CBDD, and CZDD used 245.407, 245.768, and 248.222 seconds of cutoff-substituted work. Chain reduction rescued no matrix at this
  cutoff. All three agreed on nine completed truth-unknown cases and timed out on four; those truth fields remain unchanged because
  three models do not meet the earlier four-model consensus threshold.
- `aidocs/REFERENCE_RESULTS_N_LE_100.md` records the hashes, full aggregate table, timeout composition, and unknown-case outcomes.

## 2026-08-14 — Deduplicated literature matrix occurrences without losing evidence

- Compared every literature-imported matrix against the complete corpus under exact positive whole-matrix scaling and one
  simultaneous row/column permutation. The guarded migration merged 333 redundant rows in 256 equivalence classes, leaving 3,157
  matrices while retaining all 1,048 catalog occurrence identifiers on their survivors.
- Each survivor receives the earliest primary source, the union of secondary sources and literature-solved claim objects, compatible
  truth values, all family labels, and the logical OR of all five benchmark-set memberships. The current corpus has 509 matrices with
  833 secondary links and 407 matrices with 619 unique solved-reference claims from 23 sources.
- Five classes contained one previously benchmarked permutation. Those exact measured representations were selected as survivors, so
  all 178,799 solver results and all 9,761 preprocessing/calibration results remain attached to the exact matrix ordering that was
  run. No timing was transferred or relabeled.
- Exact backup-to-current checks verified all 256 metadata unions and 333 removals. The retained importer and both provenance
  migrations rerun with no pending work; foreign keys and SQLite integrity pass.

## 2026-08-14 — Chain-reduced Dickinson decision diagrams

- Added the isolated strict-only experiments `cbdd_zed_dickinson` and `czdd_zed_dickinson` as copies of BDD Negative-Zed and ZDD
  Negative-Zed Dickinson. The Zed prepass and every Dickinson matrix calculation remain unchanged; only the support-family
  representation and its union/difference traversal change.
- Implemented Bryant's 2017 chain nodes with top and bottom variable levels. CBDD compresses adjacent OR chains, including forced-zero
  runs; CZDD compresses adjacent don't-care chains. Both APPLY implementations split whole chain ranges and preserve exact Boolean or
  set-family semantics without adding a decision-diagram dependency.
- Exhaustive four-variable interval-pair tests match brute-force uncovered-support counts. Both focused model suites and all 49
  truth-labeled smoke matrices passed for both models.
- On the standard 524-matrix strict campaign with both preprocessing stages and a five-second cutoff, CBDD completed 493 matrices
  and CZDD completed 492, with no error or mismatch. Ordinary BDD and ZDD each complete 492. CBDD uniquely completed order-28
  Johnson matrix 9627 and was 9.150% faster at the median over common completions; CZDD had the same completion set as ZDD and was
  3.438% slower at the paired median.

## 2026-08-14 — Literature-reported solution provenance

- Added `matrices.references_solved` as a required JSON array of objects with a required source ID and optional qualification comment.
  It is separate from origin and mention provenance: it records that a paper claims to have completed a copositivity,
  strict-copositivity, or equivalent global-StQP decision for the identified matrix.
- Audited the available papers and result tables and populated 569 matrices with 968 claims from 23 sources. Numerical tolerance,
  heuristic-positive, screening-only, and equivalent-global-optimization claims are retained with explicit comments rather than
  silently promoted to exact certificates.
- Excluded timeouts, question marks, nonexact bounds, unrelated complete-positivity tests, unseeded random campaigns, and ambiguous
  aggregate success counts. Positive scaling inherits claims; mere permutation does not.
- The guarded migration is idempotent and validates object shape and source existence. JSON shape, duplicate pairs, foreign keys, the
  retained importer, and SQLite integrity passed verification. `aidocs/LITERATURE_SOLVED_REFERENCES.md` preserves the audit rules.
- An independent selector-to-table recheck corrected the Hou order-21 matrix selector and added the previously omitted Sponsel
  portfolio rows and Gondzio–Yıldırım DIMACS1 rows; the unresolved DIMACS2 aggregate remains deliberately unassigned.

## 2026-08-14 — Earliest primary and additional matrix sources

- Added `matrices.additional_source_ids` as a required JSON array. `source_id` now holds the earliest located source by publication
  year, with source ID breaking same-year ties; 138 matrices moved from their immediate import paper to an earlier primary source.
  The free-form `source` text remains the provenance of the stored occurrence.
- Populated 641 matrices with 1,076 secondary source links. Evidence comes from explicit catalog projective matches, each imported
  occurrence's own source, exact positive-scale duplicate groups, and audited named or family-level reuse statements already recorded
  in the literature catalog—for example Horn, Hoffman-Pereira, Bomze-de Klerk, Badenbroek-de Klerk, BSU, BLST/ST, and Dobre-Vera.
- Secondary IDs exclude the primary and are ordered by publication year and source ID. Family-level links are deliberately
  best-effort: they record that a paper discusses or solves the identified class without claiming that it prints every member's
  coefficients.
- The guarded migration reran idempotently. All arrays are valid JSON containing existing integer source IDs with no repeated primary;
  chronological ordering, foreign keys, and SQLite integrity passed verification.

## 2026-08-14 — Removed trivial raw QP objectives from the solver corpus

- Removed 790 undeduplicated raw quadratic-program objective matrices whose negative diagonal gives an immediate coordinate-vector
  non-copositivity witness: 180 Bomze-Locatelli-Tardella StQP objectives, 12 Vandenbussche-Nemhauser BoxQP objectives, and 598
  Chen-Burer archive objectives. They had no benchmark flags, external payloads, saved solver results, or preprocessing results.
- Kept their occurrence records and source files in the exhaustive 3,290-record literature catalog, and changed the importer to leave
  their original ID slots empty. The 20 other negative-diagonal literature examples remain in the database as individual examples.
- The maintained corpus now has 3,490 matrices: 1,010 strict, 1,448 boundary, 1,009 non-copositive, and 23 with both truth fields
  unknown. The retained catalog import has 1,048 rows: 336 strict, 269 boundary, 420 non-copositive, and 23 unknown.
- The guarded removal migration, retained import dry-run, source-backed and exact-certificate replays, foreign-key check, and SQLite
  integrity check all passed.

## 2026-08-14 — Exact self-classification of literature-catalog matrices

- Added exact project-side classifications only where the stored matrix or its stated construction supplies a short checkable proof.
  The passes updated 1,552 rows: 1,489 by negative witnesses, entrywise nonnegativity, or negative-part diagonal dominance; 55 by
  Horn/cycle and clique-threshold constructions, positive definiteness, or explicit three-coordinate witnesses; and eight matrices
  of order at most five by exact enumeration of every simplex face.
- Every affected row now carries its own `truth_evidence=...` comment. The migrations reconstruct the witness, leading principal
  minors, graph transform and clique relation, or rational simplex minimum before accepting the stored truth; no floating-point
  guess or project solver result is used.
- The 1,838 imported rows now contain 336 strict, 269 boundary, and 1,210 non-copositive matrices. Both truth fields remain `NULL`
  for only 23 rows: five rounded Nowak instances, two Lund Matrix Market matrices, and sixteen Chen-Burer quadratic-program archive
  occurrences. No one-sided truth value remains.
- Corrected catalog rows 10709, 10772, and 10773 after the exact pass exposed a negative diagonal in the supposedly copositive
  portfolio case. The initial catalog builder had used an unrelated portfolio matrix; it now uses Bomze-de Klerk Example 5.4's
  printed `Q4`, from which the two later shifted test matrices are reconstructed exactly.
- All five truth/correction migrations rerun idempotently. Foreign-key checking returned no rows and SQLite integrity returned `ok`.

## 2026-08-14 — Source-backed truth for literature-catalog matrices

- Populated truth for 318 of the 1,838 newly imported literature-catalog occurrences using only classifications stated by the paper
  or repository and exact consequences of those statements. The batch contains 91 strictly copositive, 89 copositive-boundary, 90
  non-copositive, and 48 copositive rows whose strict status remains `NULL`.
- Evidence includes Ferreira et al.'s repository labels; exact clique- or stability-threshold constructions; stated zero generators,
  strict perturbations, and named copositive/non-copositive test sets; and explicit membership in PSD, DNN, SPN, completely-positive,
  or nonnegative cones. No project solver result or numerical guess was used as truth.
- Left both truth fields `NULL` on the other 1,520 imported rows. The guarded migration is
  `testdata/archive/classify_literature_catalog_truth_2026_08_14.py`; its idempotent rerun, foreign-key check, and SQLite integrity
  check all passed.

## 2026-08-14 — Undeduplicated literature-catalog matrix import

- Imported all 1,838 directly materializable new symmetric matrix occurrences from the 3,290-record literature catalog as IDs
  10685–12522. The corpus now has 4,280 rows of orders 1–4,000. Occurrences remain separate across papers and file formats because
  deduplication is explicitly deferred.
- Converted every exact rational, decimal, or binary floating-point source to a primitive integer representative by clearing positive
  denominators and the common integer factor. For the 12 nonsymmetric BoxQP objective arrays, stored the exact symmetric part
  `(Q+Q^T)/2`, which defines the same quadratic form. Sixty-nine new large matrices use hashed external Matrix Market files.
- Linked every new row to its normalized source and retained its catalog instance ID, paper locator, usage, source path, and notes.
  Both copositivity columns are `NULL` and all benchmark flags are off until later classification and set-selection work.
- The remaining 1,452 catalog records comprise 938 occurrences already matched to existing corpus matrices and 514 records without a
  directly materializable eligible symmetric numeric payload. The latter retain Julia or other generator recipes and source artifacts,
  symbolic irrational or nonsymmetric inputs, and two unrecoverable archive members in the catalog rather than inventing matrices.
- The committed database, all 69 new external hashes, source links, foreign keys, and idempotent rerun passed verification; SQLite
  `PRAGMA integrity_check` returned `ok`.

## 2026-08-14 — Direct Danninger and COPOMATRIX gates on the BDD/ZDD timeout cohort

- Directly tested one minimum-child Danninger reduction and one minimum-child COPOMATRIX reduction on the shared 32 strict
  five-second BDD Negative-Zed and ZDD Negative-Zed timeouts. Root preprocessing was bypassed; generated children would still have
  received the normal earlier prechecks. Each method had a 30-second ceiling per matrix.
- Neither method found a pivot producing at most two children on any matrix. Both returned 32 unresolved results without a timeout
  and generated zero children. Danninger used 77.889 ms of summed stage time and COPOMATRIX used 78.056 ms; median per-matrix costs
  were 1.979 us and 1.938 us respectively.
- This confirms that the two one-step gates are cheap but provide no rescue on this large timeout cohort. The order-3,361 maximum
  required about 22.85 ms for either sign scan.

## 2026-08-14 — Publication years for normalized matrix sources

- Added a required `publication_year` to all 94 source records. It records the earliest documented public appearance, so an explicit
  preprint, public archive, or repository year may precede the final journal year retained in `reference`.
- Rebuilt the small `sources` table atomically to enforce a four-digit year and preserved every source ID and matrix foreign-key link.

## 2026-08-14 — Complete source linkage for the maintained corpus

- Added 16 normalized collection, repository, and reproducible local-generator records after the initial 78-paper source migration.
  They cover SuiteSparse, QAPLIB, House of Graphs, Anymatrix, TypedMatrices.jl, SDPLIB, Network Data Repository, MATLAB's matrix
  gallery, KONECT, the Magma Hadamard database, SciPy, OR-Library, MinCOP_LDLT, and the FracESSA/coposit local generators.
- Resolved all 910 inherited `FracESSA:<id>` pointers against FracESSA's retained provenance metadata and copied the exact origin and
  URL into each maintained matrix's free-form source text. Linked the remaining repository, DIMACS, C5, and high-order generated rows
  to their normalized sources.
- All 2,442 current matrices now have a valid `source_id`; none remains unlinked. The database contains 94 source records and passed
  both `PRAGMA foreign_key_check` and `PRAGMA integrity_check` after the atomic migration.

## 2026-08-14 — Literature matrix catalog and normalized sources

- Completed a paper-by-paper copositivity and copositive-optimization matrix audit without an order or instance-count ceiling. The
  ignored local catalog retains 3,290 exact matrices, archive members, family recipes, or seeded generator invocations from 78 sources
  and separately records 40 reported campaigns whose realized matrices cannot be reconstructed.
- Generic public archives qualify only where an audited paper explicitly selects the instances. All 80 DIMACS graphs qualify because
  Zischg uses all 80; other DIMACS, Matrix Market, and QPLIB entries remain limited to paper-selected files.
- Recovered Deng-Fang-Jin-Xing's former companion ZIP through the Internet Archive. It preserves 19 MATLAB `Q` arrays and the authors'
  code, but not the thousands of other random realizations or RNG states. Six stored `result_3_*` arrays are nonsymmetric and remain
  cataloged verbatim rather than silently symmetrized.
- Added the exact Hoffman-Pereira graph enumeration reused by Peng, ten exact Horn-pattern inputs and seven seeded Julia recipes from
  Keys-Zhou-Lange, five deterministic Bomze-Eichfelder graph cases, and additional finite matrices printed by Burer-Anstreicher-Dür.
- The second audit added the 28 Manainen et al. COP-irreducible graph matrices, the Dobre-Vera `G_k` and new Vargas-Vera-Dickinson
  `L_k` families, Gökmen-Yıldırım's finite examples, four stored order-75 graphs, all 693 Chen-Burer archive files, and the later
  Liuzzi paper's 64 exact archive occurrences. De Zeeuw's unavailable 25,124-graph file and Muramatsu et al.'s 180 unseeded matrices
  remain explicit unrecoverable campaigns.
- Added the minimal `sources` table with authors, title, reference, and comment, plus nullable `matrices.source_id`. It contains all 78
  audited sources and links 901 current matrices to the earliest located source. The existing free-form `source` text remains intact.
  New catalog matrix payloads are staged for a later requested import and were not inserted by this migration.

## 2026-08-14 — One-level Sponsel on the large BDD/ZDD timeout cohort

- Directly tested the shared 32 strict five-second timeouts from BDD Negative-Zed and ZDD Negative-Zed. Root preprocessing was
  bypassed to isolate Sponsel itself; each matrix received at most one split and a generous 30-second ceiling, six times the
  reference cutoff. A single worker avoided simultaneous large exact factorizations.
- Sponsel decided zero matrices. Twenty-two bounded checks completed but remained unresolved, and ten timed out. The stage used
  349.551 CPU-seconds and 349.945 seconds wall time. The largest completion was order-496 Johnson matrix 9637, which used 28.510
  seconds, generated both children, and remained unresolved.
- The second order-496 case timed out after reaching both children. Every matrix of order 776 or greater timed out in the root `H`
  inspection before its single permitted split. The experiment therefore found no large-order rescue and shows that exact root
  factorization, not subdivision depth, makes this fallback impractical.
- Repeating the same cohort at maximum split depth two also decided zero matrices, completed 21 unresolved cases, and timed out on
  11. It used 383.112 CPU-seconds, 9.60% more than depth one; order-496 matrix 9637 changed from an unresolved completion to a
  timeout. On the 21 common completions, depth two was 163.25% slower at the median. More subdivision therefore worsened this cohort
  without adding a decision.

## 2026-08-14 — Corrected no-timeout one-split Sponsel experiment

- Clarified that the requested bound was structural, not temporal: every matrix may use zero or one Sponsel split and must finish
  without a timeout. The corrected run completed all 216 matrices. Five were accepted by the root `H` certificate; the remaining
  211 reached one split and generated 414 of 422 possible children because eight first-child rejections skipped their siblings.
- The bounded stage resolved 25 matrices (17 positive and 8 negative), left 191 unresolved, used 21.378 seconds of summed Sponsel
  work and 25.858 seconds including reconstruction of the current preprocessing state, and took 11.365 seconds wall time on four
  workers. Every decision matched truth.
- Median Sponsel time was 0.831 ms. Hildebrand IDs 10301–10304 nevertheless used 15.544 seconds, 72.7% of the entire stage; order-25
  ID 10304 alone used 7.153 seconds. The expensive total therefore comes from exact child prechecks and `H` factorizations on their
  large integers, not from accidentally traversing beyond one split.
- BDD Negative-Zed Dickinson is the fastest saved complete reference model. It completed 195 of the 216 cohort matrices and timed
  out on 21. Across the common completions, the median per-matrix shallow-Sponsel time difference was -44.46%, meaning the stage
  took 55.54% as long as the complete solve. It took only 16.53% on the 25 matrices it decided, but 91.12% on the 170 it left
  unresolved. This makes it unattractive as an unconditional precheck despite its fast successful cases.
- On the 152 common strictly copositive completions, the median stage cost was 48.01% of the complete BDD-Zed time. It was 12.49%
  on the 17 positive decisions and 82.96% on the 135 strictly copositive unresolved cases. BDD-Zed timed out on the other five
  strictly copositive matrices.
- On the 181 common ordinary-copositive completions, including strict and boundary cases, the median stage cost was 45.10% of the
  complete BDD-Zed time. It was 14.03% on the 21 copositive decisions and 83.31% on the 160 copositive unresolved cases. The complete
  copositive cohort contains 202 matrices; BDD-Zed timed out on 21 of them, which have no completed solve time for a paired ratio.
- Every one of the 25 shallow-Sponsel decisions is also among BDD-Zed's 195 completions. BDD-Zed has 170 additional completions, and
  neither solves the same remaining 21 within the saved BDD-Zed cutoff. The largest shallow-Sponsel decision is order-64 boundary
  Hamming matrix 9613, which BDD-Zed also solves; the largest positive strict acceptance is order-18 `krcgg` matrix 9185. The
  experiment therefore shows no unique large-order benefit and remains too expensive for unconditional integration.

## 2026-08-14 — Shallow Sponsel precheck experiment

- Tested the exact Sponsel root certificate plus maximum split depths one and two on the 216 strict matrices left unresolved by the
  saved ordinary-precheck, Danninger, and COPOMATRIX experiments. Generated Gram children received the ordinary exact prechecks and
  component decomposition, but did not recursively repeat Danninger or COPOMATRIX. No database rows were changed.
- The root `H` certificate alone accepted five matrices. Maximum depth one resolved 25/216 matrices (17 positive and 8 negative),
  including 20 decisions that required a split; it used 17.793 seconds of Sponsel-stage CPU time and 7.756 seconds observed wall
  time, with one timeout. Maximum depth two resolved 37/216 (23 positive and 14 negative), adding 12 decisions and losing none; it
  used 33.179 seconds of stage CPU time and 10.985 seconds wall time, with four timeouts. Every completed decision matched truth.
- The timeouts were large-digit Hildebrand boundary matrices: ID 10304 at depth one and IDs 10301–10304 at depth two. Excluding
  timeouts, median stage cost was 0.784 ms and 2.147 ms per row, but the unresolved arithmetic tail dominated total cost. The
  experiment therefore remains isolated rather than becoming a maintained precheck.

## 2026-08-14 — One-level Danninger and COPOMATRIX final pre-checks

- Added one exact Danninger reduction before one exact COPOMATRIX reduction as the last shared pre-checks. Each selects the first
  pivot attaining the fewest immediate children and runs only when that minimum is at most two. The earlier pre-checks inspect the
  order-reduced children without another dimension reduction; an unresolved Danninger step passes the original component to
  COPOMATRIX, and an unresolved COPOMATRIX step delegates it unchanged to the selected model.
- The isolated 524-matrix strict experiment found 268 matrices unresolved after the former preprocessing. Seventy-four were eligible
  for the narrow reduction, which added three positive decisions (matrix IDs 9454, 9509, and 10306), no negative decisions, and no
  wrong classifications. The other 265 remained unresolved.
- The isolated stage used 14.791 ms across all 268 matrices. On the 265 matrices still delegated, its median saved-runtime overhead
  was 0.0159% relative to Cardinality BDD and 0.0170% relative to Cardinality ZDD.
- A matching non-recursive Danninger experiment gated at two immediate children was eligible on 55 of the same 268 matrices and
  accepted 51, all positive and all through its one-child case. It overlapped COPOMATRIX on IDs 9454 and 10306, added 49 unique
  decisions, missed COPOMATRIX-only ID 9509, and used 3.732 ms of isolated reduction time. Together the two reductions decide 52
  former-precheck leftovers and leave 216 unresolved. As an independent check, all 51 matrices were permuted to put the selected
  pivot first and were accepted by the complete exact Danninger baseline with preprocessing disabled.

## 2026-08-14 — Rejection-only Zed decision-diagram revision

- Changed `bdd_zed_dickinson` and `zdd_zed_dickinson` in place. Their maximal-Zed stage still rejects immediately when an exact
  component LDLT proves that a maximal Zed block is not PD, but a PD block is now discarded instead of inserting
  $[\varnothing,J]$ into the decision diagram. The flat `dickinson_zed` model deliberately retains its positive downsets: a packed
  subset lookup does not deform its flat certificate representation and can avoid an exact principal solve.
- All three focused model suites pass. The positive-identity tests prove that BDD/ZDD Negative-Zed continue into ordinary
  cardinality-one Dickinson processing after the Zed check; the singular-PSD tests still prove immediate negative rejection.
- The replacement BDD hash `5fca72a61045c628b2db6c14e79392e0cfeef2676d2bd263d95895301069f3d1` and ZDD hash
  `ab7350362cff899f8ee5b5d311f94b6f4bcd4640f3bde462b5d34d4a796abe89` each completed 492/524 matrices in the standard strict,
  both-preprocessing, five-second campaign. Every completed result matches corpus truth; all 32 unresolved results in each model
  are timeouts. Observed four-worker wall times were 43.219 and 43.390 seconds.
- A second isolated campaign disabled all preprocessing and compared identical matrix IDs. BDD Negative-Zed changed median time by
  -1.129% over 466 common completions, -90.150% on non-SCP matrices, and +1.035% on SCP matrices; its median SCP absolute overhead
  was 0.002646 ms. ZDD Negative-Zed changed it by -0.955% overall, -89.081% on non-SCP matrices, and +1.952% on SCP matrices; its
  median SCP absolute overhead was 0.004605 ms. The positive SCP medians confirm that the rejection-only scan is extra work when it
  cannot reject; the overall medians become negative only because negative Zed rejection avoids most traversal on non-SCP inputs.
  BDD gained 22 negative completions and lost strict matrix 9627, for net +21; ZDD gained the same 22 negative completions and lost
  none.
- Relative to their superseded positive-downset binaries, the rejection-only BDD and ZDD retained the same 492 completion sets while
  reducing median time by 18.30% and 18.00% overall and by 73.76% and 69.30% on SCP matrices.
- In the isolated no-preprocessing campaign, plain `dickinson_final` completed 376/524 and `dickinson_zed` completed 380/524. Across
  373 common completions, retained Zed downsets changed median time by +22.167% overall, -46.975% on non-SCP matrices, and +85.763%
  on SCP matrices; the SCP median absolute increase was 0.021104 ms. These runs predate and disable the shared COPOMATRIX precheck.
  Restricting the SCP comparison to the 33 common completions above order 15 raises the median slowdown to 288.360%, with a
  23.241 ms median paired absolute increase.
- **Important cause identified:** positive Zed downsets can make flat cardinality-ordered Dickinson slower by suppressing the small
  supports that would have generated stronger upward certificates. On order-20 matrix 10322, 19 maximal size-two Zed blocks cover
  every singleton before it can be solved. Plain Dickinson processes 20 singletons and 19 pairs, then covers the remainder; the Zed
  variant instead processes 171 pairs and 18 triples, increasing ordinary exact solves from 39 to 189. Its valid downward cuts have
  therefore removed the opportunity to construct much wider upward Dickinson intervals. This is a certificate-generation effect,
  not primarily Zed lookup overhead and not a correctness failure.
- This suppression is frequent and not limited to Stress Test matrices or the star structure of matrix 10322. Among 33 common SCP
  completions above order 15, Zed is slower on 31 and at least twice as slow on 30; the median change is +288.360%. Fourteen are
  Representative-Core-only matrices, of which 13 are at least twice as slow. Across the 29 high-order SCP matrices whose nontrivial
  Zed blocks suppress every singleton, all 29 are at least twice as slow and the median change is +304.786%. Core-only matrices
  10427 and 10330 complete in plain Dickinson but time out with Zed, so the common-completion medians omit these additional losses.
- The same certificate-suppression mechanism applies to the existing Zischg–Dickinson Level 2 experiment. Its disconnected-support
  skip is exact for classification, but a skipped support could have generated an upward Dickinson interval covering later connected
  supersets. The current order-one-to-100 comparison is consistent with this mechanism: Zischg–Dickinson gains no completion, loses
  strict matrices 10330, 10427, and 10428, and has +77.16% median time on matrices completed by both models. Unlike positive Zed
  downsets, this implementation retains all supports through order three, so it cannot suppress singleton, pair, or triple
  generators; the analogous performance loss begins at order four.

## 2026-08-14 — Cardinality-local BDD and ZDD Dickinson experiments

- Added strict-only BDD and ZDD experiments whose active decision diagram contains only one support cardinality. Certificate
  endpoints remain packed sets, and the complete manager is discarded between cardinalities; no global covered-union diagram is
  retained.
- Replaced the original per-certificate $O(nk)$ exact-cardinality slice builder with an $O(n)$ compact unrestricted interval. This is
  exact because the active remainder $R_k$ is already a subset of $K_k$, so
  $R_k\setminus[L,U]=R_k\setminus([L,U]\cap K_k)$. Exhaustive four-dimensional interval tests and all 74 Release tests pass.
- The standard 524-matrix strict run used both preprocessing stages, a five-second cutoff, CPU 3 for the parent, and workers on CPUs
  4–7. Cardinality-BDD hash `fd1b398793d2fcfe8d55f4a72a3a2fe67efd1c55c3821cafcee4d4fcdc1726eb` completed 475 matrices,
  timed out on 49, used 359.111 seconds of cutoff-substituted work, and took 92.578 seconds wall time. Cardinality-ZDD hash
  `f7ed518402403e11d62894578c7d828df7742baac6f900bd0753fcc4da0e7665` completed 474, timed out on 50, used 364.684 seconds of
  substituted work, and took 94.629 seconds wall time. All completed classifications match corpus truth; there were no node limits,
  parse errors, or execution errors.
- Without preprocessing, both patched models completed hard order-100 matrix 10500 inside ten seconds: BDD in 6.237 seconds with
  70,976 KiB peak RSS and ZDD in 6.932 seconds with 66,880 KiB. On dense order-331 matrix 10605, both reached the ten-second cutoff;
  peak RSS was 315,248 KiB for BDD and 366,080 KiB for ZDD.

## 2026-08-14 — Dickinson Zed certificate experiments

- Added the isolated strict-only `dickinson_zed`, `bdd_zed_dickinson`, and `zdd_zed_dickinson` experiments. They realize Dickinson's
  Section 6 maximal principal $Z$-matrix construction before the unchanged flat, BDD, or ZDD Dickinson traversal.
- The models enumerate maximal nonpositive-entry cliques with Bron–Kerbosch. For each maximal block they apply Dickinson Algorithm
  3's strictly negative connected-component split, factor every component with exact fraction-free LDLT, reject any non-PD block,
  and otherwise add the complete downset $[\varnothing,J]$ to their respective coverage representation.
- Focused tests exhaustively matched maximal-clique enumeration against brute force for all 1,024 sign graphs on five vertices,
  verified zero-linked component splitting, conflicting maximal blocks, singular PSD rejection, arbitrary-precision scaling, and
  packed supports beyond 64 indices. All three models also matched stored strict truth on all 49 smoke matrices without preprocessing.
- The standard strict Representative Core/Stress campaign used both preprocessing stages, a five-second cutoff, CPU 3 for the parent,
  and workers on CPUs 4–7. Flat Dickinson Zed completed 384/524 matrices; BDD-Zed and ZDD-Zed each completed 492/524 with identical
  completion sets. Their substituted work was 718.592, 180.254, and 192.463 seconds, and observed wall time was 181.675, 47.866, and
  50.718 seconds. Every completed result matched truth; all unresolved results were timeouts.
- A targeted 30-second retry of the 32 common BDD-Zed/ZDD-Zed five-second timeouts rescued the same seven matrices in both models:
  five Hildebrand boundary matrices of orders 16–17, strict Johnson matrix 9627, and boundary Johnson matrix 9637 of order 496.
  Each staged campaign therefore completes 499/524 matrices. The 64 longer-cutoff rows and exact selection are preserved under
  `experiments/zed_decision_diagrams_30s_2026-08-14/`; the maintained five-second rows were not replaced.
- The performance interpretation is representation-dependent. Flat Dickinson Zed still enumerates every support and merely avoids
  the principal solve after recognizing a certified Zed-block subset. Small principal solves are cheap relative to the block prepass;
  at large orders, support enumeration rather than solving is the bottleneck. BDD-Zed and ZDD-Zed insert the complete Zed downset as
  one symbolic family and never generate its covered supports, turning the same theorem into genuine search-space pruning. The full
  five-second median relative time change `(model - Dickinson) / Dickinson`, grouped by all 111 represented orders, is preserved with
  the experiment results.

## 2026-08-14 — Ordinary-BDD Dickinson experiment

- Added the independent strict-only `interval_bdd_dickinson` experiment. It retains Dickinson Final's exact principal calculation
  and replaces only support enumeration: the accumulated certificate union and each exact-cardinality remainder are reduced ordered
  binary decision diagrams with ordinary `low == high` reduction.
- Exhaustive focused tests matched brute force for every pair of valid four-dimensional certificate intervals. Strict decisions,
  non-strict rejection, cooperative timeout escape, identity-certificate pruning, and a 65-dimensional packed-support case also
  passed; all 69 maintained Release tests passed.
- The standard 524-matrix strict run used both preprocessing stages, a five-second cutoff, CPU 3 for the parent, and workers on CPUs
  4–7. Hash `e69cde8969e1337e7e7e430b882f230b881dcf818726f13973b38373bfaa9fbd` completed 486 matrices and timed out on 38, with zero
  mismatch, node limit, parse error, or execution error. It completed the 484 ZDD completions plus Johnson matrices 9627 and 9628;
  its cutoff-substituted work was 205.396942 seconds and observed wall time was 55.071 seconds.
- A user-stopped 30-second follow-up retained the first 87 committed matrices from the frozen 129-matrix timeout set, then ran ZDD
  on exactly the same IDs. BDD and ZDD each completed the same 23 matrices: all eight Dickinson Final completions plus the fifteen
  order-90–104 Dannenberg–Schürmann lifts. Adaptive completed 18: the same fifteen lifts, the two shared high-order positive-definite
  precheck cases, and one unique order-256 Hamming case. All four timed out on 63 common matrices. The exact partial selection and
  decision-diagram rows are preserved under `experiments/interval_decision_diagrams_30s_2026-08-14/`.

## 2026-08-14 — Dickinson interval-aware support enumeration experiments

- Added two independent strict-only copies of Dickinson Final. `interval_recursive_dickinson` prunes recursive fixed-cardinality
  branches when one retained certificate interval contains the complete branch. `interval_zdd_dickinson` represents the union of
  retained intervals with a private zero-suppressed decision diagram and enumerates the exact set difference from each cardinality
  family. Both retain Dickinson Final's exact principal solve, nullspace choice, sign decisions, and certificate construction.
- Both generators matched brute-force enumeration for every pair of valid four-dimensional intervals. Their classifications matched
  Dickinson Final on all 49 smoke matrices without preprocessing and on 600 deterministic symmetric integer matrices of orders one
  through seven. All focused C++, Python wrapper, and dimension-above-64 checks passed.
- On strict Hildebrand matrix 10293 without preprocessing, Dickinson Final took 14.036 seconds, interval-recursive took 14.617
  seconds, and interval-ZDD took 14.150 seconds. The exact principal work dominates this input, so neither alternative improved it.
  On a 22-by-22 identity matrix, where singleton certificates cover almost the entire power set, the respective times were 39.294
  milliseconds, 0.020 milliseconds, and 0.297 milliseconds. This confirms that both representations can skip covered supports,
  while also showing that the benefit depends on certificate shape and exact-arithmetic cost.
- The strict five-second Representative Core/Stress campaign with both preprocessing stages completed 379/524 matrices with the
  recursive generator and 484/524 with the ZDD; substituted one-core work was 738.007 and 206.816 seconds. The ZDD uniquely completed
  108 matrices versus 3 unique recursive completions. All completed results matched truth and all unresolved results were timeouts.
- The first ZDD campaign exposed that its internal cooperative-timeout checkpoint was incorrectly `noexcept`; a timeout thrown from
  within diagram set algebra therefore terminated the worker. Removed `noexcept`, added a focused regression test, deleted only the
  481 partial rows under the faulty hash, and reran all 524 matrices cleanly under hash
  `94804e744aa2807ca331801fc74250bd661b5d1bca3e35a1a95d881984ae3693`.

## 2026-08-13 — Dickinson progress distinguishes retained certificates

- Added `certificates` to Dickinson support progress. It increments only after a signature is stored, while `processed` increments
  when an uncovered support enters exact solve or nullspace work. The reporter now prints a final snapshot on completion.
- A fresh strict run of Hildebrand matrix 10293 (order 17) terminated after visiting 130,918 of 131,071 supports. Existing
  certificates covered 124,457 visited supports; 6,461 supports were processed exactly; 6,460 certificates were retained; and the
  remaining processed support was the terminating nonnegative zero. Processed principal problems had mean order 6.903885 and median
  order 7.

## 2026-08-13 — FracESSA circular global-minimum experiment

- Added the independent `fracessa_circular` model. It assumes a symmetric circulant input, permits an arbitrary common diagonal,
  and classifies the global simplex minimum as negative, zero, or positive through combined CP/SCP output.
- Adapted current FracESSA revision `3dafe81335b816feffdca47ef24b693054f40af5` without adding a circulant scan: direct
  fixed-density bracelet generation, complete rotation/reflection orbit installation for superset pruning, and exact
  matrix-preserving affine-multiplier filtering. Packed support rotation and reflection retain unrestricted dimensions.
- The focused generator tests reproduced all 29 nonempty bracelets at order eight, the affine test retained the expected 23 exact
  representatives, and combined classification matched Hadeler on 400 deterministic circular matrices of orders four through eight.
- With the normal combined preprocessing profile and a 30-second cutoff, module hash
  `c6eee0a6bca47848c31466905850895d8ca2566a3fbae876a32c1c9e9ff016c1` classified 37 of the 47 Hildebrand circulants at orders
  7–25, timed out on 10, and produced no mismatch or error. It completed every instance through order 17 and one of three at order
  18; the four-worker wall time was 95.305 seconds and the cutoff-substituted one-core total was 345.297 seconds.
- Added truthful bracelet progress for the circular model and captured 30-second traces for the ten timeouts. Their observed prefixes
  reached 93.3% and 87.6% of the order-18 bracelet targets but only 1.0% at order 25. No prefix used an extra affine skip or accepted
  a KKT candidate. Constant observed-rate projections range from 32 seconds to 49 minutes; an explicit $k^3$ exact-solve-work
  scenario ranges from 37 seconds to about eight hours. These are documented scenarios rather than claimed bounds.

## 2026-08-13 — Project name is always lowercase

- Standardized the project and program name as `coposit` everywhere, including human documentation, CMake project metadata, public
  messages, model descriptions, historical notes, generator provenance, and maintained corpus source labels. Conventional uppercase
  C/C++ identifiers such as `COPOSIT_*` remain unchanged.

## 2026-08-12 — Maintained database reclaims replaced result pages

- Enabled SQLite `auto_vacuum=FULL` in the maintained corpus and canonical schema, then ran one full `VACUUM`. Repeated benchmark
  replacement had left 12,171 reusable pages (49,852,416 bytes) inside the 132,186,112-byte file even though live data occupied only
  about 82 MB. Automatic vacuuming now truncates freed tail pages after commits instead of allowing that space to accumulate.

## 2026-08-12 — Thirty-second strict timeout progress experiment

- Ran all 129 `timeout_5s_strict_set` matrices in strict mode through the public prechecked Dickinson Final and Adaptive
  Sponsel–COPOMATRIX paths, with 30 seconds per call and workers pinned to CPUs 3–9. All 258 calls completed or timed out in 17
  minutes 22 seconds; 30 completed Booleans matched truth, 228 timed out, and no call produced an execution or runner error.
- Preserved 6,966 one-second snapshots, exact matrix/input and binary identities, per-call outcomes, flattened snapshot data, and
  final-state data under `experiments/timeout_progress_2026-08-12/`. Dickinson completed 10 calls and Adaptive completed 20; both
  timed out on 103 matrices.
- Made progress lines explicitly identify `stage=preprocessing` or `stage=model` after an initial 90-call draft proved easy to
  misread. Preprocessing lines name the exact phase and work counter; model lines name the algorithm-specific metric. The first
  model node is published immediately, while later hot-loop publication remains batched every 4,096 visits.

## 2026-08-12 — Adaptive progress separates Sponsel from COPOMATRIX

- Replaced Adaptive Sponsel–COPOMATRIX's ambiguous generic proof line with separate routing, engine, internal-phase, Sponsel-node,
  actual Sponsel-split, COPOMATRIX-node, projection-child, staircase-state, forced-switch, streak, pivot, and immediate-child counters.
  Exact Sponsel `H` factorization reports once per pivot; no matrix-entry loop publishes progress, and all additions immediately
  return when `--progress` is disabled.
- Short diagnostics corrected the earlier interpretation of stale 4,096-node batching. Hildebrand 10289 reached 30 Adaptive nodes
  and 29 Sponsel splits in its first second, entered COPOMATRIX by five seconds, and reduced from order 16 to 15. Hildebrand 10294
  reached 33 Sponsel nodes and 32 splits in five seconds but had not yet used COPOMATRIX. Order-25 Hildebrand 10304 reached three
  Sponsel nodes and two splits by five seconds. The dominant sampled phase was exact `H` factorization, not an idle first node.

## 2026-08-12 — One detailed C++ analysis command replaces model-named commands

- Added `coposit-analyze --model MODEL --mode strict|non-strict|both` as the only user-facing C++ model-selection interface. It covers
  every literature baseline except superseded Dickinson 2019, plus Dickinson Final and Adaptive Sponsel–COPOMATRIX. It controls
  connected components, the pre-check stage, every individual pre-check, the principal-submatrix cutoff, and progress reporting;
  all preprocessing defaults to on.
- Replaced the model-named CMake executables with internal `coposit-analyze-MODEL` companions. Each companion still links exactly one
  solver, so runtime model selection does not introduce a multi-model binary, registry, factory, or solver-symbol collision.
- Kept unsupported predicates explicit: all nine selectable models support strict and non-strict mode; Danninger, Hadeler, and
  Dickinson Final support one-pass combined classification. Experimental variants remain available through Python/reference tooling.

## 2026-08-12 — Common strict-timeout benchmark set

- Added the `timeout_5s_strict_set` matrix flag and runner selection. Its 129 rows are the intersection of the two pre-checked strict
  five-second timeout sets for Dickinson and Adaptive Sponsel–COPOMATRIX; a completion by either algorithm excludes the matrix.
- Reused the 524-matrix research-report campaign and ran only its 1,918-row complement with the current final models. Froze all four
  exact result hashes and the guarded assignment in `testdata/archive/assign_timeout_5s_strict_set_2026_08_12.sql`.

## 2026-08-12 — Public safe mode can classify both predicates

- Added `coposit safe both MATRIX`, which runs Dickinson Final's existing combined classifier once and prints named copositive and
  strictly-copositive Boolean results. `fast both` is rejected because Adaptive Sponsel–COPOMATRIX has no one-pass combined classifier.

## 2026-08-12 — Public progress now covers preprocessing

- Extended the existing `--progress` snapshot with the active preprocessing phase: matrix scan, cheap certificates, principal
  submatrices, connected components, component scan, Frank–Wolfe, exact factorization, or model delegation.
- Published truthful phase-local work units where available: scanned rows, graph vertices, principal-face centers, Frank–Wolfe
  iterations, and exact factorization pivots. These counters are activity indicators, not percentages or ETAs.
- Kept telemetry out of mathematical decisions. With progress disabled, phase and work updates return immediately; model progress
  replaces preprocessing progress as soon as the selected solver begins.

## 2026-08-12 — Opt-in progress reporting uses algorithm-specific honest metrics

- Added `--progress` to the public and model-specific command-line interfaces. It leaves the Boolean on standard output and emits one
  status line to standard error every second.
- Kept timekeeping out of solver loops: one sleeping reporter reads relaxed-atomic snapshots that active models publish every 4,096
  visited units. Disabled progress starts no thread and performs no telemetry arithmetic.
- Defined exact support-enumeration coverage for Hadeler and Dickinson, certified simplex-volume coverage for Bundfuss and Sponsel,
  and monotone proof-obligation coverage for Du Tour, Danninger, COPOMATRIX, and Adaptive Sponsel–COPOMATRIX. Safi reports traversal
  counters because exact sliced-volume accounting would add material determinant work. None of the measures is presented as an ETA.

## 2026-08-12 — Adaptive Sponsel memory risk acknowledged

- A direct no-preprocessing strict run on scale matrix 10593 ($n=2,997$) was killed by the kernel at 39.38 GiB resident memory. The
  same direct model on matrix 10515 ($n=331$) used only 274.7 MiB through 20 minutes before manual interruption.
- Documented the structural cause: every live Sponsel level retains full dense exact children, giving base live storage
  $O(dn^2)$ at same-order depth $d$. Order 1,000 and above is a practical warning range rather than a hard threshold; an unusually
  deep branch can become memory-bound earlier.
- No comparable memory-bound Dickinson run has been observed. This closes the review action as an acknowledged operating constraint;
  it does not claim that the Adaptive traversal now has a memory limit.

## 2026-08-11 — File parsing uses one result contract

- Verified that the shared native parser already returns `PARSE_ERROR` for malformed compact text, malformed Matrix Market content,
  missing files, and unreadable files without raising through the Python API.
- Added the distinct `parse_error` reference-result status instead of collapsing native status 1 into generic `error`. Parse failures
  retain no Boolean or elapsed solver time and do not replace the persistent worker.
- Migrated all 164,192 result rows without changing their contents. SQLite integrity and foreign-key checks passed, and all 26 Python
  tests passed, including sequential, multiprocessing, and reference-run file-error coverage.

## 2026-08-11 — Result preprocessing identity made explicit

- Replaced the unused free-text `results.parameters` field and `--parameters` runner option with the constrained
  `results.preprocessing` column: `none`, `connected_components`, `pre_checks`, or `both`.
- Migrated 164,681 stored rows into 164,192 distinct structured identities. The 489 merged pairs had no conflicting completed
  classifications; a completed result was retained over a timeout, otherwise the longer-timeout and then latest row was retained.
- Kept model implementation changes distinct through the native-module SHA-256. SQLite integrity and foreign-key checks passed, and
  the focused reference-runner tests passed.

## 2026-08-11 — Low-order Dickinson shortcut consolidated in the shared precheck

- Removed coposit's direct order-at-most-three test from both `dickinson_2019` and `dickinson_final`, including the complete-input
  shortcut through order three. The baseline now follows Dickinson's coverage and certificate path on every support; the final model
  remains algorithmically identical.
- Kept the exact low-order criterion solely in the shared precheck. Public `safe` therefore applies it once before `dickinson_final`,
  while analysis calls with preprocessing `none` now run Dickinson without hidden prechecks.

## 2026-08-11 — Dickinson Final selected independently from the baseline

- Copied `models/baselines/dickinson_2019/` into the independent selected model `models/dickinson_final/`. Solver logic, tests, and
  the copied trace helper are unchanged; only the helper include path is local to the new self-contained directory.
- Kept `dickinson_2019` as the untouched literature baseline. Rewired the public `safe` companion to `dickinson_final` and registered
  `dickinson_final` as a standalone executable, Python model, CP/SCP model, and one-pass combined classifier.
- Updated the model-structure instructions and current user, Python, project, research, and algorithm documentation. The complete
  Release build and all 50 CTest checks passed.

## 2026-08-11 — Python Matrix metadata removed

- Reduced the constructor to `Matrix(matrix, matrix_id=None)`. Matrix input is compact or inline Matrix Market text, or a direct
  relative/absolute path; relative paths use the process working directory. Results no longer contain metadata.
- Removed base-directory resolution, values-only dimension metadata, `file:` handling, and expected-dimension checking from the public
  Python/native boundary. Corpus runners now convert inline database values to `dimension#values` and database-relative `file:` rows
  to direct paths before constructing `Matrix`.

## 2026-08-11 — Unselected models consolidated under experiments

- Kept only `adaptive_sponsel_copomatrix`, `baselines/`, and `experiments/` directly under `models/`. Moved every other
  coposit-created model and the former Zischg comparison into `models/experiments/`; removed the obsolete `models/legacy/` category.
- Removed the empty `generalized_dickinson/` directory left after that model's earlier purge. Updated CMake source and test paths,
  Python native-module registration, current documentation, and model-local cross-references without changing model identifiers or
  algorithms.
- The complete Release build and all 49 CTest checks passed after the move.

## 2026-08-11 — Python Matrix puts input first and makes IDs optional

- Changed the analysis constructor from `Matrix(matrix_id, matrix, metadata)` to `Matrix(matrix, matrix_id=None, metadata=None)`.
  Ordinary one-matrix calls no longer require an artificial identifier; results contain `matrix_id=None` when none was supplied.
- Migrated maintained runners, corpus classification, tests, and examples to the new order. Corpus and batch paths continue to attach
  their stable database IDs explicitly.
- Documented metadata as optional pass-through data with only two recognized input keys: positive integer `dimension` and the
  `base_directory` required by confined `file:<relative-path>` corpus references.

## 2026-08-11 — Python accepts ordinary matrix-file paths

- Extended `Matrix.matrix` source dispatch so compact `dimension#values` and inline Matrix Market remain text, an existing
  `file:<relative-path>` remains a confined corpus reference, numeric values-only text still uses its dimension metadata, and every
  other string is passed directly to the native C++ file parser.
- Stopped stripping Python compact input before parsing, so the C++ no-whitespace compact-format rule now applies equally through the
  Python interface. Added coverage for a direct path containing spaces and for compact whitespace rejection.

## 2026-08-11 — Public CLI accepts compact matrices directly

- Extended the shared one-model command adapter to treat an argument beginning with decimal digits immediately followed by `#` as
  compact FracESSA matrix text; every other non-`-` argument remains a file path. The public launcher therefore accepts quoted or
  unquoted compact text, matrix files, and standard input without adding an input-mode option.
- Made the compact parser reject whitespace. The format-dispatch boundary removes only one terminal LF or CRLF supplied by a normal
  text file or standard-input line before parsing; internal and other surrounding whitespace remains invalid.

## 2026-08-11 — End-user and analysis preprocessing surfaces separated

- Kept the normal `coposit fast|safe strict|non-strict` interface deliberately small: it always applies connected-component splitting
  and all pre-checks and offers no preprocessing selector.
- Defined model-specific binaries, Python, reference runners, and benchmark tools as analysis interfaces. They retain explicit model
  selection and, where applicable, preprocessing controls for faithful baselines and experiments.
- Removed resolved code-review point 7; the launcher and both companion paths already enforced this end-user policy, so no solver code
  or new interface was needed.

## 2026-08-11 — File-loading timeout accounting accepted

- Closed review point 6 without changing the deadline boundary. A worker's wall-clock timeout includes native file loading and parsing,
  while stored native `elapsed_ns` starts after parsing and measures preprocessing plus the model.
- This difference is accepted for the maintained corpus: loading is negligible on small inputs, while model work dominates or reaches
  the timeout on the large inputs where loading is measurable. Do not reopen it without contrary benchmark evidence.

## 2026-08-11 — Native C++ owns file-backed matrix loading

- Added one shared `matrix_parser::parse_file()` boundary and routed the C++ CLI, every model-specific Python extension, and the
  maintained preprocessing benchmark through it.
- Python now validates a corpus reference and passes its resolved filename to `compute_matrix_file()`; it no longer reads or copies
  complete Matrix Market contents across the Python-to-C++ boundary.

## 2026-08-11 — Matrix-file hashes removed from normal runs

- Kept each external row's `file_sha256` as audit evidence, but removed automatic hashing from the shared Python loader and reference
  runner. Experiments and benchmarks now read each selected Matrix Market file once for parsing instead of hashing it first.
- Existing result reuse no longer opens or hashes an external matrix file. Detecting changed corpus payloads is an explicit integrity
  audit rather than a cost paid by every solver run.

## 2026-08-11 — Test-data surface reduced to maintained assets

- Renamed the maintained database to lowercase `testdata/copos_testdata.sqlite3` and the immutable source archive to
  `testdata/archive/copos_testdata.original.sqlite3.xz`; all code and documentation now use the lowercase names.
- Left only the maintained database, exported schema, README, and required `matrices/` payload directory at the `testdata/` top level.
  Moved dated corpus generators, migrations, and their historical documentation into `testdata/archive/`.
- Deleted the generated `testdata/__pycache__/` directory. All 201 external Matrix Market files remain referenced exactly once by the
  maintained database.

## 2026-08-11 — External matrix files are bound to their matrix IDs

- Added nullable `matrices.file_sha256`: all 201 `file:` rows store the lowercase SHA-256 of their exact Matrix Market bytes, while
  all 2,241 inline rows store `NULL`. No matrix contents, IDs, classifications, benchmark flags, or result rows changed.
- The shared Python file resolver verifies `file_sha256` when supplied. The reference runner verifies every selected external file
  before applying its existing-result skip, so changed input cannot silently reuse old solver evidence.
- Updated the externalizer to record the final optimized file hash and the legacy consensus reader to pass it through. A focused test
  changes one coefficient after creating a result and confirms that the next runner invocation fails before reusing that result.

## 2026-08-11 — FracESSA extraction reference consolidated

- Moved the byte-exact original corpus archive and its historical README from `reference/fracessa/testdata/` to
  `testdata/archive/`; the compressed archive SHA-256 remains `d69aa29ec946caaf3bab74d747bf0ae9572edf9a216eeb0be5f66e28d6a0a23c`.
- Removed the unused copied FracESSA C++ extraction snapshot and the now-empty `reference/` tree. Maintained implementations and
  algorithm provenance remain in their model directories and documentation.

## 2026-08-11 — Hadeler results now record their producing binary

- Removed the Hadeler-only hash exception from the reference runner. New Hadeler rows use the same exact native-module SHA-256 identity
  as every other model; the schema permits an empty hash only for unreconstructable legacy Hadeler evidence.
- The uniform 36-batch campaign used one clean frozen Release build. It began after the build between 04:19 and 04:22 EEST; Hadeler's
  four 524-matrix batches ran from 06:46:49 through 07:07:30 EEST. The unchanged Hadeler module observed before the next shared-wrapper
  rebuild had SHA-256 `1ecd99e49df73f3955c8a2684f84c5db7e7f3420d785c452bbbf7ed5a16b2c24`.
- Backfilled that hash on exactly 2,096 proven rows: 524 Representative-Core/Stress-union matrices, two modes, with preprocessing
  disabled and with both preprocessing stages. Left 3,184 older rows empty because their producing binary is not provable.

## 2026-08-11 — One exact C++ number boundary serves FracESSA and Matrix Market input

- Added a standards-aware C++ Matrix Market parser for array and coordinate storage and real, complex, integer, and pattern fields.
  coposit accepts only Matrix Market matrices explicitly declared `symmetric`; it rejects every other structure immediately and
  mirrors the stored lower triangle without a post-parse symmetry scan. Nonzero complex imaginary parts remain incompatible with
  coposit's real-matrix boundary.
- Extended the compact format and named it the FracESSA format. It accepts full upper-triangle and short circular-symmetric input,
  exact decimals and scientific notation, plus integer fractions written `[+|-]numerator/denominator`. The denominator must be
  unsigned and nonzero; ambiguous signs such as `1/-2` and `-1/-2` are rejected.
- Both formats share one exact numeric parser. Slash fractions remain FracESSA-only, and one least common positive denominator is
  cleared before the integer-only solver core. A neutral C++ matrix-parser entry point selects the independent FracESSA or Matrix
  Market parser from the content; the C++ CLI and Python native boundary both use that entry point instead of converting Matrix
  Market through Python.
- Grouped the four behavior-preserving headers under `cpp/include/coposit/parsers/` as `exact_number_parser.hpp`,
  `fracessa_matrix_parser.hpp`, `matrix_market_parser.hpp`, and `matrix_parser.hpp`, with matching focused test names.
- Made the parser boundary solely responsible for producing a nonempty square symmetric matrix. Removed the duplicate shape and
  symmetry scans from all 24 model entry points and the shared pre-check scan; direct model calls now require a valid matrix by
  contract.

## 2026-08-11 — Copositivity terminology standardized on CP and SCP

- Standardized project language on copositivity (`CP`, the non-strict predicate) and strict copositivity (`SCP`). Removed the former
  term from owned Markdown, source comments, identifiers, test names, scripts, SQL, command examples, and reports.
- Renamed internal capability and query identifiers to `copositive`; the public mode values `copositive`, `strictly_copositive`, and
  `both`, result fields, and stored database values are unchanged.

## 2026-08-11 — Public CLI now requires fast or safe

- Replaced the Dutour-linked public executable with a thin `coposit fast|safe` launcher. `fast` executes an isolated Adaptive
  Sponsel–COPOMATRIX companion; `safe` executes an isolated Dickinson 2019 companion. No binary links both models.
- Both public methods run negative-entry connected-component splitting followed by all exact pre-checks. The public interface then
  requires `strict` or `non-strict`, with no hidden default. Every returned Boolean is exact; incomplete and resource-limited
  searches remain non-Boolean failures.
- Added method-identity, non-strict, strict-boundary, smoke, missing/unknown-method, and missing/unknown-predicate CLI checks. All 44
  Release CTest checks pass.
- Left direct-string versus matrix-file selection explicitly open until the NIST parser is complete. The current compact-text
  standard-input/file path remains transitional; no placeholder file parser or format guessing was added.

## 2026-08-11 — Strict-only models reject non-strict mode before preprocessing

- Added a fail-closed compile-time non-strict-mode capability to each one-model Python native module. Strict-only modules now reject
  `copositive` mode before connected-component splitting or pre-checks can settle the matrix.
- Added coverage for all 15 strict-only models under all four preprocessing selections and for all nine non-strict-capable models with
  preprocessing disabled and with both stages enabled.
- The focused regressions and all 35 Release CTest checks pass.

## 2026-08-11 — Large corpus matrices moved to Matrix Market files

- Added exact `file:<relative-path>` matrix references at the Python corpus boundary. References are confined to the database
  directory and accept the standard Matrix Market `matrix array integer symmetric` and `matrix coordinate integer symmetric` forms;
  every C++ model still receives the same `dimension#upper-triangle-values` input.
- Externalized the 201 matrix rows larger than 500 KB into `testdata/matrices/<matrix_id>.mtx`. Symmetric Matrix Market lower-triangle
  column-major order is identical to coposit's packed upper-triangle row-major order, so no matrix value or classification changed.
- Compared the actual encoded byte counts and retained the smaller exact representation per file: 132 arrays and 69 coordinates. All
  69 coordinate files have below one-percent stored-triangle density; every retained array is at least 75% dense. External matrix
  storage fell from 729,009,854 to 458,287,571 bytes (37%). Streaming canonical hashes matched for all 201 matrices.
- Compacted `copos_testdata.sqlite3` from 1.54 GB to 76 MB. Compared all 201 reconstructed value strings byte-for-byte with the
  pre-migration database, retained all 164,681 result rows, passed SQLite integrity and foreign-key checks, all 19 Python tests, and
  all 35 Release CTest checks.

## 2026-08-11 — Adaptive Sponsel–COPOMATRIX selects the least-branched pivot

- Corrected the maintained hybrid to calculate every local COPOMATRIX pivot's exact immediate-child count and choose the first pivot
  attaining the minimum. A minimum of at most two still triggers COPOMATRIX immediately; otherwise Sponsel remains the fallback.
- The 1,000-split progress cutoff now forces COPOMATRIX at that same minimum-child pivot instead of unconditionally using local pivot
  zero. Exact FLINT binomial counts avoid constructing any alternative pivot's children.
- Added a focused regression in which pivot zero would create two children but a later pivot creates only one. The Release model test
  and all 35 repository tests pass.
- Repeated all eight strict/non-strict and preprocessing configurations on the 524-matrix Core/Stress union with hash
  `249d413159ae27519472c474d68eb697a57af1ceb06b2129ab69d7f796856977`, a five-second cutoff, parent CPU 3, and workers 4–6. All
  4,192 rows are present; 3,576 completed and 616 timed out, with no wrong result, node limit, or execution error.
- The minimum-child rule did not improve this benchmark. No-precheck completed 457 strict and 428 non-strict calls, one and three fewer
  than the preceding binary. Both-stage preprocessing completed 465 strict and 438 non-strict calls, unchanged and two fewer. Matrix
  9648 is the material regression: its non-strict no-precheck call changed from 0.022 seconds to timeout. The research and technical
  reference reports now use the new hash.

## 2026-08-11 — Uniform three-worker empirical benchmark rerun

- Rebuilt the current Release modules, passed all 35 tests, and reran the complete 524-matrix Representative Core/Stress union once
  for all eight literature baselines and Adaptive Sponsel–COPOMATRIX: strict and non-strict modes, with preprocessing disabled and with
  both stages enabled. CPU 3 handled dispatch and serialized SQLite writes; three persistent workers used CPUs 4–6; every matrix had
  a five-second cutoff.
- All 36 batches and 18,864 rows completed without interruption. Every completed Boolean matched corpus truth. The rerun contains no
  execution errors; Danninger matrix 10244 now reaches the intended unresolved node limit instead of the historical recursive stack
  crash.
- Nine prior timeout rows completed and no prior completion became unresolved. Five transitions used unchanged binaries and expose
  cutoff/load variability; four used newer implementation-optimized no-precheck binaries. No mathematical classification changed.
- Updated the canonical reference and research report from the uniform measurements.

## 2026-08-11 — COPOMATRIX normal benchmark corrected to five seconds

- Found that the recorded no-pre-check strict COPOMATRIX union mixed a one-second cutoff for 384 Core matrices with a five-second
  cutoff for the remaining Stress matrices. Re-ran all 524 Core/Stress matrices in both strict and non-strict modes with the current
  native module and a uniform five-second cutoff.
- Strict completed 356 and timed out on 168; non-strict completed 327 and timed out on 197. This recovers four strict and one non-strict
  union completions relative to the previous rows. Dispatcher wall time was 217.947/256.273 seconds and substituted one-CPU time was
  855.417/1,007.445 seconds. Every completion matched corpus truth, with no errors or resource-limit statuses.
- Updated the canonical Core/Stress reference report and research report. Added a pre-checked reach table comparing median matrix order
  for completed, unresolved, and model-unique Dickinson and Adaptive Sponsel–COPOMATRIX results. SQLite integrity remained `ok`.

## 2026-08-11 — Both-stage preprocessing benchmarked on every literature baseline

- Ran connected-component splitting followed by all enabled pre-checks on the 524-matrix Representative Core/Stress union for every
  faithful literature baseline in strict and non-strict mode. Reused the two current Dickinson batches and completed 14 new batches on
  parent CPU 3 and solver CPUs 4–9 with a five-second per-matrix cutoff.
- Both-stage preprocessing improved every one of the 32 model/set/mode completion comparisons against the recorded normal runs.
  Aggregate union completions rose from 4,982 to 5,360; the largest individual gains were Bundfuss Stress strict (+42), Sponsel Stress
  strict (+40), and Dutour Stress strict (+33).
- The 8,384 selected both-stage rows contain 5,360 completed classifications, 3,012 timeouts, and 12 Dutour non-strict node limits, with
  zero execution errors, corpus mismatches, or impossible non-strict/strict result pairs. Database integrity and foreign keys passed.
- Safi non-strict was externally terminated after flushing 485 rows and resumed only its 39 missing rows. The report preserves this fact
  and shows the rounded combined wall time instead of inventing an exact uninterrupted measurement.

## 2026-08-11 — Core/Stress reference report condensed

- Reorganized `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` from repeated per-model subsections into wide comparison tables,
  reducing it from 496 to 125 lines without changing stored benchmark results.
- Promoted the four requested Dickinson 2019 and Adaptive Sponsel–COPOMATRIX runs with both preprocessing stages into one detailed
  Core/Stress table. Kept all preprocessing comparisons, normal-model results, truth-category coverage, baseline overlap, recorded
  exceptions, run conditions, and native-module hashes in compact grouped sections.

## 2026-08-11 — Principal-face pre-check default restored to cardinality three

- Restored the default `principal_submatrices_up_to` value from two to three for the Representative Core and Stress benchmark
  population, where most matrices are small and the five-second solver cutoff bounds expensive exceptional inputs.
- Cardinality one and two remain explicitly selectable. The stopped full-corpus experiments continue to document why cardinality
  three is unsuitable for uncensored order-3,000 preprocessing.
- Repeated all 12 requested Dickinson 2019 and Adaptive Sponsel–COPOMATRIX Core/Stress runs with connected components only,
  pre-checks only, and both stages in non-strict and strict mode. The 6,288 stored rows contain 4,872 completed classifications and
  1,416 five-second timeouts, with zero errors, node limits, corpus mismatches, or non-strict/strict contradictions.
- With the new bounded floating proposal and exact witness verification, pre-checks improved completion in every per-set comparison
  against the normal model. Both stages together led the enabled choices with 733/1,048 Dickinson and 903/1,048 adaptive union
  results completed across the two modes. `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` was updated after every run and with
  the final detailed tables and total summary.
- The complete Release build and all 35 CTest checks passed. The maintained database has exactly 524 rows in each requested batch,
  no foreign-key violations, and an `ok` integrity check.

## 2026-08-11 — Principal-face pre-check defaults to cardinality two

- Reduced the default `principal_submatrices_up_to` value from three to two after full-corpus preprocessing exposed billions of
  eligible connected negative triples at orders above 3,000. Cardinality three remains an explicit option for smaller or deliberately
  targeted inputs.
- The complete order-at-most-three decision is unchanged. Only rejection-only enumeration inside larger matrices changed.

## 2026-08-10 — Iterative exact Frank–Wolfe models purged

- Removed `exact_frank_wolfe_dickinson` and `adaptive_exact_frank_wolfe_sponsel_copomatrix`, including their implementations,
  focused tests, build and Python registrations, current documentation, generated targets, and stored result rows.
- Exact rational Frank–Wolfe trajectories caused severe arbitrary-precision numerator and denominator growth without enough
  additional completions to justify their cost. The exact one-step variants remain because they have no iterative trajectory.
- Floating Frank–Wolfe proposal paths remain bounded and use exact arithmetic only to verify a proposed witness.

## 2026-08-10 — Bounded floating Frank–Wolfe pre-check with exact verification

- Replaced the shared pre-check's rational Frank–Wolfe trajectory after an order-22 Dannenberg–Schürmann lift exposed denominator
  growth to roughly 35 million bits in only 22 steps. The trajectory now globally scales the exact integer input into `double`, starts
  at the simplex centre, and retains the dimension-sized iteration ceiling.
- Added the four agreed early exits: a scale-aware negligible Frank–Wolfe gap, a zero or ineffective computed step, an unchanged
  floating objective, or an exactly verified witness that completes the requested mode. Repeated toward vertices remain permitted;
  the iteration bound is a work budget, not one visit per coordinate.
- Floating arithmetic only proposes candidates. A candidate is converted to bounded nonnegative integer weights and evaluated as
  an exact integer quadratic form; strict mode rejects only an exact value at most zero and non-strict mode only an exact negative
  value. The positive normalization denominator is irrelevant by homogeneity and is never constructed.
- Extended the shared upper-triangle scan to retain the full row sums and maximum absolute entry needed by the proposal without
  another sign-and-sum pass. Focused coverage includes the former order-22 growth case and a `2^2000 vv^T + I` matrix whose positive
  identity contribution underflows after scaling, proving that a rounded floating zero cannot become a classification.

## 2026-08-10 — Independently selectable preprocessing reference runs

- Added the optional Python preprocessing selector `none`, `connected_components`, `pre_checks`, or `both`; the default remains
  `none`, so existing model calls and stored parameter strings are unchanged.
- Ran Dickinson 2019 and Adaptive Sponsel–COPOMATRIX in non-strict and strict mode with each of the three enabled preprocessing
  choices on the 524-matrix union of Representative Core and Stress Test. The current report selects 6,288 complete stored rows;
  every batch contains all 524 matrices, with zero execution errors, node limits, classification mismatches, or non-strict/strict
  contradictions.
- Connected components alone was the only choice that did not reduce five-second-cutoff completion. The full pre-check bundle cost
  more time than it saved on these sets. A concurrent semidefinite-pre-check update was detected during the first runs, so all six
  Dickinson batches were rerun from one stable copy of the updated module before publication.
- Updated `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` after each completed configuration pair. The complete Release suite
  passed, all 19 Python wrapper tests passed, and SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Nullity-one completion of the exact definiteness pre-check

- Extended the existing positive-semidefinite pre-check only at nullity one. It recovers one exact kernel vector from the retained
  fraction-free LDLT factorization; mixed signs prove strict copositivity, while a one-sided vector or its negation is a nonnegative
  zero and rejects strict copositivity.
- Deliberately left nullity greater than one unresolved because checking basis columns cannot decide whether their span intersects
  the nonnegative orthant. Those matrices continue to the selected final algorithm without constructing a nullspace basis.
- Non-strict-only calls still stop at positive semidefiniteness without recovering a kernel vector. Focused checks cover both
  nullity-one outcomes and higher-nullity delegation. The complete Release build and all 37 tests passed; SQLite
  `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Stream connected components without retained index lists

- Replaced the materialized `vector<vector<size_t>>` component result with visitor-based traversal over one reused multi-word
  `support` bitset. The visitor can stop traversal as soon as the requested copositivity result is known.
- The component pipeline now converts only the current component into one reused index vector, copies and solves that block
  immediately, and then reuses both buffers for the next component. A connected input still reaches the final algorithm by reference.
- Focused coverage verifies ordered traversal, the connected fast path, dimensions beyond 64 bits, and early stopping. The complete
  Release build and all 37 tests passed; SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Default-on independent preprocessing stages

- Made the standalone pre-check's seven individual options default to on; `pre_check::options::none()` provides the explicit
  zero-overhead bypass, and every check can still be disabled separately.
- Added independent top-level `pre_checks_enabled` and `connected_components` switches to the combined pipeline. Both default to on,
  while disabling components keeps the selected pre-checks on the whole matrix and disabling pre-checks keeps component splitting.
- No maintained model calls the shared pipeline, so the new defaults affect only callers that explicitly use this boundary.
- Focused tests cover all four stage combinations. The complete Release build and all 37 tests passed; SQLite
  `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Shared scan and independently selectable component pipeline

- Added one reusable exact matrix scan for symmetry, diagonal signs, negative edges, negative-part row sums, the all-ones value, and
  selected non-strict or strict principal-pair facts. The pre-check now consumes this record instead of owning another matrix scan.
- Changed connected-component discovery to consume the stored negative-entry graph without inspecting or copying the matrix. Added
  an opt-in component pipeline that reuses the original matrix when connected and computes block-specific scan facts while copying
  each disconnected principal block once.
- Kept the controls independent and default-off. Connected components may be enabled with any subset of the pre-checks, while
  disabling components runs the selected pre-checks on the whole matrix. No maintained model enables either facility.
- Preserved combined non-strict/strict aggregation across blocks, including continued non-strict checking after a strict-only boundary
  result. The complete Release build and all 37 tests passed; SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Explicit principal-face enable switch

- Replaced the temporary zero/off cutoff with `principal_submatrices`, the single enable switch for principal-face checking.
  `principal_submatrices_up_to` is now only a parameter and accepts cardinality one, two, or three; its default is three.
- The complete Release build and all 36 tests passed; SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Separate connected components and selectable principal-face cutoff

- Removed negative-entry component decomposition from `pre_check::options`. The standalone
  `connected_components::visit(matrix, visitor)` facility now emits every component principal matrix, including singletons, without
  selecting a copositivity mode, skipping a component, or aggregating a yes/no result. Connected inputs incur no matrix copy.
- Removed the redundant standalone diagonal option. `principal_submatrices_up_to` now selects zero/off or a maximum checked
  cardinality of one, two, or three; the larger-matrix route remains rejection-only, while `small_dimension` remains the separate
  complete order-at-most-three decision.
- The complete Release build and all 36 tests passed, including the three cutoff levels, combined equality classification, and the
  separately focused component traversal. SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Exact definiteness and combined pre-check classification

- Added the default-off `positive_definiteness` option. It makes one matrix copy and one exact fraction-free LDLT factorization;
  positive definiteness accepts strict copositivity, positive semidefiniteness accepts non-strict copositivity, and a failed
  certificate falls through without rejection.
- Added `pre_check::classify(matrix, options, final_classifier)` for Hadeler-, Dickinson-, and other combined classifiers. One shared
  pre-check traversal records non-strict and strict facts separately, including equality witnesses, weak acceptances, and component
  results; it does not run the pre-check twice.
- Single-mode calls reuse the same classification-aware engine through compile-time non-strict and strict queries. An invalid
  singleton is still delegated when its separate diagonal check is off, preserving independent option behavior.
- The complete Release build and all 35 tests passed, including the Hadeler and Dickinson combined-classifier suites and focused
  equality-boundary pre-check tests. SQLite `PRAGMA integrity_check` returned `ok`.

## 2026-08-10 — Shared non-strict/strict pre-check and exact witness additions

- Replaced the strict-only pre-check entry point with `pre_check::check(matrix, mode, options, final_algorithm)`. One implementation
  is templated on `copositivity_mode`; the matrix scans, graph traversal, component construction, and exact Frank–Wolfe arithmetic
  occur in one source body, while the shared mode predicate alone changes strict $>0$ from non-strict $\geq0$.
- Kept `small_dimension` as the complete order-at-most-three decision. Added the separate
  `principal_submatrices_up_to_3` option, which only rejects larger matrices. Its diagonal and pair work is fused into the existing
  scan, and it tests only connected negative-entry triples because passed components with nonnegative cross entries already prove
  every disconnected triple.
- Added a default-off centre-start exact Frank–Wolfe witness option with at most $n$ exact line-minimizing steps. A zero iterate
  rejects strict mode but remains admissible in non-strict mode, where the search continues when a descent direction exists.
- Renamed the sign-specific option fields to mode-neutral `diagonal` and `all_ones`. Positive-definiteness remains deliberately
  excluded after the preceding FracESSA ablation and the user discussion.
- Focused tests cover full small-matrix equality, negative-only pair/triangle/path faces, every whole-matrix equality boundary,
  iterative and zero-valued Frank–Wolfe outcomes, mode-dependent singleton components, dynamic supports beyond 64 coordinates, and
  input validation. All 35 Release tests pass and SQLite integrity returns `ok`.

## 2026-08-10 — Default-off individual pre-check selection

- Renamed the optional shared strict preprocessing boundary from Early Decision to Pre-Check and replaced the all-or-nothing call
  with six independent options: exact small dimensions, nonpositive diagonal rejection, nonnegative off-diagonal acceptance, Qi
  negative-part diagonal-dominance acceptance, all-ones rejection, and negative-entry component splitting.
- Every option defaults to off. An empty selection delegates directly to the final algorithm without preprocessing overhead;
  `pre_check::options::all()` reproduces the complete FracESSA sequence. No maintained model enables a pre-check yet.
- Kept each option independently correct: later checks do not assume that disabled earlier checks passed, including positive-diagonal
  requirements for nonnegative acceptance and isolated negative-graph vertices.
- Added focused coverage for default bypass, every individual decision, disabled-check dependencies, component dispatch across a
  64-bit boundary, and input validation. All 35 Release tests pass and SQLite integrity returns `ok`.

## 2026-08-10 — Extracted opt-in FracESSA early decisions

- Added the shared strict-only `early_decision::strictly_copositive(matrix, final_algorithm)` wrapper without enabling it in any
  maintained model. Calling a model's calculation directly bypasses the wrapper, so literature baselines and stored result
  identities remain unchanged while later variants can select the same preprocessing independently.
- Copied FracESSA's current generic order exactly: direct exact orders one through three, nonpositive-diagonal rejection,
  nonnegative-off-diagonal acceptance, Qi negative-part diagonal-dominance acceptance, exact all-ones rejection, and negative-entry
  connected-component dispatch to the supplied final algorithm.
- Reused coposit's existing exact small criteria and dynamic packed support. The component path therefore crosses 64-bit word
  boundaries without FracESSA's former dimension limit. Input validation and cooperative timeout checkpoints remain explicit.
- Deliberately omitted FracESSA-specific candidate/Hessian decisions, its experimental KKT route, and the positive-definiteness and
  Z-matrix checks already removed from FracESSA. A non-strict-copositivity adaptation remains deferred until its inequalities are
  considered separately rather than inferred from the strict path.
- Added focused coverage for every copied decision, connected and disconnected callback dispatch, rejection propagation, invalid
  inputs, and a 129-dimensional three-word component. All 35 Release tests pass and SQLite integrity returns `ok`.

## 2026-08-10 — Adaptive Sponsel–COPOMATRIX non-strict mode

- Extended the selected `adaptive_sponsel_copomatrix` model from strict-only operation to individually selectable non-strict and strict
  copositivity. The model does not advertise combined classification because the two predicates still require separate traversals.
- Reused the shared exact order-one-through-three classifier. Non-strict traversal permits zero diagonals, rejects an edge only when
  $\gamma^2>\alpha\beta$, uses positive semidefiniteness for Sponsel's stripped `H` certificate, and applies Xu–Yao's zero-pivot
  rule before forming a Schur child. Strict inequalities and positive definiteness remain unchanged in strict mode.
- Added focused C++ boundary, zero-pivot, wide-edge, and semidefinite-`H` checks plus a Python native-wrapper non-strict-mode check. All
  34 maintained tests passed.
- Ran both modes of binary `ac768b4f80f80dd63541ab7b8455cd4ab56beceb83db0af976f0efb326ee929f` over the 524-row
  Representative Core and Stress union with a five-second cutoff and CPUs 3/4–7. Strict completed 458 with 66 timeouts in 105.102
  seconds; non-strict completed 431 with 93 timeouts in 141.778 seconds. Every completed result matched truth, with no errors or node
  limits and no contradiction among the 431 matrices completed in both modes.

## 2026-08-10 — Selected adaptive model on Representative Core and Stress

- Extended `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` with a separate strict-only coposit-created-model section rather
  than mixing the selected hybrid into the faithful literature-baseline tables.
- Ran the current streak-1,000 `adaptive_sponsel_copomatrix` binary on the full 524-matrix Representative Core and Stress union with
  a five-second cutoff, CPU 3 for dispatch/database work, and solver workers on CPUs 4–7. It completed 458 union matrices, timed out
  on 66, and produced no node limits, errors, or mismatches.
- It completed 364/384 Representative Core and 179/240 Stress matrices. The measured union wall time was 105.268 seconds; substituted
  work was 126.059062 seconds for Representative Core and 380.047286 seconds for Stress, with the 100-row overlap counted in both.

## 2026-08-10 — Adaptive exact Frank–Wolfe Sponsel–COPOMATRIX experiment

- Added the isolated strict-only `adaptive_exact_frank_wolfe_sponsel_copomatrix` experiment under `models/experiments/`, leaving the
  selected streak-1,000 Adaptive Sponsel–COPOMATRIX and every legacy model unchanged.
- At every unresolved Sponsel Gram node, after the selected-edge test and failed strict `H` certificate, the model starts at the
  simplex centre and performs at most the current Gram order's number of exact rational Frank–Wolfe line-minimizing steps. It rejects
  only after independently verifying an exact nonpositive quadratic value; stationarity or budget exhaustion proceeds to the inherited
  split. There is no floating point, multistart, tolerance, denominator truncation, or bit-length cutoff.
- On the fixed 2,078-matrix, five-second strict snapshot, all 1,926 completed results matched truth and 152 timed out. Relative to the
  selected base it gained no completion, lost 98, and increased substituted work by 118.72%. The experiment is therefore recorded but
  not promoted.

## 2026-08-10 — Legacy model grouping

- Renamed `models/experiments/` to `models/legacy/` because the contained Zischg–Sponsel–COPOMATRIX comparison is no longer an
  active experiment and will not be used or developed further.
- Kept the legacy target, Python identifier, tests, and documentation buildable solely for reproduction of its stored results. The
  selected streak-1,000 `models/adaptive_sponsel_copomatrix/` model remains unchanged.

## 2026-08-10 — Sponsel–COPOMATRIX experiment grouping

- Kept the selected streak-1,000 `adaptive_sponsel_copomatrix` model directly under `models/` and moved the retained
  `adaptive_zischg_sponsel_copomatrix` comparison intact under `models/experiments/`.
- Preserved both public model identifiers, algorithms, cutoffs, tests, and stored result identities; only source and documentation
  paths changed.

## 2026-08-10 — Bounded Danninger depth-first traversal

- Replaced Danninger's recursive staircase walk with an explicit LIFO frame vector while preserving its down-before-right,
  plus-before-minus, and first-child depth-first order.
- Applied coposit's existing 50,000-open-node limit to active matrix checks and staircase frames. The native wrapper reports an
  exhausted limit as unresolved `node_limit`, never as a negative classification.
- Added the order-999 Johnson-Reams generalized Horn construction behind corpus matrix 10244 as a focused regression. Non-strict mode
  now reaches the node limit instead of terminating its worker with `SIGSEGV`; all baseline source-order checks remain intact.

## 2026-08-10 — Representative Core and Stress baseline reference

- Added benchmark-flag selection to `python/run_results.py`; multiple `--matrix-set` arguments select their union and still compose
  with dimension and matrix-ID bounds. A focused wrapper test covers the Representative Core and Stress Test union.
- Completed five-second non-strict and strict runs for all eight literature baselines over the 524 distinct matrices selected by the
  384-row Representative Core and 240-row Stress Test. Each of the 16 exact model/mode identities contains 524 rows, every completed
  Boolean matches corpus truth, and SQLite integrity returns `ok`.
- Danninger's non-strict traversal reproducibly exhausts the process stack on matrix 10244 (`n=999`), exits its isolated worker with
  code -11, and is stored as one explicit error. The runner replaces the worker and completes the batch; the error is never treated
  as a negative classification. Dutour node limits and all model timeouts likewise remain unresolved.
- Recorded per-model status, truth-category coverage, substituted work, union wall time, consolidated completion counts, and
  cross-model overlap in `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md`.

## 2026-08-10 — Selected depth-first COPOMATRIX after source-frontier comparison

- Temporarily implemented Xu and Yao's Algorithm 2 scheduling in an isolated `copomatrix_original` comparison model: project every
  member of the current set $F$, union all unresolved projections, and only then replace $F$ with the next frontier. The experiment
  retained `copomatrix_2011`'s exact projection mathematics and safe early checks, then was deleted after the comparison.
- In a strict-mode one-second Representative Core comparison on two workers, depth-first `copomatrix_2011` completed 302 of 384
  rows with 82 timeouts. The source-frontier model completed 280, with 57 timeouts and 47 explicit frontier-limit outcomes. Every
  completed Boolean matched corpus truth. Depth-first uniquely completed 25 rows; source-frontier uniquely completed 3.
- Both models completed the same 277 rows with no disagreement. On those rows, source-frontier scheduling took 1.859715 seconds of
  summed solver time versus 1.198887 seconds depth-first, 55.1% more; the median paired percentage was 35.6% slower. Charging one
  second to each unresolved row gives 105.859818 versus 85.070315 seconds, 24.4% more. All 34 Release tests pass and SQLite
  integrity returned `ok` before removal.
- Kept the existing `copomatrix_2011` depth-first modification: it finds early rejection witnesses without completing an entire
  frontier and retains far fewer simultaneous matrices, making it faster here and avoiding the frontier model's node-limit failures.
  After removing the comparison model and its 384 temporary result rows, all 33 Release tests pass and SQLite integrity returns `ok`.

## 2026-08-10 — Clarified baseline identity and locked source-defining traces

- Retained the `dickinson_2019` and `copomatrix_2011` identifiers. Their `ALGORITHM.md` files now state prominently that Dickinson
  adds coposit's direct order-at-most-three test, while COPOMATRIX uses depth-first principal-before-Schur traversal and early
  all-diagonal checks instead of Algorithm 2's complete-frontier set replacement. The retained Xu–Yao paper was checked directly.
- Added one test-only source-conformance trace to each of the eight literature baselines. They pin Dutour split/child order,
  Danninger plus-before-minus order, COPOMATRIX principal-before-Schur order, Hadeler numeric-mask pair order, Dickinson certificate
  coverage, Safi slicing, Bundfuss's exact 5/12 split, and Sponsel's H-certificate acceptance. Production builds contain no trace
  symbols or trace state.
- Repaired every broken local link in baseline `ALGORITHM.md` files after their move under `models/baselines/`; an automated local
  target check reports no missing links. All eight focused suites and all 33 Release tests pass; SQLite data was not modified.

## 2026-08-10 — Avoided unused Danninger arithmetic

- Danninger 1990 now constructs the division-free Schur matrix only when its negative half-cone is reached. Nonnegative pivot rows
  use only the principal block, and a mixed-sign node rejected by its plus subtree no longer allocates or fills an unused Schur
  matrix.
- Each distinct primitive positive-negative pair ray is now reduced once per recursive node and reused across every plus and minus
  staircase path. The pivot, rays, transformed children, path order, recursion, and early termination are unchanged.
- The focused Danninger test and all 33 Release tests pass. A read-only 250 ms combined-mode Smoke check completed 47 of 49 rows with
  zero mismatches; matrices 9212 and 9991 remained explicit timeouts at five seconds. SQLite integrity returned `ok`.

## 2026-08-10 — Lazy baseline child construction

- Changed Safi 2021 from eagerly materializing every child of a wide SNC slice to an iterative depth-first path frame that stores
  one parent and sparse sibling descriptions. Children, child order, first-failure behavior, and the logical open-node-limit count
  are unchanged; only the next child is materialized.
- Changed Bundfuss 2008 and Sponsel 2012 to prepare their common exact split data once, construct and inspect the first child, and
  construct the second sibling only if the first did not reject. Their λ rule, child matrices, inspection order, content reduction,
  LIFO traversal, and resource-limit check are unchanged.
- The three focused suites and all 33 Release tests pass; all 17 Python wrapper tests pass. Across 98 non-strict/strict Smoke calls per
  model, every completed result matched the corpus. A 250 ms cooperative check left one known hard non-strict case unresolved for
  each model; those same three calls remained timeouts at five seconds. SQLite integrity returned `ok`.

## 2026-08-10 — Reduced simplex-refinement open-node limit

- Reduced the shared unresolved-result guard from 1,000,000, first to 100,000, and finally to 50,000 simultaneously unfinished nodes
  for Dutour, Bundfuss, Safi, Sponsel, and Frank–Wolfe–Sponsel. The limit remains a resource outcome, never a negative classification.
- Reference runs now use four solver workers on CPUs 4–7 with CPU 3 reserved for dispatch and serialized database writes. This lowers
  concurrent memory pressure; it does not constrain Danninger, whose traversal does not use the shared unfinished-node queue.
- Timed reference workers now receive one second to return after the cooperative timeout signal. The parent records a timeout and
  replaces a worker that remains inside one long native operation, preventing that operation from growing without a resource bound.
- Completed and stored the five-second N=1–100 reference pass for all eight literature baselines in both non-strict and strict mode:
  16 complete model/mode identities of 2,084 rows each. Every completed Boolean matches the corresponding corpus truth; there are no
  execution errors. Timeouts and the Dutour/Bundfuss node-limit rows remain explicitly unresolved. Database integrity returned `ok`.

## 2026-08-10 — Completed legacy non-strict-copositivity truth by baseline unanimity

- Ran non-strict mode for all eight literature baselines on each of the 889 previously unknown legacy strict-false matrices. A five-second
  timeout or fixed node limit counted as an abstention; every completed Boolean had to agree, at least four models had to complete, and
  any disagreement, other error, or smaller completed set stopped before writing that matrix.
- Every matrix passed. The run added 415 copositive-boundary and 474 non-copositive classifications in 382.241 seconds. No completed
  models disagreed. Matrix 8159 was the sole four-result case; all other matrices had at least six completed results. Across all
  matrix/model calls, the abstention counts were Danninger 1, COPOMATRIX 1, Hadeler 42, Dickinson 15, Safi 3, Bundfuss 3, Dutour 0,
  and Sponsel 0.
- The final 2,442-row corpus contains 674 strict, 1,179 boundary, and 589 non-copositive matrices, with no unknown non-strict result.
  `PRAGMA integrity_check` returned `ok`, and no strict row violates non-strict copositivity.
- Native module SHA-256 values were Dutour `a9587187851d3088dfff17ced600f25956fd8c91929d51ce8c55916ec8a98971`, Danninger
  `117ef63b6e45e1b81c72a904c5847aad0da5839176af951a72de741288d56831`, COPOMATRIX
  `92b78a9fdf1db14e804ec4cc84e8f61a20cebfb301b20f7b15553044c24a2cc8`, Hadeler
  `4b0b41af3584f87f30625d3a421d4ff23163002a6f4bb1e07300b5ae1afce6f7`, Dickinson
  `926cb85bc1c72e186586e23bf973cfb5998ac41516710879980b53ed7bfc29d1`, Safi
  `13053f488096d33867bf58d5d55dd5c764837d33f9c327b7d2e82f41e378cd8d`, Bundfuss
  `4ddd33633d64949530ac00c0864deba811c7ab258fc1a15f144be1b5fc896108`, and Sponsel
  `e9f8a283a676aea658e451237abc1780b654c87d61244cbd37ff647577490e85`.

## 2026-08-10 — One-pass combined classification

- Added `coposit::model::classify(matrix)` to Hadeler 1983, Dickinson 2019, and Danninger 1990. Each implementation follows its
  non-strict-complete traversal once, remembers a strict-only zero, and stops both predicates only on a non-strict failure; it does not
  call the two selected modes separately.
- Added Python `mode="both"` for those three models. Successful results populate both Boolean fields as `(false, false)`,
  `(true, false)`, or `(true, true)`; unsupported models return `EXEC_ERROR`. Existing `solve(matrix, mode)` and its strict early
  termination remain unchanged.
- Added focused native and Python checks for all three classifications, unsupported-model handling, and multiprocessing forwarding.
  Independent corpus checks matched both stored truth fields without mismatches on all 746 known matrices through order 10 for
  Hadeler and Dickinson and all 326 known matrices through order 6 for Danninger.

## 2026-08-10 — Non-strict and strict decision modes for all baselines

- Replaced the strict-only model contract with `solve(matrix, copositivity_mode)`, defaulting to `strictly_copositive`. The eight
  literature baselines now also implement exact non-strict copositivity; coposit-created variants reject non-strict mode explicitly.
- Added the non-strict boundary rules from each source family: Danninger's zero-pivot reduction, Hadeler's negative-determinant
  criterion, Dickinson's continued certificate traversal after a nonnegative zero, COPOMATRIX's non-strict projection rules,
  non-strict Dutour/Bundfuss/Safi comparisons, and Sponsel's exact positive-semidefinite `H` certificate.
- Exposed the mode through the shared CLI and Python paths. Results keep `is_copositive` and `is_strictly_copositive` separate and
  populate only the requested predicate, so one run is never presented as an unproved three-state classification.

## 2026-08-10 — Restored nullable non-strict-copositivity truth

- Restored `matrices.is_copositive` alongside `is_strictly_copositive`. The field is nullable by design: the corpus now records 674
  strictly copositive matrices, 764 copositive boundary matrices, 115 proved non-copositive matrices, and 889 legacy strict-false
  matrices whose non-strict copositivity has not been established. A table constraint requires every strict matrix to be copositive.
- Reconstructed all retained original values from the immutable FracESSA snapshot: 413 retained strict rows, 56 boundary rows, 55
  non-copositive rows, and 891 then-unknown rows. Later literature provenance upgrades Horn 9162 and Hoffman–Pereira 9163 to
  boundary-copositive. Every post-extraction literature family is proved boundary or strict; the two generated negative-witness
  families contribute 60 non-copositive rows. This leaves exactly 889 unnamed legacy rows unknown rather than guessing their truth.
- Updated all six corpus importers to persist non-strict truth and changed benchmark sampling to use `is_copositive` directly rather
  than provenance-text heuristics. Smoke now contains 49 rows, including all four fast known non-copositive rows at orders 13–20.
  Representative Core remains 384 rows and now contains 192 strict, 84 boundary, all 33 known non-copositive rows through order 100,
  and 75 non-strict-unknown rows. Stress and Scale retain their 240 and 364 exact memberships.
- `testdata/archive/add_copositivity_classification_2026_08_10.sql` performs the guarded table rebuild and
  `testdata/archive/assign_benchmark_sets_2026_08_10.sql` freezes the revised memberships. These revised values supersede the provisional
  strict-only benchmark-set composition recorded immediately below.

## 2026-08-10 — Four overlapping benchmark sets

- Added the independent `smoke_set`, `representative_core`, `stress_test`, and `scale_set` Boolean columns to every maintained corpus
  row. The current assignments contain 47, 384, 240, and 364 matrices respectively; 864 distinct matrices belong to at least one set.
- Smoke is a fast order-19-or-smaller correctness gate and is wholly contained in Representative Core. Representative Core balances
  strict and not-strict truth 192/192, uses fixed order/class quotas through order 100, and round-robins families. Stress retains all
  93 matrices unresolved by all eight frozen canonical baseline runs, all historical bad-26 rows, hard family representatives, and
  maximum-order anchors. Scale contains all 358 matrices above order 100 plus the six order-51 members needed to keep both complete
  generated 90-matrix panels.
- No matrix was added: the literature corpus and the existing sparse and dense controlled high-order panels already cover every
  requested stratum. `testdata/archive/assign_benchmark_sets_2026_08_10.sql` reproduces the assignment and refuses corpus/result drift by
  checking corpus size, canonical coverage, set counts, and two exact ID moments for every set.

## 2026-08-09 — Sixty-second high-order Zischg hybrid run through order 300

- Ran the 18 new generated matrices through order 300—the available dimensions are 51, 168, and 235—with the maintained
  streak-10,000 Adaptive Zischg–Sponsel–COPOMATRIX binary. The separate parameter set
  `new_matrices_n_le_300_60s_2026-08-09` preserves the five-second rows. Parent work used CPU 3 and six workers used CPUs 4–9.
- The run finished in 120.388 seconds: 7 correct not-strict completions and 11 timeouts, with no strict completion, mismatch, error,
  or node limit. Sixty-second-substituted work was 660.825364 seconds.
- The longer cutoff gained and lost no completion relative to the corresponding five-second rows. Dense non-copositive cases solved
  at all three dimensions; dense boundary cases solved at 51 and 235 but not 168. Sparse non-copositive cases solved at 51 and 235
  but not 168. Every sparse boundary case and every strict case in both batches timed out. SQLite integrity returned `ok`.

## 2026-08-09 — Baseline model grouping and high-order Zischg hybrid run

- Moved the eight unchanged source and literature baselines—Dutour 2018, Danninger 1990, COPOMATRIX 2011, Hadeler 1983,
  Dickinson 2019, Safi 2021, Bundfuss 2008, and Sponsel 2012—under `models/baselines/`. Model identifiers and public binaries did
  not change. CMake accepts an optional source-directory argument for the existing Python-module helper; coposit-created models
  remain directly under `models/`. All 31 maintained tests passed after the move.
- Added validated `--matrix-id-from` and `--matrix-id-to` bounds to `python/run_results.py`, allowing exact targeted corpus runs
  without copying data or selecting unrelated matrices that share the same dimensions.
- Ran the maintained streak-10,000 Adaptive Zischg–Sponsel–COPOMATRIX binary, hash
  `7b510a73420787fd48c2286388a5cefd99a165941e8d7d4fb970c39f2d590b51`, on all 180 new matrices, IDs 10505–10684. The run used a
  five-second cutoff, parent CPU 3, and exactly two workers on CPUs 4 and 5. It completed in 375.164 seconds without interruption.
- The model solved 40 matrices, all not strictly copositive, and timed out on 140. It solved no strict case, reached a correct
  completion at the maximum tested order 2,997, and produced no mismatch, error, or node limit. The dense randomized batch supplied
  32 completions and 58 timeouts; the small-integer batch supplied 8 completions and 82 timeouts. Five-second-substituted work was
  705.413023 seconds, and SQLite integrity returned `ok`.
- Earlier six-worker attempts on these high-order inputs exhausted memory in Dutour and Danninger. Their partial stored rows remain
  incomplete and are not reference runs. A one-worker Danninger continuation avoided memory exhaustion but was stopped because it
  mostly produced five-second timeouts; no further baseline run was started.

## 2026-08-09 — Adaptive Zischg–Sponsel–COPOMATRIX streak-100,000 experiment

- Temporarily raised only the separate Zischg hybrid's branch-local cutoff from 10,000 to 100,000 and ran the standard five-second
  order-100 benchmark with parent CPU 3 and workers on CPUs 4–9. Hash
  `dbef25bcd36dd7c0f20dc90b7dd4a0af72d6a42b96eb195635282d4653f4598f` stores all 2,084 live-corpus rows: 2,028 correct
  completions, 56 timeouts, and no mismatch, error, or node limit.
- On the fixed 2,078-matrix reference snapshot through ID 10504, it solved 2,025 matrices—569 strict and 1,456 not strict—and timed
  out on 53, exactly matching the streak-10,000 outcome set. Five-second-substituted work was 359.189689 seconds, 0.039% below
  streak 10,000 and 0.0015% below streak 100. Relative to streak 10,000, median per-matrix time changed by -0.48% overall and -0.88%
  where both completed. The difference is too small to establish a useful speed improvement.
- The first dispatch encountered an unrelated SQLite writer lock after 1,355 stored rows; the runner resumed the same hash and
  completed the remaining 729 rows without discarding results. The maintained source, focused cutoff test, and `ALGORITHM.md` were
  restored to streak 10,000, reproducing native hash `7b510a73420787fd48c2286388a5cefd99a165941e8d7d4fb970c39f2d590b51`.
  All 31 maintained tests passed, and SQLite integrity returned `ok`.

## 2026-08-09 — Dense randomized high-order classification triplets

- Added another 90 matrices as IDs 10595–10684 at the same 30 irregular dimensions from 51 through 2,997, again with one copositive
  boundary, one strictly copositive, and one non-copositive case per dimension.
- Replaced the first batch's visible sparse-cycle pattern with unique seeded eight-coordinate column fingerprints and dense Gram
  products. At least 94.194% of every matrix's upper triangle is nonzero. The construction is deterministic pseudo-random rather
  than cryptographically random: its hidden PSD or positive-definite structure supplies exact proofs, while the failing cases have
  explicit two-coordinate negative witnesses. All entries satisfy the requested absolute bound of 100; the observed maximum is 60.
- Added `testdata/archive/import_high_order_dense_randomized_stress_2026_08_09.py`. Its verification mode regenerated all 369,334,597 matrix
  text bytes exactly. The corpus now contains 2,442 matrices: 674 strict and 1,768 not strict. SQLite integrity returned `ok`, and no
  result rows were invented for the new inputs.
- The order-100 reference snapshot still ends at ID 10504 and now excludes six later order-51 rows, three from each generated batch.

## 2026-08-09 — Adaptive Zischg–Sponsel–COPOMATRIX cutoff experiment

- Added complete standard five-second order-100 runs for streaks 1,000, 100, and 10 alongside the preserved streak-10,000 result.
  Each run used parent CPU 3, workers on CPUs 4–9, all 2,078 snapshot matrices through ID 10504, empty parameter text, and its own
  native hash. Every completion matched the snapshot truth; none of the 6,234 new rows produced an error or node limit.
- Streak 1,000 used hash `e09c1d9cbd582df7204798f3e3c55935ab9d24e6e4ba14358416e20a7b85d02e`, solved 2,024 matrices
  (568 strict and 1,456 not strict), timed out on 54, used 361.756635 seconds of substituted work, and took 63.770 seconds observed
  wall time.
- Streak 100 used hash `385e2d5606fe5f20cbbf98f91b9d50a1d7be526bd18ce387489bf331d85b782d`, solved 2,024 matrices
  (569 strict and 1,455 not strict), timed out on 54, used the experiment-low 359.195192 seconds of substituted work, and took
  63.540 seconds observed wall time.
- Streak 10 used hash `f0f9976fdabc2e481e66e6a5854e99437eff010ab8caf89a179475e9113d5763`, solved 2,020 matrices
  (569 strict and 1,451 not strict), timed out on 58, used 371.183324 seconds of substituted work, and took 65.129 seconds observed
  wall time.
- The preserved streak-10,000 configuration remains the maintained source state and the completion winner: 2,025 solved and 53
  timeouts. Relative to streak 100, it gained non-strict matrix 10274 without a loss for only 0.037% more substituted work. All four
  cutoffs retained the same original hard-26 split of six graph/lift completions and 20 Hildebrand timeouts. The reference report
  contains every identity, outcome, detailed cell, runtime, and comparison; SQLite integrity returned `ok`.

## 2026-08-09 — High-order small-integer classification triplets

- Added 90 deterministic matrices as IDs 10505–10594: one copositive boundary case, one strictly copositive case, and one
  non-copositive case at each of 30 deliberately irregular dimensions from 51 through 2,997. The exact dimension list begins
  51, 168, 235, 331 and ends 2,799, 2,908, 2,997.
- Used two seeded signed Hamiltonian cycles to form sparse Gram sums. The boundary cases are PSD direct sums with an exact
  two-coordinate nonnegative zero; the strict cases are positive definite; the failures retain positive diagonals and have an exact
  two-coordinate negative witness. Every entry has absolute value at most 10. Repeated small values are necessary at these orders,
  but every generated matrix is distinct.
- Added `testdata/archive/import_high_order_small_integer_stress_2026_08_09.py`; its verification mode regenerated all 278,382,884 matrix
  text bytes exactly. The corpus now has 2,352 matrices: 644 strict and 1,708 not strict. SQLite integrity returned `ok`, and no
  result rows were fabricated for the new inputs.
- Marked the order-100 reference report as a snapshot through ID 10504 because the new batch contains three unbenchmarked order-51
  matrices. Its existing 2,078-row result totals remain unchanged rather than silently treating missing runs as classifications.

## 2026-08-09 — Adaptive Zischg–Sponsel–COPOMATRIX order-100 reference run

- Ran all 2,078 retained matrices through dimension 100 with the standard five-second cutoff, parent CPU 3, and six persistent
  workers on CPUs 4–9. Native hash `7b510a73420787fd48c2286388a5cefd99a165941e8d7d4fb970c39f2d590b51` solved 2,025
  matrices—569 strict and 1,456 not strict—and timed out on 53. Every completion matched the corpus; there was no error or node
  limit. Observed dispatcher wall time was 63.246 seconds.
- Five-second-substituted work was 359.329316 seconds and reconstructed six-worker wall time was 59.888 seconds. Against the
  same-cutoff Adaptive Sponsel–COPOMATRIX model, projection-local Zischg decomposition gained not-strict matrix 10274 and strict
  matrix 10488 without a loss and reduced substituted work by 3.09%; both median comparisons were 0.00%.
- Against the current streak-1,000 base, it gained matrix 10274 without a loss and reduced substituted work by 0.47%; median time
  increased 1.98% overall and 2.88% where both completed. It retained the original hard-26 split of six graph/lift completions and
  20 Hildebrand timeouts and did not reduce the 42-matrix all-cone unresolved intersection. SQLite integrity returned `ok`.

## 2026-08-09 — Base Adaptive Sponsel–COPOMATRIX restored to the winning 1,000 cutoff

- Restored only `adaptive_sponsel_copomatrix` from streak 10,000 to streak 1,000 because the four preserved experiments showed that
  1,000 completed the most matrices and used the least five-second-substituted work. The existing streak-1,000 result identity is
  again the current reference selection; no corpus rerun was required.
- Kept the separate `adaptive_zischg_sponsel_copomatrix` model and its copied 10,000 cutoff unchanged.
- The rebuilt base native module reproduced stored hash `e93b85ab3c88e51be28fd5dd944bb1eb0cbe656b4b45ec5043998098852b64af`.
  All 31 maintained tests passed, and SQLite integrity returned `ok`.

## 2026-08-09 — Adaptive Zischg–Sponsel–COPOMATRIX

- Added `adaptive_zischg_sponsel_copomatrix` as an isolated copy of the finalized 10,000-streak Adaptive Sponsel–COPOMATRIX model.
- The new model applies exact Zischg–Bomze negative-entry connected-component decomposition only after COPOMATRIX constructs a
  principal, Schur, or transformed Schur child. Connected children restart the unchanged adaptive traversal; disconnected children
  are the conjunction of their proper principal component blocks. The public root and non-strict Sponsel children are not split.
- Registered the CLI, Python native module and public identifier, detailed algorithm document, and focused acceptance/rejection
  checks that confirm the production projection path invokes the decomposition. All 31 maintained tests passed and SQLite integrity
  returned `ok`. No corpus benchmark was run in this change.

## 2026-08-09 — Adaptive Sponsel–COPOMATRIX cutoff changed from 1,000 to 10,000

- Changed only the existing `adaptive_sponsel_copomatrix` branch-local Sponsel split limit from 1,000 to 10,000. No new algorithm,
  model directory, or public identifier was created. The implementation, focused cutoff check, authoritative algorithm document,
  and project overview now agree on 10,000; all 30 maintained C++ and Python tests passed.
- Preserved the complete streak-10, streak-100, and streak-1,000 result sets. The current streak-10,000 binary has hash
  `5a9d4bd44ca7372797f3d0d7e5653645b7f0e68aa5dfb9750796bf3c1bbae78a`; all four empty-parameter result sets use the same model ID
  and contain 2,078 independently addressable rows.
- The standard five-second run used parent CPU 3 and workers 4–9. Streak 10,000 solved 2,023 matrices—568 strict and 1,455 not
  strict—and timed out on 55, with no mismatch, error, or node limit in 65.900 seconds observed wall time. Five-second-substituted
  work was 370.792344 seconds.
- Relative to streak 1,000, streak 10,000 gained no completion, lost strict lift 10488, and increased substituted work 2.70%.
  Median substituted time increased 4.75% over all matrices and 6.00% where both completed. The current ten-cone unresolved
  intersection rose from 42 to 43, while all four cutoffs solved the same six graph/lift members of the original hard 26.

## 2026-08-09 — Adaptive Sponsel–COPOMATRIX cutoff changed from 10 to 1,000

- Changed only the existing `adaptive_sponsel_copomatrix` branch-local Sponsel split limit from 10 to 1,000. No new algorithm,
  model directory, or public identifier was created. The implementation, focused cutoff check, authoritative algorithm document,
  and project overview now agree on 1,000; all 30 maintained C++ and Python tests passed.
- Preserved the complete streak-10 and streak-100 result sets. The current streak-1,000 binary has hash
  `e93b85ab3c88e51be28fd5dd944bb1eb0cbe656b4b45ec5043998098852b64af`; all three empty-parameter result sets use the same model ID
  and contain 2,078 independently addressable rows.
- The standard five-second run used parent CPU 3 and workers 4–9. Streak 1,000 solved 2,024 matrices—569 strict and 1,455 not
  strict—and timed out on 54, with no mismatch, error, or node limit in 63.634 seconds observed wall time. Five-second-substituted
  work was 361.041983 seconds.
- Relative to streak 100, streak 1,000 gained strict lift 10487, lost no completion, and reduced substituted work 0.97%. Relative to
  streak 10, it completed five more matrices and reduced substituted work 3.08%. The current ten-cone unresolved intersection fell
  from 43 to 42, while all three cutoffs solved the same six graph/lift members of the original hard 26. SQLite integrity returned
  `ok`.

## 2026-08-09 — Adaptive Sponsel–COPOMATRIX cutoff changed from 100 to 10

- Changed only the existing `adaptive_sponsel_copomatrix` branch-local Sponsel split limit from 100 to 10. No second algorithm,
  model directory, or public identifier was created. The current implementation, focused cutoff check, authoritative algorithm
  document, and project overview now agree on 10; all 30 maintained C++ and Python tests passed.
- Preserved the streak-100 empty-parameter result set under hash
  `c424121ee78f2f5ccb30eb45650c23499133e6644bd8f718e16dfcb6851b8e5c`. The streak-10 build has hash
  `079a491aa7b72c162ca5c5af48da4f3d22c4062c63de748a8bbc218584c049d9`; the shared model identifier and different hashes keep both
  complete 2,078-row result sets independently addressable.
- The standard five-second run used parent CPU 3 and workers 4–9. Streak 10 solved 2,019 matrices—568 strict and 1,451 not strict—and
  timed out on 59, with no mismatch, error, or node limit in 65.579 seconds observed wall time. Five-second-substituted work was
  372.529447 seconds.
- Relative to streak 100, streak 10 gained two completions, lost six, completed four fewer overall, and increased substituted work
  2.18%. Median substituted time increased 3.26% over all inputs and 4.44% where both completed. The ten-cone unresolved intersection
  remained 43, and both settings solved the same six graph/lift members of the original hard 26. SQLite integrity returned `ok`.

## 2026-08-09 — Thirty-second retry of Adaptive Sponsel–COPOMATRIX timeouts

- Repeated exactly the 55 five-second timeouts from the order-100 Adaptive Sponsel–COPOMATRIX run with a 30-second per-matrix cutoff,
  parent CPU 3, and workers 4–9. The separate parameter set `five_second_timeouts_30s_2026-08-09` preserves the original rows.
- Hash `c424121ee78f2f5ccb30eb45650c23499133e6644bd8f718e16dfcb6851b8e5c` solved 14 matrices: all 12 high-order
  Dannenberg-Schürmann exceptional lifts, Hildebrand matrix 10274, and Johnson matrix 9168. These were 13 strict completions and one
  not-strict completion; all matched the corpus.
- The remaining 41 timeouts comprise 33 Hildebrand, four Hamming, three Johnson, and one MANN matrix. No row reached a node limit or
  error. Observed wall time was 226.706 seconds, 30-second-substituted work was 1,327.178402 seconds, and database integrity returned
  `ok`.

## 2026-08-09 — Adaptive Sponsel–COPOMATRIX order-100 reference run

- Ran all 2,078 retained matrices through dimension 100 with the standard five-second cutoff, parent CPU 3, and six persistent
  workers on CPUs 4–9. Native hash `c424121ee78f2f5ccb30eb45650c23499133e6644bd8f718e16dfcb6851b8e5c` solved 2,023 matrices:
  568 strict and 1,455 not strict. The remaining 55 timed out; there was no mismatch, error, or node limit.
- Observed wall time was 64.285 seconds. Five-second-substituted work was 364.590928 seconds, 75.69% below Sponsel and 53.77% below
  COPOMATRIX. The hybrid retained every Sponsel completion and added 237; relative to COPOMATRIX it gained 105 completions and lost
  nine.
- Adding the hybrid reduced the cumulative intersection unresolved by every selected cone algorithm from 116 to 43 matrices. On the
  original 26-case hard set it solved all six graph/lift matrices and timed out on all 20 Hildebrand circulants. The full report and
  all derived tables were updated, and SQLite integrity returned `ok`.

## 2026-08-09 — Survivor-only order-100 reference recalculation

- Replaced the pre-deduplication values in `aidocs/REFERENCE_RESULTS_N_1_TO_100.md` by recalculating every full-corpus table from the
  existing stored rows for the 2,078 retained matrices through dimension 100. No solver was rerun.
- All 23 selected result identities still cover every retained matrix and contain no classification mismatch: 586 matrices are
  strict and 1,492 are not strict. Counts, percentages, detailed outcome cells, cumulative unresolved totals, completion deltas,
  substituted work, reconstructed wall time, and per-matrix median comparisons were recomputed from those survivor rows.
- For reproducibility, substituted work charges each unresolved row its stored five-second cutoff; reconstructed wall time divides
  that work by the original run's worker count. The 26-matrix targeted appendix was rechecked separately and is unchanged because
  none of its matrix IDs was removed. The recalculated report is current again.

## 2026-08-09 — Whole-matrix projective-permutation corpus deduplication

- Audited all 2,417 maintained matrices with exact integer arithmetic under `B=cP^TAP`, where one positive scalar multiplies the
  entire matrix and `P` is one simultaneous row-and-column permutation. Fifty-eight equivalence classes contained 213 database
  matrices; keeping the lowest stable ID in every class removed 155 redundant whole matrices.
- The removed set comprised 154 FracESSA-derived matrices and Dickinson-de Zeeuw ID 10132, which is a pure permutation of
  Kostyukova-Tchemisova ID 9957. Every removed origin and family was merged into its representative. The associated 5,111 result
  records were deleted by the existing foreign-key cascade rather than reassigned to a different exact input.
- The maintained corpus now contains 2,262 matrices: 614 strict and 1,648 not strict. Dimensions 1 through 100 contain 2,078 matrices,
  of which 586 are strict. A fresh full audit found zero remaining positive-scale/permutation duplicate classes; SQLite integrity and
  foreign-key checks passed.
- Positive diagonal congruences with nonconstant diagonal factors remain intentional distinct inputs. In particular, FracESSA IDs 61
  and 731 remain present and unchanged. The former 2,233-matrix order-100 reference report is labeled as a historical pre-deduplication
  snapshot rather than presenting its timings as measurements of the retained representatives.

## 2026-08-09 — Adaptive Sponsel–COPOMATRIX with forced progress

- Added `adaptive_sponsel_copomatrix` as an isolated coposit-created model. It takes the first COPOMATRIX pivot with at most two
  children; otherwise it applies Sponsel's exact strict `H` certificate and inherited Bundfuss split.
- Each branch forces COPOMATRIX at pivot zero after 100 consecutive Sponsel splits and resets the counter after every lower-order
  projection child. This bounds same-order subdivision and gives the hybrid a finite mathematical recursion tree.
- Registered the CLI, Python native module, public identifier, authoritative algorithm document, and focused checks.

## 2026-08-09 — Adaptive Dutour–COPOMATRIX with forced progress

- Added `adaptive_dutour_copomatrix` as an isolated coposit-created model combining exact maximum-ratio Dutour subdivision with the
  Xu–Yao COPOMATRIX projection.
- The adaptive gate takes the first COPOMATRIX pivot with at most two immediate children. Otherwise it uses Dutour, but forces
  COPOMATRIX at pivot zero after 100 consecutive same-order Dutour splits on each branch and resets the counter after every order
  reduction.
- Registered the CLI, Python native module, public Python identifier, and focused model checks. The finite cutoff supplies a genuine
  termination argument while retaining the cheap two-child Dutour route between projections.
- Completed the standard five-second order-1-through-100 reference run with parent CPU 3 and workers 4–9. The model solved 2,082 of
  2,233 matrices (489 strict and 1,593 not strict), timed out on 151, and produced no mismatch, error, or node limit. Its 133.140-second
  wall time and complete detailed results are recorded in `aidocs/REFERENCE_RESULTS_N_1_TO_100.md`; database integrity returned `ok`.

## 2026-08-09 — Thirty-second original-cone run on the 26 bad matrices

- Repeated the 26 matrices unsolved by every selected five-second cone run using only the six maintained historical or source-derived
  cone baselines: Dutour 2018, Danninger 1990, COPOMATRIX 2011, Safi 2021, Bundfuss 2008, and Sponsel 2012. coposit-created Adaptive
  Dutour-Danninger and Frank–Wolfe Sponsel were deliberately excluded.
- The run used a 30-second per-matrix cutoff, parent CPU 3, workers 4–9, and parameters
  `bad26_original_cone_30s_2026-08-09`. All 156 rows are stored in the maintained database; every completion matched the corpus and
  database integrity returned `ok`, with no error or node-limit result.
- COPOMATRIX solved Hildebrand matrices 10276 and 10278 and strict lift 10420. Sponsel solved strict graph matrices 9171 and 9175.
  Dutour, Danninger, Safi, and Bundfuss solved none. The longer cutoff therefore rescued five distinct matrices and left 21
  unresolved by all six baselines. Exact hashes, timings, aggregate counts, and all matrix-level outcomes are in
  `aidocs/REFERENCE_RESULTS_BAD26_ORIGINAL_CONE_30S.md`.

## 2026-08-09 — Nullity-aware Support-Pruned Dickinson

- Added `nullity_support_pruned_dickinson` as an isolated copy of `support_pruned_dickinson`. The nonsingular branch, certificate
  theorem, strict-zero termination, recursive generator, non-strict interval lookup, and globally covered-support prune are unchanged.
- Extended the shared fraction-free LDLT factorization with complete exact nullspace-basis recovery from the retained singular
  factors. Existing one-vector callers remain unchanged. A focused rank sweep verified that every returned basis has the exact
  requested nullity, independent columns, and zero matrix product.
- Singular supports now select coverage deliberately: both signs at nullity one; every exact sign interval, breakpoint, both signs,
  and the direction at infinity at nullity two; and both signs of every exact LDLT basis column at larger nullity. Candidates maximize
  the exact number of covered supports at strictly larger cardinalities, then interval width and upper-set size.
- Added a model-local mathematical specification and focused checks that prove the better nullity-one orientation, the globally
  optimal nullity-two signature on a constructed matrix, and the best-signed-basis rule above nullity two. The complete Release build
  and all 28 C++/Python tests passed; 1,600 deterministic random symmetric integer matrices through order eight matched
  `support_pruned_dickinson`.
- The clean five-second order-100 run used parent CPU 3 and workers 4–9. Hash
  `247259ce36461b297ef6c33fc422fddfc122836ccba7cee10539f83000844e71` covered all 2,233 matrices, solved 2,062, and timed out on
  171 with no mismatch, error, or node limit in 150.366 seconds observed wall time. Database integrity returned `ok`.
- Its completion set exactly matched Support-Pruned Dickinson. Relative to that model, five-second-substituted work increased 0.09%
  and observed wall time increased 0.18%, while the median per-matrix time improved 0.67% over all inputs and 4.35% among inputs both
  completed. Stronger singular-certificate selection reduced typical completed-case time but did not improve the aggregate cutoff
  result or rescue another matrix.

## 2026-08-09 — Support-Pruned Dickinson hybrid

- Added `support_pruned_dickinson` as a separate self-contained coposit-created model, leaving `dickinson_2019` unchanged. It retains
  Dickinson's exact solves, nullspace branch, strict-zero termination, and general coverage intervals. When a retained signature has
  `N_A(u)` equal to the complete index universe, the copied FracESSA support generator now removes every later branch containing
  `support(u)` before that support is emitted. Signatures with a proper upper bound remain non-strict Dickinson lookup entries.
- Added a detailed model-local `ALGORITHM.md` and focused checks for global upward pruning, bounded-interval safety, singular vectors,
  arbitrary-precision scaling, boundary matrix 9161, packed supports beyond 64 coordinates, and public validation. The complete
  Release build and all 27 C++/Python tests passed; 1,600 deterministic random matrices through order eight matched base Dickinson.
- The clean five-second order-100 run used parent CPU 3 and workers 4–9. Hash
  `b68989630ecc0af715fe22e13143a7b78569523418f0fd46dcc9acba9f02b35c` covered all 2,233 matrices, solved 2,062, and timed out on
  171 with no mismatch, error, or node limit in 150.091 seconds observed wall time. Database integrity returned `ok`.
- Relative to Dickinson, the hybrid had the same solved total but exchanged two near-cutoff strict outcomes: Johnson matrix 9627
  completed in 3.289 seconds and Dannenberg-Schürmann lift 10428 timed out. Five-second-substituted work increased 0.37%, direct wall
  time increased 0.36%, and the median change on 2,060 comparable completed timings was a 5.47% slowdown. The safe prune therefore
  did not improve aggregate performance on this corpus.
- Repeated the targeted 26 matrices solved by no five-second cone model under parameters
  `bad26_strict_zero_5s_2026-08-09`. Support-Pruned Dickinson solved 15 and timed out on 11, exactly matching Dickinson's family cells
  and Hildebrand cutoff. The expanded 338-row table contains 186 correct completions and 152 timeouts with no mismatch, error, or
  node limit. Full cells, hashes, cumulative cutoffs, and timing comparisons are in `aidocs/REFERENCE_RESULTS_N_1_TO_100.md`.

## 2026-08-09 — Immediate strict-zero termination in every Dickinson model

- Changed all eight maintained Dickinson-named models to decide only the strict question along their Dickinson path. When the exact
  singular branch produces a one-signed null vector, it now returns `false` immediately before constructing a coverage signature or
  full matrix product. Any later exact nonnegative-zero check also returns immediately. A zero-free completed certificate still
  proves `true` through Dickinson's Lemma 5.2 and Corollary 5.3.
- Applied the control-flow change independently to `dickinson_2019`, `rhs_dickinson`, `frank_wolfe_dickinson`,
  `one_step_frank_wolfe_dickinson`, `exact_frank_wolfe_dickinson`, `pairwise_frank_wolfe_dickinson`,
  `support_polished_frank_wolfe_dickinson`, and `zischg_dickinson`. Removed the redundant quadratic-zero calculation from the
  non-strict positive-right-hand-side paths; RHS Dickinson retains its general exact post-solve check because it searches modified
  right-hand sides.
- The complete Release build and all 26 C++/Python tests passed. Eight clean five-second runs used parent CPU 3 and workers on CPUs
  4–9. Every run covered all 2,233 matrices with no mismatch, error, or node limit; database integrity returned `ok`.
- Final solved/timeout counts and observed wall times were: Dickinson 2,062/171 in 149.557 s; RHS Dickinson 2,057/176 in 154.221 s;
  Frank–Wolfe Dickinson 2,065/168 in 147.402 s; Exact One-Step 2,063/170 in 149.463 s; Exact Multi-Step 2,066/167 in 147.031 s;
  Pairwise 2,066/167 in 147.184 s; Support-Polished 2,067/166 in 147.055 s; and Zischg–Dickinson 2,059/174 in 152.405 s.
- Against each preceding hash, every affected model's five-second-substituted worker total decreased, from 0.14% for Frank–Wolfe
  Dickinson to 2.10% for Exact One-Step. Base Dickinson newly completed non-strict Hamming matrix 9613 in 15.095 ms and Johnson
  matrix 9628 in 1.202 ms; strict matrix 10331 crossed the cutoff in the opposite direction. Full hashes, detailed cells, current
  comparisons, and prior-hash deltas are in `aidocs/REFERENCE_RESULTS_N_1_TO_100.md`.
- Repeated the 26 matrices solved by no five-second cone model as a separate targeted experiment using the same five-second cutoff,
  parent CPU 3, and workers 4–9. Parameters `bad26_strict_zero_5s_2026-08-09` preserve all 312 rows independently: 171 correct
  completions, 141 timeouts, and no mismatch, error, or node limit. Immediate zero termination did not move the Hildebrand cutoff;
  the updated compact table now includes all Dickinson, Hadeler, FracESSA, and Zischg counterparts.

## 2026-08-09 — Zischg Level 2 support variants

- Added independent `zischg_hadeler`, `zischg_dickinson`, and `zischg_fracessa` models. They build the exact packed negative-entry
  graph once and apply the locally derived Zischg Level 2 rule inside each model's cardinality-ordered traversal. They do not perform
  the Level 1 complete-matrix split. Disconnected supports above order three skip the model's factorization, certificate, or KKT work.
- Added reusable packed-support union, intersection, removal, clearing, and swapping operations. Each model keeps its own graph and
  traversal policy. Complete negative graphs and supports whose root reaches every other selected vertex take constant-control-flow
  shortcuts before the reusable packed BFS.
- Corrected the FracESSA integration after an aborted provisional run exposed five strict matrices with no accepted connected KKT
  candidate. That is a valid strict outcome: a disconnected support may maximize at a negative value, while every nonnegative
  failure has a connected-support witness. Matrix 9371 now protects this invariant, and only the aborted model hash's rows were
  removed before the final clean runs.
- The complete Release build and all 26 C++/Python tests passed. Final five-second runs used parent CPU 3 and workers on CPUs 4–9;
  all three covered 2,233 matrices with no mismatch, error, or node limit. `zischg_hadeler` hash
  `91bbc1349d328c719a9129993f701afde53d31325e4cc1bc8636ab64e982de55` solved 2,048 and timed out on 185 in 164.592 seconds.
  `zischg_dickinson` hash `d88a00ce2831b79bc5e646399af268c65e2f5ab3a4c86b207abc36afc6b662e3` solved 2,057 and timed out on 176 in
  154.048 seconds. `zischg_fracessa` hash `2deb23a21b7fe34b291be324ca9e1f8b1d059c5a7a0af9af0670547a4521b4e8` solved 2,058 and
  timed out on 175 in 155.550 seconds. Database integrity returned `ok`.
- Against their base models with five seconds substituted for timeouts, Zischg–Hadeler reduced summed work 0.946% and rescued three
  strict completions without a loss; Zischg–Dickinson increased summed work 1.969% and lost four near-cutoff strict completions;
  Zischg–FracESSA reduced summed work 2.943% and rescued six strict completions without a loss.

## 2026-08-08 — Exact multi-step Frank–Wolfe Dickinson

- Added `exact_frank_wolfe_dickinson` as a separate self-contained coposit model. It keeps the bounded centre-plus-seven-start,
  64-step search and Dickinson fallback, but represents every iterate by homogeneous nonnegative integers and performs every line
  minimization exactly. It has no floating arithmetic, tolerance, or rational reconstruction.
- Capped a proposed homogeneous denominator at 4,096 bits. Crossing the cap only ends that Frank–Wolfe start and continues with the
  next start or exact Dickinson, so it cannot change a classification. The first uncapped trial already timed out on strict order-four
  inputs because exact denominator growth dominated the search; that run was stopped after 417 rows and 43 timeouts, and its partial
  obsolete-hash rows were removed before the final run.
- Added focused one-step, genuine multi-step, arbitrary-precision scaling, fallback, dimension, and validation checks. The
  warning-as-error compile was clean; the complete Release build and all 23 C++/Python tests passed.
- The fresh five-second order-100 run used parent CPU 3 and six workers on CPUs 4–9. Hash
  `f5e1fdf5afb6ab18a7ed3d37939acbf40aece2bf0c47fe8c273317e768369080` solved 2,065 matrices and timed out on 168, with no
  mismatch, error, or node limit, in 147.270 seconds observed wall time. The database integrity check returned `ok`.
- Relative to exact one-step, it solved six additional non-strict matrices and lost none; five-second-substituted work fell 4.13%,
  although the median matrix was 48.81% slower overall and 75.90% slower where both completed. Relative to floating multi-step, it
  solved the same total but exchanged two near-cutoff strict completions for two exact non-strict witnesses; substituted work fell
  0.99%, while the corresponding median slowdowns were 66.80% and 100.00%.

## 2026-08-08 — Exact one-step Frank–Wolfe Dickinson variant

- Added `one_step_frank_wolfe_dickinson` as a separate self-contained coposit model, leaving `dickinson_2019` unchanged. It starts
  at the simplex centre, chooses the first minimum-row-sum vertex, minimizes that one line exactly, and otherwise runs the unchanged
  Dickinson certificate traversal.
- Kept the front end integer-only. The rational minimizer is represented by positive homogeneous weights
  `z = (q-p)1 + np e_j`; rejection requires an exact FLINT evaluation with `z^T A z <= 0`. There is no floating arithmetic,
  tolerance, reconstruction, iteration, or restart.
- The focused exact-path, fallback, arbitrary-precision, dimension, and validation checks passed; the warning-as-error syntax check
  was clean; and the complete Release build and all 22 C++/Python tests passed.
- The fresh five-second order-100 run used parent CPU 3 and six workers on CPUs 4–9. Hash
  `03e3ef005a20aefafc01447b9a77192aba4aeabe7b1c1470726be7cf9dbb89f9` solved 2,059 matrices and timed out on 174, with no
  mismatch, error, or node limit, in 152.925 seconds observed wall time. It rescued no Dickinson timeout and lost the two strict
  near-cutoff completions 10428 and 10331. With five seconds substituted for timeouts, summed work was 893.890245 seconds, 0.91%
  above Dickinson; the median per-matrix difference was 10.93% faster overall and 19.39% faster where both completed.

## 2026-08-08 — Exact one-step Frank–Wolfe Sponsel variant

- Added `frank_wolfe_sponsel` as a separate coposit-created model, leaving `sponsel_2012` unchanged. Every node that is not accepted
  by the strict Sponsel `H` certificate receives one exact Frank–Wolfe line minimization from its simplex centre toward the
  minimum-row-sum vertex before the inherited Bundfuss split.
- Kept the new rejection entirely rational and exact. Row sums give the direction and closed-form step; homogeneity turns the line
  point into integer weights, and FLINT rejects only when their quadratic value is nonpositive. No floating representation,
  tolerance, reconstruction, iteration, or restart was added.
- Added a focused four-dimensional case whose centre and selected two-generator restriction are positive but whose exact line point
  has value `-100`, plus arbitrary-precision scaling, fallback, dimension, and validation checks.
- The complete Release build and all 21 C++/Python tests passed. The fresh five-second reference run covered all 2,233 matrices through
  order 100 using parent CPU 3 and six workers on CPUs 4–9: 1,943 solved, 290 timed out, no errors, no node limits, no classification
  mismatches, and 253.113 seconds observed wall time. It completed two order-three matrices that `sponsel_2012` timed out on and lost
  no Sponsel completion. The database integrity check returned `ok`; full results are in `aidocs/REFERENCE_RESULTS_N_1_TO_100.md`.

## 2026-08-08 — Pairwise-away Frank–Wolfe Dickinson

- Added `pairwise_frank_wolfe_dickinson` as a separate self-contained coposit model. It replaces only the bounded proposal step:
  the global minimum-product coordinate is the toward vertex, the maximum-product positive coordinate is the away vertex, and the
  feasible bound transfers at most the away coordinate's current mass. The centre, seven deterministic vertex starts,
  64-step limit, exact integer reconstruction, and complete Dickinson fallback remain unchanged.
- Kept the pairwise search one-sided and safe. Floating arithmetic only proposes a vector; every rejection has an exactly evaluated
  nonzero integer witness `z >= 0` with `z^T A z <= 0`. A focused higher-order witness check, arbitrary-precision scaling check,
  warning-enabled build, Python wrapper check, and all 21 current Release tests passed.
- Ran all 2,233 matrices through order 100 with the five-second limit, parent CPU 3, and six workers on CPUs 4–9. Native hash
  `09deeee3467def72543e9358daedcdd4dfaf8caf1fca7b438f9a220a153ff833` completed 2,065 matrices and timed out on 168, with no
  disagreement, error, or node limit; the final uncontended replacement run took 147.586 seconds and database integrity passed.
- Relative to optimized `frank_wolfe_dickinson`, the pairwise rule converted non-strict Hamming matrix 9610 (`n=64`) from a timeout
  to a correct 0.353 ms completion, while strict Dannenberg–Schürmann lift 10331 (`n=29`) moved from a 4.864-second completion to a
  timeout. With five seconds substituted for timeouts, summed work fell 0.44% to 861.776250 seconds; the median per-matrix timing
  difference was 0.00%, both overall and on the 2,064 matrices both models completed.

## 2026-08-08 — Exact support-polished Frank–Wolfe Dickinson

- Added `support_polished_frank_wolfe_dickinson` as a separate self-contained coposit model. After a best normalized Frank–Wolfe
  value at most `1e-12` fails exact dyadic reconstruction, it retains every positive finite pre-rounding coordinate and tests that
  one principal support exactly. Supports through order three use the shared direct formulas; larger nonsingular supports use one
  fraction-free KKT solve, and singular supports use one exact kernel vector. Only an exact negative vector or nonnegative zero can
  reject, and an inconclusive test falls through to the unchanged Dickinson traversal.
- Added a focused four-dimensional case whose essential coordinate is smaller than the `2^-40` reconstruction grid. Dyadic
  reconstruction loses that coordinate, while the exact two-coordinate support test recovers the negative witness. The complete
  1,412-matrix order-10 check had no disagreement, the warning-enabled compile passed, and all 21 Release tests passed.
- Ran all 2,233 matrices through order 100 with the five-second limit, parent CPU 3, and six workers on CPUs 4–9. Native hash
  `ed5b9f497a86a44fa93aee8363684afc8e9dfd9a8f87c6565efd9c0022c91c44` completed 2,065 matrices and timed out on 168, with no
  disagreement, error, or node limit; the final uncontended replacement run took 147.093 seconds and database integrity passed.
- Relative to optimized `frank_wolfe_dickinson`, polishing rescued non-strict Hamming matrix 9610 (`n=64`) and Johnson matrix 9631
  (`n=70`) in under 0.5 ms each, while two strict Dannenberg–Schürmann lifts that had finished near five seconds timed out. Summed
  five-second-substituted work was 858.574667 seconds: 0.81% below Frank–Wolfe Dickinson and 3.08% below Dickinson 2019.

## 2026-08-08 — Frank–Wolfe proposal-path optimization

- Kept the bounded centre-plus-seven-start, 64-step search unchanged. A simulated dimension cutoff saved only 0.02% of the old
  summed five-second order-100 result and was rejected as noise rather than added as another policy constant.
- Moved the exact coordinate witness test ahead of floating allocation, formed the centre row sums from one symmetric triangle,
  cached normalized diagonal entries, and allocated exact reconstruction weights only if a floating candidate needs verification.
  No exact decision, restart, iteration, or Dickinson fallback rule changed.
- On all 1,412 matrices through dimension 10, five alternating single-CPU repetitions reduced the Frank–Wolfe Dickinson median
  summed native time from 52.340 ms to 49.431 ms; a repeated post-change run measured 50.960 ms. All classifications matched.
- Re-ran all 2,417 matrices with the same five-second limit and CPU placement. Hash
  `8f1143df6ac87744650f543dd12770e2267a2e20d8df368609beacae1abd6b3d` completed 2,180 matrices, timed out on 237, and had no
  disagreement, error, or node limit. It converted the prior timeout for strict order-29 matrix 10331 into a correct completion at
  4.864 seconds. Wall time was 205.931 seconds, and the database integrity check passed.

## 2026-08-08 — Frank–Wolfe Dickinson five-second full result set

- Ran all 2,417 maintained matrices with `frank_wolfe_dickinson`, a five-second cooperative timeout, the parent dispatcher and
  database writer pinned to CPU 3, and six persistent workers pinned to CPUs 4–9. The canonical empty parameter string and native
  module hash `5a42e3f456d947701313f1e9616b8a8439788eb7b549248817b9d5fc3624e1e6` identify the stored result set.
- Completed 2,179 matrices and recorded 238 timeouts as unresolved. Completed results comprise 451 strict and 1,728 non-strict
  matrices; the timeouts comprise 177 expected-strict and 61 expected-non-strict matrices. There were no classification
  disagreements, errors, or node-limit results. Wall time was 206.358 seconds and the slowest completed matrix took 4.756 seconds.

## 2026-08-08 — Frank–Wolfe witness front end for Dickinson

- Added `frank_wolfe_dickinson` as a separate coposit-created strict model, leaving `dickinson_2019` unchanged. It runs a bounded
  Frank–Wolfe simplex search from the centre and up to seven smallest-diagonal vertices, with at most 64 closed-form line steps per
  start, then falls back to the complete Dickinson certificate traversal.
- Kept the proposal phase floating and the decision exact. One global power-of-two scale handles arbitrary-size input integers
  without materializing another dense matrix. Every proposed counterexample is reconstructed as a nonzero nonnegative integer
  vector and its quadratic value is verified with FLINT before rejection.
- The focused check uses a five-dimensional matrix whose all-ones value is positive and whose principal faces through order three
  are strictly copositive; a genuine Frank–Wolfe move finds its exact negative witness. The arbitrary-precision-scaled version takes
  the same branch.
- Across all 1,412 matrices through dimension 10, the front end exactly rejected 789 of the 1,033 non-strict matrices and every
  classification matched the corpus. Five alternating single-CPU Release repetitions gave median summed native times of 35.301 ms
  for `dickinson_2019` and 52.590 ms for `frank_wolfe_dickinson`; median wall times were 38.435 ms and 55.831 ms.
- With identical one-second cooperative limits over all 2,417 matrices on six pinned workers, Dickinson completed 2,164 and timed
  out on 253. Frank–Wolfe Dickinson completed 2,173 and timed out on 244. It rescued nine non-strict Hamming or Johnson matrices
  (IDs 9611, 9613, 9619, 9620, 9628, 9632, 9635, 9637, and 9638), lost no Dickinson completion, and produced no classification
  disagreement or other failure status.

## 2026-08-08 — Exact right-hand-side Dickinson experiment

- Removed the maintained `generalized_dickinson` pair-sum model, all build and Python wiring, and its algorithm document after its
  conic-combination scan proved substantially slower than the unchanged Dickinson baseline. The prior entry below remains as the
  historical reason for the removal.
- Added `rhs_dickinson` as a separate coposit-created experiment. For each uncovered nonsingular support above order one, it sweeps
  the exact rays `1 + t e_k`, evaluates every rational sign breakpoint and intervening interval, and retains the one vector with the
  widest Dickinson coverage interval.
- Added a focused branch check using strict corpus matrix 9259. Across the 1,412 matrices through dimension 10, the RHS model selected
  a nonconstant right-hand side in 315 matrices and for 2,869 generated certificates; all classifications matched the corpus.
- Five alternating single-CPU Release repetitions over those 1,412 matrices gave median summed native times of 41.236 ms for
  `dickinson_2019` and 173.978 ms for `rhs_dickinson`; median wall times were 45.450 ms and 178.329 ms.
- With identical one-second cooperative limits over all 2,417 matrices on six pinned workers, `dickinson_2019` completed 2,164 and
  timed out on 253. `rhs_dickinson` completed 2,159 and timed out on 258. It rescued no Dickinson timeout and lost five order-13/14
  completions; every completed result matched the corpus and neither model produced another failure status.

## 2026-08-08 — Generalized Dickinson pair certificates

- Added `generalized_dickinson` as a separate coposit-created model, leaving the published `dickinson_2019` baseline unchanged. When
  non-strict coverage fails, it tests exact equal-weight sums of eligible earlier certificate vectors and accepts only sums satisfying
  Dickinson's original support and product conditions.
- Retained sparse parent values and full exact products so product-sign cancellation is decided without approximation. Successful
  sums record strict nonnegative zeros and cache only packed coverage signatures; they do not become recursive combination parents.
- Added a focused branch check using strict corpus matrix 9159. The full dimension-1-through-10 corpus comparison classified all
  1,412 matrices correctly for both models.
- Five alternating single-CPU Release repetitions over those 1,412 matrices gave median summed native times of 34.741 ms for
  `dickinson_2019` and 314.514 ms for `generalized_dickinson`; median wall times were 37.883 ms and 317.851 ms. The exact pair scan is
  therefore correct but substantially slower on this corpus.

## 2026-08-08 — Shared exact low-order strict test

- Extracted Hadeler's exact order-one, order-two, and order-three strict-copositivity formulas into the model-independent
  `cpp/include/coposit/small_strict_copositivity.hpp`. The indexed entry point checks a principal matrix without allocating a copy.
- Routed Hadeler through the shared formulas without changing its traversal. Dickinson now applies them before certificate coverage
  on every visited subset through order three; a pass continues through normal Dickinson construction. FracESSA applies them before
  KKT work on every generated support through order three; a pass continues through its normal candidate and pruning tests.
- Added a focused shared test. The complete Release build and all 16 tests passed, including higher-dimensional Hadeler, Dickinson,
  and FracESSA cases that exercise low-order rejection. A read-only comparison of all 1,412 corpus matrices through order 10 found
  zero mismatches for each of the three rebuilt native modules.

## 2026-08-08 — One-vector singular LDLT recovery

- Extended the shared fraction-free LDLT factorization to retain exact rank and recover one exact integer kernel vector from a
  singular partial factorization. It performs one triangular back-substitution and constructs no nullspace basis.
- Replaced Hadeler's restored full-nullspace calculation with exact nullity from LDLT and one vector only for nullity one. Replaced
  Dickinson's full FLINT nullspace with one LDLT-derived vector for every positive nullity, as permitted by Algorithm 2.
- Added shared checks across all singular ranks through dimension 12, including coordinate swaps and additions.
- Across three repetitions on all 1,412 corpus matrices through dimension 10, median summed native time fell from 37.890 to 32.465 ms
  for Hadeler (14.3%) and from 54.083 to 49.942 ms for Dickinson (7.7%), with no classification mismatch.

## 2026-08-07 — Initial standalone extraction

- Copied the exact integer core, 1,569-matrix SQLite corpus, extraction handoff, mathematical research, retained experiments, and
  FracESSA integration references without modifying the FracESSA worktree.
- Established an integer-only C++17 API and stdin/file CLI. Removed the FracESSA fraction-matrix dependency.
- Deliberately omitted connected-component decomposition from the maintained path until the base checker is established. Its proof,
  benchmark, and reference implementation remain preserved.
- Release configuration, compilation, focused tests, CLI smoke test, database integrity check, and byte-for-byte corpus comparison
  passed on 2026-08-07.

## 2026-08-07 — Self-contained model layout

- Established `models/<model-name>/` as the home of complete, intentionally duplicated solver implementations.
- Limited shared C++ code to exact storage, parsing, the one-function model contract, and the one-matrix executable protocol.
- Made copying the closest existing model the normal starting point for a new variant. Cross-model algorithm deduplication now
  requires an explicit decision that the code is stable project-wide infrastructure.

## 2026-08-07 — Provenance-safe baseline naming

- Renamed the generic `cone` model to `dutour_2018`: one author name plus the year its pair decomposition and strict-copositivity
  traversal entered Polyhedral Common. The model records the pinned 2026 source revision separately.
- Established author/year identifiers exclusively for faithful historical or external baselines. coposit-created mathematical
  variants require separate descriptive names so correctness and runtime comparisons cannot silently mix originals with hybrids.
- Removed the FracESSA low-dimensional and sign-certificate additions from this baseline. FLINT integers, Gram-only state,
  fraction-free ratio comparison, and exact row-and-column updates remain as non-mathematical implementation optimizations.
- Standardized the user-facing program name and binary as `coposit` and `coposit`.

## 2026-08-07 — Faithful Bundfuss baseline

- Added `bundfuss_2008` as the exact baseline for Bundfuss and Dür's 2008 simplicial-partition model, with formulas and traversal
  pinned to J. M. G. Salmerón's preserved 2018 plain `bundfuss` implementation.
- Kept the maintained code independent of the unlicensed upstream source. Positive integer-scaled Gram matrices reproduce the
  rational convex splits without adding rational matrix storage, floating-point epsilon decisions, or shared algorithm helpers.
- Preserved the minimum edge, three lambda candidates, child evaluation order, and LIFO traversal. Deliberately omitted the separate
  `zbund` monotonicity route and every coposit shortcut.
- Recorded that boundary matrix 9161 can refine without terminating. Future time-limited runs must classify that outcome as
  unresolved rather than false.

## 2026-08-07 — Optimized Hadeler baseline

- Added `hadeler_1983`, pinned to FracESSA commit `36902a3d`, as the last optimized principal-submatrix implementation before the
  cone replacement and connected-component work.
- Preserved the one-factorization/one-system negative-determinant path: each principal matrix solves only `C y = -1`; no full inverse
  or adjugate is constructed. Singular matrices use one restored exact nullspace.
- Promoted the retained general fraction-free LDLT solver into shared model-independent infrastructure and preserved its FLINT LGPL
  attribution. Replaced only the fixed-width subset mask with a dynamic traversal in the same numeric-mask order.

## 2026-08-07 — Slim corpus schema

- Reduced the maintained corpus from four tables and six indexes to one six-column `matrices` table. Flattened the 498 provenance
  rows into optional source and family fields, preserving both origins for the four multiply sourced matrices and all 15 families.
- Removed derived/import-only matrix fields, 56 literature benchmark rows, and 3,138 rows from two historical algorithm sessions.
  Those records remain available in the compressed byte-exact source snapshot rather than defining the active corpus contract.
- Preserved all 1,569 IDs, dimensions, exact upper triangles, and strict classifications byte-for-byte. SQLite integrity passed;
  the active database shrank from 71,491,584 to 68,915,200 bytes. The exact source compresses to about 1.5 MB.

## 2026-08-07 — Python model selection without model coupling

- Added the `pycoposit` sequential and bounded multiprocessing paths using the proven PyFracESSA queue protocol and completion-order
  results.
- Kept the C++ link-time model boundary: each native extension links exactly one solver, while Python maps the required algorithm
  identifier to that module. No solver code, runtime factory, or shared model implementation was introduced.
- Kept timeouts out of this first wrapper. Parse, execution, and worker failures remain distinct from Boolean classifications; a
  future timed corpus runner must add an explicit unresolved outcome.

## 2026-08-07 — Exceptional and equality stress corpus

- Added 25 permutation/projective-new exceptional extremal order-6 matrices from Hildebrand's case-34 parametrization. Every
  retained positive diagonal scaling exceeded a 250 ms Dutour screen; representative cases also exceeded five seconds while the
  Danninger, Hadeler, Dickinson, and Safi baselines rejected their exact zero directions quickly.
- Added 29 Väliaho-style almost strictly copositive matrices `D*(nI-J)*D` of orders 5 through 12. Their positive full-support kernel
  vectors make equality exact; these matrices are positive semidefinite boundary cases and are deliberately not labeled
  exceptional.
- Verified an exact nonnegative zero for every added matrix, found no existing match after primitive scaling and simultaneous
  permutation, populated the existing Horn and Hoffman-Pereira provenance, and passed SQLite integrity checking. The maintained
  corpus now has 1,623 matrices; the compressed original FracESSA snapshot remains byte-exact and unchanged.

## 2026-08-07 — Second exceptional-matrix research pass

- Retained four open primary papers locally: the complete exceptional `COP(6)` classification, the 2026 higher-dimensional extremal
  extension, Hildebrand's circulant-zero-pattern construction, and Štrekelj-Zalar's 2025 exceptional-matrix construction.
- Added IDs 9711–9737 from rational half-angle realizations of Afonin-Hildebrand-Dickinson Cases 13.1 and 18, and IDs 9738–9756
  from the explicit Kostyukova-Tchemisova Example-5 extension. Positive diagonal congruences were retained only when the selected
  model took at least 200 ms or exceeded the one-second screen.
- Verified the papers' parameter conditions, exact nonnegative zero directions, primitive/permutation novelty, and SQLite integrity.
  Of the 23 one-second screen timeouts, 15 remained unresolved after five seconds. The corpus now has 1,669 matrices; all 46 new
  rows are exact exceptional boundary cases and the immutable FracESSA source snapshot is unchanged.

## 2026-08-07 — Binary-identified reference results

- Added the `results` table with one row per matrix, lowercase model ID, and native-extension SHA-256. Changed binaries retain
  separate rows; identical binaries resume without repeating completed work unless explicitly rerun.
- Kept execution status independent from mathematical output. Only `ok` rows contain a Boolean and native elapsed time; `timeout`
  and `error` rows contain `NULL` classifications.
- Added the stdlib-only `python/run_results.py` runner with a dimension interval, hard per-matrix timeout, and explicit Linux CPU
  list. One disposable process is pinned to each selected CPU, while the parent performs every SQLite write serially.

## 2026-08-07 — Hoffman-Pereira graph corpus

- Preserved Brendan McKay's non-isomorphic connected graph catalogs for orders 5 through 10 and added a stdlib-only reproducible
  importer implementing the Hoffman-Pereira extremality characterization.
- Added every new nontrivial class through order 9 and the first 130 qualifying order-10 classes, excluding the existing Horn and
  order-7 Hoffman-Pereira cycle classes. These 199 exact exceptional matrices are deliberately non-strict equality cases.
- Added four exact Kostyukova-Tchemisova examples and the strict exceptional rational matrix `C` from Strekelj-Zalar. All 204 stored
  rows exactly match regenerated rows, Hadeler classified every row as expected, SQLite integrity passed, and the corpus now has
  1,873 matrices.

## 2026-08-07 — Additional literature extreme matrices

- Retained five additional primary papers and added a stdlib-only importer for 200 exact matrices not drawn from the
  Hoffman-Pereira catalog sample: 24 Hildebrand `COP(5)` forms, 72 forms from Baston's two theorem families, 73 generalized Horn
  matrices from Johnson-Reams, and 31 nontrivial Dickinson-de Zeeuw Table-2 matrices.
- Extended the unbounded Johnson-Reams odd-order construction through order 151, the endpoint that makes the approved batch exactly
  200 rows. The selected model plus Danninger, Hadeler, and Safi classified all 200 as non-strict; Dickinson classified 77 within
  250 ms and timed out on 123, preserving the intended equality stress.
- Regenerated every inserted row exactly, passed SQLite integrity checking and the maintained Release suite, and left the immutable
  FracESSA snapshot unchanged. The corpus now has 2,073 matrices.

## 2026-08-07 — High-order Johnson-Reams sample

- Kept the 73 generalized Horn matrices at every odd order from 7 through 151 unchanged and appended 84 more at irregular odd orders
  beginning 163, 175, 181, and 199, then roughly every ten dimensions through 999.
- Regenerated the combined 284-row literature import exactly and assigned the new matrices IDs 10161 through 10244. The corpus now
  has 2,157 matrices, including 1,729 non-strict equality cases.

## 2026-08-07 — High-support and sum-of-squares-hard literature matrices

- Retained the Dickinson 2019 certificate paper, Laurent-Vargas hierarchy paper, Hildebrand-Afonin order-6 structure paper, and
  Baumert's 1967 sequel. Baumert's printed radical form was not approximated because the maintained corpus is exact integer-only.
- Added IDs 10245–10304: three Dickinson Case-9 extreme forms, three exact rational Hildebrand-Afonin forms outside the first
  Parrilo level, seven Laurent-Vargas direct sums outside every Parrilo level, and 47 Hildebrand circulant extreme forms with
  minimal-zero support `n-2`. Exact angle inequalities, determinant certificates, polynomial coefficients, and zeros are replayed
  by one standard-library importer.
- Every circulant form exceeded a 250 ms Dutour screen; orders 7, 8, 9, 11, 15, and 25 exceeded five seconds. Danninger, Hadeler,
  Dickinson, and Safi rejected the order-7 equality case within five seconds, while Bundfuss also timed out. The corpus now has
  2,217 matrices, including 1,789 non-strict equality cases; exact row replay, SQLite integrity, and all 10 Release tests passed.

## 2026-08-07 — Open-node resource limit

- Limited Dutour 2018, Bundfuss 2008, and Safi 2021 to 1,000,000 simultaneously unfinished nodes per matrix computation. Danninger,
  Hadeler, Dickinson, and Adaptive Dutour-Danninger deliberately remain unchanged.
- Added the unresolved `node_limit` result status. It has no Boolean classification and is distinct from `false`, timeout, and
  execution failure.
- Corpus matrix 9660, which previously contributed to a multi-worker out-of-memory failure, reached the one-million-node limit in
  about 345 ms with about 346 MiB maximum resident memory in a direct Release native-module check on this machine and is retained
  as the focused cutoff regression.

## 2026-08-07 — Strict perfect copositive lift families

- Retained Dannenberg-Schürmann's 2023 paper and imported its exact indefinite perfect seed `I`, exceptional perfect certificate
  `E`, and 198 consecutive repeated-coordinate lifts: 100 matrices of orders 3–102 from `I` and 100 of orders 5–104 from `E`.
- Lemma 5.1 and Corollaries 5.6–5.7 prove every row strictly and perfectly copositive while preserving the seed's SPN or exceptional
  component. The importer checks and propagates an exact minimal vector with final coordinate at least two, which is the lifting
  condition. The paper's `E/3` is stored as its primitive integer numerator.
- Dutour handled both seeds quickly but exceeded five seconds on the order-20 `I` lift and order-10 exceptional lift. Exact replay,
  SQLite integrity, and all 12 Release tests passed. IDs 10305–10504 bring the corpus to 2,417 matrices, including 628 strict cases.

## 2026-08-07 — Hash-independent Hadeler baseline results

- Made Hadeler 1983 the deliberate exception to binary-identified result rows. Its fixed baseline stores an empty binary hash, so a
  rebuild of shared wrapper code does not create another Hadeler result set.
- Existing Hadeler results remain authoritative and newly added matrices extend that same matrix/parameter result set. Every other
  model continues to store and distinguish the exact native-extension SHA-256.

## 2026-08-07 — FracESSA first-order global-minimum model

- Added `fracessa`, a coposit-created adaptation that applies FracESSA's exact safe first-order candidate search to `Q=-A` and decides
  strict copositivity from exact KKT payoff signs.
- Removed all ESS/NSS, local-maximum, inertia, reduced-Schur, and second-order stability work. Superset pruning remains correct for
  the global value because every global maximizer is KKT and a KKT point on a containing support cannot improve the contained KKT
  point's payoff.
- Made this model the sole explicit exception allowed to retain FracESSA's 63-bit support masks. It rejects order greater than 63;
  the shared core and every other model remain unrestricted by that representation limit.
- Changed the strict-only decision to stop at the first accepted KKT payoff greater than or equal to zero. Removed best-payoff
  comparison and storage because exhaustive value computation is unnecessary when the output is only strict copositivity.
- Replaced all 2,131 stored FracESSA rows through order 63 using the rebuilt module, a five-second cutoff, and CPUs 3 through 9. The
  run solved 2,032 matrices with zero mismatches, timed out on 99, and completed in 80.422 seconds wall time.

## 2026-08-07 — Packed dynamic support representation

- Added the shared `coposit::support` class. It stores `ceil(n / 64)` unsigned 64-bit words and provides constant-time membership,
  wordwise subset tests, lowest-index lookup, and ordered index extraction without a fixed-width dimension ceiling.
- Replaced FracESSA's single-word support and forbidden-set storage with the shared class. Cardinality order, conceptual numeric-mask
  order, delayed forbidden-set activation, lowest-index bucketing, branch pruning, and candidate mathematics remain unchanged.
- Removed the temporary FracESSA order-63 exception from the project instructions and public validation.
- The complete 14-test Release suite passed. The rebuilt native module matched all 1,412 corpus classifications through order 10,
  and the documented matrix 10289 still returned not strictly copositive.
- Changed Dickinson's two coverage sets and current subset to the same shared packed representation. Its ordered index vector remains
  only for direct principal-matrix access; subset order, certificate construction, and mathematical decisions are unchanged.
- On three repetitions over all 1,412 corpus matrices through order 10, the median summed native time fell from 76.671 ms to
  56.212 ms, a 26.7% reduction, with no classification mismatch. Hadeler remains on ordered indices because it performs no support
  containment tests and would gain only conversion overhead from a packed duplicate.

## 2026-08-13 — Explicit diagonal in compact circular input

- Changed the short FracESSA format from `floor(n/2)` implicit-zero-diagonal values to `floor(n/2)+1` values beginning with the
  common diagonal. This preserves exact nonzero diagonal payoffs instead of requiring callers to normalize a game first.
- Kept full upper-triangular input unchanged and made former short strings fail the value-count check rather than acquire an
  ambiguous new meaning.

## 2026-08-21 — SAT-C2 scheduled curvature walks

- Added `sat_c2` as an isolated copy of SAT-C1 with bounded active-set walks before and during the ordinary low/high traversal.
- Walks visit only globally open supports, never revisit their current path, do not backtrack, and visit at most `n` supports.
  Candidate upward, downward, and KKT closures remain buffered until the walk ends and are verified exactly before SAT insertion.
- Scheduled the walks globally and alternately: after the initial singleton and top walks, wait for `n-1` ordinary supports, walk
  from the next low-frontier support, wait another `n-1`, then walk from the next high-frontier support, repeating low/high.
