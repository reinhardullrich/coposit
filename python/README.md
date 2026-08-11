# pycoposit

`pycoposit` exposes the maintained C++ models through the same three-layer shape as PyFracESSA:

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

result = run("hadeler_1983", Matrix(matrix_id=1, matrix="2#1,0,1"))
assert result["is_strictly_copositive"] is True

boundary = run("hadeler_1983", Matrix(matrix_id=2, matrix="2#1,-1,1"), mode="copositive")
assert boundary["is_copositive"] is True

classification = run("hadeler_1983", Matrix(matrix_id=3, matrix="2#1,-1,1"), mode="both")
assert classification["is_copositive"] is True
assert classification["is_strictly_copositive"] is False
```

Multiprocessing uses the portable `spawn` method by default, so normal Python entry-point protection is required:

```python
from pycoposit import MPConfig, Matrix, run_multiprocessing


def main() -> None:
    matrices = [Matrix(1, "2#1,0,1"), Matrix(2, "2#1,-2,1")]
    for result in run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2)):
        print(result)


if __name__ == "__main__":
    main()
```

`Matrix.matrix` normally contains `dimension#upper-triangle-values`. Values-only text is also accepted when
`Matrix.metadata["dimension"]` is an integer. It may instead contain `file:<relative-path>` for an exact symmetric integer Matrix
Market array; file references additionally require `Matrix.metadata["base_directory"]`. Corpus references are relative to the SQLite
database directory.

Every result contains `algorithm`, `mode`, `matrix_id`, integer `status`, `is_copositive`, `is_strictly_copositive`, `elapsed_ns`,
`error_message`, and the input `metadata`. For `copositive` or `strictly_copositive`, only the selected field contains `True` or
`False`; the other is `None`. `mode="both"` fills both fields after one traversal and is supported by `danninger_1990`,
`hadeler_1983`, and `dickinson_2019`, listed by `COMBINED_CLASSIFICATION_ALGORITHMS`. Their only possible pairs are
`(False, False)`, `(True, False)`, and `(True, True)`. Other models return `EXEC_ERROR` for `both`. Both fields are `None` on any
failure. The eight baselines under `models/baselines/` and `adaptive_sponsel_copomatrix` support the two individually selected modes.
Other Coposit-created variants return `EXEC_ERROR` for `copositive` mode rather than silently applying their strict rules. Status
codes are `OK=0`,
`PARSE_ERROR=1`, `EXEC_ERROR=4`, `TIMEOUT=5`,
`NODE_LIMIT=6`, and `INTERNAL_ERROR=255`. Dutour 2018, Bundfuss 2008, Sponsel 2012, Frank–Wolfe Sponsel, and Safi 2021 return
`NODE_LIMIT` instead of a Boolean if a split would exceed 50,000 simultaneously unfinished nodes.

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
    --preprocessing both \
    --parameters "pivot=first; split=largest"
```

`--matrix-set` accepts one or more of `smoke_set`, `representative_core`, `stress_test`, and `scale_set`, and runs their union. It can
be combined with the dimension and matrix-ID bounds. Without it, the runner selects all rows inside those bounds.

`--preprocessing` accepts `none`, `connected_components`, `pre_checks`, or `both`. The default `none` preserves the linked model
unchanged. Non-default choices wrap that model in the shared exact preprocessing pipeline and add a canonical
`preprocessing=...` entry to the stored parameter text.

`--parameters` stores an optional free-text description with every row; its default is the empty string. The stored `binary_sha256`
is the hash of the selected model's native extension. A timeout or worker failure stores a `NULL` classification. Existing rows for
the same matrix, model, parameter text, and binary are skipped so interrupted runs resume. Pass `--rerun` to replace every selected
row, or `--retry-timeouts` to replace only timeout rows for that exact model, parameter text, and binary.

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
