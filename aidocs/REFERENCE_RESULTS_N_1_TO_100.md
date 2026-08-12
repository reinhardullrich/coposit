# Reference Results For Dimensions 1 Through 100

Last updated: 2026-08-10

This report has two explicit scopes. `SCP` means a strict-copositivity check; `CP` means a copositivity check using the non-strict
inequality. The current baseline section covers all 2,084 retained matrices with `n <= 100`, through corpus ID 10597, in both modes.
The later all-model comparison remains the preserved 2,078-matrix snapshot
through ID 10504 because the Coposit-created variants have not all been rerun on the six later order-51 matrices.

## Current Eight-Baseline Completion

All eight literature baselines were run in both modes with a five-second per-matrix cutoff, CPU 3 reserved for dispatch and database
writes, and four persistent solver workers pinned to CPUs 4–7. Dutour, Danninger, Bundfuss, Safi, and Sponsel use the current 50,000
unfinished-node limit; the other baselines do not use that shared limit. Every model/mode identity has exactly 2,084 rows. Every
completed result matches the corresponding stored corpus truth, there are no execution errors, and every timeout or node-limit result
remains unresolved.

The current corpus contains 588 strictly copositive, 989 copositive-boundary, and 507 non-copositive matrices. Non-strict truth is known
for every row.

| Model | Native-module SHA-256 | SCP ok | SCP timeout | SCP node limit | CP ok | CP timeout | CP node limit |
|---|---|---:|---:|---:|---:|---:|---:|
| Bundfuss 2008 | <code>73d547f0f949bd953c2d115201f568d92a86d80f32b344d785c25cc92c688622</code> | 1,749 | 333 | 2 | 1,620 | 462 | 2 |
| COPOMATRIX 2011 | <code>75a89f5d88057a65cef727fa52e976578fb786f31f757cbac5a4dd726dd9ff33</code> | 1,928 | 156 | 0 | 1,839 | 245 | 0 |
| Danninger 1990 | <code>f5b73a0336dc95abe519b2f92682911d2911c169962bf71b5ca0ce561a8405a1</code> | 1,798 | 286 | 0 | 1,684 | 400 | 0 |
| Dickinson 2019 | <code>7d710f0561249f81364a6651ecb626bb04225ca1a5497f108b972d2f28d928d6</code> | 1,910 | 174 | 0 | 1,806 | 278 | 0 |
| Dutour 2018 | <code>b3272e36609e8c1c4af770ab4235fe3c9a1dc5638be37517d38a17e7de4d10ae</code> | 1,764 | 289 | 31 | 1,613 | 427 | 44 |
| Hadeler 1983 — LDLT reference | _(empty; legacy unidentified rows)_ | 1,894 | 190 | 0 | 1,745 | 339 | 0 |
| Safi 2021 | <code>f976207182391af88ae1ef79992a319627ee8d6158553e8052765cf7656d047c</code> | 1,848 | 236 | 0 | 1,716 | 368 | 0 |
| Sponsel 2012 | <code>29c2f67e5e7cbfaea2c0d3a1d6ed657ed17ebd2f7f9e6c2199127acaaeb4d76d</code> | 1,791 | 293 | 0 | 1,697 | 387 | 0 |

## Preserved All-Model Strict Snapshot

The remainder of this document is the retained-corpus comparison after whole-matrix positive-scale/permutation deduplication. The
22 previously reported models were recalculated from their surviving stored rows without rerunning them. Adaptive Sponsel–COPOMATRIX
was then run with streak limits 10, 100, 1,000, and 10,000 on the same retained corpus. Adaptive Zischg–Sponsel–COPOMATRIX was
subsequently run with streak limits 10, 100, 1,000, 10,000, and 100,000. This preserved comparison restricts every identity to the
same 2,078 rows through corpus ID 10504.

## Scope And Notation

The report covers all 2,078 snapshot matrices with 1 <= n <= 100. The snapshot truth is strict copositivity:

- **strictly copositive** means the stored strict result is true;
- **not strictly copositive** means the stored strict result is false; non-strict copositivity may still hold;
- **solved** means the stored model result is ok, and every such result matches the corpus;
- **node limit**, **timeout**, and **error** are unresolved outcomes, never negative classifications.

Every detailed result cell has the form matrix count (average distinct n). The average uses each represented dimension once.

All selected result identities use `preprocessing=none` and a five-second cutoff. The seven historical cone runs used seven
workers; all other complete reference runs used six. Reconstructed wall time is substituted worker work divided by that run's
worker count. Substituted work uses stored native time for a completed result and the stored five-second cutoff for every
unresolved outcome. It is a deterministic reconstruction from the result rows, not the originally observed dispatcher wall clock.

## Selected Stored Result Identities

