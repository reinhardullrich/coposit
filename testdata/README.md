# Copositivity Test Corpus

`Copos_testdata.sqlite3` is the slim maintained copositivity corpus.

- Matrices: 2,442
- Dimensions: 1 through 3,361
- Strictly copositive: 674
- Copositive but not strictly copositive: 1,179
- Not copositive: 589
- Ordinary copositivity not established: 0
- Schema: two tables and no manually created indexes

`matrices` stores the stable ID, dimension, exact matrix data, expected strict result, nullable ordinary-copositivity result,
optional source, optional family, and four independent Boolean benchmark flags: `smoke_set`, `representative_core`, `stress_test`,
and `scale_set`. Strict copositivity requires ordinary copositivity. `is_copositive = NULL` means that ordinary copositivity has not
been established; it must not be read as false. The current corpus has no such rows. New rows default to no benchmark set.
Small matrices keep their comma-separated upper triangle inline. Large rows contain `file:matrices/<matrix_id>.mtx`, resolved relative
to the database directory. Those files use whichever standard Matrix Market `array integer symmetric` or
`coordinate integer symmetric` representation is smaller in bytes. Symmetric array order and sorted symmetric coordinates both encode
the lower triangle; coordinate files omit zeros. `externalize_large_matrices.py` performs the conversion, chooses the smaller exact
representation for every external file, and compacts the database when it moves new rows.
`assign_benchmark_sets_2026_08_10.sql` preserves the guarded pre-consensus assignment procedure; the stored flags remain unchanged
after the ordinary classifications were completed. `aidocs/BENCHMARK_SETS.md` explains the assignment and current composition.
Source strings flatten the former one-to-one provenance rows; every retained representative preserves the origins of projectively
equivalent matrices removed on 2026-08-09. All 15 extracted families are preserved. Twenty-one added exceptional/equality family labels
cover 849 distinct literature and derived matrices, including the previously unlabeled Horn and Hoffman-Pereira rows.

The corpus keeps one lowest-ID representative under

\[
B=cP^TAP,\qquad c>0,
\]

where `P` is one simultaneous row-and-column permutation and `c` multiplies the entire matrix. The exact integer audit removed 155
redundant matrices from 58 equivalence classes and merged every removed origin into its representative. Positive diagonal congruences
`DAD` with nonconstant diagonal `D` are deliberately distinct corpus inputs and are not removed. Results attached to deleted exact
inputs are deleted with them rather than being reassigned to a differently represented matrix.

`results` stores one reference result per matrix, lowercase model ID, free-text parameter description, and normally the exact
native-extension SHA-256. Hadeler 1983 is the deliberate exception: it is the fixed baseline and stores an empty hash, so new corpus
matrices extend the same result set instead of starting a new one after a rebuild. The parameter description is empty when a model has
no configurable choices; different descriptions identify distinct results. Status is `ok`, `timeout`, `node_limit`, or `error`; only
`ok` rows may contain a Boolean classification and native elapsed time. The applied timeout, UTC timestamp, and optional failure
message remain with every row. For every other model, a changed parameter description or binary hash creates a distinct result.

IDs 9711 through 9737 are rational half-angle instances from Cases 13.1 and 18 of the Afonin-Hildebrand-Dickinson order-6
classification. IDs 9738 through 9756 are positive diagonal congruences of the explicit 9-by-9 extremal extension in
Kostyukova-Tchemisova (2026), Example 5. Every one of these 46 exact boundary matrices exceeded 200 ms in its initial selected-model
screen; 23 exceeded one second, and 15 still exceeded a five-second confirmation cutoff. Exact nonnegative zeros were checked before
insertion, and timeout remains benchmark evidence rather than a Boolean result.

`preprocessing_results` is the separate, deliberately small store for preprocessing-only experiments. It references a matrix row and
records the run ID, requested mode, one of the three pipeline configurations, status, native time, delegate count, and the explicit
ternary preprocessing outcome `positive`, `negative`, or `unresolved`. It does not represent the pre-check as a complete solver and
does not duplicate matrix metadata.

IDs 9757 through 9955 are 199 non-isomorphic Hoffman-Pereira exceptional boundary classes generated from McKay's connected graph
catalogs: every new class through order 9 and the first 130 qualifying order-10 classes. IDs 9956 through 9959 are the four remaining
exact integer examples from Kostyukova-Tchemisova (2026), Examples 1 and 5. ID 9960 is the rational strict exceptional matrix `C`
from Strekelj-Zalar. The reproducible importer and retained graph6 source catalogs record the exact selection.

