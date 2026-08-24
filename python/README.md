# pycoposit Analysis Interface

`pycoposit` is the analysis interface. It exposes explicit model and preprocessing selection through the same three-layer shape as
PyFracESSA and has no `fast`, `safe`, or implicit model aliases:

- `compute_matrix(algorithm, matrix, mode=None, preprocessing="both", diagnostics=False,
  collect_certificate_joint_distribution=False, model_parameter=None)` is the one-matrix `coposit` adapter;
- `run(algorithm, matrices, mode=None, preprocessing="both", diagnostics=False, model_parameter=None)` is the sequential path;
- `run_multiprocessing(algorithm, matrices, mp_config=None, mode=None, preprocessing="both", model_parameter=None)` is the bounded
  process path and yields iterables in completion order.

When `mode` is omitted, every Dickinson-, Hadeler-, and FracESSA-based model selects `both`; Danninger 1990 does the same. A model
without one-pass combined classification requires an explicit `"copositive"` or `"strictly_copositive"` mode.

Every call requires one of these algorithm identifiers:

```text
dutour_2018
danninger_1990
copomatrix_2011
adaptive_dutour_danninger
adaptive_dutour_copomatrix
adaptive_sponsel_copomatrix
adaptive_zischg_sponsel_copomatrix
hadeler_1983
dickinson_2019
dense_bitset_dickinson
interval_recursive_dickinson
bdd_dickinson
zdd_dickinson
cbdd_dickinson
cbdd_halfspace_dickinson
upper_endpoint_cbdd_dickinson
cbdd_dickinson_improved_1
wide_certificate_cbdd_dickinson
multithreaded_cbdd_dickinson
ceiling_pruned_dickinson
kernel_cone_dickinson
affine_companion_dickinson
layered_singular_lift_dickinson
breadth_first_singular_lift_dickinson
czdd_dickinson
sat_dickinson
sat_halfspace_dickinson
sat_halfspace_rays_dickinson
sat_b1
sat_b2
sat_b3
bdd_b3
sat_b4
sat_b5
nbc_b6
nbc_b7
improved_nbc_b7
improved_nbc_b8
improved_nbc_b9
improved_nbc_g2
sat_c1
sat_c2
sat_c3
sat_c4
f1
f2
g1
sat_a1
sat_a2
sat_a3
sat_a4
sat_a5
sat_halfspace_lp_dickinson
sat_halfspace_milp_dickinson
sat_halfspace_rays_lookahead_dickinson
sat_halfspace_rays_wide_dickinson
wide_certificate_sat_dickinson
xxx
xxx_two
clingo_dickinson
clingo_halfspace_dickinson
support_pruned_dickinson
nullity_support_pruned_dickinson
rhs_dickinson
frank_wolfe_dickinson
one_step_frank_wolfe_dickinson
pairwise_frank_wolfe_dickinson
support_polished_frank_wolfe_dickinson
safi_2021
bundfuss_2008
sponsel_2012
frank_wolfe_sponsel
fracessa
zischg_hadeler
zischg_dickinson
zischg_fracessa
```

Python invokes the same `coposit --model ...` command as C++ callers. That command selects an isolated one-model companion; Python
does not import a separate model-specific extension. Every Dickinson-, Hadeler-, and FracESSA-based model supports individually
selected CP and SCP and classifies both predicates in one traversal. Danninger 1990 also has that capability.

`dense_bitset_dickinson` allocates one packed bit for every Boolean-lattice support and traverses surviving bits by cardinality.
Set either `COPOSIT_DENSE_BITSET_MAX_N` or `COPOSIT_DENSE_BITSET_MAX_GIB`; setting neither uses a one-GiB bitmap limit.

`wide_certificate_cbdd_dickinson`, `wide_certificate_sat_dickinson`, and `sat_halfspace_rays_wide_dickinson` accept
`model_parameter="50"`, `"75"`, `"90"`, or any other integer percentage from 0 through 100. The argument is required. The
percentage applies to the remaining width $n-k$; each model has one implementation and one internal companion.

`xxx_two` requires `model_parameter="alternating"` or `model_parameter="ascending"`. The former alternates low and high seed
cardinalities; the latter exhausts seed cardinalities from smallest to largest.

