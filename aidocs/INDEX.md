# Agent Documentation Index

This file routes to every other Markdown document under `aidocs/` without duplicating its contents.

## Current

- `BENCHMARK_SETS.md` — **current** — composition, guarded selection rules, overlap, and SQL access for the four corpus benchmark
  flags: Smoke, Core and Stress Test, N ≤ 100, and higher-order literature-solved.
- `PROJECT.md` — **current** — concise maintained structure, experiment contract, processing pipeline, model inventory, interfaces,
  dependencies, corpus, and ownership boundary.
- `REFERENCE_RESULTS_N_1_TO_100.md` — **current** — five-second non-strict and strict results for all eight literature baselines on
  every retained matrix through order 100, plus the preserved 2,078-row strict-only comparison of all earlier variants and cutoffs.
- `REFERENCE_RESULTS_N_LE_100.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on the complete generated
  N ≤ 100 set, using both preprocessing stages.
- `REFERENCE_RESULTS_N_GT_100_SOLVED.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on every
  higher-order matrix with a literature-reported solve.
- `REFERENCE_RESULTS_LITERATURE_UNSOLVED.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on every
  matrix carrying an explicit literature failure claim.
- `REFERENCE_RESULTS_BAD26_ORIGINAL_CONE_30S.md` — **current** — targeted 30-second results for the six historical or source-derived
  cone baselines on the 26 matrices unsolved by every five-second cone run.
- `REFERENCE_RESULTS_ALL_MODELS_FOR_HUMANS.md` — **current** — compact grouped Core/Stress results across all completed maintained
  models in the current campaign.
- `REFERENCE_RESULTS_BASELINES_AND_EXPERIMENTS.md` — **current** — five-second ordinary-copositivity Smoke and Core/Stress results
  for the literature cone baselines and completed coposit-created cone experiments.

## Research

- `XXX_IMPLEMENTATION_PLAN.md` — **research implementation plan** — planned exact SAT-guided KKT support search without cardinality
  traversal, shared fraction-free integer $LDL^T$, singular nullspace-to-boundary navigation, exact upward/downward blocks, guarded
  heuristic exclusions, staged tests, and unresolved design choices.
- `KKT_GUIDED_DICKINSON_COVERAGE_LOOP.md` — **research design** — alternating numerical KKT-support discovery, exact upward and
  downward verification, SAT-selected uncovered supports, and ordinary Dickinson completion.
- `SINGULAR_LIFT_DICKINSON_RESEARCH.md` — **research finding** — exact border and root-cone lifting theorems, completeness and
  accessibility counterexamples, BFS/A* guarantees, safe pruning rules, direct kernel-space alternatives, and order-45 evidence.
- `ORDINARY_DICKINSON_CERTIFICATE_ENGINE_RESEARCH.md` — **research finding** — transfer of singular-support results to bounded
  Dickinson intervals, exact stacked-flat and affine searches, activation-loss counterexamples, and staged certificate-engine proposals.
- `BPQY_DICKINSON_CERTIFICATE_GEOMETRY.md` — **research finding** — designated-zero support size, Dickinson interval width, exact
  digit-growth evidence, ten-second outcomes, and the order-25 matrix-12580 diagnostics trace.
- `BPQY_N50_RAYS_LONG_RUN.md` — **research result** — per-layer timing and certificate upper-cardinality distributions for the
  long SAT-Halfspace-Rays Dickinson run on three order-50 BPQY COP matrices with $\rho_0=12,25,38$.
- `HILDEBRAND_CIRCULANT_SAT_HALFSPACE_RAYS_LIVE.md` — **research result** — final order-20–24 outcome plus the captured
  per-layer certificate and upper-cardinality distributions for the order-22 and order-23 Hildebrand circulants.
- `AFFINE_RHS_DICKINSON_CERTIFICATE_GEOMETRY.md` — **research finding** — exact $L/U/d$ identities, affine right-hand-side path
  geometry, singularity distinctions, objective-equivalence theorems, and the order-25 BPQY two-path experiment.
- `PREPROCESSING_PIPELINE_DESIGN.md` — **current implementation** — complete root-check, component, ordinary-check,
  bounded-reduction, child-pipeline, single-switch, outcome, scan-reuse, and diagnostics specification.
- `KUZMANOVIC_100000_MATRIX_SCREEN.md` — **completed screen** — exact reconstruction, complete ordinary-copositivity results,
  timing, published-label audit, and the evidence for removing the separate 100,000-row staging table.
- `LITERATURE_SOLVED_REFERENCES.md` — **current evidence audit** — semantics, inclusion boundary, qualifications, exclusions, and
  reproducibility for the source-linked papers that report solving identified corpus matrices.
- `LITERATURE_UNSOLVED_REFERENCES.md` — **current evidence audit** — explicit method-level timeout, memory, numerical, inconclusive,
  and wrong-result claims linked to identified corpus matrices.
- `LITERATURE_MATRIX_FAMILIES.md` — **research** — source-backed constructions, proved properties, and reported difficulty or unusual
  behavior for 850 literature source occurrences represented by 849 distinct corpus matrices, excluding coposit's own benchmark conclusions.
- `WIDE_CERTIFICATE_THRESHOLD_EXPERIMENT.md` — **research result** — complete 75%, 90%, and 95%-of-remaining CBDD threshold outcomes
  and sparse $(k,d,count)$ distributions for BPQY matrix 12649.

## Historical

- `CHANGES.md` — **historical** — searchable history of meaningful decisions, results, and evidence that are not clear from Git.
- `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` — **historical** — five-second results for the former 524-matrix union of
  Representative Core and Stress; it predates the current 512-matrix fused flag.
- `REFERENCE_RESULTS_HADELER_BASED.md` — **current** — completed five-second Smoke and 512-matrix Core/Stress comparison of every
  model in the Hadeler, Dickinson, and FracESSA inheritance group.
- `CODE_REVIEW_2026-08-12.md` — **historical** — dated review whose public-interface findings were superseded when the `fast` and
  `safe` layers were removed; its remaining observations are retained as review evidence.
- `CODE_REVIEW_2026-08-11.md` — **historical** — completed predecessor review of preprocessing mathematics, model wiring, corpus
  storage, runner behavior, result provenance, resource handling, tests, and documentation.
- `history/COPOSITIVITY_CHECKER_EXTRACTION_HANDOFF.md` — **historical** — completed extraction map, inherited mathematical contract,
  source evidence, and the state transferred from FracESSA on 2026-08-07.
- `history/INTEGER_STABILITY_COPOSITIVITY_2026-08-06.md` — **historical** — retired integer Hadeler implementation, proof, tests,
  and benchmark evidence from FracESSA.
