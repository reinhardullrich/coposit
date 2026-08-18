# Diagnostics Reporting

Long command-line runs can opt into a diagnostics line every second:

```bash
cpp/build/coposit --model adaptive_sponsel_copomatrix --mode non-strict --diagnostics '2#1,-1,1'
cpp/build/coposit --model dickinson_2019 --mode both --diagnostics matrix.mtx
```

Diagnostics is written to standard error. Standard output remains the single final `true` or `false`, so redirecting or parsing the
result does not change. Sequential Python calls can request the same lines with `diagnostics=True`; the multiprocessing and
reference-runner interfaces do not emit them.

Every line begins with an explicit stage. `stage=preprocessing` is followed by the current pre-check or component-pipeline phase.
`stage=model` appears only after delegation to the selected algorithm and names that algorithm's diagnostics metric. The first model
support and its support-specific outcome counters are published immediately. Other hot-loop snapshots remain batched every 4,096
visited units. The experimental FracESSA Circular model uses batches of 256 bracelets because each representative includes a larger
exact candidate calculation. CBDD and CZDD publish their decision-diagram counters every 200 recursive diagram operations.

The experiment command also reports its current preprocessing phase. Depending on the selected path, this is
`matrix scan`, `root checks`, `connected components`, `component scan`, `principal submatrices`, `negative-part diagonal dominance`,
`all-ones`, `Frank-Wolfe`, `Motzkin-Straus`, `exact factorization`, `negative-part factorization`, `Z-matrix`, `Danninger`, `COPOMATRIX`, or
`model delegation`. Where a truthful unit exists,
`work=current/maximum` reports rows, graph vertices, principal-face centers, Frank–Wolfe iterations, exact factorization pivots, or
candidate reduction pivots. The line also identifies the current matrix or component dimension. These counters show activity inside
the current phase; they are not percentages or ETAs.

## What The Percentage Means

The percentage names its denominator because the algorithms do not share one meaningful notion of completed work:

| Models | Report | Definition |
|---|---|---|
| Hadeler 1983, Dickinson 2019 | `metric=support` | Visited nonempty principal supports divided by $2^n-1$; Dickinson also separates certificate hits, exact processing, and retained certificates. |
| Dense-Bitset Dickinson | `metric=support` | Exact systems plus newly cleared live bitmap bits divided by $2^n-1$; zero bitmap bits are jumped over without support construction. |
| Ceiling-Pruned Dickinson | `metric=support` | Emitted supports plus exactly counted forbidden-branch skips divided by $2^n-1$; exact processing, retained ceiling certificates, and their sparse $(k,d)$ distribution are separate. |
| CBDD and CZDD experiments | `metric=decision-diagram` | Current phase and cardinality, time in that cardinality, supports emitted for exact processing, retained certificates, allocated diagram nodes, and cumulative diagram operations; no percentage is claimed. |
| Bundfuss 2008, Sponsel 2012 | `metric=simplex` | Sum of the certified child-simplex volumes relative to the original simplex. |
| Du Tour 2018 | `metric=proof` | The parent obligation is divided equally between its two required child cones. |
| Danninger 1990, COPOMATRIX 2011 | `metric=proof` | Sum of completed recursive proof-obligation weights. A parent weight is divided equally among its required children. |
| Adaptive Sponsel–COPOMATRIX | `metric=adaptive` | The same proof weight, plus separate Sponsel and COPOMATRIX routing and work counters. |
| Safi 2021 | `metric=traversal` | Nodes, certificates, splits, open nodes, and depth; no percentage is claimed. |
| FracESSA Circular experiment | `metric=bracelet` | Cardinality, emitted bracelets, affine skips, exact KKT tests, and accepted candidates; no percentage is claimed. |

Support coverage is the exact position in an exhaustive support enumeration. Simplex coverage is geometric. Du Tour is not labeled
as geometric volume because its stored cone rays are not kept under a common affine normalization. Proof coverage is a monotone
bookkeeping measure that reaches 100% when every required child is certified, but it is not a geometric volume.

None of these percentages is an estimate of remaining wall time. Exact arithmetic and branching costs vary: the final support or a
small remaining region can take longer than everything visited before it. A negative witness can also terminate a run before its
enumeration or positive proof reaches 100%.

For Dickinson, `visited` counts every enumerated support. `covered` counts supports discharged by an already retained certificate,
without forming or factoring their principal matrix. `processed` counts uncovered supports sent to the exact solve or nullspace
path. `certificates` increments only after the resulting signature has been stored. A terminating negative witness or strict zero is
therefore processed but is not a certificate. The displayed `coverage` percentage is `visited / (2^n-1)`; the certificate-hit share
is instead `covered / visited`.

For Dense-Bitset Dickinson, `visited` counts a support when its exact system is processed and also counts every other live bit first
cleared by that support's certificate. Already-zero bits are not counted twice. Thus its percentage is exact Boolean-lattice coverage,
although covered supports are never reconstructed or individually visited.

