# Hadeler-Based Model Reference Results

Last updated: 2026-08-16

Status: current experiment complete.

## Protocol

All valid rows use the current Release companions rebuilt at 2026-08-16 17:57 after the latest shared-preprocessing changes. Runs
request combined copositivity and strict-copositivity classification (`both`), enable diagnostics, and use a five-second cooperative
timeout per matrix. CPU 3 performs dispatch and SQLite writes; native workers use CPUs 4–9. Results are committed to
`experiments/diagnostics.sqlite3` after every matrix. Hadeler-based rows from earlier companion hashes were removed before this pass.

Smoke contains 49 matrices. Core and Stress contains 512 matrices and excludes every matrix already decided fully by preprocessing.
`Solved / timeout / other` never counts a timeout or process failure as a negative classification.

## Current Results

| Model | Parameter or input contract | Smoke solved / timeout / other | Core solved / timeout / other | State |
|---|---|---:|---:|---|
| `affine_companion_dickinson` | — | 49 / 0 / 0 | 452 / 60 / 0 | complete; current hash `9fbfcd76f275f2877f7be775ad73961bad04c025c48ff1efa64bb3bc69295e70`; diagnostics complete |
| `bdd_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `606833c6cf6bd32b8aacae72a85417ec6dc9d70adb501790e7aed33fae8152f3`; diagnostics complete |
| `breadth_first_singular_lift_dickinson` | — | 49 / 0 / 0 | 452 / 60 / 0 | complete; current hash `86087e19ab4c6d55645976ce8977093af8d534293b265517dec9e71dfadda436`; diagnostics complete |
| `cbdd_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `3d6414895409db18a5a2e5a70297a9bb0090004e9a0d0d749d5b7fa4225436e2`; diagnostics complete |
| `upper_endpoint_cbdd_dickinson` | — | 49 / 0 / 0 | 456 / 56 / 0 | complete; current hash `5f199c3da0701cedbad684f5f163be97eff673a63e7321644401534201cab175`; diagnostics complete |
| `cbdd_dickinson_improved_1` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `f6287b5ca7ebcd0b84851a0c6b945100fc2a10c9af3e8bf6e14fbdec3ca1bf11`; diagnostics complete |
| `ceiling_pruned_dickinson` | — | 49 / 0 / 0 | 452 / 60 / 0 | complete; current hash `284829ce1d78292fd410bc96ddeb1fc771258bb5b1b39dc950d3bcabc86f08db`; diagnostics complete |
| `clingo_sat_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `6064632cbc17c385a83cc36d343b9fecec7a3383ed6812bd684c91348f0a5ef3`; diagnostics complete |
| `czdd_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `cd3b15b804eba8ad1460f62ac118fd8c301846f912dcf50083662978da55a70a`; diagnostics complete |
| `dense_bitset_dickinson` | default 1 GiB dense-family cap | 49 / 0 / 0 | 379 / 62 / 71 | complete; 71 explicit size-limit errors; current hash `7eb9d486e4ca403f8bffad636f44dfa43dcc2ddad85d59f24308357c861f6808`; diagnostics complete |
| `dickinson_2019` | — | 49 / 0 / 0 | 380 / 132 / 0 | complete; current hash `db839d4b33c67390eaf390ff4d3ea7e91599f76e3a55df04e5e564f1657d8e16`; diagnostics complete |
| `fracessa` | — | 49 / 0 / 0 | 373 / 139 / 0 | complete; current hash `4855cc7794c3ce7a1ffef0002f1df78f1b509b0a57e308e1d4124c15e19776ef`; diagnostics complete |
| `frank_wolfe_dickinson` | — | 49 / 0 / 0 | 380 / 132 / 0 | complete; current hash `fafe93e93465b479d39ce98b00c5faa5c61a8adc820f4dfd57a783c8b32922a9`; diagnostics complete |
| `hadeler_1983` | — | 49 / 0 / 0 | 372 / 140 / 0 | complete; current hash `5441864a91ce8277df403f2ffcc51526aaae6770ba0e265d7409cbf95c2b4f14`; diagnostics complete |
| `interval_recursive_dickinson` | — | 49 / 0 / 0 | 379 / 133 / 0 | complete; current hash `0522a71a97fb836cdc23770417ad92c14d19c6047d235fa23f0b9308ed4da8af`; diagnostics complete |
| `kernel_cone_dickinson` | — | 49 / 0 / 0 | 452 / 60 / 0 | complete; current hash `e7ef99ffbaa28d190fc913f113260f9306d399e2801f59b54828c01903d327a4`; diagnostics complete |
| `layered_singular_lift_dickinson` | — | 49 / 0 / 0 | 452 / 60 / 0 | complete; current hash `350ce9364c3b7668b6dd848f55e70a96f733f19722e8975c2422e6ab66e1ef60`; diagnostics complete |
| `multithreaded_cbdd_dickinson` | internally parallel | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `82b8e79dfe6879a6d09be372830d8b70f0d87e33a965b0eab40babda1a561f45`; diagnostics complete |
| `nullity_support_pruned_dickinson` | — | 49 / 0 / 0 | 380 / 132 / 0 | complete; current hash `eb10b9cd82485841432a044b057ad97dc35255b594f12dff9a8c9c84a2b9b01a`; diagnostics complete |
| `one_step_frank_wolfe_dickinson` | — | 49 / 0 / 0 | 380 / 132 / 0 | complete; current hash `ba397746515edc3f4405fda44b43d8d9d5ace799117f032ee5ec06c030f1e846`; diagnostics complete |
| `pairwise_frank_wolfe_dickinson` | — | 49 / 0 / 0 | 380 / 132 / 0 | complete; current hash `37e83d3190b260c219e6dd0c5148cb1bc28578345cc4bbc2786581d1d23c5738`; diagnostics complete |
| `rhs_dickinson` | — | 49 / 0 / 0 | 374 / 138 / 0 | complete; current hash `f38a9b9bbdc66fc875c7a5e99c30e3bb48fc60922e858c95fcbd68fe75920f7e`; diagnostics complete |
| `sat_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `8e8e120fa9f914c7394676138ddd897936b8ea675e9ee5e939d841128fa464f1`; diagnostics complete |
| `support_polished_frank_wolfe_dickinson` | — | 49 / 0 / 0 | 381 / 131 / 0 | complete; current hash `149ca4834609770d1cbe48e7f184d9f9286bb6c949159c43287bd40549978dad`; diagnostics complete |
| `support_pruned_dickinson` | — | 49 / 0 / 0 | 379 / 133 / 0 | complete; current hash `e663c6bdd159d53d9656e944d46f8d78daf8f7529d5090c73d77ecdcca4765ab`; diagnostics complete |
| `wide_certificate_cbdd_dickinson` | percentage 90 | 49 / 0 / 0 | 452 / 60 / 0 | complete; stored as `wide_certificate_cbdd_dickinson@90`; current hash `0b19379add05b0828d5b4178b5abb1dc7267c0931e6811f0848f6f0a675f7818`; diagnostics complete |
| `wide_certificate_sat_dickinson` | percentage 90 | 49 / 0 / 0 | 452 / 60 / 0 | complete; stored as `wide_certificate_sat_dickinson@90`; current hash `5146c7ac1be858834935b8e4bce9a3ae3920bb9235b819590f167113ea9ec2e0`; diagnostics complete |
| `zdd_dickinson` | — | 49 / 0 / 0 | 457 / 55 / 0 | complete; current hash `8c561d4f2f14758b374177119c3a9062156a714e177df011e3efac93f35f3c1c`; diagnostics complete |
| `zischg_dickinson` | — | 49 / 0 / 0 | 379 / 133 / 0 | complete; current hash `dabbfa9212ad7021d21113baccbe4e0b2aeb297e49ec8a4621916ad077c3bc6a`; diagnostics complete |
| `zischg_fracessa` | — | 49 / 0 / 0 | 375 / 137 / 0 | complete; current hash `39b3318dae5027f0070d280d1641e96e3b7c9b563c0eea0c40f6dff830511354`; diagnostics complete |
| `zischg_hadeler` | — | 49 / 0 / 0 | 374 / 138 / 0 | complete; current hash `51751ecac10a73b73292762ec2db9eaf2d61952782292cd05451fe7c8d397617`; diagnostics complete |

## Verification

All 31 maintained model identities have exactly 512 current-hash rows, combined mode, complete preprocessing, and nonempty diagnostics.
All completed classifications agree with available corpus truth. `PRAGMA integrity_check` returns `ok`. Timeouts and explicit model
limits remain unresolved and are never counted as negative classifications.
