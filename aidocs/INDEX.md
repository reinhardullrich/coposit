# Agent Documentation Index

This file routes to every other Markdown document under `aidocs/` without duplicating its contents.

## Current

- `BENCHMARK_SETS.md` — **current** — composition, guarded selection rules, overlap, and SQL access for the seven corpus benchmark
  flags: Smoke, Representative Core, Stress, Scale, Timeout 5s Strict, N ≤ 100, and higher-order literature-solved.
- `CODE_REVIEW_2026-08-12.md` — **current** — prioritized open review of correctness, public preprocessing cost, progress,
  distribution, parser resources, model targets, Python/process overhead, tests, and documentation.
- `PROJECT.md` — **current** — concise maintained structure, public contract, processing pipeline, model inventory, interfaces,
  dependencies, corpus, and ownership boundary.
- `REFERENCE_RESULTS_N_1_TO_100.md` — **current** — five-second non-strict and strict results for all eight literature baselines on
  every retained matrix through order 100, plus the preserved 2,078-row strict-only comparison of all earlier variants and cutoffs.
- `REFERENCE_RESULTS_N_LE_100.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on the complete generated
  N ≤ 100 set, using both preprocessing stages.
- `REFERENCE_RESULTS_N_GT_100_SOLVED.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on every
  higher-order matrix with a literature-reported solve.
- `REFERENCE_RESULTS_LITERATURE_UNSOLVED.md` — **current** — ten-second strict ZDD-Zed, CBDD-Zed, and CZDD-Zed results on every
  matrix carrying an explicit literature failure claim.
- `REFERENCE_RESULTS_REPRESENTATIVE_CORE_AND_STRESS.md` — **current** — five-second non-strict and strict results for all eight
  literature baselines plus a separate mode-explicit section for selected coposit-created models on Representative Core and Stress.
- `REFERENCE_RESULTS_BAD26_ORIGINAL_CONE_30S.md` — **current** — targeted 30-second results for the six historical or source-derived
  cone baselines on the 26 matrices unsolved by every five-second cone run.

## Research

- `BPQY_DICKINSON_CERTIFICATE_GEOMETRY.md` — **research finding** — designated-zero support size, Dickinson interval width, exact
  digit-growth evidence, ten-second outcomes, and the order-25 matrix-12580 progress trace.
- `PREPROCESSING_PIPELINE_DESIGN.md` — **current implementation** — complete root-check, component, ordinary-check,
  bounded-reduction, child-pipeline, single-switch, outcome, scan-reuse, and progress specification.
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
- `CODE_REVIEW_2026-08-11.md` — **historical** — completed predecessor review of preprocessing mathematics, model wiring, corpus
  storage, runner behavior, result provenance, resource handling, tests, and documentation.
- `history/COPOSITIVITY_CHECKER_EXTRACTION_HANDOFF.md` — **historical** — completed extraction map, inherited mathematical contract,
  source evidence, and the state transferred from FracESSA on 2026-08-07.
- `history/INTEGER_STABILITY_COPOSITIVITY_2026-08-06.md` — **historical** — retired integer Hadeler implementation, proof, tests,
  and benchmark evidence from FracESSA.