| Model | Native-module SHA-256 | Workers |
|---|---|---:|
| Hadeler 1983 — LDLT reference | _(empty; legacy unidentified rows)_ | 6 |
| Zischg–Hadeler | <code>91bbc1349d328c719a9129993f701afde53d31325e4cc1bc8636ab64e982de55</code> | 6 |
| Dutour 2018 | <code>1b8263b9d3b68b7c6919ce8a508d64cb801692977dbfe5aa45f39ccde7d95b67</code> | 7 |
| Danninger 1990 | <code>bc67b28681faea6c5c677e1472fd429a838a5cc34f38ed1a3ee05276a6eac312</code> | 7 |
| COPOMATRIX 2011 | <code>7b5fa023c9ff053eaf2fd03cadafae3da28121d90d179f70ea893a800b88ee47</code> | 7 |
| Adaptive Dutour-Danninger | <code>fcce74308ea2e493372b5a9f9eab23f755388a43787dfff11d0ef8ac167970db</code> | 7 |
| Adaptive Dutour–COPOMATRIX | <code>e87a08bf953e41fe933348fdd386e7a0b29218e8e002af597d2d0c4fcb5febd8</code> | 6 |
| Adaptive Sponsel–COPOMATRIX — streak 100 | <code>c424121ee78f2f5ccb30eb45650c23499133e6644bd8f718e16dfcb6851b8e5c</code> | 6 |
| Adaptive Sponsel–COPOMATRIX — streak 10 | <code>079a491aa7b72c162ca5c5af48da4f3d22c4062c63de748a8bbc218584c049d9</code> | 6 |
| Adaptive Sponsel–COPOMATRIX — streak 1,000 (current) | <code>e93b85ab3c88e51be28fd5dd944bb1eb0cbe656b4b45ec5043998098852b64af</code> | 6 |
| Adaptive Sponsel–COPOMATRIX — streak 10,000 | <code>5a9d4bd44ca7372797f3d0d7e5653645b7f0e68aa5dfb9750796bf3c1bbae78a</code> | 6 |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | <code>385e2d5606fe5f20cbbf98f91b9d50a1d7be526bd18ce387489bf331d85b782d</code> | 6 |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10 | <code>f0f9976fdabc2e481e66e6a5854e99437eff010ab8caf89a179475e9113d5763</code> | 6 |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | <code>e09c1d9cbd582df7204798f3e3c55935ab9d24e6e4ba14358416e20a7b85d02e</code> | 6 |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 (current) | <code>7b510a73420787fd48c2286388a5cefd99a165941e8d7d4fb970c39f2d590b51</code> | 6 |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100,000 | <code>dbef25bcd36dd7c0f20dc90b7dd4a0af72d6a42b96eb195635282d4653f4598f</code> | 6 |
| Dickinson 2019 | <code>bd3beac5785dd1ef5c2936d8a32eea73a5a9912d3e5cfa6ede7929df1050e5db</code> | 6 |
| Support-Pruned Dickinson | <code>b68989630ecc0af715fe22e13143a7b78569523418f0fd46dcc9acba9f02b35c</code> | 6 |
| Nullity Support-Pruned Dickinson | <code>247259ce36461b297ef6c33fc422fddfc122836ccba7cee10539f83000844e71</code> | 6 |
| RHS Dickinson | <code>0b90b2d9e4e5283b3bcbef44d971ceec40642463989c503215176a586bb00434</code> | 6 |
| Zischg–Dickinson | <code>e77518c2e2e889139ce04001d29d143b68852ce4f8754f90f187b382046b7be7</code> | 6 |
| Frank–Wolfe Dickinson | <code>84d544ea5ab757ea03e00ad959877c47bc58266f79022e33bc994290e687410b</code> | 6 |
| Exact One-Step Frank–Wolfe Dickinson | <code>3380b3b7884f04bc6ed8653520b33e2e5bc3901869260207f811edec037432d4</code> | 6 |
| Pairwise Frank–Wolfe Dickinson | <code>7ade72366a555f11c3c881c8e85a770435951f99ab7a63a027c604962a48afe6</code> | 6 |
| Support-Polished Frank–Wolfe Dickinson | <code>a87431120f177efe4a6fd9e72a503bb5c1c8e064f07c8394d77dc4f9e9e88b2d</code> | 6 |
| FracESSA | <code>6bfaffa4d4bfdd1f912b33af7f8fc28e6d5c13139e6f426ba9207b121fcfb2b4</code> | 6 |
| Zischg–FracESSA | <code>2deb23a21b7fe34b291be324ca9e1f8b1d059c5a7a0af9af0670547a4521b4e8</code> | 6 |
| Safi 2021 | <code>0bd6d3d0430c0d3d01f39659073eeabc1e21ae885c9b788d5ee201c07fd9b3d1</code> | 7 |
| Bundfuss 2008 | <code>475a228ac7a4aca81380c8b2ef169cf29c74a603f3db0e1d031343de7922013f</code> | 7 |
| Sponsel 2012 | <code>e047dec119eef94666b51646dbf434aedd6507bc97dbff340717067bfd7a1416</code> | 7 |
| Frank–Wolfe Sponsel | <code>009cdeec98f77755e8905fcdcd93ecfc0360abc5b686ecdfb32fb3f098896f16</code> | 6 |

## Corpus Composition

| Corpus truth | Matrices | Share of corpus | Distinct dimensions | Average distinct n |
|---|---:|---:|---:|---:|
| Strictly copositive | 586 | 28.20% | 100 | 50.50 |
| Not strictly copositive | 1,492 | 71.80% | 83 | 43.69 |
| **Total** | **2,078** | **100.00%** | **100** | **50.50** |

## Overall Model Comparison

Solved, node-limit, timeout, and error percentages use all 2,078 matrices. Strict solved uses the 586 strict matrices,
and not-strict solved uses the 1,492 not-strict matrices.

