# Reference Results: Literature-Unsolved Matrices

## Scope

This campaign selected every matrix that then had at least one explicit literature failure claim in `references_unsolved`: 173 matrices of
orders 8–3,361, comprising 39 strictly copositive and 134 not-strictly-copositive cases. A claim can concern a different algorithm or
decision formulation; this run measures whether coposit's three fastest decision-diagram Dickinson variants can classify the same
matrix.

Each model uses strict-copositivity mode, both preprocessing stages, a ten-second per-matrix cutoff, CPU 2 for dispatch and serialized
SQLite writes, and persistent native workers on CPUs 3–9. `--rerun` measures every selected matrix with the listed native module.
Cutoff-substituted work uses measured native time for completions and ten seconds for each timeout.

## Results

| Model | Native SHA-256 | Solved | Timeout | Strict | Not strict | Mismatch | Substituted work | Completed median | Wall time |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ZDD-Zed Dickinson | `35cfa7ca43250947ef69668de8cea70f4a149839cd597ed43e9c92a679cb648d` | 154 | 19 | 39 | 115 | 0 | 219.998 s | 0.667 ms | 36.479 s |
| CBDD-Zed Dickinson | `0a117367b191a25c2a72fc7a9f84780a92eac23f1298be41bdba926efc14eea8` | 154 | 19 | 39 | 115 | 0 | 219.560 s | 1.223 ms | 36.036 s |
| CZDD-Zed Dickinson | `b31ec652f69bf53b66e3ea13af848c3e173a8c4d891574c5d010fe5cfcc29483` | 154 | 19 | 39 | 115 | 0 | 219.803 s | 0.680 ms | 36.188 s |

All three models solved the same 154 matrices and timed out on the same 19. Every completed classification matches the stored truth;
there was no node limit, parse error, execution error, or model disagreement. Thus these methods classify 89.0% of the matrices that
carry an explicit failure claim from another literature algorithm within ten seconds.

The common timeout tail consists entirely of DIMACS clique encodings:

| Family | Count | Orders | Matrix IDs |
|---|---:|---|---|
| Brock | 11 | 200, 400, 800 | 9574, 9575, 9583, 9586, 9589, 9592, 9595, 9598, 9601, 9604, 9607 |
| Hamming | 2 | 1,024 | 9622, 9625 |
| Keller | 2 | 776, 3,361 | 9643, 9645 |
| MANN / Steiner triple system | 4 | 378, 1,035, 3,321 | 9650, 9651, 9653, 9655 |

CBDD-Zed has the lowest cutoff-substituted work and wall time, but its advantage is below half a second of substituted work; at this
cutoff, the three representations have identical coverage.
