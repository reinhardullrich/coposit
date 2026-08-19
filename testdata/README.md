# Copositivity Test Corpus

`copos_testdata.sqlite3` is the slim maintained copositivity corpus.

It contains corpus and reference data plus a two-column cache of the fastest eligible local diagnostic result. Mutable benchmark
measurements remain in the ignored local `experiments/diagnostics.sqlite3` database.

- Matrices: 3,523
- Dimensions: 1 through 5,000
- Strictly copositive: 1,002
- Copositive but not strictly copositive: 1,332
- Not copositive: 1,086
- Copositive with strict status not yet established: 0
- Strict and non-strict copositivity not yet established: 103
- Schema: the `sources` and `matrices` tables, with no views, triggers, or manually created indexes

The maintained directory contains this database, its `schema.sql`, the reproducible `diagnostics_schema.sql`, this README, and the
required `matrices/` payload directory.
One-time construction and migration material is retained under `archive/`.

`matrices` stores the stable ID, dimension, exact matrix data, nullable strict and non-strict copositivity results,
optional free-form source text, normalized primary `source_id`, the `additional_source_ids` bibliography, the `references_solved`
and `references_unsolved` literature-claim arrays, optional family, and four
independent Boolean benchmark flags:
`smoke_set`, `core_and_stress_test`, `n_le_100`, and `n_gt_100_solved`. The first two are curated stored memberships and default
off. `n_le_100` is generated directly from `dimension <= 100`; `n_gt_100_solved` is generated from order above 100 plus a nonempty
`references_solved` array. Both generated definitions also require `preprocessing_solved = 0`. Neither generated membership can
drift, and both automatically include future qualifying rows. A known
strict-positive result requires a known non-strict-positive result. `NULL` in either truth column means that result has not been
established; it must not be read as false.
`preprocessing_solved` is true for every retained matrix completely classified by the new maintained depth-2 combined preprocessing
workflow in the five-second corpus run, its sixty-second timeout continuation, the focused ten-minute Motzkin--Straus follow-up, or a
stored combined diagnostic result with `preprocessing_outcome=resolved` and zero model delegations. Partial CP/SCP facts do not
qualify. Updating from shorter diagnostics is additive: it never clears evidence established by a longer run. Every benchmark
selection excludes these rows so it measures model traversal rather than an exact preprocessing decision.
Small matrices keep their comma-separated upper triangle inline. Large rows contain `file:matrices/<matrix_id>.mtx`, resolved relative
to the database directory, and `file_sha256` binds each reference to the SHA-256 of its exact file bytes. Inline rows keep that column
`NULL`. The hash is retained for occasional explicit integrity audits; normal solver and benchmark runs do not recompute it. Those
files use whichever standard Matrix Market
`array integer symmetric` or
`coordinate integer symmetric` representation is smaller in bytes. Symmetric array order and sorted symmetric coordinates both encode
the lower triangle; coordinate files omit zeros. The archived `externalize_large_matrices.py` records the completed conversion that
selected the smaller exact representation for every external file and compacted the database. The archived
`assign_benchmark_sets_2026_08_10.sql` preserves the guarded original Core and Stress assignments;
`fuse_core_and_stress_test_2026_08_16.sql` records their current union without the precheck-trivial generated panel.
`retire_scale_and_timeout_sets_2026_08_16.sql` removes the two retired stored flags. `aidocs/BENCHMARK_SETS.md` explains the
current assignments and composition.
`add_preprocessing_solved_2026_08_16.sql` records the original guarded 2,115-row preprocessing result flag. The current maintained
preprocessing record flags 2,765 matrices; its bulk exact run evidence is retained in `experiments/preprocessing_depth_2026-08-15/`
and `experiments/diagnostics.sqlite3`.
`exclude_preprocessing_solved_from_benchmarks_2026_08_16.py` records the initial guarded curated replacements and current generated-set
definitions. `refresh_benchmark_sets_after_motzkin_straus_2026_08_16.sql` removes the additional preprocessing-complete members and
restores stored Smoke and Core-and-Stress membership to 49 and 512 rows. The additive preprocessing flags are not removed from those
stored lists; the effective selectors currently contain 46 and 469 rows, and comprehensive N ≤ 100 contains 731 matrices. The older
`add_n_le_100_set_2026_08_14.sql` and `add_n_gt_100_solved_set_2026_08_14.sql` files preserve their original pre-preprocessing
definitions.
`replace_hildebrand_circulants_2026_08_16.py` replaces the 17 order-15–25 Hildebrand parameter points by one diversified low-digit
exact representative per order, removes their obsolete diagnostics, and refills six boundary Core slots at the same orders.
`fastest_elapsed_ns` stores the shortest eligible completed native time, and `fastest_result_ref` identifies its exact diagnostic row
by model, mode, preprocessing, and binary SHA-256. An eligible row must be `ok`, use combined `both` classification, and agree with
every known corpus truth. For a `preprocessing_solved` row, those fields instead intentionally record the shortest complete shared-
preprocessing decision: this isolates the cost that actually solved that matrix before any model traversal. The 36 retained raw
depth-2 continuations are represented by exact `preprocessing_depth_2` diagnostic rows for this purpose. Predicate-only measurements
never enter either selection. The reference runner refreshes ordinary cache rows in the same serialized batch transaction used to
write diagnostics.
Free-form source strings preserve the provenance of the stored occurrence; every retained representative also preserves the origins
of projectively equivalent matrices removed on 2026-08-09 and 2026-08-14. `source_id` points to the earliest located paper, archive, repository, or
reproducible local generator for the matrix. `additional_source_ids` is a compact JSON list of other sources that explicitly print,
use, or discuss that matrix or its identified class. Solver-result claims are normalized separately in `references_solved` and need not
be duplicated here. The list excludes the primary source and is ordered by publication year,
then source ID. It is a best-effort bibliography rather than a claim that every family-level citation names every coefficient array.
The 513 nonempty lists currently contain 837 secondary links. They come from explicit catalog-to-matrix matches, imported occurrence
provenance, exact positive-scale duplicate groups, and audited named or family-level reuse statements.
`references_solved` is a JSON array of objects with required integer `source_id` and optional text `comment`. It records papers that
claim a completed copositivity, strict-copositivity, or equivalent global-StQP decision for that identified matrix; comments qualify
numerical tolerances, heuristics, or equivalent formulations when needed. Its 462 nonempty rows contain 678 claims from 23 sources.
An empty array means no identifiable completed claim was found in the audit, not that the matrix has never been solved. See
`aidocs/LITERATURE_SOLVED_REFERENCES.md` for the evidence and exclusions.
`references_unsolved` has the same source-linked object structure but requires a comment naming the failing method and outcome. Its
175 nonempty rows contain 234 explicit timeout, memory, numerical, inconclusive, or wrong-result claims from 11 papers. The same paper
may occur in both arrays when different methods in that paper have different outcomes; 151 matrices currently have both kinds of
claim. See `aidocs/LITERATURE_UNSOLVED_REFERENCES.md` for the conservative inclusion rule and deliberate exclusions.
The `sources` table deliberately has only authors, title,
earliest documented public year, bibliographic reference, and an optional provenance comment. The year records the first located
public appearance, including a preprint, public archive, or repository when that predates a journal issue. Its 96 rows comprise
literature, collection, repository, and local-generator sources. Some literature rows are prepared for later
imports; a later use or repository location is also retained in `comment`. All 15 extracted families are preserved. Twenty-one added
exceptional/equality family labels cover 843 distinct literature and derived matrices, including the previously unlabeled Horn and
Hoffman-Pereira rows.