| Model | Solved | Strict solved | Not-strict solved | Node limit | Timeout | Error |
|---|---:|---:|---:|---:|---:|---:|
| **Hadeler 1983 — LDLT reference** | 1,890 (90.95%) | 421 (71.84%) | 1,469 (98.46%) | 0 (0.00%) | 188 (9.05%) | 0 (0.00%) |
| Zischg–Hadeler | 1,893 (91.10%) | 424 (72.35%) | 1,469 (98.46%) | 0 (0.00%) | 185 (8.90%) | 0 (0.00%) |
| Dutour 2018 | 1,760 (84.70%) | 376 (64.16%) | 1,384 (92.76%) | 31 (1.49%) | 287 (13.81%) | 0 (0.00%) |
| Danninger 1990 | 1,797 (86.48%) | 391 (66.72%) | 1,406 (94.24%) | 0 (0.00%) | 281 (13.52%) | 0 (0.00%) |
| COPOMATRIX 2011 | 1,927 (92.73%) | 479 (81.74%) | 1,448 (97.05%) | 0 (0.00%) | 151 (7.27%) | 0 (0.00%) |
| Adaptive Dutour-Danninger | 1,929 (92.83%) | 476 (81.23%) | 1,453 (97.39%) | 0 (0.00%) | 148 (7.12%) | 1 (0.05%) |
| Adaptive Dutour–COPOMATRIX | 1,927 (92.73%) | 475 (81.06%) | 1,452 (97.32%) | 0 (0.00%) | 151 (7.27%) | 0 (0.00%) |
| Adaptive Sponsel–COPOMATRIX — streak 100 | 2,023 (97.35%) | 568 (96.93%) | 1,455 (97.52%) | 0 (0.00%) | 55 (2.65%) | 0 (0.00%) |
| Adaptive Sponsel–COPOMATRIX — streak 10 | 2,019 (97.16%) | 568 (96.93%) | 1,451 (97.25%) | 0 (0.00%) | 59 (2.84%) | 0 (0.00%) |
| Adaptive Sponsel–COPOMATRIX — streak 1,000 (current) | 2,024 (97.40%) | 569 (97.10%) | 1,455 (97.52%) | 0 (0.00%) | 54 (2.60%) | 0 (0.00%) |
| Adaptive Sponsel–COPOMATRIX — streak 10,000 | 2,023 (97.35%) | 568 (96.93%) | 1,455 (97.52%) | 0 (0.00%) | 55 (2.65%) | 0 (0.00%) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | 2,024 (97.40%) | 569 (97.10%) | 1,455 (97.52%) | 0 (0.00%) | 54 (2.60%) | 0 (0.00%) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10 | 2,020 (97.21%) | 569 (97.10%) | 1,451 (97.25%) | 0 (0.00%) | 58 (2.79%) | 0 (0.00%) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | 2,024 (97.40%) | 568 (96.93%) | 1,456 (97.59%) | 0 (0.00%) | 54 (2.60%) | 0 (0.00%) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 (current) | 2,025 (97.45%) | 569 (97.10%) | 1,456 (97.59%) | 0 (0.00%) | 53 (2.55%) | 0 (0.00%) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100,000 | 2,025 (97.45%) | 569 (97.10%) | 1,456 (97.59%) | 0 (0.00%) | 53 (2.55%) | 0 (0.00%) |
| Dickinson 2019 | 1,907 (91.77%) | 437 (74.57%) | 1,470 (98.53%) | 0 (0.00%) | 171 (8.23%) | 0 (0.00%) |
| Support-Pruned Dickinson | 1,907 (91.77%) | 437 (74.57%) | 1,470 (98.53%) | 0 (0.00%) | 171 (8.23%) | 0 (0.00%) |
| Nullity Support-Pruned Dickinson | 1,907 (91.77%) | 437 (74.57%) | 1,470 (98.53%) | 0 (0.00%) | 171 (8.23%) | 0 (0.00%) |
| RHS Dickinson | 1,902 (91.53%) | 437 (74.57%) | 1,465 (98.19%) | 0 (0.00%) | 176 (8.47%) | 0 (0.00%) |
| Zischg–Dickinson | 1,904 (91.63%) | 434 (74.06%) | 1,470 (98.53%) | 0 (0.00%) | 174 (8.37%) | 0 (0.00%) |
| Frank–Wolfe Dickinson | 1,910 (91.92%) | 438 (74.74%) | 1,472 (98.66%) | 0 (0.00%) | 168 (8.08%) | 0 (0.00%) |
| Exact One-Step Frank–Wolfe Dickinson | 1,908 (91.82%) | 438 (74.74%) | 1,470 (98.53%) | 0 (0.00%) | 170 (8.18%) | 0 (0.00%) |
| Pairwise Frank–Wolfe Dickinson | 1,911 (91.96%) | 438 (74.74%) | 1,473 (98.73%) | 0 (0.00%) | 167 (8.04%) | 0 (0.00%) |
| Support-Polished Frank–Wolfe Dickinson | 1,912 (92.01%) | 438 (74.74%) | 1,474 (98.79%) | 0 (0.00%) | 166 (7.99%) | 0 (0.00%) |
| FracESSA | 1,897 (91.29%) | 428 (73.04%) | 1,469 (98.46%) | 0 (0.00%) | 181 (8.71%) | 0 (0.00%) |
| Zischg–FracESSA | 1,903 (91.58%) | 434 (74.06%) | 1,469 (98.46%) | 0 (0.00%) | 175 (8.42%) | 0 (0.00%) |
| Safi 2021 | 1,843 (88.69%) | 437 (74.57%) | 1,406 (94.24%) | 0 (0.00%) | 235 (11.31%) | 0 (0.00%) |
| Bundfuss 2008 | 1,744 (83.93%) | 456 (77.82%) | 1,288 (86.33%) | 2 (0.10%) | 332 (15.98%) | 0 (0.00%) |
| Sponsel 2012 | 1,786 (85.95%) | 490 (83.62%) | 1,296 (86.86%) | 0 (0.00%) | 292 (14.05%) | 0 (0.00%) |
| Frank–Wolfe Sponsel | 1,788 (86.04%) | 490 (83.62%) | 1,298 (87.00%) | 0 (0.00%) | 290 (13.96%) | 0 (0.00%) |

Adaptive Dutour-Danninger still has one explicit error: matrix 9681 (n=6) exited with code -11.
Relative to COPOMATRIX, Adaptive Dutour–COPOMATRIX gained 16 not-strict and 16 strict; lost 12 not-strict and 20 strict (32 gained and 32 lost) and changed substituted work by -1.74%.
Relative to Adaptive Dutour-Danninger, Adaptive Dutour–COPOMATRIX gained 7 not-strict and 6 strict; lost 8 not-strict and 7 strict (13 gained and 15 lost) and changed substituted work by -2.13%.

With streak 100, Adaptive Sponsel–COPOMATRIX completed every matrix completed by Sponsel and added 77 strict and 160 not-strict
completions. Relative to COPOMATRIX, it gained 89 strict and 16 not-strict completions while losing nine not-strict completions.
Relative to Adaptive Dutour–COPOMATRIX, it gained 93 strict and three not-strict completions without losing any. Its
five-second-substituted work was 75.69% below Sponsel, 53.77% below COPOMATRIX, and 52.95% below Adaptive Dutour–COPOMATRIX.

Changing the streak from 100 to 10 gained strict lift 10487 and not-strict Hildebrand matrix 10274, but lost strict lift 10489 and
five not-strict matrices: 9611, 9632, 9647, 9648, and 10269. It therefore completed four fewer matrices and increased substituted
work by 2.18%. Median substituted time increased 3.26% over all matrices and 4.44% where both configurations completed.

Changing the streak from 100 to 1,000 added strict lift 10487 without losing any streak-100 completion. Substituted work fell 0.97%,
while the median change was 0.00% both over all matrices and where both completed. Relative to streak 10, streak 1,000 gained strict
lift 10489 and not-strict matrices 9611, 9632, 9647, 9648, and 10269, while losing not-strict Hildebrand matrix 10274. It completed
five more matrices, used 3.08% less substituted work, and improved the corresponding medians by 2.85% and 3.53%.

Changing the streak from 1,000 to 10,000 gained no completion and lost strict lift 10488. Substituted work increased 2.70%; median
substituted time increased 4.75% over all matrices and 6.00% where both configurations completed.

At the same 10,000 cutoff, adding projection-local Zischg decomposition gained not-strict Hildebrand matrix 10274 and strict lift
10488 without losing a completion. It reduced five-second-substituted work by 3.09%; the median per-matrix change was 0.00% both
over the complete corpus and where both models completed. Relative to the current streak-1,000 base, it gained matrix 10274 without
a loss and reduced substituted work by 0.47%, although median time increased 1.98% overall and 2.88% where both completed.

