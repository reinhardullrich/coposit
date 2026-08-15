# Reference Results For Representative Core And Stress Test

Last updated: 2026-08-14

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

### Dickinson interval-enumeration experiments

The five original strict-only experiments retain Dickinson Final's exact principal calculation and differ only in support
enumeration. The later SAT-Zed row uses the same strict decision and adds its rejection-only maximal-Zed stage. These batches used
the same five-second cutoff and both preprocessing stages, with CPU 3 for the parent and four workers on CPUs 4–7. They are separate
from the earlier uniform three-worker campaign; `work` is the comparable one-core total obtained by substituting five seconds for
every timeout.

| Model | Core solved/unresolved | Core work | Core truth: strict / boundary / not | Stress solved/unresolved | Stress work | Stress truth: strict / boundary / not | Union solved/unresolved | Union work | Four-worker wall |
|---|---:|---:|---|---:|---:|---|---:|---:|---:|
| Interval-Recursive Dickinson | 318/66 | 338.604 s | 134/192 · 105/110 · 79/82 | 117/123 | 620.706 s | 35/131 · 74/96 · 8/13 | 379/145 | 738.007 s | 187.387 s |
| Interval-BDD Dickinson | 371/13 | 75.464 s | 188/192 · 104/110 · 79/82 | 202/38 | 205.160 s | 121/131 · 73/96 · 8/13 | 486/38 | 205.397 s | 55.071 s |
| Interval-ZDD Dickinson | 369/15 | 76.395 s | 187/192 · 103/110 · 79/82 | 200/40 | 206.481 s | 120/131 · 72/96 · 8/13 | 484/40 | 206.816 s | 55.316 s |
| SAT-Zed Dickinson | **373/11** | **57.308 s** | 189/192 · 105/110 · 79/82 | **204/36** | **189.038 s** | 122/131 · 74/96 · 8/13 | **488/36** | **189.238 s** | **48.202 s** |
| Cardinality-BDD Dickinson | 368/16 | 123.602 s | 184/192 · 105/110 · 79/82 | 191/49 | 354.106 s | 109/131 · 74/96 · 8/13 | 475/49 | 359.111 s | 92.578 s |
| Cardinality-ZDD Dickinson | 368/16 | 124.059 s | 184/192 · 105/110 · 79/82 | 190/50 | 358.785 s | 108/131 · 74/96 · 8/13 | 474/50 | 364.684 s | 94.629 s |

BDD and ZDD completed 484 common matrices. BDD additionally completed Johnson matrices 9627 and 9628, both of order 28; ZDD had no
completion that BDD missed. Across their common completions, BDD's median per-matrix time was 11.90% lower than ZDD's. All 1,572
rows are present, all 1,349 completed decisions match corpus truth, and all 223 unresolved rows are timeouts. There are no node
limits, parse or execution errors. The retained Recursive, BDD, and ZDD binary hashes are
`3bfbbe88b9e33b518bf590fc8228208efd592287265879add2bb127dfb263c26`,
`e69cde8969e1337e7e7e430b882f230b881dcf818726f13973b38373bfaa9fbd`, and
`94804e744aa2807ca331801fc74250bd661b5d1bca3e35a1a95d881984ae3693`, respectively.

SAT-Zed completed every matrix completed by Interval-Recursive plus 109 more, every Interval-BDD completion plus two more, and every
Interval-ZDD completion plus four more. All 488 completed classifications match corpus truth; the other 36 are timeouts. On the 379
common Interval-Recursive completions, SAT's median paired relative time was 7.672% higher, so its much lower substituted work comes
from avoiding hard-case timeouts rather than accelerating the typical easy common matrix. Its retained binary hash is
`89c0d6197cc2be1d3f45cad2f963246e849790530c7f4597025c3b1ef0aa9c71`.

#### One-minute retry against CBDD-Zed

Each model's own five-second timeout cohort was restarted once with a 60-second cutoff. Combined time counts the first five-second
attempt too, so a rescued matrix has `5 s + retry time` and a repeated timeout represents 65 seconds of attempted work.

| Model | Five-second solved / timeout | Rescued at 60 s | Combined solved / timeout | Combined work | Combined wall |
|---|---:|---:|---:|---:|---:|
| CBDD-Zed Dickinson | 493 / 31 | 5 | **498 / 26** | **1,775.753 s** | **475.279 s** |
| SAT-Zed Dickinson | 488 / 36 | 5 | 493 / 31 | 2,109.312 s | 540.664 s |

Both models rescued exactly matrices 10289--10293, the order-16/17 Hildebrand boundary matrices, and every rescued classification
matches corpus truth. CBDD finished all five retries faster. After the combined stages, CBDD has six unique completions (9610, 9611,
9631, 9632, 9647, and 9648), SAT has one (9612), and 492 matrices are common completions. The exact retained CBDD five-second binary
was no longer available for the retry, so the historical 31-matrix cohort was retried with the current mathematically unchanged CBDD
binary; SAT used the byte-identical binary from its five-second run. Full retry rows are stored in
`experiments/sat_vs_cbdd_60s_2026-08-15/results.sqlite3`.

