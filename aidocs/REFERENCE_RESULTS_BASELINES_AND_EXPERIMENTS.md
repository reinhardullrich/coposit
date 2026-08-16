# Baseline and Experimental Model Reference Results

Last updated: 2026-08-16

Status: complete.

## Protocol

The campaign covers the physical model directories under `models/baselines/` and `models/experiments/`. It requests ordinary
copositivity only (`copositive`), enables complete preprocessing and diagnostics, and uses a five-second cooperative timeout per
matrix. CPU 3 performs dispatch and SQLite writes; native workers use CPUs 4–9. Results are committed to
`experiments/diagnostics.sqlite3` after every matrix.

Smoke contains 49 matrices. Core and Stress contains 512 matrices and excludes every matrix already decided fully by preprocessing.
`Solved / timeout / other` never counts a timeout, process failure, or unsupported mode as a negative classification. These are
ordinary-predicate results, not complete copositivity/strict-copositivity classifications.

## Results

| Model | Kind | Smoke solved / timeout / other | Core solved / timeout / other | State |
|---|---|---:|---:|---|
| `bundfuss_2008` | literature baseline | 30 / 19 / 0 | 221 / 291 / 0 | complete; current hash `a428be12c9906c9354935c0c0870a1b2bd3a8a42140c0aa1824f37af0cdaf446` |
| `copomatrix_2011` | literature baseline | 45 / 4 / 0 | 349 / 163 / 0 | complete; current hash `b4364d0fcd35501b1e5781bcac45170a25d5697d3d8c71a9209445be91af00f0` |
| `danninger_1990` | literature baseline | 45 / 4 / 0 | 344 / 168 / 0 | complete; current hash `afdd77f4ac91ade70a79aa7f7a79798b8fc2b30f7eafd21af8fd5691b0d25db7` |
| `dutour_2018` | literature baseline | 33 / 10 / 6 | 249 / 220 / 43 | complete; `other` is the documented 50,000-open-node limit; current hash `5376b28ce3b1a09bb808894e4286542e4f58a7f707ebe598398bffc819e86d94` |
| `safi_2021` | literature baseline | 44 / 5 / 0 | 318 / 194 / 0 | complete; current hash `f738f08ec798a1ddd41d337323fbf19285c87620436f7aec0688d51f103f8b4b` |
| `sponsel_2012` | literature baseline | 29 / 20 / 0 | 230 / 282 / 0 | complete; current hash `053baffc267898c91950d0eb013ab5fcfec98434d43af0ab3743b552eb46ae26` |
| `adaptive_sponsel_copomatrix` | coposit experiment | 44 / 5 / 0 | 403 / 109 / 0 | complete; current hash `7e44f1b2ff610403a2d6ff52740538748f8d13ad197bc18ac545ed3c16a15eae` |
| `adaptive_dutour_copomatrix` | coposit experiment | 41 / 8 / 0 | 296 / 216 / 0 | complete; current hash `5c6777871741ab5e3377e05f9d64a953358d199bbf75bc38876f7acd96a75d17` |
| `adaptive_dutour_danninger` | coposit experiment | 33 / 8 / 8 | 254 / 213 / 45 | complete; `other` is the shared 50,000-open-node limit; current hash `beb616de8b139f8e7c62238d2e5964654d0a33684ea7962889a1fd09a0cb7ae8` |
| `adaptive_zischg_sponsel_copomatrix` | coposit experiment | 44 / 5 / 0 | 405 / 107 / 0 | complete; current hash `a5aa1b8ede93af972f7300d25b4ff31157138c7e9f970d6cc0b8afa10cf61565` |
| `frank_wolfe_sponsel` | coposit experiment | 29 / 20 / 0 | 230 / 282 / 0 | complete; current hash `3a8c6c08dcaf8c29e39abf2cdd07ecc8e448b90510eb1495d939bca115a5a609` |

## Human Summary

Among the ordinary-copositivity cone models, `adaptive_zischg_sponsel_copomatrix` completed the most cases: 405 of 512. Its parent
`adaptive_sponsel_copomatrix` completed 403. The strongest literature baselines were `copomatrix_2011` with 349 and
`danninger_1990` with 344. `frank_wolfe_sponsel` completed exactly the same 230 cases as `sponsel_2012` under this protocol.

## Verification

Every listed model has exactly 512 rows under its recorded current binary hash. All rows use ordinary-copositivity mode, complete
preprocessing, a five-second timeout, and nonempty diagnostics. Every completed result with known corpus truth agrees with that
truth. After verification, 16,589 obsolete-hash rows belonging to the four newly rebuilt experimental models were removed. Both
SQLite databases passed `PRAGMA integrity_check`.
