# Reference Results: Higher-Order Literature-Solved Matrices

## Scope

This campaign used every matrix then in the generated `n_gt_100_solved` set: every retained matrix above order 100 with at least one
paper-reported completed solve in `references_solved`. That 58-matrix snapshot spans orders 120–1,000 and contains 6 strict, 8 boundary, and 44 non-copositive
cases backed by 112 claims from eight literature sources.

Each model uses strict-copositivity mode, both preprocessing stages, a ten-second per-matrix cutoff, CPU 2 for dispatch and serialized
SQLite writes, and persistent native workers on CPUs 3–9. `--rerun` measures every selected matrix with the listed native module.
Cutoff-substituted work uses measured native time for completions and ten seconds for each timeout.

## Results

| Model | Native SHA-256 | Solved | Timeout | Strict solved | Not-strict solved | Mismatch | Substituted work | Wall time |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| ZDD-Zed Dickinson | `8b35bc2489db0086fb3c6fcfe1c9f7579ecde786ebd37df843ca7476630a6c78` | 56 | 2 | 6 | 50 | 0 | 39.059 s | 11.079 s |
| CBDD-Zed Dickinson | `f2141101bb5d2c848d212702c228e2ae8985d021ffc7cc28becb5bd26efc0955` | 56 | 2 | 6 | 50 | 0 | 39.048 s | 11.145 s |
| CZDD-Zed Dickinson | `61f0df6855788983bd69952a9633df91be1ee3711f4c4408dcd7faa0f6c1de2f` | 56 | 2 | 6 | 50 | 0 | 39.586 s | 11.131 s |

All three models timed out on exactly matrix 9575 (order 200) and matrix 9651 (order 378), both known not strictly copositive. None
produced a node limit, parse error, execution error, or wrong classification. The completed-run medians for ZDD-Zed, CBDD-Zed, and
CZDD-Zed are 9.688 ms, 10.376 ms, and 11.017 ms, respectively. At this cutoff, the three decision-diagram representations have the
same coverage; CBDD-Zed has the lowest cutoff-substituted work, but its 0.011-second advantage over ZDD-Zed is negligible.

## Ten-minute retry of the two timeouts

The narrowly lowest-work model, CBDD-Zed, was rebuilt and rerun on the two ten-second timeouts in parallel with a 600-second cutoff,
strict mode, and both preprocessing stages. The retry used native SHA-256
`110be8e001b3990cb899ed8e4b4a058f73f7b0134b9ab314028a167b2ed1a87d`.

| Matrix | Order | Result | Native time |
|---:|---:|---|---:|
| 9575 | 200 | not strictly copositive | 14.207 s |
| 9651 | 378 | timeout | 600 s cutoff |

Thus the longer retry resolves the Brock matrix but not the MANN/Steiner matrix. Both rows and the rebuilt-module hash are retained in
the database; the original ten-second results remain separate under their earlier hash.
