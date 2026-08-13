# Project Changes

This append-only file records meaningful decisions, results, and evidence that are not clear from Git. Do not log routine edits here.

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
