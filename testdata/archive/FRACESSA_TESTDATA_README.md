# FracESSA Test Data

`fracessa_testdata.sqlite3` is the canonical store for exact test matrices,
complete expected candidate results where available, and timing data.

`copos_testdata.sqlite3` is the separate exact test corpus for general strict-copositivity and copositivity decisions. It is not
limited by FracESSA's dimension-63 support representation. Its `matrices` table contains 1,569 permutation-inequivalent exact
integer matrices with dimensions 1-3,361: 427 are strictly copositive, 56 are copositive but not strictly copositive, 55 are
non-copositive, and 1,031 legacy non-strict rows have not yet been separated into the latter two classes. The nullable
`is_copositive` field distinguishes this unknown legacy state from a proved non-copositive result. The original 1,069 rows are
integer-scaled reduced B matrices constructed while replaying exact candidate baselines. Their nullable `fracessa_matrix_id` is
the only link back to `fracessa_testdata.sqlite3`; candidate and support data are deliberately not duplicated here.

IDs 9157-9163 are published examples M1-M7 from Bras, Eichfelder, and Judice, and IDs 9164-9165 are the strict and non-copositive
sides of their C5 graph construction. The August 7 general-corpus import added 490 representatives: 78 from the 81-file
`Copositivity/Matrices` graph corpus, 329 from the 330 exact rational test inputs in `AlexOertel/MinCOP_LDLT`, and 83 matrices
constructed from the 29 official Second DIMACS Challenge clique graphs reported by Žilinskas. Three Johnson files collapse under
simultaneous row-and-column permutation, and one Oertel-Schürmann file is an exact duplicate. `matrix_sources` retains all 494
general-import source occurrences plus the four `bad matrices` records, for 498 rows covering 494 matrix representatives and 15
families. The rows include repository URLs, exact commits or archive SHA-256 values, original entries, denominator scales,
classification arguments, and links to published benchmark rows.

The `bad matrices` family records the four locally important algorithmic stress cases and their origins. ID 9161 is matrix M5 from
Brás, Eichfelder, and Júdice, a copositive boundary matrix on which exact Bundfuss refinement timed out. ID 9656 is the constructed
strictly positive definite matrix $D\operatorname{tridiag}(-1,2,-1)D$ of order 15 with $D=\operatorname{diag}(1,2,1,2,\ldots)$,
which was pathological for the former cone split. IDs 811 and 813 are the copositive but non-strict scaled reduced-B matrices derived
from QAPLIB `nug24:A` and `nug25:A`; direct Danninger recursion timed out on both. Their `matrix_sources` rows retain the complete
construction or citation and the exact classification argument.