The cardinality-local models completed 474 common matrices. Cardinality-BDD additionally completed matrix 10490, an order-90
Dannenberg–Schürmann lift; Cardinality-ZDD had no unique completion. BDD's median per-matrix time was 8.28% lower across the common
completions. All 1,048 rows are present, all 949 completed decisions match corpus truth, and all 99 unresolved rows are timeouts.
The retained Cardinality-BDD and Cardinality-ZDD hashes are
`fd1b398793d2fcfe8d55f4a72a3a2fe67efd1c55c3821cafcee4d4fcdc1726eb` and
`f7ed518402403e11d62894578c7d828df7742baac6f900bd0753fcc4da0e7665`, respectively.

### Negative-Zed comparison

BDD Negative-Zed and ZDD Negative-Zed use maximal Zed blocks only for immediate negative rejection; PD blocks are discarded and do
not enter the decision diagram. Flat Dickinson Zed retains PD-block downsets because they remain cheap packed subset tests rather
than changing a symbolic support-family representation. The outcome runs below used strict mode, both preprocessing stages, a
five-second cutoff, CPU 3 for the parent, and workers on CPUs 4–7.

| Model | Core solved/unresolved | Core work | Stress solved/unresolved | Stress work | Union solved/unresolved | Union work | Four-worker wall |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dickinson Final | 319/65 | 332.990 s | 116/124 | 625.533 s | 380/144 | 732.367 s | 184.735 s |
| Dickinson Zed, retained downsets | 323/61 | 319.198 s | 122/118 | 600.881 s | 384/140 | 718.592 s | 181.675 s |
| Interval-BDD Dickinson | 371/13 | 75.464 s | 202/38 | 205.160 s | 486/38 | 205.397 s | 55.071 s |
| BDD Negative-Zed | 377/7 | 35.948 s | 208/32 | 165.695 s | 492/32 | 165.843 s | 43.219 s |
| CBDD Negative-Zed | **378/6** | **35.703 s** | **209/31** | **165.284 s** | **493/31** | **165.415 s** | **43.186 s** |
| Interval-ZDD Dickinson | 369/15 | 76.395 s | 200/40 | 206.481 s | 484/40 | 206.816 s | 55.316 s |
| ZDD Negative-Zed | 377/7 | 36.272 s | 208/32 | 166.449 s | 492/32 | 166.662 s | 43.390 s |
| CZDD Negative-Zed | 377/7 | 36.186 s | 208/32 | 166.495 s | 492/32 | 166.632 s | **43.177 s** |

The chain comparisons use the same prechecked outcome rows above. Per-matrix relative changes use `(chain - ordinary) / ordinary`
where both models completed:

| Comparison | Common completions | Median all | Median SCP | Median non-SCP | Unique chain completions | Unique ordinary completions |
|---|---:|---:|---:|---:|---:|---:|
| BDD Negative-Zed → CBDD Negative-Zed | 492 | -9.150% | -22.819% | 0.000% | 1 | 0 |
| ZDD Negative-Zed → CZDD Negative-Zed | 492 | +3.438% | +13.164% | -0.250% | 0 | 0 |

CBDD's unique completion is strict Johnson matrix 9627 of order 28, completed in 4.865547 seconds. CBDD also reduced substituted
one-core work by 0.258%. CZDD retained exactly ZDD's completion set; its substituted work changed by -0.018%, while its positive
median paired time shows that chain reduction did not improve this ZDD workload.

The following Zed-versus-plain timing comparison is a separate clean campaign with preprocessing disabled for the original six
binaries. This removes connected components, shared prechecks, and the later COPOMATRIX precheck from the comparison. Per-matrix
relative changes use
`(Zed - plain) / plain` over identical matrix IDs completed by both models:

| Comparison | Common completions | Median all | Median SCP | Median SCP absolute change | Median non-SCP | Completion change |
|---|---:|---:|---:|---:|---:|---:|
| Dickinson Final → Dickinson Zed | 373 | +22.167% | +85.763% | +0.021104 ms | -46.975% | +4 |
| Interval-BDD → BDD Negative-Zed | 466 | -1.129% | +1.035% | +0.002646 ms | -90.150% | +21 |
| Interval-ZDD → ZDD Negative-Zed | 466 | -0.955% | +1.952% | +0.004605 ms | -89.081% | +22 |

The SCP medians are positive in all three matrix-paired comparisons, as expected: a rejection-only Zed scan is extra work when it
cannot reject the matrix. The BDD/ZDD overall medians remain slightly negative because the same comparison includes 220 common
non-SCP matrices, on which a non-PD maximal Zed block can avoid almost the entire Dickinson traversal. Thus the negative overall
median does not mean that the Zed scan accelerates SCP inputs.

The SCP slowdown is frequent rather than an isolated stress-matrix effect. Its frequency and magnitude grow sharply with order:

| Order | Common SCP completions | Zed slower | At least 2x as slow | Median relative change | Median absolute change |
|---|---:|---:|---:|---:|---:|
| 1–5 | 29 | 17 | 11 | +20.726% | +0.001333 ms |
| 6–10 | 56 | 44 | 11 | +43.310% | +0.012251 ms |
| 11–15 | 20 | 18 | 10 | +97.921% | +0.288708 ms |
| 16–26 | 33 | 31 | 30 | +288.360% | +23.240745 ms |

