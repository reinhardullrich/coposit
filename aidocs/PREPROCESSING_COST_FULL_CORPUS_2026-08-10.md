# Preprocessing-Only Cost on the Full Matrix Corpus

## Status

Current. The connected-components-only pair is complete. The strict pre-check-only run was stopped after 2,440 matrices when its
former cardinality-three default exposed excessive work on the final two graph encodings. A strict combined experiment with a
temporary cardinality-two default was stopped at 2,437 matrices after confirming that every remaining matrix has one connected
component. The project default was subsequently restored to cardinality three for the mostly small, timeout-bounded Core/Stress
benchmarks; the uncensored full-corpus results here remain historical evidence against using it indiscriminately at high order.

## Scope and method

The experiment runs only Coposit's shared preprocessing pipeline on every one of the 2,442 rows currently stored in `matrices`,
covering dimensions 1 through 3,361. It runs ordinary and strict mode separately with these configurations, once each:

1. connected components only;
2. all seven pre-checks only;
3. connected components followed by all seven pre-checks on the resulting blocks.

There is no native timeout and no parent hard timeout. Every matrix runs in a fresh Release process. The final full-corpus pass used
CPUs 3–9. A one-worker start established the memory footprint of the first large exact factorization before earlier unfinished work
was resumed on more workers without repeating completed rows.

The measured `elapsed_ns` is native preprocessing time only: input parsing, process creation, database reads, scheduling, and CSV
writes are outside it. The supplied final callback returns `true` immediately, so `delegate calls` counts unresolved whole matrices
or component blocks and `false decisions` counts matrices rejected by preprocessing itself. The source CSV files are under
`experiments/preprocessing_cost_2026-08-10/results/`; the same rows are upserted into the single `preprocessing_results` table under
run ID `floating_exact_verified_no_timeout_2026_08_10`. The cardinality-two combined run uses the distinct run ID
`floating_exact_verified_cutoff2_no_timeout_2026_08_11`. Outcomes are explicitly `positive`, `negative`, or `unresolved`. This table
is separate from complete-model `results` because delegation is an inconclusive preprocessing outcome, not a positive copositivity classification.

Times below are sums of per-matrix native elapsed time, not multi-worker wall time.

## Completed results

| Mode | Preprocessing | Completed | Cutoffs/errors | Native total | Median | Slowest matrix | Delegate calls | False decisions |
|---|---|---:|---:|---:|---:|---|---:|---:|
| Strict | connected components | 2,442 / 2,442 | 0 | 3.338574 s | 0.005750 ms | 10582: 301.194 ms | 21,303 | 0 |
| Ordinary | connected components | 2,442 / 2,442 | 0 | 3.274684 s | 0.005875 ms | 10589: 243.312 ms | 21,303 | 0 |
| Strict | all pre-checks, former cutoff 3 | 2,440 / 2,442; stopped | 0 among completed | 9,743.951217 s | 0.025021 ms | 10683: 836.326 s | 577 | 1,628 |
| Ordinary | all pre-checks | pending | — | — | — | — | — | — |
| Strict | components + all pre-checks, cutoff 2 | 2,437 / 2,442; stopped | 0 among completed | 5,871.556233 s | 0.035667 ms | 10674: 430.965 s | 694 | 1,499 |
| Ordinary | components + all pre-checks | pending | — | — | — | — | — | — |

## Preliminary observation

Connected-component discovery itself completed every matrix, including all 131 matrices of order at least 1,000. Its cost is almost
entirely in the large generated matrices: the full-corpus median remains about six microseconds even though the two native totals are
slightly above three seconds.

The stopped strict run omitted only matrix 9655 (MANN, order 3,321) and matrix 9645 (Keller, order 3,361). Isolated Release probes
showed that component discovery cost only 33.019 ms and 41.172 ms respectively. The actual order-three criterion sustained about
5.9 million eligible triples per second on both matrices, implying roughly 16–17 minutes for a complete triple pass. Exact
fraction-free LDLT was substantially worse: MANN's first three 100-pivot blocks took 27.127 s, 43.004 s, and 157.357 s; Keller's
first block took 98.457 s and its second had not finished after another 189 s. The probes were stopped rather than completing a
multi-hour factorization. This evidence motivated the temporary cardinality-two full-corpus experiment; the project default was
later restored to three for the Core/Stress benchmark population.

The strict combined cardinality-two run omitted only generated dense positive-definite matrices 10677, 10680, and 10683 and the
same MANN and Keller matrices. Completed components-only results show exactly one delegated component for all five, with component
costs between 33.019 ms and 41.172 ms. Continuing the combined run would therefore have repeated whole-matrix exact factorization
without testing any additional decomposition. The run was deliberately stopped and all 2,437 completed rows were retained.