`multithreaded_cbdd_dickinson` defaults to seven C++ worker threads pinned consecutively to CPUs 3–9 inside one matrix call. Set
`COPOSIT_CBDD_WORKERS` to another positive count and `COPOSIT_CBDD_FIRST_CPU` to the first CPU. When it is called through
`run_multiprocessing()`, multiply the process count by this internal thread count when budgeting CPUs and exact-arithmetic scratch
memory. With `diagnostics=True`, completed supports and certificates appear after each parallel batch in the normal one-second
decision-diagram status line.

Serial `cbdd_dickinson`, `cbdd_halfspace_dickinson`, `upper_endpoint_cbdd_dickinson`, `czdd_dickinson`, `sat_dickinson`,
`sat_halfspace_dickinson`, `sat_halfspace_rays_dickinson`, `sat_a1`, `sat_a2`, `sat_a3`, `sat_a4`, `sat_a5`, `sat_halfspace_lp_dickinson`, `sat_halfspace_milp_dickinson`,
`sat_halfspace_rays_lookahead_dickinson`,
`sat_halfspace_rays_wide_dickinson`, `wide_certificate_sat_dickinson`, `clingo_dickinson`, and `clingo_halfspace_dickinson`
diagnostics also print the sparse
joint histogram `certificate_k_d_u_counts=[(k,d,upper_size,count),...]`. Here `k` is the support cardinality at which a certificate was
generated, `d = |U| - |L|` is its number of free indices, and `upper_size = |U|` shows how high the interval reaches. The histogram
is collected only while visible diagnostics or the explicit diagnostics capture is enabled.

`ceiling_pruned_dickinson` uses the normal support diagnostics line and the same sparse histogram. Its `visited` count includes emitted
supports and exactly counted forbidden-branch skips, `covered` is the skipped part, `processed` counts exact Dickinson systems, and
`certificates` counts only retained certificates with full upper endpoint.

`kernel_cone_dickinson` keeps that traversal and, at a singular root of nullity greater than one, searches the exact projected
nullspace cone directly for additional full-upper-endpoint certificates. It does not traverse a graph of lifted principal supports.

`affine_companion_dickinson` additionally tests whether the singular system $A_Ix=\mathbf1$ is consistent. It searches the complete
affine line when the nullity is one and tests one retained-factorization particular solution at higher nullity before continuing the
same homogeneous and ordinary Dickinson fallback.

`cbdd_dickinson_improved_1` keeps ordinary CBDD Dickinson and replaces its singular one-vector step with both nullity-one
orientations, one consistent affine particular solution, and the complete stacked local-coordinate/outside-product line family at
nullity two. It batches the local intervals and retains only intervals that add coverage to the exact current CBDD union.

`upper_endpoint_cbdd_dickinson` keeps ordinary CBDD Dickinson and, before activating `[L,U]`, solves the distinct larger support
`U` once when `A_U` is nonsingular. The probe is not recursive.

`layered_singular_lift_dickinson` and `breadth_first_singular_lift_dickinson` use the same line. Their `processed` counter is
`outer_processed + lifted_processed`, so it can exceed `visited`. Lift diagnostics also separate duplicate and covered skips, the
cache size, current and maximum lifted cardinality and depth, and—for breadth first—the current and maximum FIFO frontier. Their
certificate distribution is `(root_k,lifted_k,|U|,|L|,count)`. Certificates found while lifting are retained only after the current
outer cardinality is complete.

## Build And Run

From the repository root:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
PYTHONPATH=python python3 -m unittest discover -s python/tests -v
```

The source-tree adapter finds `cpp/build/coposit`; an installed package keeps `coposit` and its isolated model companions beside
`pycoposit`. Set `COPOSIT` to an explicit launcher path only when using another build tree.

Sequential use accepts one `Matrix` or an iterable:

```python
from pycoposit import Matrix, run

result = run("hadeler_1983", Matrix("2#1,0,1"))
assert result["is_strictly_copositive"] is True

boundary = run("hadeler_1983", Matrix("2#1,-1,1"), mode="copositive")
assert boundary["is_copositive"] is True

classification = run("hadeler_1983", Matrix("2#1,-1,1"), mode="both")
assert classification["is_copositive"] is True
assert classification["is_strictly_copositive"] is False