The batch originally stored at IDs 9961 through 10160 contributed 200 exact non-strict source occurrences independent of the
Hoffman-Pereira catalog selection: 24 rational members of Hildebrand's exceptional extreme `COP(5)` family, 54 members of Baston's
all-orders basic extreme family, 18
members of Baston's distinct cyclic family, 73 Johnson-Reams generalized Horn matrices for every odd order 7 through 151, and all
31 stability-3 or stability-4 cop-irreducible graph examples in Dickinson-de Zeeuw Table 2. Dickinson-de Zeeuw ID 10132 is a
simultaneous permutation of Kostyukova-Tchemisova ID 9957, so ID 10132 was removed and its complete provenance was merged into ID
9957; 199 distinct representatives remain from the batch's ID range. The Johnson-Reams family is unbounded;
151 is only the endpoint that fills the user-approved 200-row batch.

IDs 10161 through 10244 append 84 more Johnson-Reams generalized Horn matrices without changing that dense odd-order block. Their
odd orders begin 163, 175, 181, and 199, then follow an irregular roughly-ten-dimension spacing through 999.
`import_literature_extremes_2026_08_07.py` regenerates all 284 rows from both batches.

IDs 10245 through 10304 add 60 exact matrices from a third literature pass: three Dickinson order-6 Case-9 extreme forms, three
Hildebrand-Afonin order-6 forms outside Parrilo's first sum-of-squares level, seven Laurent-Vargas direct sums outside every level
of that hierarchy, and 47 Hildebrand circulant extreme forms of orders 7 through 25 whose minimal zeros have support `n-2`.
All 47 circulant forms exceeded the 250 ms Dutour screen; representatives at orders 7, 8, 9, 11, 15, and 25 exceeded five seconds.
`import_hard_literature_matrices_2026_08_07.py` verifies every theorem condition and zero exactly before reproducing the rows.

IDs 10305 through 10504 add 200 strict perfect copositive matrices from Dannenberg-Schürmann: the printed indefinite order-3 seed
`I` and every lift through order 102, followed by the printed exceptional order-5 certificate `E` and every lift through order 104.
The paper's Lemma 5.1 preserves perfect copositivity and the positive copositive minimum under each repeated-coordinate lift;
Corollaries 5.6 and 5.7 preserve the two seeds' respective SPN and exceptional components. The importer retains an exact minimal
vector with final coordinate at least two across every lift. Representative Dutour runs exceeded five seconds at order 20 for the
`I` family and already at order 10 for the exceptional family. The paper's rational scale `E/3` is stored as its primitive integer
numerator because positive scaling does not change strict copositivity.

IDs 10505 through 10594 add 90 deterministic high-order small-integer stress matrices: three cases at each of 30 deliberately
irregular dimensions from 51 through 2,997. Each dimension has one positive-semidefinite copositive boundary matrix, one
positive-definite strictly copositive matrix, and one matrix that is not copositive despite having a positive diagonal. Two seeded
signed Hamiltonian cycles make the sparse entries visibly mixed while keeping every entry's absolute value at most 10. The boundary
case has an exact two-coordinate nonnegative zero; the failing case has an exact two-coordinate negative witness. The schema records
both ordinary boundary and non-copositive cases as not strictly copositive, while their `family` and `source` fields preserve the
ordinary distinction. `import_high_order_small_integer_stress_2026_08_09.py` regenerates and verifies all 90 rows exactly.

IDs 10595 through 10684 add a second classification triplet at the same 30 dimensions. These matrices replace the visible sparse
cycle pattern with dense pseudo-random Gram data: every column receives a unique seeded eight-coordinate fingerprint with coordinates
in `{-2,-1,1,2}`, and at least 94.194% of stored entries are nonzero. The PSD boundary cases hide one exact nonnegative zero inside
a rank-`n-1` regularizer; the strict cases add the identity; and the non-copositive cases overwrite one seeded off-diagonal pair to
give an exact two-coordinate negative witness while retaining a positive diagonal. Every entry has absolute value at most 60, despite
the approved bound of 100. `import_high_order_dense_randomized_stress_2026_08_09.py` regenerates all 90 rows exactly using NumPy.

The byte-exact 2026-08-07 FracESSA source database is preserved as
`reference/fracessa/testdata/Copos_testdata.original.sqlite3.xz`. Its decompressed SHA-256 is
`a6691d68241f496a9876f9da59772e07fb92b5ae9df1cca954d645696a0c488d`. It retains the removed literature and historical-run tables
for provenance or experiment replay.

`schema.sql` is the complete maintained schema. `add_copositivity_classification_2026_08_10.sql` is the guarded, rerunnable migration
that reconstructs ordinary truth from the immutable source snapshot and the later proved families.

```bash
sqlite3 testdata/Copos_testdata.sqlite3 'PRAGMA integrity_check;'
```
