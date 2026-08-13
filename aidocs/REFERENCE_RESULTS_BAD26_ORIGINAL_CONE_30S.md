# Original Cone Baselines On The 26 Bad Matrices — 30 Seconds

Last verified: 2026-08-09

## Scope

This targeted experiment repeats the 26 matrices that no selected cone algorithm solved in the complete five-second reference runs.
It includes only the six maintained historical or source-derived cone baselines:

- Dutour 2018;
- Danninger 1990;
- COPOMATRIX 2011;
- Safi 2021;
- Bundfuss 2008;
- Sponsel 2012.

Adaptive Dutour-Danninger and Frank–Wolfe Sponsel are excluded because they are coposit-created variants, not original baselines.
“Solved” means the model returned a strict-copositivity Boolean matching the corpus. A timeout, node limit, or error remains
unresolved and is never counted as a negative classification.

The set contains 20 Hildebrand circulants, five graph-encoding matrices, and one Dannenberg-Schürmann lift. Five matrices are strictly
copositive and 21 are not strictly copositive.

## Run Configuration

- Timeout: 30 seconds per matrix.
- Dispatcher and database writer: CPU 3.
- Six persistent single-threaded native workers: CPUs 4 through 9.
- Historical campaign label before the result schema adopted a structured preprocessing column: `bad26_original_cone_30s_2026-08-09`.
- Stored rows: 156, covering all 26 matrices for all six models.
- Database integrity after import: `ok`.

| Model | Native-module SHA-256 |
|---|---|
| Dutour 2018 | `1b8263b9d3b68b7c6919ce8a508d64cb801692977dbfe5aa45f39ccde7d95b67` |
| Danninger 1990 | `bc67b28681faea6c5c677e1472fd429a838a5cc34f38ed1a3ee05276a6eac312` |
| COPOMATRIX 2011 | `7b5fa023c9ff053eaf2fd03cadafae3da28121d90d179f70ea893a800b88ee47` |
| Safi 2021 | `0bd6d3d0430c0d3d01f39659073eeabc1e21ae885c9b788d5ee201c07fd9b3d1` |
| Bundfuss 2008 | `475a228ac7a4aca81380c8b2ef169cf29c74a603f3db0e1d031343de7922013f` |
| Sponsel 2012 | `2bd8f3502f6278e645c6d080ecf3503c547ffdaf868eced6dee8c98e557112cd` |

## Aggregate Results

`Substituted work` is the sum of completed native times plus 30 seconds per timeout. It measures the recorded worker work rather
than dispatcher balance.

| Model | Solved | Strict solved | Not-strict solved | Timeout | Node limit | Error | Observed wall | Substituted work |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Dutour 2018 | 0 | 0 | 0 | 26 | 0 | 0 | 150.381 s | 780.000000 s |
| Danninger 1990 | 0 | 0 | 0 | 26 | 0 | 0 | 150.376 s | 780.000000 s |
| COPOMATRIX 2011 | 3 | 1 | 2 | 23 | 0 | 0 | 129.995 s | 721.805532 s |
| Safi 2021 | 0 | 0 | 0 | 26 | 0 | 0 | 150.423 s | 780.000000 s |
| Bundfuss 2008 | 0 | 0 | 0 | 26 | 0 | 0 | 150.427 s | 780.000000 s |
| Sponsel 2012 | 2 | 2 | 0 | 24 | 0 | 0 | 134.723 s | 744.300407 s |
| **Total model–matrix runs** | **5** | **3** | **2** | **151** | **0** | **0** | **866.325 s** | **4,586.105939 s** |

All six baselines had timed out on all 26 matrices at five seconds. At 30 seconds, COPOMATRIX rescued Hildebrand matrices 10276 and
10278 and strict lift 10420; Sponsel rescued strict graph matrices 9171 and 9175. These are five distinct matrices, so 21 remain
unresolved by every original cone baseline at the longer cutoff. Every completed classification matched the corpus, and no run
reached a node limit or returned an error.

## Matrix-Level Results

| Matrix ID | n | Family | Truth | Dutour 2018 | Danninger 1990 | COPOMATRIX 2011 | Safi 2021 | Bundfuss 2008 | Sponsel 2012 |
|---:|---:|---|---|---|---|---|---|---|---|
| 10276 | 11 | Hildebrand circulant | not strict | timeout | timeout | solved 11.632 s | timeout | timeout | timeout |
| 10278 | 12 | Hildebrand circulant | not strict | timeout | timeout | solved 9.723 s | timeout | timeout | timeout |
| 10282 | 13 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10283 | 13 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10284 | 14 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10285 | 14 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10286 | 14 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10287 | 14 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10288 | 15 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 9171 | 16 | `c-fat` | strict | timeout | timeout | timeout | timeout | timeout | solved 9.977 s |
| 10289 | 16 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10290 | 16 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10291 | 16 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10292 | 16 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10293 | 17 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 9172 | 18 | `c-fat` | strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 9175 | 18 | `cisqrg` | strict | timeout | timeout | timeout | timeout | timeout | solved 14.324 s |
| 9237 | 18 | `krcgg` | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10294 | 18 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10295 | 18 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10296 | 18 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10297 | 19 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 9176 | 20 | `cisqrg` | strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10298 | 20 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10299 | 20 | Hildebrand circulant | not strict | timeout | timeout | timeout | timeout | timeout | timeout |
| 10420 | 20 | Dannenberg-Schürmann lift | strict | timeout | timeout | solved 10.451 s | timeout | timeout | timeout |

The eighteen still-unresolved Hildebrand matrices begin at dimension 13. The three unresolved graph matrices are 9172, 9176, and
9237. COPOMATRIX is the only baseline that solved the strict lift, while Sponsel is the only baseline that solved any graph input.