Within the Zischg hybrid, streak 100 solved 2,024 matrices with 359.195192 seconds of substituted work. Moving to streak 1,000
exchanged strict matrix 10489 for not-strict matrix 10274 and increased work by 0.71%. Moving from 1,000 to 10,000 recovered 10489
without losing a completion and reduced work by 0.67%. Streak 100,000 reproduced the complete streak-10,000 outcome set exactly.
Its 359.189689 seconds of work were 0.039% lower than streak 10,000 and 0.0015% lower than streak 100; the latter is a 0.005504-second
aggregate difference. Relative to streak 10,000, its median per-matrix changes were -0.48% overall and -0.88% where both completed.
These tiny timing differences do not justify changing the maintained 10,000 cutoff. Streak 10 solved four fewer matrices than streak
100 and required 3.34% more work.

## Reconstructed Runtime Comparison

| Model | Reconstructed wall time | One-core-equivalent substituted work |
|---|---:|---:|
| **Hadeler 1983 — LDLT reference** | 162.788 s (2:42.788) | 976.727 s (16:16.727) |
| Zischg–Hadeler | 161.247 s (2:41.247) | 967.484 s (16:07.484) |
| Dutour 2018 | 236.851 s (3:56.851) | 1,657.959 s (27:37.959) |
| Danninger 1990 | 204.814 s (3:24.814) | 1,433.696 s (23:53.696) |
| COPOMATRIX 2011 | 112.665 s (1:52.665) | 788.657 s (13:08.657) |
| Adaptive Dutour-Danninger | 113.110 s (1:53.110) | 791.772 s (13:11.772) |
| Adaptive Dutour–COPOMATRIX | 129.156 s (2:09.156) | 774.934 s (12:54.934) |
| Adaptive Sponsel–COPOMATRIX — streak 100 | 60.765 s (1:00.765) | 364.591 s (6:04.591) |
| Adaptive Sponsel–COPOMATRIX — streak 10 | 62.088 s (1:02.088) | 372.529 s (6:12.529) |
| Adaptive Sponsel–COPOMATRIX — streak 1,000 (current) | 60.174 s (1:00.174) | 361.042 s (6:01.042) |
| Adaptive Sponsel–COPOMATRIX — streak 10,000 | 61.799 s (1:01.799) | 370.792 s (6:10.792) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | 59.866 s (0:59.866) | 359.195 s (5:59.195) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10 | 61.864 s (1:01.864) | 371.183 s (6:11.183) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | 60.293 s (1:00.293) | 361.757 s (6:01.757) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 (current) | 59.888 s (0:59.888) | 359.329 s (5:59.329) |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100,000 | 59.865 s (0:59.865) | 359.190 s (5:59.190) |
| Dickinson 2019 | 145.820 s (2:25.820) | 874.918 s (14:34.918) |
| Support-Pruned Dickinson | 146.354 s (2:26.354) | 878.122 s (14:38.122) |
| Nullity Support-Pruned Dickinson | 146.484 s (2:26.484) | 878.906 s (14:38.906) |
| RHS Dickinson | 150.536 s (2:30.536) | 903.218 s (15:03.218) |
| Zischg–Dickinson | 148.310 s (2:28.310) | 889.862 s (14:49.862) |
| Frank–Wolfe Dickinson | 144.067 s (2:24.067) | 864.403 s (14:24.403) |
| Exact One-Step Frank–Wolfe Dickinson | 145.851 s (2:25.851) | 875.104 s (14:35.104) |
| Pairwise Frank–Wolfe Dickinson | 143.320 s (2:23.320) | 859.921 s (14:19.921) |
| Support-Polished Frank–Wolfe Dickinson | 142.428 s (2:22.428) | 854.566 s (14:14.566) |
| FracESSA | 156.268 s (2:36.268) | 937.611 s (15:37.611) |
| Zischg–FracESSA | 151.670 s (2:31.670) | 910.019 s (15:10.019) |
| Safi 2021 | 175.086 s (2:55.086) | 1,225.599 s (20:25.599) |
| Bundfuss 2008 | 246.535 s (4:06.535) | 1,725.742 s (28:45.742) |
| Sponsel 2012 | 214.295 s (3:34.295) | 1,500.062 s (25:00.062) |
| Frank–Wolfe Sponsel | 249.393 s (4:09.393) | 1,496.355 s (24:56.355) |
| **Total excluding the Hadeler 1983 baseline** | **4,010.861 s (1:06:50.861)** | **25,368.519 s (7:02:48.519)** |

### Strict-only Dickinson comparison

The current Dickinson result is the baseline. Median percentages compare per-matrix substituted times; the both-completed
median uses only matrices for which both models completed. Negative values are faster.

| Model | Solved | Substituted work | Difference from Dickinson | Completion difference | Median, all comparable | Median, both completed |
|---|---:|---:|---:|---|---:|---:|
| Dickinson 2019 | 1,907 | 874.918040 s | — | — | — | — |
| Support-Pruned Dickinson | 1,907 | 878.121823 s | +0.37% | gained 1 strict; lost 1 strict | +0.23% | +6.93% |
| Nullity Support-Pruned Dickinson | 1,907 | 878.905789 s | +0.46% | gained 1 strict; lost 1 strict | +0.00% | -3.72% |
| RHS Dickinson | 1,902 | 903.218179 s | +3.23% | lost 5 not-strict | +20.03% | +30.41% |
| Frank–Wolfe Dickinson | 1,910 | 864.403398 s | -1.20% | gained 2 not-strict and 1 strict | +0.00% | -1.73% |
| Exact One-Step Frank–Wolfe Dickinson | 1,908 | 875.103711 s | +0.02% | gained 1 strict | -14.09% | -21.78% |
| Pairwise Frank–Wolfe Dickinson | 1,911 | 859.920824 s | -1.71% | gained 3 not-strict and 1 strict | +0.00% | +9.83% |
| Support-Polished Frank–Wolfe Dickinson | 1,912 | 854.565710 s | -2.33% | gained 4 not-strict and 1 strict | +11.95% | +24.55% |
| Zischg–Dickinson | 1,904 | 889.862255 s | +1.71% | lost 3 strict | +56.03% | +77.16% |

Changed completion IDs relative to Dickinson:

