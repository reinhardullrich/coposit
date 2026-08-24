# Corpus Archive

This directory preserves one-time corpus construction and migration material. None of it is used by the maintained C++ or Python
runtime.

- `copos_testdata.original.sqlite3.xz` is the immutable byte-exact FracESSA source database.
- `FRACESSA_TESTDATA_README.md` is its historical documentation.
- `import_*.py` and `corpus_matrix.py` preserve the exact matrix-generation and verification procedures used for dated corpus imports.
- `import_negated_fracessa_games_2026_08_20.py` imports primitive exact negations of current FracESSA games while collapsing only
  positive whole-matrix scalings in the same coordinate order.
- `externalize_large_matrices.py` and `add_matrix_file_hashes_2026_08_11.py` preserve the completed external-file migration.
- `restore_fastest_result_cache_from_diagnostics_2026_08_15.sql` restores and verifies the compact fastest-result cache after benchmark
  rows moved into the separate diagnostics database.
- `reset_fastest_result_cache_to_both_2026_08_16.sql` clears and reconstructs those fields using only complete `both`-mode results,
  independently of preprocessing selection.
- `refresh_corpus_from_diagnostics_2026_08_18.sql` adds 17 unanimous combined classifications, adds 33 zero-delegation preprocessing
  results without clearing earlier long-timeout flags, and verifies the fastest-result cache against every eligible combined run.
- `refresh_truth_and_fastest_after_xxx_2026_08_18.sql` adds the two later unanimous XXX classifications, verifies the additive
  preprocessing flags, and refreshes only the two fastest-result fields from exact combined diagnostics.
- `refresh_truth_and_fastest_from_combined_results_2026_08_24.sql` adds 13 unanimous combined classifications from completed B7/G2
  evidence and refreshes the fastest-result fields from every eligible exact combined diagnostic row.
- `record_preprocessing_times_2026_08_18.sql` imports the 36 retained complete depth-2 preprocessing outcomes that predate central
  capture, then makes every preprocessing-complete matrix's timing fields point to its shortest exact preprocessing decision.
- `reshape_generated_stress_2026_08_15.py` removes the 120 sparse/dense generated stress matrices above order 1,000, their dependent
  diagnostics and external payloads, and inserts 90 matrices at 15 additional irregular dimensions from 43 through 199.
- `remove_generated_stress_2026_08_16.py` removes the remaining 150 precheck-trivial generated matrices, their 21 external payloads,
  dependent local diagnostics, and the two now-unused generator sources.
- `fuse_core_and_stress_test_2026_08_16.sql` replaces the separate Core and Stress flags with their union while excluding the
  precheck-trivial sparse/dense generated panel, which was still retained in Scale at that point.
- `retire_scale_and_timeout_sets_2026_08_16.sql` removes the obsolete Scale and Timeout 5s Strict columns after guarding their final
  memberships and corpus size.
- `add_preprocessing_solved_2026_08_16.sql` marks all 2,115 retained matrices completely classified by the new maintained depth-2
  workflow in the five-second corpus run or its sixty-second continuation; partial facts are excluded.
- `exclude_preprocessing_solved_from_bpqy_benchmark_2026_08_23.sql` moves the existing named-set preprocessing exclusion into the
  generated BPQY benchmark flag itself, reducing raw membership from 413 to the unchanged 404 effective model inputs.
- `exclude_preprocessing_solved_from_benchmarks_2026_08_16.py` removes those rows from both curated benchmark sets, deterministically
  refills Smoke to 49 and Core and Stress to 512, and changes both generated set definitions to exclude preprocessing-complete rows.
- `refresh_benchmark_sets_after_motzkin_straus_2026_08_16.sql` removes the 21 additional Smoke and 226 additional Core members solved
  by the current preprocessing pipeline, then records the exact unresolved replacements that restore the curated 49- and 512-row
  sizes. N ≤ 100 remains comprehensive and therefore falls naturally to 754 rows.