The corpus collapses only direct positive whole-matrix scalings

\[
B=cA,\qquad c>0,
\]

in the same coordinate order. A nontrivial simultaneous row-and-column permutation `P^TAP` remains a separate matrix because solver
traversal and runtime can depend on coordinate order. The earlier 2026-08-09 and 2026-08-14 audits initially merged permutations as
well as scales. The recovery audit restored Dickinson-de Zeeuw matrix 10132 and 40 literature-import rows whose equivalence required
a nontrivial permutation; 293 direct-scale literature duplicates remain merged. A temporary recovery of 1,396 old FracESSA reduced-B
coordinate orderings was deliberately removed because those extraction occurrences add no useful literature test inputs. No timing is
moved between coordinate orderings or scales. Positive diagonal congruences `DAD` with nonconstant diagonal `D` remain distinct.

The local diagnostics database's `results` table stores one benchmark result per matrix, lowercase model ID, requested decision mode,
preprocessing choice, and exact native-extension SHA-256. It keeps the stop status, classifications, elapsed time, cutoff, timestamp,
failure message, full diagnostic text, and any sparse certificate joint distribution supplied by the model. Every `run_results.py`
campaign enables this capture automatically. A temporary `running` row is updated once per second, then replaced by the final
`ok`, `parse_error`, `timeout`, `node_limit`, or
`error` row. This preserves the last diagnostic state even when a native worker cannot return within the cooperative timeout grace.
Hadeler rows predating 2026-08-11 may retain an empty legacy hash because their producing binary cannot be reconstructed; the runner
no longer creates such rows.