- **Support-Pruned Dickinson:** gained 9627; lost 10428.
- **Nullity Support-Pruned Dickinson:** gained 9627; lost 10428.
- **RHS Dickinson:** gained —; lost 10284, 10285, 10286, 10287, 10288.
- **Frank–Wolfe Dickinson:** gained 9611, 9632, 10331; lost —.
- **Exact One-Step Frank–Wolfe Dickinson:** gained 10331; lost —.
- **Pairwise Frank–Wolfe Dickinson:** gained 9610, 9611, 9632, 10331; lost —.
- **Support-Polished Frank–Wolfe Dickinson:** gained 9610, 9611, 9631, 9632, 10331; lost —.
- **Zischg–Dickinson:** gained —; lost 10330, 10427, 10428.

### Related model comparisons

| Variant | Baseline | Completion difference | Variant work | Baseline work | Work difference | Median, all comparable | Median, both completed |
|---|---|---|---:|---:|---:|---:|---:|
| Zischg–Hadeler | Hadeler 1983 — LDLT reference | gained 3 strict | 967.484165 s | 976.727363 s | -0.95% | +53.19% | +76.13% |
| Zischg–FracESSA | FracESSA | gained 6 strict | 910.018663 s | 937.610829 s | -2.94% | -0.85% | -10.75% |
| Frank–Wolfe Sponsel | Sponsel 2012 | gained 2 not-strict | 1496.355107 s | 1500.062009 s | -0.25% | +0.00% | -6.76% |
| Adaptive Sponsel–COPOMATRIX — streak 100 | Sponsel 2012 | gained 77 strict and 160 not-strict | 364.590928 s | 1500.062009 s | -75.69% | -31.02% | -23.06% |
| Adaptive Sponsel–COPOMATRIX — streak 100 | COPOMATRIX 2011 | gained 89 strict and 16 not-strict; lost 9 not-strict | 364.590928 s | 788.657441 s | -53.77% | -33.33% | -31.28% |
| Adaptive Sponsel–COPOMATRIX — streak 10 | streak 100 | gained 1 strict and 1 not-strict; lost 1 strict and 5 not-strict | 372.529447 s | 364.590928 s | +2.18% | +3.26% | +4.44% |
| Adaptive Sponsel–COPOMATRIX — streak 1,000 | streak 100 | gained 1 strict | 361.041983 s | 364.590928 s | -0.97% | +0.00% | +0.00% |
| Adaptive Sponsel–COPOMATRIX — streak 10,000 | streak 1,000 | lost 1 strict | 370.792344 s | 361.041983 s | +2.70% | +4.75% | +6.00% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10 | Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | gained 1 not-strict; lost 5 not-strict | 371.183324 s | 359.195192 s | +3.34% | +0.00% | +0.18% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | gained 1 not-strict; lost 1 strict | 361.756635 s | 359.195192 s | +0.71% | +0.00% | +0.00% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 | Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | gained 1 strict | 359.329316 s | 361.756635 s | -0.67% | +0.60% | +1.20% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 | Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | gained 1 not-strict | 359.329316 s | 359.195192 s | +0.04% | +0.00% | +0.07% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100,000 | Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 | no change | 359.189689 s | 359.329316 s | -0.04% | -0.48% | -0.88% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10 | Adaptive Sponsel–COPOMATRIX — streak 10 | gained 1 strict | 371.183324 s | 372.529447 s | -0.36% | +0.00% | +0.00% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 100 | Adaptive Sponsel–COPOMATRIX — streak 100 | gained 1 strict | 359.195192 s | 364.590928 s | -1.48% | +0.00% | +0.08% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000 | Adaptive Sponsel–COPOMATRIX — streak 1,000 | gained 1 not-strict; lost 1 strict | 361.756635 s | 361.041983 s | +0.20% | +0.00% | +0.00% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 | Adaptive Sponsel–COPOMATRIX — streak 10,000 | gained 1 strict and 1 not-strict | 359.329316 s | 370.792344 s | -3.09% | +0.00% | +0.00% |
| Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 | Adaptive Sponsel–COPOMATRIX — streak 1,000 | gained 1 not-strict | 359.329316 s | 361.041983 s | -0.47% | +1.98% | +2.88% |

Zischg–Hadeler gained strict matrices 9181, 9190, and 10325. Zischg–FracESSA gained strict matrices 10327–10330 and
10424–10425. Frank–Wolfe Sponsel gained not-strict matrices 13 and 15. None of the three variants lost a completion.
At equal cutoffs, Adaptive Zischg–Sponsel–COPOMATRIX gained strict matrix 10489 at streak 10, strict matrix 10487 at streak 100,
exchanged strict 10489 for not-strict 10274 at streak 1,000, and gained matrices 10274 and 10488 without a loss at streak 10,000.

At streak 100, Adaptive Sponsel–COPOMATRIX changed the aggregate behavior much more strongly than the one-step Frank–Wolfe addition:
it retained every Sponsel completion while cutting substituted work by more than three quarters. Its COPOMATRIX comparison is not a
strict superset because nine non-strict COPOMATRIX completions timed out in the hybrid. Moving the forced projection forward to
streak 10 did not improve the five-second aggregate. Moving it back to streak 1,000 produced the best completion count and substituted
work of the four tested cutoffs; extending the streak to 10,000 lost one completion and required more substituted work.

## Detailed Model Results