- `mark_ten_minute_preprocessing_solved_2026_08_17.sql` marks the 22 Motzkin--Straus matrices completely classified by the later
  ten-minute continuation. Its 23 timeouts remain unmarked; curated benchmark replacement is deliberately separate.
- `replace_hildebrand_circulants_2026_08_16.py` replaces the 17 order-15–25 Hildebrand points by one diversified low-digit exact
  representative per order, deletes their obsolete local diagnostics and four payloads, and preserves the 512-row Core set with six
  same-order boundary replacements.
- `add_independent_angle_hildebrand_2026_08_19.py` adds one exact Hildebrand circulant at each order 15–30 using independently
  selected rational angle parameters. The original equally spaced-angle representatives and their diagnostics remain unchanged.
- `add_literature_sources_2026_08_14.py` preserves the initial normalized 78-source migration and 901 literature links.
- `complete_source_links_2026_08_14.py` adds collection, repository, and local-generator sources and completes all current matrix
  source links.
- `add_source_publication_year_2026_08_14.py` adds the earliest documented public year to all 94 normalized sources.
- `add_additional_matrix_sources_2026_08_14.py` makes the earliest located source primary and stores the original 1,076 explicit or
  audited class-level reference links; deduplication unions them into 833 links on 509 surviving matrices.
- `add_literature_solved_references_2026_08_14.py` adds the guarded `references_solved` JSON-object arrays for the original 968
  identifiable occurrence claims; deduplication unions them into 619 matrix-class claims documented in
  `aidocs/LITERATURE_SOLVED_REFERENCES.md`.
- `add_literature_unsolved_references_2026_08_14.py` adds 232 explicit method-specific failure claims on 173 retained matrices to the
  guarded `references_unsolved` arrays documented in `aidocs/LITERATURE_UNSOLVED_REFERENCES.md`.
- `import_literature_catalog_2026_08_14.py` reconstructs 1,838 directly materializable new catalog occurrences, excludes the 790 raw
  QP objectives with a negative diagonal, and imports the 1,048 retained occurrences without deduplication or inferred truth labels.
- `classify_literature_catalog_truth_2026_08_14.py` applies the 318 paper- or repository-backed truth labels while leaving every
  unsupported classification `NULL`.
- `correct_bomze_deklerk_portfolio_2026_08_14.py` replaces the initial unrelated portfolio reconstruction by Bomze-de Klerk's
  printed `Q4` and its two exact shifted test matrices.
- `classify_obvious_literature_truth_2026_08_14.py` reconstructs 699 retained short exact matrix certificates and records each proof
  in the corresponding source comment.
- `classify_constructed_literature_truth_2026_08_14.py` reconstructs 55 exact Horn, clique-threshold, positive-definite, or explicit
  negative-witness decisions.
- `classify_small_exact_literature_truth_2026_08_14.py` classifies eight matrices of order at most five by exact enumeration of every
  simplex face and records the rational minimum and minimizer.
- `remove_trivial_raw_qp_objectives_2026_08_14.py` removes the same 790 objectives from a database populated before the importer
  exclusion, after guarding their source groups, stable ID fingerprints, negative diagonals, benchmark flags, and dependent results.
- `add_n_le_100_set_2026_08_14.sql` adds the generated, drift-free membership for every matrix with `dimension <= 100` and guards
  the current 2,619-row selection.
- `add_n_gt_100_solved_set_2026_08_14.sql` adds generated membership for every matrix above order 100 with a nonempty
  `references_solved` claim array and guards the current 58-row selection.
- `add_fastest_result_cache_2026_08_14.sql` adds and backfills the sortable fastest elapsed time and its exact composite result
  reference, then installs the candidate view and synchronization triggers.
- `deduplicate_literature_import_2026_08_14.py` merges the 333 redundant occurrences in the 256 positive-scale/permutation classes
  touched by the literature import. It unions truth, family, bibliography, solved and unsolved references when present, and all five
  benchmark flags. A sole
  benchmarked representation survives its class so stored timing and preprocessing rows remain attached to their exact input.