# Interactive one-second status lines go to stderr; ordinary result data is unchanged.
run("cbdd_dickinson", Matrix("matrix.mtx"), diagnostics=True)
```

`Matrix` has two fields:

| Field | Required | Meaning |
|---|:---:|---|
| `matrix` | yes | Compact matrix text, inline Matrix Market text, or a file path. |
| `matrix_id` | no | Optional signed 64-bit label copied into the result; it does not affect parsing or mathematics. |

Multiprocessing uses the portable `spawn` method by default, so normal Python entry-point protection is required:

```python
from pycoposit import MPConfig, Matrix, run_multiprocessing


def main() -> None:
    matrices = [Matrix("2#1,0,1", matrix_id=1), Matrix("2#1,-2,1", matrix_id=2)]
    for result in run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2)):
        print(result)


if __name__ == "__main__":
    main()
```

`Matrix.matrix` normally contains the compact FracESSA `dimension#values` format. It accepts a full row-major upper triangle or the
short circular-symmetric form containing `floor(dimension/2)+1` values: the common diagonal followed by the successive circular
distances. Exact values may be integers, decimals, scientific notation, or fractions. A fraction is written
`[+|-]numerator/denominator`; the nonzero denominator has no sign. Compact matrix text must not contain whitespace and must include
its own dimension prefix.

An ordinary string that is neither compact `dimension#values` nor inline Matrix Market text is treated as a file path and passed to
C++ for loading. Python string literals require quotes; single and double quotes are equivalent, and a path stored in a variable works
identically. Absolute paths are accepted. Relative paths are interpreted from the process's current working directory:

```python
from pycoposit import Matrix, run

run("hadeler_1983", Matrix("matrix.mtx"))
run("hadeler_1983", Matrix('matrix.mtx'))
path = "matrix.mtx"
run("hadeler_1983", Matrix(path))
```

The maintained database still stores large matrices as internal `file:<relative-path>` references. Corpus runners resolve those
references against the database directory before constructing `Matrix`; `file:` is not part of the Python user interface. C++ opens
and parses the resulting direct path without a Python content copy. Matrix Market real/scientific numbers are exact, while slash
fractions are available only in the compact FracESSA format.

Every result contains `algorithm`, `mode`, optional `matrix_id`, integer `status`, `is_copositive`, `is_strictly_copositive`,
`elapsed_ns`, and `error_message`. For `copositive` or `strictly_copositive`, only the selected field contains `True` or
`False`; the other is `None`. `mode="both"` fills both fields after one traversal and is supported by every model listed in
`COMBINED_CLASSIFICATION_ALGORITHMS`: all Dickinson-, Hadeler-, and FracESSA-based models plus Danninger 1990. Their only possible pairs are
`(False, False)`, `(True, False)`, and `(True, True)`. Other models return `EXEC_ERROR` for `both`. Both fields are `None` on any
failure. The eight baselines, `adaptive_sponsel_copomatrix`, and every combined-capable family model support the two individually
selected modes. Other coposit-created variants return `EXEC_ERROR` for `copositive` mode rather than silently applying strict rules. The
capability check happens before connected-component splitting or pre-checks, so preprocessing cannot bypass that restriction.
Status codes are `OK=0`,
`PARSE_ERROR=1`, `EXEC_ERROR=4`, `TIMEOUT=5`,
`NODE_LIMIT=6`, and `INTERNAL_ERROR=255`. Dutour 2018, Danninger 1990, Bundfuss 2008, Sponsel 2012, Frank–Wolfe Sponsel, and Safi 2021
return `NODE_LIMIT` instead of a Boolean if a split would exceed 50,000 simultaneously unfinished nodes.

The multiprocessing runner bounds pending work by `min(queue_maxsize, workers * prefetch_per_worker)`, reports dead workers, and
terminates workers if its result iterator is closed early. It does not implement per-matrix timeouts yet.

## Reference Results

`run_results.py` runs one model over dimension and matrix-ID ranges, pins one persistent single-threaded worker process to each
requested worker CPU, applies a cooperative timeout to every matrix, and pins its dispatcher and database writer to a separate parent
CPU. It reads the tracked corpus database but writes by default to the ignored local `experiments/diagnostics.sqlite3` database:

```bash
PYTHONPATH=python python3 python/run_results.py hadeler_1983 \
    --timeout-seconds 5 \
    --matrix-set core_and_stress_test \
    --parent-cpu 3 \
    --cpus 4 5 6 7 \
    --preprocessing both
```

