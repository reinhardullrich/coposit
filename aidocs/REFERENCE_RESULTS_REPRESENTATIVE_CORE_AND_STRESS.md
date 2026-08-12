# Reference Results For Representative Core And Stress Test

Last updated: 2026-08-11

Status: current. All Adaptive Sponsel–COPOMATRIX rows use the minimum-child pivot rule.

## Benchmark And Notation

| Set | Matrices | Orders | Strict | Boundary copositive | Not copositive |
|---|---:|---:|---:|---:|---:|
| Representative Core | 384 | 1–100 | 192 | 110 | 82 |
| Stress Test | 240 | 5–3,361 | 131 | 96 | 13 |

The sets overlap on 100 matrices, so their union contains 524 distinct matrices. The uniform 36-batch campaign used a five-second
per-matrix cutoff, CPU 3 for dispatch and serialized SQLite writes, and three persistent native workers on CPUs 4–6. After the
Adaptive pivot correction, all eight of its mode/preprocessing batches were repeated once under the same conditions.

`Strict` and `non-strict` name the requested decision mode. In result cells, `solved/unresolved` counts matrices; unresolved means
timeout, node limit, or execution error, never a negative classification. `Work` substitutes five seconds for each unresolved result.
Truth coverage is written `strict / boundary / not copositive`, with each entry shown as completed/available.

## Both Preprocessing Stages — Requested Detailed Results

`Both` uses the fused pipeline: globally valid checks run during the root scan, negative-entry components are then visited, and
Frank–Wolfe plus exact definiteness are deferred to each component. Principal-submatrix pre-checks run at the root with the restored
cardinality-three default.

| Model | Mode | Core solved/unresolved | Core work | Core truth: strict / boundary / not | Stress solved/unresolved | Stress work | Stress truth: strict / boundary / not | Union solved/unresolved | Union wall |
|---|---|---:|---:|---|---:|---:|---|---:|---:|
| Dickinson 2019 | Strict | 319/65 | 333.184 s | 135/192 · 105/110 · 79/82 | 116/124 | 625.546 s | 34/131 · 74/96 · 8/13 | 380/144 | 246.954 s |
| Dickinson 2019 | Non-strict | 296/88 | 450.854 s | 135/192 · 82/110 · 79/82 | 107/133 | 671.253 s | 34/131 · 65/96 · 8/13 | 353/171 | 292.611 s |
| Adaptive Sponsel–COPOMATRIX | Strict | 367/17 | 111.698 s | 183/192 · 102/110 · 82/82 | 183/57 | 361.423 s | 107/131 · 65/96 · 11/13 | 465/59 | 126.448 s |
| Adaptive Sponsel–COPOMATRIX | Non-strict | 345/39 | 226.698 s | 183/192 · 84/110 · 78/82 | 168/72 | 445.770 s | 107/131 · 53/96 · 8/13 | 438/86 | 175.481 s |

All 2,096 rows are present. Every unresolved result is a timeout; there are no node limits, execution errors, corpus mismatches, or
impossible `non-strict=false, strict=true` combinations among results completed in both modes.

## Preprocessing Comparison

`None` is the normal linked model. `Components` enables only connected-component splitting; `pre-checks` runs the checks on the
whole matrix; `both` uses the fused root-check/component pipeline described above. Core and Stress columns are completed counts.
Union columns are `solved/unresolved`; wall time is `strict/non-strict`.

| Model | Preprocessing | Core strict | Core non-strict | Stress strict | Stress non-strict | Union strict | Union non-strict | Wall strict/non-strict |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Dickinson 2019 | None | 317 | 290 | 114 | 102 | 376/148 | 343/181 | 254.148/310.433 s |
| Dickinson 2019 | Components | 317 | 291 | 114 | 102 | 376/148 | 344/180 | 128.158/156.443 s |
| Dickinson 2019 | Pre-checks | **319** | 295 | **116** | 105 | **380/144** | 351/173 | 126.200/149.498 s |
| Dickinson 2019 | Both | **319** | **296** | **116** | **107** | **380/144** | **353/171** | 246.954/292.611 s |
| Adaptive Sponsel–COPOMATRIX | None | 364 | 340 | 178 | 162 | 457/67 | 428/96 | 140.637/191.319 s |
| Adaptive Sponsel–COPOMATRIX | Components | 365 | 340 | 180 | 162 | 460/64 | 428/96 | 135.340/194.089 s |
| Adaptive Sponsel–COPOMATRIX | Pre-checks | **367** | 344 | **182** | 166 | **464/60** | 436/88 | 128.227/178.756 s |
| Adaptive Sponsel–COPOMATRIX | Both | **367** | **345** | **183** | **168** | **465/59** | **438/86** | 126.448/175.481 s |