The DIMACS matrices use the exact Motzkin-Straus construction $Q_\lambda=\lambda(E-A)-E$. For each of the 27 graphs with a stated
exact clique number $\omega$, the database stores the strictly copositive case $\lambda=\omega+1$, copositive boundary case
$\lambda=\omega$, and non-copositive case $\lambda=\omega-1$. Keller6 and MANN_a81 have only certified clique lower bounds, so each
contributes only the rigorously implied non-copositive case; no boundary or strict label is inferred. `published_benchmarks` stores
all 27 rows of Table 2 and all 29 rows of Table 3 from Julius Žilinskas, *Copositive Programming by Simplicial Partition*,
[DOI 10.15388/Informatica.2011.345](https://doi.org/10.15388/Informatica.2011.345), including family, graph size, clique status,
reported result, search counts, and elapsed or allowed time. These published values remain separate from similarly named generated
files unless exact source identity is established.

Exact duplicates and matrices related by one simultaneous row-and-column permutation are collapsed to their lowest-ID
representative. Dense upper-triangle text remains the canonical representation. `matrix_sha256` replaces a full-text uniqueness
index so the 65 MiB large-matrix payload is not duplicated inside SQLite; imports verify a hash match against the complete matrix
text. The corpus uses only exact or exact-fallback source baselines. The 18 unverified fast-only source baselines and three
affine-symmetry image rows that production never solves separately remain excluded.

The local `tests` table stores one row per matrix and measured algorithm result. It follows the provenance convention of the main
timing table: session, timestamp, machine, CPU, build label, source reference, Git revision, and exact binary SHA-256. The
copositivity-specific fields add an algorithm name/description, `ok`/`timeout`/`error` status, independently nullable
strict-copositivity and copositivity results, correctness, node count, and diagnostics. `target_ns`, `iterations`, `measured_wall_ns`, `elapsed_ns`,
and `timeout_ns` retain the timing context in nanoseconds; `elapsed_ns` is the algorithm-reported per-run median when
`iterations > 1`. Use `target_ns = 0` and `iterations = 1` for a one-shot run. A null
A null copositivity result means that the tested algorithm did not determine it; a null `is_correct` means that correctness could
not be assessed from the available expected classification. `published_benchmarks` remains separate because it records literature
results rather than measurements of the current implementation.

The current snapshot contains 1,375 distinct strategically normalized matrices. The 1,064 exact or exact-fallback baselines and 18
additional unverified fast-only results have 68,707 stored candidate representatives whose multipliers represent 112,381
candidates and 96,730 ESS. The other 293 rows are catalog-only and have null candidate fields.

It contains each distinct matrix from Tables 1 and 2 of the
Bomze-Schachinger-Ullrich ESS-growth paper exactly once. IDs 18 and 26 hold the
exact published Table 1 matrices that replaced same-property alternatives;
IDs 80-90 hold the previously missing Table 2 base and constructed matrices.
Redundant alternatives formerly at IDs 12 and 21 were removed. IDs 56-66 are
staged complete-multipartite many-ESS benchmark matrices. IDs 67-79 are
deterministic random-integer coverage matrices; together with the existing rows,
every dimension from 2 through 25 has at least one circular and one non-circular
matrix. IDs 45-47 preserve the unsafe-filter, LU-boundary, and failed-proof
verified-search regressions. IDs 2688-2695 are the eight previously missing payoff games obtained from the 2014 and 2015
Bomze-Schachinger-Ullrich copositive constructions. Each stores the primitive integer form of `(dJ-S)/g`, so the papers'
isolated copositive zeroes become isolated global maximizers, hence strict local maximizers and ESS.
Exact safe analysis finds the published lower-bound counts, plus two additional ESS for the order-8 game and three for the
order-9 game. The order-15
source matrix `S15` was already present as ID 24. IDs 91-117 originally held SuiteSparse Matrix Collection
imports, IDs 118-269 are QAPLIB imports, and IDs 270-280 are TSPLIB95 imports.
IDs 281-313 are Biq Mac Library imports, and IDs 314-319 are Magma Hadamard
database imports. Thirty QPLIB objective imports retain their original IDs
between 320 and 1925; the gaps are the deliberately removed constraint-only
rows. IDs 1926-1982 are OR-Library imports, IDs 1983-1990 are KONECT imports,
IDs 1991-2137 are the House of Graphs stratified sample, IDs 2138-2176 are the
Network Data Repository stratified sample, and IDs 2177-2206 originally held SDPLIB `F0`
objective-matrix imports. Diagonal sampling subsequently removed SuiteSparse ID 105 and SDPLIB IDs 2177-2179 and 2204-2206.
IDs 2207-2209 preserve one exact false rejection for
each former fast per-support candidate condition: the pivot cutoff, probability
sign, and outside-payoff margin. Current fast uses its small-pivot and
precision-span fallbacks to recover them. No complete SQLite matrix suite is
currently wired into CTest.

IDs 2996-2998 preserve three exact full-support ESS that current fast search falsely rejects through its probability check. They
include failures with accepted pivots above $10^{-12}$, $10^{-10}$, and $10^{-2}$. Each row stores the exact safe candidate and a
human-readable calibration log stating the observed fast failure. ID 2998 has fast calibration `-1` because its failed
full-support decision would otherwise continue into an impractical exhaustive dimension-33 search.

IDs 2210-2687 contain 450 retained exact representatives from a combined audit of Anymatrix, TypedMatrices.jl, and Matrix
Depot. The raw selection contributed 478 rows. Circular normalization removed exact duplicate ID 2215; the positive-scale audit
then removed IDs 2212-2214, 2220, and 2254 in favor of older strategically equivalent rows, dimension-one consolidation
removed IDs 2210-2211 in favor of ID 314, and diagonal sampling removed all 20 Strakos rows. Six additional
selected matrices were already present at IDs 48, 49, 314, 1995, 2001, and 2155 and were tagged rather than duplicated. The
selection covers 51 eligible generator families and 34 documented or structural property classifications across five dimension
bands. See `../aidocs/reference/MATRIX_GENERATOR_CATALOGUE_AUDIT.md` for the complete scope, exclusions, revisions, sampling rule,
and validation record.

The timing table contains 2,357 CPU-2 persistent-Pybind median rows: the previous 892-row collection of 81-row `classic`, current quick-suite fast and safe,
`equilibrated-fast`, FP-S01, FP-S02, FP-S02+FP-S03, and Werner very-unsafe panels; 20 current fast-timeout retry rows; 77-row safe
panels immediately before and after the C++ FLINT-wrapper extraction; 66 Werner exact rows; and four current circular-normalization
rows split equally between fast and safe, plus 820 Werner very-unsafe and 645 Werner exact historical rows added on August 4.
Catalog-only rows are excluded automatically. The paired builds use one-second native medians for 77 matrices
of dimension at least 3; IDs 47, 65-66, and 90 are excluded by the 30-second
rule. All paired ESS counts match. Build label, revision, and binary hash
identify every stored build.

Matrix rows contain nullable `fast_calibration_ns` and `safe_calibration_ns` values used only to choose benchmark iteration
counts. Fast calibration covers all 1,375 matrices with 1,081 positive measurements and 294 `-1` timeouts; safe has 1,064 positive
measurements and 311 timeouts. No calibration field remains null, and every row has a completed audit timestamp. The current
whole-matrix classifications are 1,168 without fallback, 135 `precision_span`, four `equilibration_invalid`, and 68
`equilibration_non_convergence`. A value of `-1` records a calibration run killed at its cutoff
and selects one benchmark iteration. Positive integer nanoseconds preserve the native value exactly; divide by `1000.0` when
displaying decimal microseconds.

The database-maintenance helpers described below live in the local-only, ignored `scripts/` directory. They are preserved in the
maintainer worktree but are not included in public GitHub clones or releases.

Run `scripts/calibrate_matrices.py fast` or `scripts/calibrate_matrices.py safe` from the repository root to fill only missing
calibrations with the canonical Release Pybind module on CPU 2 and a one-second cutoff. When candidate fields are missing, either
method also stores weighted counts, support-size structures, and representative candidate rows for a matrix that finishes.
These per-method commands never overwrite existing calibration or baseline values; clear a calibration field explicitly before
intentionally refreshing it. Use `scripts/calibrate_matrices.py fast|safe --retry-timeouts --cutoff-seconds 120` to retry only the selected
method's `-1` rows once. Repeat `--cpu ID` to process matrices concurrently on explicitly selected cores; one matrix remains
pinned to each core and SQLite writes stay serialized. A completed fast result is exact when `safe_fallback` is non-null;
otherwise it remains an unverified fast result when safe calibration is still `-1`.
The calibration script classifies the whole-matrix fast fallback before starting the timed process, so even a killed calibration
stores the correct `safe_fallback` value.

Run `scripts/calibrate_matrices.py audit` for the ordered full consistency-calibration pass. It dispatches
`dimension ASC, matrix_id ASC`, runs fast before safe, and sizes each method to approximately one second from its previous
calibration. Each method independently uses the greater of 120 seconds and 120% of its positive stored calibration; a missing or
`-1` calibration uses 120 seconds. A fast timeout skips safe and sets both calibrations to `-1`. A non-null fast
`safe_fallback` copies the exact fast measurement to safe instead of running safe again. The pass compares complete candidate
output, fills missing results, preserves conflicting stored results, and commits after every matrix. Repeated `--cpu ID` options
run matrices concurrently; each matrix's fast and safe work remains on its assigned CPU. By default the command resumes rows whose
`calibration_timestamp` is null; `--refresh-all` starts a new pass over every row.

The August 2 two-minute fast retry attempted all 319 previous timeouts. Two rows completed before CPU 2 was reserved, and the
remaining work used performance CPUs 3 through 9. Twenty-one matrices completed, adding 683 representative rows for 841 weighted
candidates and 236 ESS; two results used exact fallback and 19 remain unverified fast results. The other 298 matrices timed out:
41 were classified for exact fallback and 257 remained on the fast path.

## Circular Storage Normalization

`scripts/normalize_circular_matrices.py` audits every matrix with exact `Fraction` arithmetic. For an eligible circulant matrix
with common diagonal value `d`, it stores the strategically equivalent zero-diagonal matrix `A - dJ`, where `J` is the all-ones
matrix, in compact circular form. Subtracting `d` from every entry preserves all best-response comparisons, candidates, and ESS;
only every payoff changes by `-d`. Subtracting `d` only from the diagonal would not be equivalent.

The retained converted rows and exact recorded constants are ID 1: `0` and ID 2203: `1`. Each matrix description
records its own constant and states that the preceding provenance describes the unnormalized source matrix. Normalized IDs 39 and
41 duplicated older ID 1, so the newer rows were removed. The generator-catalogue import normalized 53 additional rows by the same
rule; normalized ID 2215 exactly duplicated later-removed ID 44. Positive-scale deduplication then removed IDs 38 and 44 in favor
of ID 1. Zero-game consolidation removed ID 43 in favor of the retained dimension-50 ID 2203. ID 314 is the sole dimension-one
exception: it is mathematically circulant, but compact storage would contain zero values while the parser requires one value, so it
remains full and is documented as non-circular storage.

Normalization invalidated and recomputed the affected baselines and calibrations. Eighteen `classic`/Werner timing rows for the
old stored matrices were removed; after both cleanups the replacement current-build panel contains fast and safe measurements for
IDs 1 and 2203. Its dimension-two rows are retained only as normalization checks and remain excluded from aggregate benchmark
comparisons.

## SuiteSparse Imports

The SuiteSparse Matrix Collection import uses the following reproducible rule:

- The matrix is square, real, exactly symmetric after Matrix Market expansion,
  and has dimension at most 63.
- Matrix Market `pattern` entries become 1. Integer entries remain integers.
  Every finite decimal or scientific-notation `real` token is converted to its
  exact reduced fraction; this preserves the downloaded file exactly but does
  not claim that an underlying physical model was symbolically rational.
- The dense exact matrix is stored in FracESSA upper-triangle format. An exact circulant matrix of dimension at least two is first
  normalized by subtracting its common diagonal value from every entry, then stored in compact zero-diagonal circular format; the
  source value and transformation remain in its description.
- Exact dense rational duplicates are not imported. SuiteSparse ID 2758,
  `Mycielski/mycielskian2`, was skipped because it equals existing matrix ID 1.
- Dimensions 26-63 use `size_class = "super_large"`.

The official 2,904-entry index contains 56 real square matrices through order
63. Exact parsing accepts 28 symmetric files and rejects the other 28; ID 2758
duplicates existing matrix ID 1, initially leaving 27 imports at IDs 91-117. Diagonal sampling later removed ID 105
(`HB/bcsstm01`), leaving 26 SuiteSparse rows. Source page, collection ID, original Matrix
Market field type, group, title, date, and author are retained in metadata.

## NIST Matrix Market Audit

The finite NIST Matrix Market search over every symmetric category and
`0 < n < 64` returns ten names. Nine Matrix Market files remain downloadable;
all are exactly symmetric. Eight are exact SuiteSparse duplicates. `BFW62B`
has the same source and sparsity pattern as SuiteSparse `Bai/bfwb62`, with only
eight last-digit differences of at most `1e-21`, so it is retained as rounded
alternate provenance. NIST withdrew the incorrect `LAP 25` Matrix Market
assembly. The earlier `FIDAP005` audit likewise identifies a lower-precision
version of SuiteSparse `FIDAP/ex5` rather than a distinct game.

NIST therefore adds no matrix row. Corresponding SuiteSparse rows carry
`"nist_matrix_market"` and retain the NIST page in `description`. Parameterized
generators remain outside this finite downloadable-file audit.

## QAPLIB Imports

The official QAPLIB `qapdata` archive contains 136 fixed quadratic-assignment
instances. Each file stores one dimension followed by two dense integer
matrices, A and B. The import tests A and B independently, retains only exact
symmetric matrices with dimensions 1-63, and globally deduplicates their dense
values before choosing compact circular or upper-triangle FracESSA storage.

Of 109 in-range instance files, 182 matrix occurrences are symmetric. Thirty are repeated matrices within QAPLIB, leaving 152
exact distinct source matrices at IDs 118-269. The later zero-game consolidation removed circular zero matrix ID 158, leaving 151
QAPLIB rows. The source is
the QAPLIB dataset by Burkard, Cela, Karisch, Rendl, Anjos, and Hahn,
[DOI 10.7488/ds/3428](https://doi.org/10.7488/ds/3428), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The database stores
the primary `instance:role` identifier and lists any identical alternate source
occurrences in `description`.

## TSPLIB95 Imports

The official [TSPLIB95 symmetric-TSP archive](https://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp/ALL_tsp.tar.gz)
contains 111 problem files. Seventeen declare `EDGE_WEIGHT_TYPE: EXPLICIT`; six
have dimensions above FracESSA's limit, leaving 11 exact symmetric integer
matrices with dimensions 17-58. Coordinate-derived distances, tours, and the
separate asymmetric problem categories are not imported.

The retained `FULL_MATRIX`, `LOWER_DIAG_ROW`, and `UPPER_ROW` representations
were expanded exactly and checked edge-for-edge against TSPLIB95's independent
official XML files. All 11 are non-circular and distinct from each other and
from every existing dense exact database matrix. They are catalog-only rows at
IDs 270-280; dimensions 26-63 use `size_class = "super_large"`.

## Biq Mac Library Imports

The official [Biq Mac Library archive](https://biqmac.aau.at/library/tar_files/biqmac_all.tar.gz)
contains 468 files representing 343 logical binary-quadratic and Max-Cut
instances. Its 125 dense/sparse pairs match exactly. Applying the dimension-63
limit leaves 33 matrices: 10 Beasley binary-quadratic instances, 13
Glover-Kochenberger-Alidaee binary-quadratic instances, and 10 Rudy Max-Cut
graphs. The other 310 logical instances are too large.

All retained entries are exact integers. Sparse binary-quadratic entries are
expanded under the library's symmetric-Q contract; each Max-Cut graph becomes
its symmetric zero-diagonal weighted adjacency matrix. The 46 individual files
behind the retained rows match their aggregate-archive copies byte-for-byte.
All 33 matrices are non-circular and globally distinct. They are catalog-only
rows at IDs 281-313; dimensions 26-63 use `size_class = "super_large"`. The audited
archive SHA-256 is `887ed2a8187fff2cf941d3c6aad3953ffd9904700dd86710fc2bd09736670e5a`.

## Magma Hadamard Imports

The official Magma downloads provide separate archives for
[non-skew Hadamard matrices](https://magma.maths.usyd.edu.au/magma/download/db/hadamard.tar.gz)
and [skew-Hadamard matrices](https://magma.maths.usyd.edu.au/magma/download/db/hadamard_skew.tar.gz).
Both downloads match Magma's published MD5 and SHA-1 values. Their compact
binary representation was decoded independently and checked against the exact
degree-16 representative printed in the Magma handbook.

The non-skew database contains 5,391 inequivalent representatives, of which
4,474 have degree at most 63. Every in-range matrix satisfies `H H^T = nI`, but
only six are themselves symmetric: representative 1 at each of degrees 1, 2,
4, 8, 16, and 32. All 638 matrices in the separate skew database have degrees
36, 44, or 52; all satisfy the skew-Hadamard property and none is symmetric.
The six retained exact `+/-1` source matrices are globally distinct. Dimension-one ID 314 is mathematically circulant but remains
in full storage because the compact representation would have no values; IDs 315-319 are non-circular. They are catalog-only rows
at IDs 314-319; degree 32 uses `size_class = "super_large"`.

The audited SHA-256 values are
`69930089fe46dd59cb0c48c73e1cfd03928b2e25958b1ce22de7a9f647e0cad7`
for the non-skew archive and
`1aa7f7fef8e541c7078ed89431a42a1814a786d74fb0f0d777f06babded5f210`
for the skew archive.

## QPLIB Imports

The official [QPLIB](https://qplib.zib.de/) catalog contains 453 quadratic-
optimization instances and is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The import uses the
35 canonical `.qplib` files with 1-63 variables rather than downloading the
775 MB multi-format bundle. QPLIB's defining publication is available at
[DOI 10.1007/s12532-018-0147-4](https://doi.org/10.1007/s12532-018-0147-4).

Only an explicitly stored quadratic objective is imported. QPLIB stores its
lower triangle, which is mirrored exactly to recover the symmetric Hessian.
Quadratic-constraint matrices, linear objective vectors, and generally
rectangular linear-constraint matrices are excluded. Finite decimal and
scientific-notation coefficients are converted to their exact reduced
fractions, preserving the source tokens without claiming a more precise
underlying physical value.

Thirty of the 35 files have an explicit quadratic objective. Their 30 objective
matrices are pairwise distinct, duplicate no earlier database row, and are not
circular. Every coefficient and role was independently cross-checked with
PyQPLIB 0.1.7. The rows retain their original noncontiguous IDs between 320 and
1925, are catalog-only, and use `size_class = "super_large"` at dimensions 26-63.

## OR-Library Imports

The audit covers every locally maintained problem family in J.E. Beasley's
official [OR-Library index](https://people.brunel.ac.uk/~mastjjb/jeb/info.html),
plus its still-hosted urban-transit page. OR-Library material is published
under the [MIT license](https://people.brunel.ac.uk/~mastjjb/jeb/orlib/legal.html).
Only explicitly stored square matrices with exact rational entries, exact
symmetry, and dimensions 1-63 are eligible. Rectangular problem tables,
coordinate-derived distances, shortest-path matrices derived from edge lists,
and external collections merely linked by OR-Library are outside this import.

The retained data comprise 23 binary-quadratic Q matrices, 23 capacitated
minimum-spanning-tree cost matrices, six aircraft-separation matrices, two CAB
hub-location matrices, one portfolio correlation matrix, and two urban-transit
demand matrices. The binary-quadratic files state a maximization problem and
have the opposite sign from the corresponding Biq Mac minimization copies, so
they are distinct games rather than duplicates.

The in-range source audit found 68 symmetric matrix occurrences. Ten matrices
are repeated between `capmst1` and `capmstnew.zip`, and `portreb1` repeats the
`port1` correlation matrix, leaving 57 exact distinct imports at IDs 1926-1982.
Two additional in-range `capmstnew.zip` tables, two aircraft-separation tables,
and all six in-range corporate withholding-tax tables are asymmetric. The
urban-transit demand files `td1` and `td2` were recovered from 2011 Internet
Archive snapshots of the now-missing official files; `td3` exceeds dimension
63, while the time files contain nonnumeric `-` entries for absent links and
are not rational matrices. All 57 imports are non-circular, globally new, and
catalog-only; dimensions 26-63 use `size_class = "super_large"`.

## COMPl_e_ib Audit

The official [COMPl_e_ib 1.1](https://www.compleib.de/) archive defines 168
control-system benchmarks. Its benchmark state matrix is `A`; the other output
arrays describe input, output, noise, and weighting channels and are not
independent benchmark matrices. Of the 168 state matrices, 57 exceed
FracESSA's dimension-63 limit. All 111 in-range state matrices were constructed
from the archive's `COMPleib.m` and `.mat` files and compared exactly with their
transposes. None is symmetric, so COMPl_e_ib contributes no catalog row.

Square identity, zero, and weighting arrays synthesized while assembling a
control problem are deliberately not imported as separate games.

## SLICOT Model-Reduction Audit

The official [SLICOT model-reduction collection](https://www.slicot.org/20-site/126-benchmark-examples-for-model-reduction)
contains 18 linear-system benchmarks. Seventeen have orders from 84 to 10,913
and exceed FracESSA's dimension limit. The only in-range case is the order-48
building model `build.mat`; its stored state matrix `A` is not symmetric. The
complete MAT file is byte-identical to COMPl_e_ib's already-audited `lah.mat`,
so the collection contributes no catalog row.

## KONECT Imports

The official [KONECT network collection](https://konect.cc/networks/) currently
lists 1,326 datasets. The in-range audit downloads all 23 available unipartite
networks with at most 63 vertices and reconstructs their adjacency matrices
exactly from KONECT's whitespace-separated edge files. Unweighted edges become
1; signed and positive integer weights are retained exactly; repeated directed
edges, where present, are summed at their matrix position. Seven in-range
bipartite networks are excluded because their native adjacency tables are
rectangular rather than FracESSA game matrices.

All nine undirected files are symmetric. One of the 14 directed files,
`moreno_taro`, is also exactly symmetric because every directed edge has its
reciprocal; the other 13 directed matrices fail exact symmetry. The Dolphins
and Zachary karate-club matrices exactly duplicate existing SuiteSparse IDs 114
and 115, which now retain KONECT as alternate provenance. The other eight
matrices are globally new, non-circular, catalog-only rows at IDs 1983-1990.
Dimensions 26-63 use `size_class = "super_large"`. KONECT publishes the files and
their source citations but does not state one collection-wide dataset license,
so this catalog makes no broader licensing claim.

## House of Graphs Sample

The official [House of Graphs](https://houseofgraphs.org/) contained 29,139 graphs on August 2, 2026. Its complete
order-1-through-63 query returned 23,988 graphs. Every canonical graph6 string was decoded as an exact, unweighted,
undirected, simple graph and checked against the API adjacency list before its zero-diagonal `0/1` adjacency matrix was
considered for import.

The sample crosses five dimension bands (`1-8`, `9-16`, `17-25`, `26-44`, and `45-63`) with ten categories: acyclic,
connected bipartite cyclic, connected planar non-bipartite, connected nonplanar, disconnected cyclic, regular, dense
(density at least 0.5), vertex-transitive, asymmetric, and an unrestricted control. For each populated stratum, the three
lowest SHA-256 ranks of `20260802|dimension_band|category|graph_id` were selected. This fixed rule makes the nominally
random sample reproducible without storing or depending on API result order.

All 50 strata were populated. The dimension-45-through-63 disconnected-cyclic stratum contained only one graph, and House of
Graphs ID 21044 was selected independently by two strata. Removing that overlap produced 147 unique matrices: 29, 30, 30, 30, and
28 from the five dimension bands. The later zero-game consolidation removed empty graphs at database IDs 1992, 2080, and 2127.
The retained 144 rows have band counts 27, 30, 29, 30, and 28; none exactly duplicates an existing dense matrix, and five admit
compact circular storage. Rows occupy IDs 1991-2137, retain their House of Graphs ID, canonical graph6 string, source name when
available, selected strata, source page, and seed, and use `size_class = "super_large"` at dimensions 26-63.

House of Graphs documents canonicalization, duplicate rejection, and downloadable formats for further personal use, but
the audit found no collection-wide data-license statement. The catalog therefore records provenance without making a
broader licensing claim.

## Network Data Repository Sample

The current [Network Data Repository](https://networkrepository.com/network-data.php) category indexes expose 6,628 graph
rows. Of these, 1,241 report dimensions 1-63 across 15 categories. The deterministic sample crosses each category with five
dimension bands (`1-8`, `9-16`, `17-25`, `26-44`, and `45-63`) and ranks candidates by SHA-256 with seed `20260802`. Ranked
candidates are examined until at most three globally new, directly represented symmetric matrices are found per populated
category/band.

The source archive, not merely the index metadata, determines eligibility. A Matrix Market file must be square and either
declare symmetric storage or satisfy exact symmetry after full expansion. An edge list must be explicitly undirected in its
metadata or comments, or contain exact reciprocal edges. Arbitrary external vertex IDs are replaced by their sorted dense
order. Integer, decimal, and fractional weights remain exact. Rectangular matrices, unsymmetric matrices, temporal edge
streams, bipartite tables without a square adjacency matrix, malformed files, and matrices requiring forced symmetrization
are excluded. Exact duplicates already in the 558-row pre-import database are skipped without changing the existing row.

The resulting 39 catalog-only imports occupy IDs 2138-2176: 15 animal-social, 15 cheminformatics, six protein, two DIMACS,
and one biological matrix. Their dimension-band counts are 6, 6, 7, 10, and 10 respectively. Twenty use
`size_class = "super_large"`, 11 have non-unit weights, and one admits compact circular storage. All 39 source archives round-trip to
their stored exact matrices, and none duplicates another database row. The repository's
[data policy](https://networkrepository.com/policy.php) states a Creative Commons Attribution-ShareAlike license without
naming a version; each row retains its dataset page for source-specific attribution.

## SDPLIB Imports

The [SDPLIB 1.2 mirror](https://github.com/vsdp/SDPLIB) contains 92 semidefinite-programming test problems in SDPA sparse
block-diagonal format. Thirty problems have an aggregate matrix dimension from 1 through 63. For each eligible problem,
the import retains only matrix number zero, `F0`, as the objective coefficient matrix. The 1,799 `F1...Fm` constraint
matrices are deliberately excluded, and the source blocks are expanded into one complete matrix rather than cataloged as
independent submatrices.

IDs 2177-2206 initially held the resulting 30 objective matrices: four control, 15 H-infinity, two infeasible-dual, two
infeasible-primal, three quadratic-assignment, one theta, and three truss problems. Diagonal sampling removed control IDs
2177-2179 and truss IDs 2204-2206. The retained 24 rows comprise one control, 15 H-infinity, two infeasible-dual, two
infeasible-primal, three quadratic-assignment, and one theta matrix. Exact parsing converts finite decimal
and scientific-notation tokens to reduced fractions. The source matrices are pairwise distinct and duplicate no earlier source
matrix. `theta1` is circulant with source diagonal `1`; ID 2203 stores the strategically
equivalent compact matrix obtained by subtracting `1` from every entry, and its description records the transformation. Each row
links to its exact source file. The
current GitHub mirror declares GPL-3.0; this catalog makes no broader licensing claim.

## Matrix Generator Catalogue Imports

[Anymatrix](https://github.com/north-numerical-computing/anymatrix),
[TypedMatrices.jl](https://github.com/TypedMatrices/TypedMatrices.jl), and
[Matrix Depot](https://github.com/JuliaMatrices/MatrixDepot.jl) overlap heavily, so they are audited and sampled as one generator
catalogue. The eligible pool contains 2,678 distinct exact symmetric matrices of dimensions 1 through 63 from 51 mathematical
families. Deterministic SHA-256 ranking with seed `20260802` retains up to three matrices from each populated
property-by-dimension-band stratum, then ensures that every eligible family has at least one representative.

The resulting 484 selected matrices populate 166 property-band strata. Six matched existing IDs 48, 49, 314, 1995, 2001, and
2155 exactly. The raw IDs 2210-2687 held the other 478. Normalization changed 53 full circulant rows to compact `A - dJ` storage
and exposed exact duplicate ID 2215; the later positive-scale audit removed another five source rows in favor of older rows, and
dimension-one consolidation removed IDs 2210-2211. Diagonal sampling then removed all 20 Strakos rows. The retained 450 new rows
therefore comprise 78 compact circular matrices and no dimension-one or Strakos row. Retained-row counts by selection band are
69, 90, 86, 104, and 101; compact counts are 14, 18, 15, 17, and 14. In the original
selected source set, only two populated strata contain fewer than three distinct eligible matrices: `fixed_size` at dimensions
45-63 has two, and `unimodular` at dimensions 1-8 has one. The other `unimodular` bands are empty rather than under-sampled.

All formulas and source matrices are evaluated as exact integers or rational numbers. Irrational, transcendental, random-real,
rectangular, nonsymmetric, and dimension-above-63 families are excluded. The complete acceptance and rejection audit, source
revisions, family list, exact normalization checks, and retained counts are recorded in
`../aidocs/reference/MATRIX_GENERATOR_CATALOGUE_AUDIT.md`.

## Tables

### `matrices`

Each row stores one exact matrix input and its summary:

- `matrix_id`: stable FracESSA verification ID.
- `dimension`: number of strategies, from 1 through 63.
- `size_class`: `small` for dimensions 1-8, `medium` for 9-16, `large` for 17-25, and `super_large` for 26-63.
- `is_cs`: 1 for compact circular-symmetric input, otherwise 0.
- `matrix`: the exact comma-separated input values, without the `n#` prefix.
- `candidate_count` and `ess_count`: complete weighted baseline counts, or null
  together when the matrix is cataloged but not analyzed.
- `candidate_structure`: JSON object mapping support size to weighted candidate
  count.
- `ess_structure`: JSON object mapping support size to weighted ESS count. Both
  structure fields are null exactly when both count fields are null.
- `gamma_lower_bound`: generated real value `ess_count ** (1 / dimension)`, or null when `ess_count` is null. The
  Bomze-Schachinger-Ullrich paper calls this the lower bound for $\gamma$ implied by the matrix; its result table labels the column
  `$\gamma \geq$`.
- `origin`: where the matrix came from and why it was retained.
- `tags`: JSON array of short qualitative categories such as `"numerical_edge"`, `"support_frontier"`, or
  `"generator_catalogue"`. Matrix size belongs only in `size_class`.
- `name`: short human-readable matrix name.
- `family` and `subfamily`: broad and narrow matrix classifications used for
  selecting related fixtures.
- `description`: fuller provenance and purpose text.
- `source_url`: original website or DOI when an external source is known.
- `original_format`: source representation or construction method.
- `original_id`: identifier used by the source, when one exists.
- `created_at`: first known project use or current-row creation as
  `YYYY-MM-DD`.
- `fast_calibration_ns` and `safe_calibration_ns`: nullable native-duration estimates used to choose benchmark sample counts.
  Positive values are nanoseconds, while `-1` marks a calibration timeout. They are matrix metadata, not benchmark
  observations.
- `calibration_timestamp`: ISO-8601 UTC timestamp of the latest full consistency-calibration attempt.
- `calibration_comment`: human-readable, indented, append-only JSON array of full calibration attempts. Every entry records its
  timestamp, target, actual CPU, fast and safe cutoffs, requested and completed iterations, measured calibrations, fallback
  handling, and any mismatch or filled candidate data.
- `safe_fallback`: null when fast search prepares and uses its double matrix, otherwise `precision_span`,
  `equilibration_invalid`, or `equilibration_non_convergence`. This records only a whole-matrix switch to safe search; an exact
  retry for one support does not set it.
  `matrix_overview` places it immediately after `gamma_lower_bound`.

The stable `matrix_id` remains the program-facing identifier. `origin` preserves
the existing human provenance text, while `source_url` provides a
machine-readable external origin without overloading that text. A null
`source_url` or `original_id` means that the matrix was constructed inside the
project or that no external source was recorded; it does not mean the source was
searched exhaustively.

Legacy rows are marked by `family = "historical"`. When their exact insertion
day is unknown, `created_at` uses January 1 of the known year as an explicit
placeholder; for example, `2014-01-01` means only "legacy matrix known from
2014." Its purpose is to distinguish long-standing matrices from cases added to
the current suite in 2026, not to assert an exact historical day.

The database enforces uniqueness on `(dimension, matrix)`. `matrix` alone cannot be unique because compact input omits its
dimension, so identical token strings can legitimately describe different-sized matrices. Import audits also compare exact stored
value vectors within the same dimension and `is_cs` class. If two differ only by a positive nonzero rational multiplier, the lower
matrix ID is retained. Negative multipliers are not duplicates because they reverse payoff comparisons.

The corpus retains exactly three diagonal matrices: dimension-one ID 314, compact all-zero dimension-50 ID 2203, and nonzero
dimension-60 ID 2180. The other 27 diagonal rows were removed to avoid overrepresenting this structurally trivial family. This is
an explicit benchmark-corpus sampling choice; distinct diagonal games are not generally mathematically equivalent.

For example, `{"1":8,"4":2}` means eight support-one results and two
support-four results. Empty ESS structure is stored as `{}`.

### `candidates`

Each row mirrors one candidate output row. A circular row stores only the
smallest support in its rotation/reflection orbit, and its non-null `multiplier`
is the number of distinct supports represented. A non-circular row has a null
multiplier. Exact fractions and vectors remain text; `payoff_double` is also
text so database reads cannot alter formatting. `(matrix_id, candidate_id)` is
the primary key, and a support may occur only once for a matrix.

Fixed facts already represented by columns, including size, circular symmetry,
counts, and support-size structures, are not duplicated in `tags`.

### `timings`

Each row is one sequential analyzer timing measurement for one matrix. A
session may contain several builds, but each build is measured by a separate
runner invocation. Rows record the machine and pinned CPU, human build label,
moving source reference such as `main`, immutable revision, binary SHA-256,
Pybind or CLI backend, search method in the historically named `mode` column, whole-matrix `safe_fallback`, target and measured wall durations,
iteration count, median native duration in nanoseconds, observed ESS count,
and an optional comment.

The observed count remains separate from the expected count in `matrices`, so a
report can expose fast-method mismatches without hiding or rejecting their
timings. The report prints the same Bomze-Schachinger-Ullrich exponential-growth
lower bound exposed as `matrices.gamma_lower_bound`. Fast rows with non-null `safe_fallback` remain visible but are excluded from
speed-ratio summaries because they measured safe search rather than the double path.
Historical timing rows may retain the legacy generic value `equilibration`; current binaries distinguish invalid arithmetic from
non-convergence.

## Scope

`python -m pyfracessa.timing` reads matrices from this database and writes timing
observations back to `timings`. It is deliberately a sequential, Linux
CPU-affinity runner, not part of the multiprocessing wrapper. One Pybind process
stays open for all selected methods and matrices in a build. The matching matrix calibration chooses
`ceil(target / calibration)` samples; a calibration at or above the target chooses one run. The default target is 0.5 seconds.
The stored result is the median returned `elapsed_ns`. Wall time is recorded as metadata but does not choose the sample count or
result. A missing calibration is a hard error. The tool also supports legacy CLI timers whose output unit is supplied on the
command line, but those fresh-process rows must not be mixed with persistent-Pybind microbenchmarks. No active
matrix-verification runner is wired into CTest yet.

The schema is defined in `schema.sql`. The C++ runtime does not read this
database; the timing tool uses Python's standard `sqlite3` module.

## Integrity

Basic database checks are:

```bash
sqlite3 testdata/fracessa_testdata.sqlite3 'PRAGMA integrity_check;'
sqlite3 testdata/fracessa_testdata.sqlite3 'PRAGMA foreign_key_check;'
```