- `restore_permutation_variants_2026_08_14.py` records the recovery audit that reconstructed previously merged coordinate orderings.
  `remove_old_fracessa_permutation_variants_2026_08_14.py` records the final policy: retain the 41 recovered literature matrices but
  remove the 1,396 bulk FracESSA reduced-B extraction orderings and their dependent historical results.
- `import_kuzmanovic_preprocessing_results_2026_08_14.py` stages the thesis's 100,000 exact random matrices and published
  preprocessing labels in `kuzmanovic_test_matrices`. The labels are experimental outputs, not trusted truth for the maintained
  `matrices` table. The staging table is intentionally absent after the completed screen; this script retains reproducibility.
- `run_kuzmanovic_cbdd_zed_2026_08_14.py` records the ten-second ordinary-copositivity CBDD-Zed Dickinson result directly on every
  Kuzmanovic staging row, including whether it agrees with the published label. `no_answer` is not treated as a truth claim. The
  completed results and removal decision are preserved in `../../aidocs/KUZMANOVIC_100000_MATRIX_SCREEN.md`.
- `generate_bpqy_julia185_matrices.jl` reproduces the 450 seeded Julia 1.8.5 numerical constructions from Bomze-Peng-Qiu-Yildirim;
  `import_bpqy_julia185_matrices_2026_08_14.py` imports their exact primitive dyadic upper triangles. Their intended boundary
  classification is recorded as provenance but is not assigned to the rounded numerical materializations.
  `remove_bpqy_spn_2026_08_22.py` removes the 150 SPN materializations and their stored diagnostics from the maintained corpus while
  preserving the reproducible artifact and all COP and PSD materializations.
- `generate_bpqy_julia185_extension_2026_08_19.jl` applies the same Julia 1.8.5 Float64 recipes to COP and PSD matrices of orders
  20, 30, 35, 40, and 45, with the paper's three designated support sizes and reported seeds 1 through 25.
  `import_bpqy_julia185_extension_2026_08_19.py` imports the resulting 750 primitive dyadic integer materializations without assigning
  either copositivity truth field; `bpqy_julia185_extension_2026_08_19.tsv` is the reproducible generated artifact.
  `classify_bpqy_extension_from_sat_halfspace_rays_2026_08_19.py` copies only completed ten-second exact combined classifications
  into corpus truth and verifies their fastest-result cache; timeouts remain unknown.
- `generate_bpqy_julia185_cop_orders_10_15_55_60_2026_08_22.jl` applies the same COP recipe at orders 10, 15, 55, and 60, using
  three admissible designated-support sizes and seeds 1 through 25. Its TSV artifact contains 300 primitive dyadic integer
  materializations; `import_bpqy_julia185_cop_orders_10_15_55_60_2026_08_22.py` imports them without inferring truth from the ideal
  boundary construction. `classify_bpqy_cop_orders_10_15_55_60_from_sat_b3_2026_08_22.py` copies only completed exact SAT-B3
  classifications, zero-delegation preprocessing evidence, and eligible fastest timings into the corpus; timeouts remain unknown.
- `add_bpqy_benchmark_2026_08_22.sql` reconciles 57 saved exact BPQY COP classifications left out of corpus truth, then adds the
  generated `bpqy_benchmark` flag for every strict or still-unknown BPQY COP lift. Named-set selection separately excludes
  preprocessing-complete rows.
- `add_bpqy_quick_test_2026_08_23.sql` adds the fixed six-matrix `bpqy_quick_test` selector and preserves the SAT-B3 timing evidence
  used to choose its order-30–60, 17.7–96.8-second development panel.
- The dated `.sql` files preserve applied schema, classification, result, and benchmark-set migrations.

The maintained corpus is `../copos_testdata.sqlite3`; its current schema is `../schema.sql`, and externally stored matrices are under
`../matrices/`.