Both stages give the largest completion count: 733/1,048 Dickinson results and 903/1,048 Adaptive Sponsel–COPOMATRIX results across
the two modes. Relative to pre-checks alone, component splitting adds two Dickinson and three adaptive union completions.

The six enabled preprocessing configurations contain 6,288 rows: 4,875 completed and 1,413 timed out. There are zero node limits,
errors, mismatches, or logical contradictions. Every Adaptive configuration now uses the same minimum-child binary and three-worker
layout. Dickinson's components-only and pre-checks-only wall measurements retain their earlier six-worker layout, so those dispatcher
wall times are not directly comparable across models.

## Literature Baselines: Normal Versus Both Preprocessing Stages

All 16 mode-specific baseline batches were rerun. Each Core and Stress cell is
`normal → both (gain)` in completed matrices. Union cells show only the both-stage run as `solved/unresolved`; wall time is
`strict/non-strict` for the both-stage runs.

| Model | Core strict | Core non-strict | Stress strict | Stress non-strict | Both union strict | Both union non-strict | Both wall strict/non-strict |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dutour 2018 | 278 → 289 (+11) | 240 → 249 (+9) | 58 → 91 (+33) | 34 → 53 (+19) | 336/188 | 274/250 | 321.297/408.811 s |
| Danninger 1990 | 254 → 268 (+14) | 219 → 235 (+16) | 65 → 72 (+7) | 55 → 63 (+8) | 316/208 | 278/246 | 352.245/417.486 s |
| COPOMATRIX 2011 | 306 → 313 (+7) | 280 → 290 (+10) | 82 → 89 (+7) | 73 → 81 (+8) | 369/155 | 342/182 | 266.231/309.493 s |
| Hadeler 1983 | 307 → 310 (+3) | 271 → 281 (+10) | 110 → 112 (+2) | 95 → 100 (+5) | 370/154 | 335/189 | 271.305/334.924 s |
| Dickinson 2019 | 317 → 319 (+2) | 290 → 296 (+6) | 114 → 116 (+2) | 102 → 107 (+5) | 380/144 | **353/171** | 246.954/292.611 s |
| Safi 2021 | 292 → 301 (+9) | 257 → 274 (+17) | 68 → 85 (+17) | 54 → 76 (+22) | 353/171 | 320/204 | 291.601/349.399 s |
| Bundfuss 2008 | 278 → 303 (+25) | 234 → 258 (+24) | 45 → 87 (+42) | 33 → 56 (+23) | 357/167 | 291/233 | 288.322/398.070 s |
| Sponsel 2012 | 304 → 324 (+20) | 262 → 275 (+13) | 66 → 106 (+40) | 64 → 69 (+5) | **381/143** | 308/216 | 248.575/368.784 s |

Both stages improve every one of the 32 model/set/mode comparisons. Across the eight baselines and two modes, completed union results
increase from 4,991 to 5,363: 372 additional decisions. The largest individual gains are Bundfuss Stress strict (+42), Sponsel Stress
strict (+40), and Dutour Stress strict (+33). Sponsel has the largest strict union completion count, one ahead of Dickinson; Dickinson
has the largest non-strict count.

The 8,384 both-stage result rows contain 5,363 completions, 3,009 timeouts, and 12 Dutour non-strict node limits. There are zero execution
errors, zero corpus mismatches, and zero impossible `non-strict=false, strict=true` combinations among the 2,495 pairs completed in both
modes. Every current batch completed without interruption.

## Normal Model Results

These are runs without external preprocessing. Literature baselines remain separate in meaning from the Coposit-created adaptive
model, even though one table keeps their benchmark data together. Each result cell is `solved/unresolved · substituted work`.