When `--mode` is omitted, a model with one-pass combined classification runs in `both` mode. A model without that capability stops
with an explicit error; run it once with `--mode copositive` and once with `--mode strictly_copositive`. A matrix is fully classified
only when both predicates are known. A completed strict-only call returning `false` is a partial predicate result, not a solved
matrix, because it does not distinguish a copositive boundary matrix from a non-copositive matrix.

`--matrix-set` accepts one or more of `smoke_set`, `core_and_stress_test`, `n_le_100`, `n_gt_100_solved`, `bpqy_benchmark`,
`bpqy_quick_test`, and `references_unsolved`, and runs their union. The first six names select
Boolean corpus flags; `references_unsolved` selects rows with at least one explicit literature failure claim. It can
be combined with the dimension and matrix-ID bounds. Every named set selector excludes `preprocessing_solved = 1`, including the
derived `references_unsolved` selector. Without a set selector, the runner selects all rows inside the explicit bounds.
`--matrix-ids ID...` further restricts a run to an explicit set of positive matrix IDs.
`--without-results` further restricts the selection to matrices that have no row at all in `results`, independently of model or status.
`--results-database PATH` overrides the diagnostics database. A custom `--database` without this option keeps the historical single-file
behavior, which is useful for disposable test databases.

`--model-parameter VALUE` is required for parameterized models. The three wide-certificate models use an integer percentage;
`xxx_two` uses `alternating` or `ascending`. Parameterized result rows append `@VALUE` to the selected model identifier so different
values of the same binary do not overwrite one another.

For `dense_bitset_dickinson`, the mutually exclusive `--dense-bitset-max-n N` and `--dense-bitset-max-gib GIB` options pass the
corresponding allocation limit to every persistent worker.

`--preprocessing` accepts `none` or `both`. The default `both` runs the complete fixed shared exact preprocessing pipeline;
`none` must be selected explicitly for an unchanged linked-model measurement. The enabled pipeline includes connected components,
all exact checks, and depth-bounded Danninger and COPOMATRIX reductions. Their internal maximum reduction depth is two and is not a
Python option. The selected value is stored in the constrained `results.preprocessing` column and forms part of the result identity.

The stored `binary_sha256` is the hash of the selected model companion executed through `coposit`. A timeout or worker failure stores a `NULL`
classification. Existing rows for the same matrix, model, mode, preprocessing choice, and binary are skipped so interrupted runs
resume. Pass `--rerun` to replace every selected row, or `--retry-timeouts` to replace only timeout rows for that exact identity.
Normal runs do not hash external matrix files before parsing or before reusing an existing result.
The same serialized batch transaction refreshes `matrices.fastest_elapsed_ns` and `matrices.fastest_result_ref` for affected IDs,
using the fastest eligible `ok` result that agrees with every known corpus truth value. Only one-pass combined `both` results are
eligible; preprocessing may be enabled or disabled. Predicate-only measurements are excluded.

Every `run_results.py` campaign automatically captures the complete one-second diagnostic text and any sparse certificate
distribution supplied by the selected model. CBDD and CZDD store `(support cardinality, free indices, upper-set cardinality, count)`
quadruples; other supported experiments retain their triples. While a matrix is active, the parent replaces its `running` row once
per second. The final status then replaces that row. Consequently, a hard timeout retains the most recent diagnostics and distribution
even if the solver cannot return during the cooperative grace period. There is no diagnostics-off runner option while this benchmark
policy is active.

Each persistent Python worker starts `coposit` for one matrix at a time. On POSIX the launcher immediately replaces itself with the
selected model companion. At a matrix deadline the parent sends `SIGUSR1` to the worker, which forwards it to the companion; the
native handler only sets a signal-safe flag, and the model returns `TIMEOUT` at its next safe checkpoint. The worker then accepts the next matrix.
If the native call has not returned one second after the signal, the parent records the timeout with its latest saved diagnostic
snapshot, terminates that worker, and starts a replacement on the same CPU.
A `node_limit` row is likewise unresolved and has no Boolean classification. A single long FLINT operation can delay a cooperative
timeout return until the operation finishes. Ctrl-C stops new assignments and lets already-running matrices finish or reach their
configured timeout before the runner exits. Runs of at most 100 matrices print every result; larger runs print diagnostics at most once
per second, plus every mismatch or error and the final result. A bounded queue passes completed results to the SQLite writer on the
parent CPU. The writer drains everything currently queued into one transaction; both the queue and each batch are limited to twice
the worker count. An orderly stop drains every row, while a hard process or machine failure can lose at most the current bounded batch.