IDs 9711 through 9737 are rational half-angle instances from Cases 13.1 and 18 of the Afonin-Hildebrand-Dickinson order-6
classification. IDs 9738 through 9756 are positive diagonal congruences of the explicit 9-by-9 extremal extension in
Kostyukova-Tchemisova (2026), Example 5. Every one of these 46 exact boundary matrices exceeded 200 ms in its initial selected-model
screen; 23 exceeded one second, and 15 still exceeded a five-second confirmation cutoff. Exact nonnegative zeros were checked before
insertion, and timeout remains benchmark evidence rather than a Boolean result.

The diagnostics database's `preprocessing_results` is the separate, deliberately small store for preprocessing-only experiments. It
records the corpus matrix ID, run ID, requested mode, one of the three pipeline configurations, status, native time, delegate count,
and the explicit ternary preprocessing outcome `positive`, `negative`, or `unresolved`. It does not represent the pre-check as a
complete solver and does not duplicate matrix metadata.

Kuzmanovic's separate 100,000-matrix preprocessing archive was reconstructed, completely classified with exact ordinary CBDD-Zed
Dickinson, documented in `../aidocs/KUZMANOVIC_100000_MATRIX_SCREEN.md`, and then removed from the maintained database. Its six
separately printed exact examples remain in the main corpus.

IDs 9757 through 9955 are 199 non-isomorphic Hoffman-Pereira exceptional boundary classes generated from McKay's connected graph
catalogs: every new class through order 9 and the first 130 qualifying order-10 classes. IDs 9956 through 9959 are the four remaining
exact integer examples from Kostyukova-Tchemisova (2026), Examples 1 and 5. ID 9960 is the rational strict exceptional matrix `C`
from Strekelj-Zalar. The reproducible importer and retained graph6 source catalogs record the exact selection.

The batch originally stored at IDs 9961 through 10160 contributed 200 exact literature-source occurrences independent of the
Hoffman-Pereira catalog selection: 24 rational members of Hildebrand's exceptional extreme `COP(5)` family, 54 members of Baston's
all-orders basic extreme family, 18
members of Baston's distinct cyclic family, 73 Johnson-Reams generalized Horn matrices for every odd order 7 through 151, and all
31 stability-3 or stability-4 cop-irreducible graph examples in Dickinson-de Zeeuw Table 2. Dickinson-de Zeeuw ID 10132 is a
simultaneous permutation of Kostyukova-Tchemisova ID 9957 and is retained as its own coordinate ordering; all 200 batch matrices are
therefore present. The Johnson-Reams family is unbounded;
151 is only the endpoint that fills the user-approved 200-row batch.

IDs 10161 through 10244 append 84 more Johnson-Reams generalized Horn matrices without changing that dense odd-order block. Their
odd orders begin 163, 175, 181, and 199, then follow an irregular roughly-ten-dimension spacing through 999.
`archive/import_literature_extremes_2026_08_07.py` regenerates all 284 rows from both batches.

IDs 10245 through 10304 originally added 60 exact matrices in a third literature pass: three Dickinson order-6 Case-9 extreme forms, three
Hildebrand-Afonin order-6 forms outside Parrilo's first sum-of-squares level, seven Laurent-Vargas direct sums outside every level
of that hierarchy, and 47 Hildebrand circulant extreme forms of orders 7 through 25 whose minimal zeros have support `n-2`.
All 47 circulant forms exceeded the 250 ms Dutour screen; representatives at orders 7, 8, 9, 11, 15, and 25 exceeded five seconds.
`archive/import_hard_literature_matrices_2026_08_07.py` verifies every theorem condition and zero exactly before reproducing the rows.
The current family keeps the 30 original order-7--14 points and IDs 13024--13034, one exact order-15--25 representative per order.
The latter were selected from the low-digit part of a finite rational grid while deliberately varying the two construction parameters;
no timing from a removed parameter point was transferred to a replacement.

IDs 10305 through 10504 add 200 strict perfect copositive matrices from Dannenberg-Schürmann: the printed indefinite order-3 seed
`I` and every lift through order 102, followed by the printed exceptional order-5 certificate `E` and every lift through order 104.
The paper's Lemma 5.1 preserves perfect copositivity and the positive copositive minimum under each repeated-coordinate lift;
Corollaries 5.6 and 5.7 preserve the two seeds' respective SPN and exceptional components. The importer retains an exact minimal
vector with final coordinate at least two across every lift. Representative Dutour runs exceeded five seconds at order 20 for the
`I` family and already at order 10 for the exceptional family. The paper's rational scale `E/3` is stored as its primitive integer
numerator because positive scaling does not change strict copositivity.