| Model | Kind | Core strict | Core non-strict | Stress strict | Stress non-strict | Union wall strict/non-strict |
|---|---|---|---|---|---|---:|
| Dutour 2018 | Baseline | 278/106 · 547.954 s | 240/144 · 742.249 s | 58/182 · 919.706 s | 34/206 · 1,043.003 s | 366.291/449.763 s |
| Danninger 1990 | Baseline | 254/130 · 668.297 s | 219/165 · 838.432 s | 65/175 · 875.186 s | 55/185 · 925.923 s | 389.611/455.902 s |
| COPOMATRIX 2011 | Baseline | 306/78 · 403.759 s | 280/104 · 540.669 s | 82/158 · 791.991 s | 73/167 · 840.301 s | 289.646/339.203 s |
| Hadeler 1983 | Baseline | 307/77 · 405.891 s | 271/113 · 598.904 s | 110/130 · 676.013 s | 95/145 · 756.587 s | 277.307/357.413 s |
| Dickinson 2019 | Baseline | **317/67 · 343.272 s** | **290/94 · 485.884 s** | **114/126 · 636.027 s** | **102/138 · 696.554 s** | 254.148/310.433 s |
| Safi 2021 | Baseline | 292/92 · 473.765 s | 257/127 · 663.151 s | 68/172 · 862.311 s | 54/186 · 936.151 s | 332.837/411.604 s |
| Bundfuss 2008 | Baseline | 278/106 · 556.016 s | 234/150 · 775.221 s | 45/195 · 989.963 s | 33/207 · 1,050.254 s | 382.249/467.257 s |
| Sponsel 2012 | Baseline | 304/80 · 422.908 s | 262/122 · 629.423 s | 66/174 · 888.847 s | 64/176 · 895.346 s | 330.404/396.131 s |
| Adaptive Sponsel–COPOMATRIX | Coposit-created | 364/20 · 129.692 s | 340/44 · 251.757 s | 178/62 · 386.996 s | 162/78 · 468.420 s | 140.637/191.319 s |

Relative to the preceding first-narrow-pivot binary, the minimum-child rule changed no completed Boolean classification but moved
four calls from completed to timeout and rescued none: strict matrix 10488 and non-strict matrices 9648, 10044, and 10489. Three old
calls were already within 0.47 seconds of the cutoff; matrix 9648 is the material algorithmic regression, changing from 0.022 seconds
to timeout. Substituted union work increased by 7.492 seconds in strict mode and 9.857 seconds in non-strict mode.

### Completed Coverage By Stored Truth

Each cell is `strict / boundary / not copositive`, with completed/available counts.

| Model | Core strict mode | Core non-strict mode | Stress strict mode | Stress non-strict mode |
|---|---|---|---|---|
| Dutour 2018 | 100/192 · 96/110 · 82/82 | 100/192 · 60/110 · 80/82 | 10/131 · 37/96 · 11/13 | 10/131 · 14/96 · 10/13 |
| Danninger 1990 | 98/192 · 93/110 · 63/82 | 98/192 · 64/110 · 57/82 | 10/131 · 55/96 · 0/13 | 10/131 · 45/96 · 0/13 |
| COPOMATRIX 2011 | 133/192 · 97/110 · 76/82 | 133/192 · 78/110 · 69/82 | 24/131 · 53/96 · 5/13 | 24/131 · 47/96 · 2/13 |
| Hadeler 1983 | 123/192 · 105/110 · 79/82 | 123/192 · 69/110 · 79/82 | 29/131 · 73/96 · 8/13 | 29/131 · 58/96 · 8/13 |
| Dickinson 2019 | 133/192 · 105/110 · 79/82 | 133/192 · 78/110 · 79/82 | 33/131 · 74/96 · 7/13 | 33/131 · 62/96 · 7/13 |
| Safi 2021 | 118/192 · 96/110 · 78/82 | 118/192 · 64/110 · 75/82 | 17/131 · 46/96 · 5/13 | 17/131 · 32/96 · 5/13 |
| Bundfuss 2008 | 126/192 · 83/110 · 69/82 | 126/192 · 53/110 · 55/82 | 23/131 · 20/96 · 2/13 | 23/131 · 8/96 · 2/13 |
| Sponsel 2012 | 147/192 · 86/110 · 71/82 | 147/192 · 60/110 · 55/82 | 39/131 · 24/96 · 3/13 | 39/131 · 23/96 · 2/13 |
| Adaptive Sponsel–COPOMATRIX | 181/192 · 101/110 · 82/82 | 181/192 · 81/110 · 78/82 | 105/131 · 63/96 · 10/13 | 105/131 · 50/96 · 7/13 |