The high-order result is not caused by the Stress Test label. Of those 33 matrices, 14 belong to Representative Core but not Stress
Test; 13 are slower with Zed, and all 13 are at least twice as slow. The other 19 belong to both sets; 18 are slower and 17 are at
least twice as slow. The affected families include Hamming, Keller, c-fat, cisqrg, ggen, Sanchis, Johnson, and both
Dannenberg–Schürmann lift families. Moreover, core-only SCP matrices 10427 and 10330 finish in plain Dickinson at 2.403 and 2.661
seconds but time out at five seconds in Dickinson Zed. The comparison includes only common completions, so those two losses are not
part of the relative-time medians.

The sign graph is a better predictor than corpus membership. Among the 138 common SCP completions, 13 matrices have no vertex
covered by a nontrivial Zed block and show only +0.878% median change. Fifteen have some but not all singleton supports suppressed
and show +131.093%; 110 have every singleton suppressed and show +87.075%. Restricting the last group to order greater than 15 gives
29 matrices: all 29 are slower, all 29 are at least twice as slow, and their median change is +304.786%. Large order plus widespread
singleton suppression is therefore the recurring dangerous structure; the precise star pattern of matrix 10322 is not required.

Over all 132 common completions above order 15, including non-SCP matrices, the median is instead -51.426% because 99 are non-SCP
matrices benefiting from early Zed rejection; their separate median change is -71.781%. This mixture must not be used to infer that
positive Zed downsets help high-order SCP inputs.

**Important causal finding: retained positive Zed downsets can suppress stronger ordinary Dickinson certificates.** A focused trace
of strict order-20 matrix 10322 explains its 10.267164 ms plain time versus 37.319700 ms Dickinson-Zed time. Both traversals visit all
$2^{20}-1=1,048,575$ supports. Plain Dickinson solves only 39 systems: all 20 singletons and 19 pairs. Their upward certificate
intervals cover the other 1,048,536 supports. Dickinson Zed first retains 19 maximal size-two Zed blocks, which together mark every
singleton and those 19 pairs as already covered. Because coverage is checked before solving, it generates no singleton certificate.
It consequently solves the other 171 pairs and then 18 triples, or 189 systems in total, before covering the remainder. The Zed
variant therefore performs 4.85 times as many ordinary exact solves on this matrix. The slowdown is not primarily the cost of
looking up 19 blocks: the downward Zed cuts prevent the creation of the singleton certificates whose upward intervals are far wider.

In the prechecked outcome campaign, BDD Negative-Zed gained seven negative matrices and lost strict Johnson matrix 9627. ZDD
Negative-Zed gained eight negative matrices
and lost none. Relative to their superseded positive-downset binaries, both rejection-only models retained the same 492 completion
sets while reducing median SCP time by 73.76% for BDD and 69.30% for ZDD. All 4,192 rows in this table are present; every completed
classification matches corpus truth, and every unresolved row is a timeout. Current hashes are
`5d8509a85b7db037afd06ac4e3b1f4b9bf4955c521ce67e9393c682ffe968ce9` (Dickinson Final),
`88ce07a280cc0dc206f453d43948671276514e62fc8d9e1ed879267d2b9c0c8a` (Dickinson Zed),
`5fca72a61045c628b2db6c14e79392e0cfeef2676d2bd263d95895301069f3d1` (BDD Negative-Zed),
`f2141101bb5d2c848d212702c228e2ae8985d021ffc7cc28becb5bd26efc0955` (CBDD Negative-Zed),
`ab7350362cff899f8ee5b5d311f94b6f4bcd4640f3bde462b5d34d4a796abe89` (ZDD Negative-Zed), and
`61f0df6855788983bd69952a9633df91be1ee3711f4c4408dcd7faa0f6c1de2f` (CZDD Negative-Zed).
The isolated no-preprocessing hashes are `bd4e720f6b54866b19f65476e76116c2692abbc68c88fcd59fad5034bd98abef` and
`efe73c5002a93c619365032f540f558f64752c0fb12439f3ef535c6a10b95076` for flat Dickinson,
`bd98fdfb8b8870c0bbc7f0a429ae10ae6bff30b5e5588fcbd54843cffe650840` and
`6cd83d32872d2ee618dece9b69132ad534c44d32ef2ba4cf30b86a03c290a140` for BDD, and
`56e95cd9ad82c2043b93e75815c5f72991b113c9c920826710f0f528a08248c3` and
`9fc4f8bfdc6a343d6a598e7958666fb1218db20c5b3b3ccaad06e16d771ec8f4` for ZDD, with the plain hash first in each pair.

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

These are runs without external preprocessing. Literature baselines remain separate in meaning from the coposit-created adaptive
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
| Adaptive Sponsel–COPOMATRIX | coposit-created | 364/20 · 129.692 s | 340/44 · 251.757 s | 178/62 · 386.996 s | 162/78 · 468.420 s | 140.637/191.319 s |

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
