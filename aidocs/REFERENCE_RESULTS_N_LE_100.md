# Reference Results: N ≤ 100 Decision Diagrams

## Scope

The original campaign ran every matrix then in the generated `n_le_100` set: all 2,619 matrices retained at that point with order at
most 100. Each model uses
strict-copositivity mode, both preprocessing stages, a ten-second per-matrix cutoff, CPU 2 for dispatch and serialized SQLite writes,
and persistent native workers on CPUs 3–9. `--rerun` ensures that every selected matrix is measured with the listed native module.

That snapshot contains 813 strictly copositive, 1,075 boundary, 718 non-copositive, and 13 truth-unknown matrices. A completion on an unknown
matrix is reported as unverified rather than as a mismatch. Cutoff-substituted work uses measured native time for completions and ten
seconds for every timeout.

## Results

| Model | Native SHA-256 | Solved | Timeout | Known strict solved | Known not-strict solved | Unknown solved | Unknown timeout | Mismatch | Substituted work | Wall time |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ZDD-Zed Dickinson | `8b35bc2489db0086fb3c6fcfe1c9f7579ecde786ebd37df843ca7476630a6c78` | 2,599 | 20 | 809 | 1,781 | 9 | 4 | 0 | 245.407 s | 44.005 s |
| CBDD-Zed Dickinson | `f2141101bb5d2c848d212702c228e2ae8985d021ffc7cc28becb5bd26efc0955` | 2,599 | 20 | 809 | 1,781 | 9 | 4 | 0 | 245.768 s | 44.084 s |
| CZDD-Zed Dickinson | `61f0df6855788983bd69952a9633df91be1ee3711f4c4408dcd7faa0f6c1de2f` | 2,599 | 20 | 809 | 1,781 | 9 | 4 | 0 | 248.222 s | 44.656 s |

All three models have exactly the same completion and timeout sets. The 20 common timeouts are twelve Hildebrand circulant boundary
matrices at orders 17–25, four known-strict graph matrices at orders 45–70, and four truth-unknown order-40/50 matrices. No model
produced a node limit, parse error, or execution error.

The effective-time medians are 0.004584 ms for ZDD, 0.004959 ms for CBDD, and 0.004500 ms for CZDD. Relative to ZDD, CBDD's total
cutoff-substituted work is 0.15% higher and CZDD's is 1.15% higher. The completion result is therefore a tie; this campaign provides
no evidence that chain reduction rescues an order-at-most-100 case at ten seconds.

## Previously Unknown Truth

All three exact models agree on every completed truth-unknown matrix: IDs 10777, 10778, 10779, 10781, 10782, 12074, 12178, and 12201
return strictly copositive, while ID 12181 returns not strictly copositive. IDs 12090, 12091, 12198, and 12214 time out in all three
models. These runs remain stored as unverified experiment results; the corpus truth fields were not changed because three models do
not meet the previously chosen four-model consensus threshold.

## New-import combined classification

After a later import, the generated set grew automatically to 3,084 matrices. The 465 new order-at-most-100 rows, IDs 12523–13023,
were run with CBDD-Zed Dickinson in combined mode, both preprocessing stages, and a 30-second cutoff. CPU 2 handled dispatch and
serialized database writes; CPUs 3–9 ran seven native workers.

| Native SHA-256 | Classified | Strict | Boundary | Not copositive | Timeout | Known mismatch | Substituted work | Wall time |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `73a11c9c5a045136a3364541558ec2ee7b65396dc7affadcdb880f3affaf3dd6` | 306 | 97 | 0 | 209 | 159 | 0 | 5,380.289 s | 804.119 s |

All 15 matrices with existing truth completed and matched. Of the 450 truth-unknown imports, 291 received a combined experimental
classification and 159 timed out. The experiment rows remain separate from maintained corpus truth.
