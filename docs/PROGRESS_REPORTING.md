# Progress Reporting

Long command-line runs can opt into a progress line every second:

```bash
cpp/build/coposit safe strict --progress matrix.mtx
cpp/build/coposit fast non-strict --progress '2#1,-1,1'
cpp/build/coposit-analyze --model dickinson_final --mode strict --progress matrix.mtx
```

Progress is written to standard error. Standard output remains the single final `true` or `false`, so redirecting or parsing the
result does not change. The Python and reference-runner interfaces do not emit these interactive status lines.

Every line begins with an explicit stage. `stage=preprocessing` is followed by the current pre-check or component-pipeline phase.
`stage=model` appears only after delegation to the selected algorithm and names that algorithm's progress metric. The first model
node is published immediately; later hot-loop snapshots remain batched every 4,096 visited units.

The public `fast` and `safe` commands also report their current preprocessing phase. Depending on the selected path, this is
`matrix scan`, `cheap certificates`, `principal submatrices`, `connected components`, `component scan`, `Frank-Wolfe`,
`exact factorization`, or `model delegation`. Where a truthful unit exists, `work=current/maximum` reports rows, graph vertices,
principal-face centers, Frank–Wolfe iterations, or exact factorization pivots. The line also identifies the current matrix or
component dimension. These counters show activity inside the current phase; they are not percentages or ETAs.

## What The Percentage Means

The percentage names its denominator because the algorithms do not share one meaningful notion of completed work:

| Models | Report | Definition |
|---|---|---|
| Hadeler 1983, Dickinson 2019, Dickinson Final | `metric=support` | Visited nonempty principal supports divided by $2^n-1$. |
| Bundfuss 2008, Sponsel 2012 | `metric=simplex` | Sum of the certified child-simplex volumes relative to the original simplex. |
| Du Tour 2018 | `metric=proof` | The parent obligation is divided equally between its two required child cones. |
| Danninger 1990, COPOMATRIX 2011 | `metric=proof` | Sum of completed recursive proof-obligation weights. A parent weight is divided equally among its required children. |
| Adaptive Sponsel–COPOMATRIX | `metric=adaptive` | The same proof weight, plus separate Sponsel and COPOMATRIX routing and work counters. |
| Safi 2021 | `metric=traversal` | Nodes, certificates, splits, open nodes, and depth; no percentage is claimed. |

Support coverage is the exact position in an exhaustive support enumeration. Simplex coverage is geometric. Du Tour is not labeled
as geometric volume because its stored cone rays are not kept under a common affine normalization. Proof coverage is a monotone
bookkeeping measure that reaches 100% when every required child is certified, but it is not a geometric volume.

None of these percentages is an estimate of remaining wall time. Exact arithmetic and branching costs vary: the final support or a
small remaining region can take longer than everything visited before it. A negative witness can also terminate a run before its
enumeration or positive proof reaches 100%.

The public `fast` and `safe` commands use their fused pre-check/component pipeline before model traversal. A model percentage is
local to the component currently being solved; it is not combined across components with unrelated search spaces.

Adaptive Sponsel–COPOMATRIX additionally reports `engine` and `phase`. The phases distinguish pivot routing, Sponsel edge search,
`H` construction and exact factorization, exact split construction, and COPOMATRIX partition, principal-block, Schur-block,
staircase, and transform work. `sponsel_nodes` and `copomatrix_nodes` count routing decisions; `sponsel_splits` counts actual
two-child Sponsel splits; `copomatrix_children` counts projection children entered; `staircase` counts Xu–Yao staircase states;
`forced_copomatrix` counts cutoff-triggered switches. `streak`, the selected one-based `pivot`, its immediate `pivot_children`, the
current dimension, and the exact-factorization pivot counter provide the current local context. A saturated immediate-child count is
printed with `>=`. These are activity counters, not estimates of remaining runtime.

## Runtime Cost

The solver never queries the clock and never formats output in its hot loop. With progress enabled it updates ordinary local
counters and publishes a relaxed-atomic snapshot once per 4,096 visited units, plus coarse phase changes. Adaptive staircase and
child counters retain the same 4,096-unit batching; exact `H` factorization reports once per pivot, not per matrix entry. A separate
sleeping thread wakes every second to read and print the latest snapshot. Preprocessing phase changes use the same relaxed-atomic
snapshot. Geometric and proof weights are approximate telemetry only and no telemetry participates in an exact mathematical
decision. Without `--progress`, no reporter thread exists and progress calls return immediately.