The 150 deterministic project-generated sparse/dense matrices were removed from the maintained corpus on 2026-08-16. Exact
definiteness or their explicit two-coordinate witness resolved every row during preprocessing, so they did not exercise a selected
model in normal runs. Their dated generators, reshape migration, and guarded removal script remain under `archive/`.

The byte-exact 2026-08-07 FracESSA source database is preserved as
`testdata/archive/copos_testdata.original.sqlite3.xz`. Its decompressed SHA-256 is
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`. It retains the removed literature and historical-run tables
for provenance or experiment replay.

`schema.sql` is the complete maintained corpus schema; `diagnostics_schema.sql` creates the separate local benchmark database. The
archived `add_copositivity_classification_2026_08_10.sql` records the migration
that reconstructed non-strict truth from the immutable source snapshot and the later proved families.
`archive/add_literature_sources_2026_08_14.py` records the initial normalized 78-source insertion and 901 literature links.
`archive/complete_source_links_2026_08_14.py` adds the 16 corpus-source records, preserves detailed FracESSA origins in the free-form
text, and links the 2,442 pre-catalog matrices.

The 1,048 retained catalog occurrences originally imported into ID slots 10685 through 12522 are now represented by 864 distinct
matrix rows. Each survivor's free-form source text retains every merged catalog instance ID, paper locator, usage, and source path.
Every retained row is an exact primitive integer representative of the source matrix or quadratic form and has normalized paper provenance. A conservative first
pass obtained 318 classifications directly from papers or repositories. Separate exact reconstruction passes then used only explicit
integer witnesses, entrywise or diagonal-dominance certificates, positive definiteness, Motzkin-Straus graph thresholds, and complete
exact simplex-face enumeration through order five. The retained imported totals are 336 strict, 269 boundary, and 420 non-copositive
rows; both truth fields stay `NULL` on 23 rows. Every project-derived decision is recorded in that row's free-form source text. The
import covers exact printed matrices, paper-selected graph
transforms, MATLAB arrays, Matrix Market and MPS/QPLIB
quadratic matrices, and 93 retained Chen-Burer archive occurrences. Fifty-five retained catalog representatives
use external Matrix Market payloads. Another 790 materializable raw quadratic-program objectives remain in the literature catalog but
are deliberately absent from the solver corpus: each has a negative diagonal entry, so a coordinate vector proves non-copositivity
immediately and the bulk collections add no useful copositivity-test information. This removes 180 Bomze-Locatelli-Tardella StQP
objectives, 12 Vandenbussche-Nemhauser BoxQP objectives, and 598 Chen-Burer archive objectives; the other 20 negative-diagonal
literature examples remain. Of the other catalog records, 938 already point to existing corpus matrices and 514 do not
provide a directly materializable eligible symmetric numeric matrix: they are generator recipes or source artifacts, symbolic
irrational or nonsymmetric data, or two unrecoverable archive members. `archive/import_literature_catalog_2026_08_14.py` reproduces
and validates the retained import without deduplicating it; the original ID slots remain stable and the exclusions become gaps.
`archive/classify_literature_catalog_truth_2026_08_14.py` reapplies and verifies only the paper- or repository-backed truth labels.
`archive/classify_obvious_literature_truth_2026_08_14.py`,
`archive/classify_constructed_literature_truth_2026_08_14.py`, and
`archive/classify_small_exact_literature_truth_2026_08_14.py` reconstruct every later exact certificate before accepting its stored
truth and evidence comment. `archive/correct_bomze_deklerk_portfolio_2026_08_14.py` records the corrected printed `Q4` and two
derived portfolio rows. `archive/remove_trivial_raw_qp_objectives_2026_08_14.py` is the guarded migration for databases created before
the exclusion was added to the importer. `archive/deduplicate_literature_import_2026_08_14.py` records the initial
positive-scale/permutation deduplication; `archive/restore_permutation_variants_2026_08_14.py` and
`archive/remove_old_fracessa_permutation_variants_2026_08_14.py` record the later recovery of literature coordinate orderings and the
deliberate exclusion of bulk FracESSA extraction orderings.

```bash
sqlite3 testdata/copos_testdata.sqlite3 'PRAGMA integrity_check;'
```