For Ceiling-Pruned Dickinson, `visited` is the sum of emitted supports and supports skipped in complete forbidden recursive branches.
Each skipped branch has the exact binomial size $\binom{r}{s}$, so the displayed support coverage remains meaningful without
enumerating its members. `covered` is that skipped count, `processed` counts exact Dickinson systems, and `certificates` counts only
certificates with full upper endpoint. `certificate_k_d_counts` records their generating cardinality $k$ and $d=n-|L|$.

Layered Singular-Lift Dickinson and Breadth-First Singular-Lift Dickinson add exact work outside that outer enumeration.
`processed = outer_processed + lifted_processed`; duplicate and already-covered lift routes, cache size, current and maximum lifted
cardinality and depth are reported separately. The breadth-first model also reports its current and maximum FIFO frontier.
`certificate_root_k_lifted_k_u_l_counts=[(root_k,lifted_k,upper_size,lower_size,count),...]` records the root cardinality where
lifting began, the lifted principal-matrix order where the vector was obtained, $|U|$, $|L|$, and the frequency. Here every retained
certificate is a ceiling certificate, so `upper_size` equals the matrix order, but it remains explicit.

CBDD and CZDD remove covered support families symbolically, so counting every member of those families would defeat the
algorithm. Their diagnostics line therefore does not reuse Dickinson's exhaustive-support percentage. `emitted_supports` counts only
uncovered supports sent to the exact solve. `dd_nodes_allocated` is the cumulative number of diagram nodes allocated, not the live
root size, and `dd_operations` counts recursive union and subtraction operations. The phases distinguish cardinality-family
construction, exact support solving, certificate union, and certificate subtraction. `cardinality_elapsed`
resets exactly when a new cardinality begins.

The serial CBDD and CZDD lines also report
`certificate_k_d_u_counts=[(k,d,upper_size,count),...]`: `k` is the generating support cardinality, `d=|U|-|L|` is the number of
free indices, `upper_size=|U|` is the upper endpoint's cardinality, and `count` is the frequency of that exact combination. This
histogram exists only when visible diagnostics or explicit diagnostics capture enables telemetry.

When enabled, the fixed preprocessing pipeline runs before model traversal. The selected model receives only unresolved connected-
component matrices, so each model-stage diagnostics line describes the current delegated component.

Every preprocessing and delegated-model line also retains the summary of the original matrix. `preprocessing_outcome` is `running`
until preprocessing returns, then `resolved` when no component remains for the model or `pending` when one or more components remain.
`component_split`, `components_seen`, and `largest_component` describe the top-level negative-entry connected components actually
visited. `pending_components` and `largest_pending_component` describe the unresolved matrices returned to the dispatcher.
`reduction_child_checks`, `maximum_reduction_depth`, and `reduction_decisions` describe the certificate-only Danninger and COPOMATRIX
work; their children are never delegated. `model_delegations` counts actual calls to the selected model. On interruption, the
`running` summary remains partial by design.

Adaptive Sponsel–COPOMATRIX additionally reports `engine` and `phase`. The phases distinguish pivot routing, Sponsel edge search,
`H` construction and exact factorization, exact split construction, and COPOMATRIX partition, principal-block, Schur-block,
staircase, and transform work. `sponsel_nodes` and `copomatrix_nodes` count routing decisions; `sponsel_splits` counts actual
two-child Sponsel splits; `copomatrix_children` counts projection children entered; `staircase` counts Xu–Yao staircase states;
`forced_copomatrix` counts cutoff-triggered switches. `streak`, the selected one-based `pivot`, its immediate `pivot_children`, the
current dimension, and the exact-factorization pivot counter provide the current local context. A saturated immediate-child count is
printed with `>=`. These are activity counters, not estimates of remaining runtime.

## Runtime Cost

The solver never queries the clock or formats output in its hot loop. With diagnostics enabled, the decision-diagram models read the
clock once when each cardinality begins. Support models publish their small relaxed-atomic counters after every support event. Other
model counters publish once per 4,096 visited units, plus coarse phase changes. Adaptive staircase and child counters retain the same
4,096-unit batching; FracESSA Circular publishes once per 256 bracelets; exact `H` factorization reports once per pivot. A separate
sleeping thread wakes every second to read and print the latest snapshot. Preprocessing phase changes use the same relaxed-atomic
snapshot. Geometric and proof weights are approximate telemetry only and no telemetry participates in an exact mathematical
decision. On normal completion the reporter prints one final snapshot, even when less than one second has passed since its previous
line, so terminating work and retained-certificate counts are not lost. Without `--diagnostics`, no reporter thread exists and diagnostics
calls return immediately. The decision-diagram models retain only a null diagnostics pointer in that case; their only added hot-loop
check is the existing 4,096-operation timeout checkpoint. With diagnostics enabled, a separately compiled path publishes its latest
diagram counters every 200 operations while retaining the same timeout cadence.