<table border="1" cellpadding="4" style="border-collapse: collapse; border: 3px solid currentColor;">
  <thead style="border-bottom: 3px solid currentColor;">
    <tr><th>Model</th><th>Corpus truth</th><th>Solved</th><th>Node limit</th><th>Timeout</th><th>Error</th></tr>
  </thead>
  <tbody>
    <tr><th rowspan="2">Hadeler 1983 — LDLT reference</th><td>Strictly copositive</td><td>421 (11.50)</td><td>0 (—)</td><td>165 (61.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,469 (43.37)</td><td>0 (—)</td><td>23 (28.50)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Zischg–Hadeler</th><td>Strictly copositive</td><td>424 (12.00)</td><td>0 (—)</td><td>162 (61.50)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,469 (43.37)</td><td>0 (—)</td><td>23 (28.50)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Dutour 2018</th><td>Strictly copositive</td><td>376 (9.61)</td><td>0 (—)</td><td>210 (53.41)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,384 (43.69)</td><td>31 (6.00)</td><td>77 (15.50)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Danninger 1990</th><td>Strictly copositive</td><td>391 (10.50)</td><td>0 (—)</td><td>195 (57.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,406 (43.37)</td><td>0 (—)</td><td>86 (23.62)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">COPOMATRIX 2011</th><td>Strictly copositive</td><td>479 (50.50)</td><td>0 (—)</td><td>107 (58.43)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,448 (43.37)</td><td>0 (—)</td><td>44 (25.11)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Dutour-Danninger</th><td>Strictly copositive</td><td>476 (50.50)</td><td>0 (—)</td><td>110 (54.46)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,453 (43.69)</td><td>0 (—)</td><td>38 (17.95)</td><td>1 (6.00)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Dutour–COPOMATRIX</th><td>Strictly copositive</td><td>475 (50.50)</td><td>0 (—)</td><td>111 (54.46)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,452 (43.69)</td><td>0 (—)</td><td>40 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Sponsel–COPOMATRIX — streak 100</th><td>Strictly copositive</td><td>568 (50.50)</td><td>0 (—)</td><td>18 (80.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,455 (43.69)</td><td>0 (—)</td><td>37 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Sponsel–COPOMATRIX — streak 10</th><td>Strictly copositive</td><td>568 (50.50)</td><td>0 (—)</td><td>18 (80.12)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,451 (43.37)</td><td>0 (—)</td><td>41 (23.40)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Sponsel–COPOMATRIX — streak 1,000 (current)</th><td>Strictly copositive</td><td>569 (50.50)</td><td>0 (—)</td><td>17 (79.56)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,455 (43.69)</td><td>0 (—)</td><td>37 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Sponsel–COPOMATRIX — streak 10,000</th><td>Strictly copositive</td><td>568 (50.50)</td><td>0 (—)</td><td>18 (80.06)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,455 (43.69)</td><td>0 (—)</td><td>37 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Zischg–Sponsel–COPOMATRIX — streak 100</th><td>Strictly copositive</td><td>569 (50.50)</td><td>0 (—)</td><td>17 (79.56)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,455 (43.69)</td><td>0 (—)</td><td>37 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Zischg–Sponsel–COPOMATRIX — streak 10</th><td>Strictly copositive</td><td>569 (50.50)</td><td>0 (—)</td><td>17 (79.56)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,451 (43.37)</td><td>0 (—)</td><td>41 (23.40)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Zischg–Sponsel–COPOMATRIX — streak 1,000</th><td>Strictly copositive</td><td>568 (50.50)</td><td>0 (—)</td><td>18 (80.12)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,456 (43.69)</td><td>0 (—)</td><td>36 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Zischg–Sponsel–COPOMATRIX — streak 10,000 (current)</th><td>Strictly copositive</td><td>569 (50.50)</td><td>0 (—)</td><td>17 (79.56)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,456 (43.69)</td><td>0 (—)</td><td>36 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Adaptive Zischg–Sponsel–COPOMATRIX — streak 100,000</th><td>Strictly copositive</td><td>569 (50.50)</td><td>0 (—)</td><td>17 (79.56)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,456 (43.69)</td><td>0 (—)</td><td>36 (22.26)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Dickinson 2019</th><td>Strictly copositive</td><td>437 (14.50)</td><td>0 (—)</td><td>149 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,470 (43.37)</td><td>0 (—)</td><td>22 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Support-Pruned Dickinson</th><td>Strictly copositive</td><td>437 (14.50)</td><td>0 (—)</td><td>149 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,470 (43.37)</td><td>0 (—)</td><td>22 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Nullity Support-Pruned Dickinson</th><td>Strictly copositive</td><td>437 (14.50)</td><td>0 (—)</td><td>149 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,470 (43.37)</td><td>0 (—)</td><td>22 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">RHS Dickinson</th><td>Strictly copositive</td><td>437 (14.50)</td><td>0 (—)</td><td>149 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,465 (43.37)</td><td>0 (—)</td><td>27 (27.53)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Zischg–Dickinson</th><td>Strictly copositive</td><td>434 (14.00)</td><td>0 (—)</td><td>152 (63.50)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,470 (43.37)</td><td>0 (—)</td><td>22 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Frank–Wolfe Dickinson</th><td>Strictly copositive</td><td>438 (15.00)</td><td>0 (—)</td><td>148 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,472 (43.69)</td><td>0 (—)</td><td>20 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Exact One-Step Frank–Wolfe Dickinson</th><td>Strictly copositive</td><td>438 (15.00)</td><td>0 (—)</td><td>148 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,470 (43.37)</td><td>0 (—)</td><td>22 (29.54)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Pairwise Frank–Wolfe Dickinson</th><td>Strictly copositive</td><td>438 (15.00)</td><td>0 (—)</td><td>148 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,473 (43.69)</td><td>0 (—)</td><td>19 (26.67)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Support-Polished Frank–Wolfe Dickinson</th><td>Strictly copositive</td><td>438 (15.00)</td><td>0 (—)</td><td>148 (64.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,474 (43.69)</td><td>0 (—)</td><td>18 (22.73)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">FracESSA</th><td>Strictly copositive</td><td>428 (12.50)</td><td>0 (—)</td><td>158 (62.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,469 (43.37)</td><td>0 (—)</td><td>23 (28.50)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Zischg–FracESSA</th><td>Strictly copositive</td><td>434 (14.50)</td><td>0 (—)</td><td>152 (63.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,469 (43.37)</td><td>0 (—)</td><td>23 (28.50)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Safi 2021</th><td>Strictly copositive</td><td>437 (50.50)</td><td>0 (—)</td><td>149 (55.89)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,406 (43.69)</td><td>0 (—)</td><td>86 (21.95)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Bundfuss 2008</th><td>Strictly copositive</td><td>456 (50.50)</td><td>0 (—)</td><td>130 (55.89)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,288 (43.37)</td><td>2 (3.00)</td><td>202 (20.88)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Sponsel 2012</th><td>Strictly copositive</td><td>490 (50.50)</td><td>0 (—)</td><td>96 (58.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,296 (43.37)</td><td>0 (—)</td><td>196 (20.19)</td><td>0 (—)</td></tr>
  </tbody>
  <tbody style="border-top: 3px solid currentColor;">
    <tr><th rowspan="2">Frank–Wolfe Sponsel</th><td>Strictly copositive</td><td>490 (50.50)</td><td>0 (—)</td><td>96 (58.00)</td><td>0 (—)</td></tr>
    <tr><td>Not strictly copositive</td><td>1,298 (43.37)</td><td>0 (—)</td><td>194 (20.88)</td><td>0 (—)</td></tr>
  </tbody>
</table>

## Algorithm Classes

- **Submatrix class:** Hadeler 1983, Dickinson 2019, RHS Dickinson, Zischg–Hadeler, and Zischg–Dickinson.
- **Support-generation submatrix hybrids:** Support-Pruned Dickinson and Nullity Support-Pruned Dickinson.
- **Witness-first submatrix hybrids:** Frank–Wolfe Dickinson, Exact One-Step Frank–Wolfe Dickinson, Pairwise Frank–Wolfe Dickinson,
  and Support-Polished Frank–Wolfe Dickinson.
- **Cone class:** Dutour 2018, Danninger 1990, COPOMATRIX 2011, Adaptive Dutour-Danninger, Adaptive Dutour–COPOMATRIX, Adaptive
  Sponsel–COPOMATRIX, Adaptive Zischg–Sponsel–COPOMATRIX, Safi 2021, Bundfuss 2008, and Sponsel 2012.
- **Witness-first cone hybrid:** Frank–Wolfe Sponsel.
- **First-order KKT class:** FracESSA and Zischg–FracESSA.

## Matrices Solved By No Cone Algorithm

The cutoffs are cumulative and inclusive. A matrix counts only when all eleven selected five-second cone-algorithm results are
unresolved through a node limit, timeout, or error.
The remaining columns are each named model's cumulative unresolved total over the complete corpus; they are not restricted to the
no-cone intersection.

Adaptive Sponsel–COPOMATRIX counts once in that intersection, using its current streak-1,000 binary. The preserved streak-10,
streak-100, and streak-10,000 results are shown in the comparison tables but are not treated as additional algorithms. Relative to
the streak-10,000 intersection, restoring streak 1,000 removes strict order-88 matrix 10488 from the common unresolved set.
Adaptive Zischg–Sponsel–COPOMATRIX likewise counts once, using its current streak-10,000 binary; its preserved streak-10,
streak-100, streak-1,000, and streak-100,000 identities are comparisons rather than additional algorithms. It does not further
reduce the 42-matrix intersection.

| Maximum dimension | Matrices solved by no cone algorithm | Unsolved by Dickinson 2019 | Unsolved by Support-Pruned Dickinson | Unsolved by Nullity Support-Pruned Dickinson | Unsolved by Frank–Wolfe Dickinson | Unsolved by Exact One-Step Frank–Wolfe Dickinson | Unsolved by Pairwise Frank–Wolfe Dickinson | Unsolved by Support-Polished Frank–Wolfe Dickinson | Unsolved by Hadeler 1983 | Unsolved by FracESSA |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `n <= 10` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `n <= 20` | 20 | 11 | 11 | 11 | 11 | 11 | 11 | 11 | 12 | 12 |
| `n <= 30` | 27 | 21 | 21 | 21 | 20 | 20 | 20 | 20 | 38 | 31 |
| `n <= 40` | 27 | 41 | 41 | 41 | 40 | 40 | 40 | 40 | 58 | 51 |
| `n <= 50` | 28 | 64 | 64 | 64 | 63 | 63 | 63 | 63 | 81 | 74 |
| `n <= 60` | 28 | 84 | 84 | 84 | 83 | 83 | 83 | 83 | 101 | 94 |
| `n <= 70` | 31 | 111 | 111 | 111 | 108 | 110 | 107 | 106 | 128 | 121 |
| `n <= 80` | 31 | 131 | 131 | 131 | 128 | 130 | 127 | 126 | 148 | 141 |
| `n <= 90` | 32 | 151 | 151 | 151 | 148 | 150 | 147 | 146 | 168 | 161 |
| `n <= 100` | 42 | 171 | 171 | 171 | 168 | 170 | 167 | 166 | 188 | 181 |

### Zischg variant cumulative unresolved totals

These columns use the same cumulative and inclusive cutoffs as the model columns in the preceding table. They count every unresolved
matrix for the named model, not only matrices in the no-cone intersection.

| Maximum dimension | Zischg–Hadeler unresolved | Zischg–Dickinson unresolved | Zischg–FracESSA unresolved | Adaptive Zischg–Sponsel–COPOMATRIX streak-10,000 unresolved |
|---:|---:|---:|---:|---:|
| `n <= 10` | 0 | 0 | 0 | 3 |
| `n <= 20` | 12 | 11 | 12 | 28 |
| `n <= 30` | 35 | 24 | 25 | 35 |
| `n <= 40` | 55 | 44 | 45 | 35 |
| `n <= 50` | 78 | 67 | 68 | 36 |
| `n <= 60` | 98 | 87 | 88 | 36 |
| `n <= 70` | 125 | 114 | 115 | 42 |
| `n <= 80` | 145 | 134 | 135 | 42 |
| `n <= 90` | 165 | 154 | 155 | 43 |
| `n <= 100` | 185 | 174 | 175 | 53 |

### The Original 26 Bad Matrices Through `n <= 20`

Here, **bad matrix** retains its original operational meaning: all eight cone-algorithm runs available before Adaptive
Dutour–COPOMATRIX timed out. It does not by itself mean that the matrix is singular, on the copositive boundary, or mathematically
degenerate. The 26 matrices produced 208 timeouts; none of these results reached the node limit or returned an error.

| Construction | Database family | Matrix IDs | Dimensions | Strict classification |
|---|---|---|---|---|
| Hildebrand circulant | Exceptional boundary / circulant support `n-2` | 10276, 10278, 10282–10299 | 11–20 | 20 not strict |
| Graph clique encoding | `c-fat` | 9171–9172 | 16, 18 | 2 strict |
| Graph clique encoding | `cisqrg` | 9175–9176 | 18, 20 | 2 strict |
| Graph clique encoding | `krcgg` | 9237 | 18 | 1 not strict |
| Dannenberg-Schürmann lift | Strict exceptional perfect copositive lift | 10420 | 20 | 1 strict |

Thus 20 matrices are members of one family. Five more use one common graph construction but come from three graph families, and the
last matrix uses a separate perfect-copositive lift construction.

Frank–Wolfe Sponsel timed out on all 26 matrices and removed none when it became the eighth cone algorithm. Adaptive
Dutour–COPOMATRIX subsequently solved `cisqrg` matrix 9175 and `krcgg` matrix 9237. All four Adaptive Sponsel–COPOMATRIX streak
settings and all five Adaptive Zischg–Sponsel–COPOMATRIX settings solved all six graph/lift cases but none of the 20 Hildebrand
circulants in this
original set. The current eleven-model intersection through `n <= 20` therefore contains exactly those 20 Hildebrand matrices.

#### Submatrix and KKT models on these 26 matrices

These are fresh targeted runs with a five-second per-matrix cutoff, parent CPU 3, and workers on CPUs 4–9. They are stored under
the historical campaign label `bad26_strict_zero_5s_2026-08-09`. The current schema merges those rows into the same structured
`preprocessing=none` identity as the complete-corpus reference rows. Every cell is `solved / unresolved`.

| Model | Hildebrand circulant (20) | `c-fat` (2) | `cisqrg` (2) | `krcgg` (1) | Strict lift (1) | Total (26) |
|---|---:|---:|---:|---:|---:|---:|
| Hadeler 1983 | 8 / 12 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **14 / 12** |
| Zischg–Hadeler | 8 / 12 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **14 / 12** |
| Dickinson 2019 | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| Support-Pruned Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| RHS Dickinson | 4 / 16 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **10 / 16** |
| Zischg–Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| Frank–Wolfe Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| Exact One-Step Frank–Wolfe Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| Pairwise Frank–Wolfe Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| Support-Polished Frank–Wolfe Dickinson | 9 / 11 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **15 / 11** |
| FracESSA | 8 / 12 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **14 / 12** |
| Zischg–FracESSA | 8 / 12 | 2 / 0 | 2 / 0 | 1 / 0 | 1 / 0 | **14 / 12** |

All twelve models solved all six graph/lift matrices. Their unresolved cases are exclusively the non-strict Hildebrand circulants.
Base Dickinson, Support-Pruned Dickinson, Zischg–Dickinson, and all four Frank–Wolfe variants solve the nine cases through dimension
15; neither immediate strict-zero termination nor support-generation pruning extends that cutoff in this family. RHS Dickinson
solves four through dimension 13. Hadeler, Zischg–Hadeler, FracESSA, and Zischg–FracESSA solve eight through dimension 14. Across the
338 targeted rows there are 186 correct completions and 152 timeouts, with no mismatch, error, or node limit.

#### Hildebrand circulant family: high-support boundary zeros

All 20 Hildebrand matrices are exceptional, extremal, circulant boundary matrices. Each has cyclic minimal zeros supported on `n-2`
coordinates, so the zero proving failure of strict copositivity uses between 9 and 18 coordinates. No diagonal or two-generator face
contains that zero. The cone algorithms' inexpensive vertex and edge tests therefore cannot see the actual witness.

Their circulant rows also have balanced positive and negative off-diagonal entries and no zero entries. No root row satisfies the
Adaptive Dutour-Danninger narrow-pivot gate. A root Danninger step has between 210 and 92,378 staircase children across these 20 cases;
the count grows from the balanced sign partition even though the recursion later reduces dimension. Same-dimensional subdivision can
instead refine toward the high-support zero without ever generating it exactly.

Exact rational-angle construction and denominator clearing give these primitive integer matrices coefficients between 497 and 4,660
bits. This does not change their mathematics, but it makes every exact child update, Schur product, comparison, and factorization more
expensive. Coefficient size is a compounding cost rather than the sole cause: nearby order-11 and order-12 family members with large
coefficients were still solved by one cone model.

#### Graph families: one construction, global clique structure

The five `c-fat`, `cisqrg`, and `krcgg` matrices all use the same graph encoding. For a graph `G` and parameter `d`, the diagonal and
non-edge entries are `d`, while edge entries are `-1`. Motzkin-Straus makes the matrix strictly copositive when `d` equals the clique
number and places it on the boundary when `d` is one less.

The four `c-fat` and `cisqrg` rows are strict interior matrices and are all full rank. Their difficulty is therefore not singularity or
an exact zero. Every negative two-generator ratio is only `1/d^2`, so the local edge tests are far from rejection; the algorithms must
resolve the graph's global clique structure. Because the answer is strict, no branch can reject early and the complete cone cover must
be certified. Their mixed-sign root rows give Danninger staircase counts as large as 92,378.

The `krcgg` row is also full rank, but it is a boundary instance with a zero supported on a ten-vertex clique. That global zero is again
invisible to vertex and edge tests. Its first root row is easy, but other rows have mixed-sign staircase counts up to 24,310, and the
difficult structure reappears below the root.

#### Dannenberg-Schürmann lift: strict, singular, and highly repeated

Matrix 10420 is not a boundary counterexample. It is strictly and perfectly copositive, obtained by applying 15 coordinate-duplication
lifts to the exceptional order-five seed. Its final 16 rows and columns are identical, so the order-20 matrix has rank 5 and nullity 15.
The kernel directions are mixed-sign coordinate differences and therefore do not contradict strict copositivity.

The seed also contains a negative two-generator ratio of `2500/2501`: extremely close to rejection, but still strictly on the passing
side. The matrix is exceptional, so Sponsel's strict `H` certificate cannot accept it at the root. The cone models consequently face a
strict input with many symmetric equivalent regions, no negative witness, and near-boundary local geometry; they do not exploit the
duplicate-coordinate structure.

#### Shared failure mechanism

- Dutour, Safi, Bundfuss, and the subdivision part of Sponsel keep the same dimension. High-support zeros may be approached without
  being generated, while strict inputs require all generated regions to pass.
- Danninger and COPOMATRIX reduce dimension, but balanced positive/negative pivot rows create combinatorially many children first.
- Adaptive Dutour-Danninger falls back to same-dimensional Dutour refinement when no narrow row exists.
- Adaptive Dutour–COPOMATRIX completed graph matrices 9175 and 9237 but timed out on the other 24 original cases, including all 20
  Hildebrand circulants; its forced dimension reduction therefore helped selectively rather than removing this shared hard set.
- All four Adaptive Sponsel–COPOMATRIX streak settings completed all six graph/lift cases but timed out on every Hildebrand
  circulant in the original hard set. All five Adaptive Zischg–Sponsel–COPOMATRIX cutoffs had the same split on these 26 cases.
  Combining Sponsel and
  forced COPOMATRIX progress removed the non-Hildebrand part, but neither the base nor projection-local component decomposition
  removed the dominant boundary family.
- Exceptional matrices defeat Sponsel's root `H` certificate, exact large coefficients increase per-node cost, and none of these
  baselines recognizes graph symmetry or duplicate coordinates.
- Frank–Wolfe Sponsel's single centre-to-vertex line did not expose any of these high-support boundary zeros and cannot reject the
  strict graph or lifted inputs, so each case continued into the inherited Sponsel partition and timed out.

The dominant group is therefore one deliberately adversarial boundary family, but there is no single degeneracy shared by all 26.
Their common operational property is that the available cone certificates are local while the decisive structure is global.
