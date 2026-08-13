# pycoposit Analysis Interface

`pycoposit` is the analysis interface. It exposes explicit model and preprocessing selection through the same three-layer shape as
PyFracESSA; these controls are intentionally absent from the normal `coposit fast|safe strict|non-strict` command:

- `compute_matrix(algorithm, matrix, mode="strictly_copositive", preprocessing="none")` is the one-matrix native adapter;
- `run(algorithm, matrices, mode="strictly_copositive", preprocessing="none")` is the sequential path;
- `run_multiprocessing(algorithm, matrices, mp_config=None, mode="strictly_copositive", preprocessing="none")` is the bounded
  process path and yields iterables in completion order.

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
dickinson_final
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

Each native extension links exactly one self-contained model. Python selects the extension; model implementations are not merged or
deduplicated.

## Build And Run

From the repository root:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
PYTHONPATH=python python3 -m unittest discover -s python/tests -v
```

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
`False`; the other is `None`. `mode="both"` fills both fields after one traversal and is supported by `danninger_1990`,
`hadeler_1983`, `dickinson_2019`, and `dickinson_final`, listed by `COMBINED_CLASSIFICATION_ALGORITHMS`. Their only possible pairs are
`(False, False)`, `(True, False)`, and `(True, True)`. Other models return `EXEC_ERROR` for `both`. Both fields are `None` on any
failure. The eight baselines under `models/baselines/`, `adaptive_sponsel_copomatrix`, and `dickinson_final` support the two
individually selected modes.
Other coposit-created variants return `EXEC_ERROR` for `copositive` mode rather than silently applying their strict rules. The
capability check happens before connected-component splitting or pre-checks, so preprocessing cannot bypass that restriction.
Status codes are `OK=0`,
`PARSE_ERROR=1`, `EXEC_ERROR=4`, `TIMEOUT=5`,
`NODE_LIMIT=6`, and `INTERNAL_ERROR=255`. Dutour 2018, Danninger 1990, Bundfuss 2008, Sponsel 2012, Frank–Wolfe Sponsel, and Safi 2021
return `NODE_LIMIT` instead of a Boolean if a split would exceed 50,000 simultaneously unfinished nodes.

The multiprocessing runner bounds pending work by `min(queue_maxsize, workers * prefetch_per_worker)`, reports dead workers, and
terminates workers if its result iterator is closed early. It does not implement per-matrix timeouts yet.

## Reference Results

`run_results.py` runs one model over dimension and matrix-ID ranges, pins one persistent single-threaded worker process to each
requested worker CPU, applies a cooperative timeout to every matrix, and pins its dispatcher and database writer to a separate parent CPU:

```bash
PYTHONPATH=python python3 python/run_results.py hadeler_1983 \
    --timeout-seconds 5 \
    --matrix-set representative_core stress_test \
    --parent-cpu 3 \
    --cpus 4 5 6 7 \
    --preprocessing both
```

`--matrix-set` accepts one or more of `smoke_set`, `representative_core`, `stress_test`, `scale_set`, and
`timeout_5s_strict_set`, and runs their union. It can
be combined with the dimension and matrix-ID bounds. Without it, the runner selects all rows inside those bounds.

`--preprocessing` accepts `none`, `connected_components`, `pre_checks`, or `both`. The default `none` preserves the linked model
unchanged. Non-default choices wrap that model in the shared exact preprocessing pipeline. The selected value is stored in the
constrained `results.preprocessing` column and forms part of the result identity.

The stored `binary_sha256` is the hash of the selected model's native extension. A timeout or worker failure stores a `NULL`
classification. Existing rows for the same matrix, model, mode, preprocessing choice, and binary are skipped so interrupted runs
resume. Pass `--rerun` to replace every selected row, or `--retry-timeouts` to replace only timeout rows for that exact identity.
Normal runs do not hash external matrix files before parsing or before reusing an existing result.

Each worker loads its model once. At a matrix deadline the parent sends `SIGUSR1`; the native handler only sets a signal-safe flag,
and the model returns `TIMEOUT` at its next safe checkpoint. The worker then accepts the next matrix without reloading the module.
If the native call has not returned one second after the signal, the parent records the timeout, terminates that worker, and starts a
replacement on the same CPU.
A `node_limit` row is likewise unresolved and has no Boolean classification. A single long FLINT operation can delay a cooperative
timeout return until the operation finishes. Ctrl-C stops new assignments and lets already-running matrices finish or reach their
configured timeout before the runner exits. Runs of at most 100 matrices print every result; larger runs print progress at most once
per second, plus every mismatch or error and the final result. A bounded queue passes completed results to the SQLite writer on the
parent CPU. The writer drains everything currently queued into one transaction; both the queue and each batch are limited to twice
the worker count. An orderly stop drains every row, while a hard process or machine failure can lose at most the current bounded batch.
