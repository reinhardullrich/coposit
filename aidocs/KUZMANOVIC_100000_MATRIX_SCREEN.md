# Kuzmanović 100,000-Matrix Screen

## Conclusion

The complete 100,000-matrix result archive accompanying Dejan Kuzmanović's 2022 master's thesis, *Copositivity detection — a
preprocessing study*, was reconstructed exactly and checked with coposit's exact CBDD-Zed Dickinson model. Every matrix received an
ordinary-copositivity decision. The entire four-worker run finished in 8.255 seconds, with no timeout, node limit, parse failure, or
execution error.

These matrices were therefore removed from the maintained database after this screen. They are low-order random matrices, the great
majority are rejected by very cheap conditions, and none exposed an unresolved case. Keeping 100,000 similarly generated benchmark rows
would heavily skew corpus statistics without adding a substantially different solver capability, matrix structure, or difficulty
class. The reproducible importer and runner remain under `testdata/archive/`; the six separately printed exact Kuzmanović examples
remain in the main corpus with source ID 96.

## Source material and reconstruction

The source archive contains symmetric integer matrices of orders 5 through 20. Its generator used entries of the form
`Random.Next(0, 60) - Random.Next(0, 20)`, hence values from -19 through 59, and did not retain random seeds. The archived matrices,
rather than a newly seeded approximation, were imported so that the published examples and labels could be checked exactly.

| Order | Matrices |
| ---: | ---: |
| 5 | 10,000 |
| 6–19 | 5,000 at each order |
| 20 | 20,000 |
| **Total** | **100,000** |

The imported rows retained the published example number, exact matrix, published output label, optional reported vector and quadratic
value, source filename, and a SHA-256 of the exact source record. The source ZIP has SHA-256
`3bc06b1eba6ce3d4566c67e6d5ce63b83bd5d23481580e0f9c13e21bb8d5cc17`.

The thesis archive reports:

| Published output | Matrices |
| --- | ---: |
| `copositive` | 5,988 |
| `not_copositive` | 91,162 |
| `no_answer` | 2,850 |

Of the 91,162 published negative decisions, 85,006 already come from a negative diagonal entry. The remaining reported preprocessing
cases account for 1,458, 474, 0, and 4,224 decisions respectively. This concentration in cheap negative cases is a principal reason
the archive is not representative of the maintained benchmark corpus.

## Exact copositivity run

The check used `cbdd_zed_dickinson` in ordinary-copositivity mode with preprocessing disabled, a nominal ten-second limit per matrix,
CPU 3 for dispatch, and CPUs 4–7 for four persistent solver workers. The exact native module had SHA-256
`909db23f342fd1f3e1ab1bf3377a129fc18576d64fe5f15be7e21eaeb7f49768`.

| Measure | Result |
| --- | ---: |
| Matrices assigned | 100,000 |
| Exact Boolean decisions | 100,000 |
| Timeouts / node limits / errors | 0 / 0 / 0 |
| Wall time | 8.255 s |
| Throughput | about 12,114 matrices/s |
| Sum of recorded native call time | 1.785 s |
| Native call time, minimum | 0.625 µs |
| Native call time, median | 5.083 µs |
| Native call time, mean | 17.854 µs |
| Native call time, 95th percentile | 27.250 µs |
| Native call time, 99th percentile | 191.375 µs |
| Native call time, maximum | 16.698 ms |

The exact model classified 8,554 matrices as copositive and 91,446 as not copositive. This includes complete decisions for all 2,850
rows on which the published preprocessing study returned `no_answer`: 2,596 copositive and 254 not copositive.

## Comparison with the published preprocessing labels

`no_answer` is not a truth claim. Among the other 97,150 rows, the exact result agreed with 97,114 published labels and disagreed
with 36:

| Published output | Exact copositive | Exact not copositive | Agreement |
| --- | ---: | ---: | ---: |
| `copositive` | 5,955 | 33 | 5,955 / 5,988 |
| `not_copositive` | 3 | 91,159 | 91,159 / 91,162 |
| `no_answer` | 2,596 | 254 | not applicable |

All 36 disagreements were independently rerun with exact Dickinson Final in ordinary-copositivity mode; Dickinson Final agreed with
CBDD-Zed on every one. Seven of the 33 matrices published as copositive but classified as non-copositive already have an exact failing
order-two principal submatrix. For one of the three matrices published as non-copositive but classified as copositive, the archived
purported witness contains a negative coordinate and is therefore not a valid copositivity witness; the other two contain no published
witness. Consequently, the source labels were retained only as historical experimental output and were never promoted to maintained
truth fields.

## Removal decision

The separate `kuzmanovic_test_matrices` staging table was deleted after the completed screen. No row was merged into the maintained
`matrices` table, so no benchmark membership, truth value, literature-solved claim, or historical timing row had to be transferred.
The deletion does not affect the six separately printed Kuzmanović matrices already retained in the main corpus, source ID 96, the
450 Bomze–Peng–Qiu–Yıldırım matrices, or their solver results.

The reconstruction remains reproducible from:

- `testdata/archive/import_kuzmanovic_preprocessing_results_2026_08_14.py`
- `testdata/archive/run_kuzmanovic_cbdd_zed_2026_08_14.py`