### Literature-Baseline Completion Overlap

| Set | Mode | All 8 solve | 4–7 solve | 1–3 solve | None solve |
|---|---|---:|---:|---:|---:|
| Representative Core | Strict | 216 | 101 | 33 | 34 |
| Representative Core | Non-strict | 174 | 111 | 46 | 53 |
| Stress Test | Strict | 18 | 80 | 37 | 105 |
| Stress Test | Non-strict | 6 | 75 | 42 | 117 |

Dickinson has the largest completion count among faithful baselines in both modes on both sets. Adaptive Sponsel–COPOMATRIX completes
47 more Core and 64 more Stress matrices in strict mode, and 50 more Core and 60 more Stress matrices in non-strict mode. These are
five-second-cutoff benchmark observations, not claims of mathematical dominance.

The eight baseline strict runs took 2,622.493 seconds of dispatcher wall time; their non-strict runs took 3,187.706 seconds. All completed
classifications match the corpus. Dutour's unresolved counts include three Core and ten Stress node limits in each mode. Danninger's
non-strict Stress result includes one unresolved node limit on matrix 10244; the bounded iterative traversal replaces the historical
stack-exhaustion crash without turning the resource outcome into a classification.

## Recorded Native Modules

| Model | Normal SHA-256 | Both-preprocessing SHA-256 |
|---|---|---|
| Dutour 2018 | `2c93638b3f15d8403d52bf16137d7022f317353bc4f81cb60b3fc0ada5087cb1` | `2c93638b3f15d8403d52bf16137d7022f317353bc4f81cb60b3fc0ada5087cb1` |
| Danninger 1990 | `8ec1d80a8673cc98ef853a1593fa6b3f9155a094a903c52e61efd5907deb9bfe` | `8ec1d80a8673cc98ef853a1593fa6b3f9155a094a903c52e61efd5907deb9bfe` |
| COPOMATRIX 2011 | `68543778fe8c82702a21ae01a7915216b181cc51728ead50a0981f08209af075` | `68543778fe8c82702a21ae01a7915216b181cc51728ead50a0981f08209af075` |
| Hadeler 1983 | `1ecd99e49df73f3955c8a2684f84c5db7e7f3420d785c452bbbf7ed5a16b2c24` | `1ecd99e49df73f3955c8a2684f84c5db7e7f3420d785c452bbbf7ed5a16b2c24` |
| Dickinson 2019 | `e34d25851717f3ce018cfefb605aff189511e715992eb5642958cb68b916b726` | `e34d25851717f3ce018cfefb605aff189511e715992eb5642958cb68b916b726` |
| Safi 2021 | `94b5e8b549113ce5d12ec73d03e52558297b1a42aacbd54bc060f53e659f04d1` | `94b5e8b549113ce5d12ec73d03e52558297b1a42aacbd54bc060f53e659f04d1` |
| Bundfuss 2008 | `29091289c0be49889035976249f77a1ed32542ec34325eecd2ba94337fb4370d` | `29091289c0be49889035976249f77a1ed32542ec34325eecd2ba94337fb4370d` |
| Sponsel 2012 | `a360fa203b398709b65399669abba8f8bbe7db9b0a52d05aa0d649f08e119887` | `a360fa203b398709b65399669abba8f8bbe7db9b0a52d05aa0d649f08e119887` |
| Adaptive Sponsel–COPOMATRIX | `249d413159ae27519472c474d68eb697a57af1ceb06b2129ab69d7f796856977` | `249d413159ae27519472c474d68eb697a57af1ceb06b2129ab69d7f796856977` |
