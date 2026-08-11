# Coposit

Coposit decides exact ordinary and strict copositivity for nonempty symmetric integer matrices. The eight literature baselines support
both selectable modes; Adaptive Sponsel–COPOMATRIX does too, while the other Coposit-created variants remain explicitly strict-only.
Hadeler 1983, Dickinson 2019, and Danninger 1990 can additionally classify both predicates in one traversal.

## Build And Test

Required: CMake 3.18 or newer, a C++17 compiler, Python 3.11 or newer, FLINT, MPFR, and GMP. CMake fetches pybind11 and GoogleTest.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

## Run

Input is `dimension#upper-triangle-values`, using exact base-10 integers in row-major upper-triangle order:

```bash
echo '2#1,0,1' | cpp/build/coposit
cpp/build/coposit matrix.txt
echo '2#1,-1,1' | cpp/build/coposit --mode copositive
```

The command prints `true` or `false` for the selected predicate. `--mode` accepts `copositive` or `strictly_copositive` and defaults
to `strictly_copositive`. Invalid input or a request for ordinary mode from a strict-only variant exits with status 2.

Solver sources live under `models/<model-name>/`, literature baselines under `models/baselines/<model-name>/`, active experiments
under `models/experiments/<model-name>/`, and retained legacy models under `models/legacy/<model-name>/`. The selected `dutour_2018`
model builds as the user-facing `coposit` executable.
The available model targets `danninger_1990`, `copomatrix_2011`, `adaptive_dutour_danninger`,
`adaptive_dutour_copomatrix`, `adaptive_sponsel_copomatrix`,
`adaptive_zischg_sponsel_copomatrix`, `hadeler_1983`,
`dickinson_2019`, `support_pruned_dickinson`,
`nullity_support_pruned_dickinson`, `rhs_dickinson`, `frank_wolfe_dickinson`,
`one_step_frank_wolfe_dickinson`,
`pairwise_frank_wolfe_dickinson`, `support_polished_frank_wolfe_dickinson`, `safi_2021`, `bundfuss_2008`, `sponsel_2012`,
`frank_wolfe_sponsel`, `fracessa`, `zischg_hadeler`, `zischg_dickinson`, and `zischg_fracessa` use the same input protocol.
Model algorithm code is intentionally copied rather than shared so variants can diverge and interweave algorithms freely. The
`fracessa` target decides
whether the simplex minimum is positive from exact first-order candidate payoff signs for `-A`, stopping at the first nonnegative
payoff. Its supports use the shared packed representation with `ceil(n / 64)` machine words, so it has no fixed-width dimension limit.

## Python

The source-tree package mirrors FracESSA's thin native, sequential, and bounded multiprocessing paths. The first argument selects a
maintained algorithm:

```python
from pycoposit import MPConfig, Matrix, run, run_multiprocessing

matrix = Matrix(matrix_id=1, matrix="2#1,0,1")
single_result = run("hadeler_1983", matrix)
ordinary_result = run("hadeler_1983", Matrix(2, "2#1,-1,1"), mode="copositive")
classification = run("hadeler_1983", Matrix(3, "2#1,-1,1"), mode="both")
assert classification["is_copositive"] is True
assert classification["is_strictly_copositive"] is False

if __name__ == "__main__":
    results = list(run_multiprocessing("hadeler_1983", [matrix], MPConfig(workers=2)))
```

After the CMake build, run source-tree scripts with `PYTHONPATH=python`. Package builds use `pyproject.toml`. See
`python/README.md` for the result contract and all algorithm identifiers.

Reference runs use a cooperative per-matrix timeout and explicit CPU IDs:

```bash
PYTHONPATH=python python3 python/run_results.py hadeler_1983 \
    --timeout-seconds 5 --dimension-from 2 --dimension-to 20 \
    --parent-cpu 3 --cpus 4 5 6 7 8 9
```

Completed, timed-out, and failed runs are stored in `testdata/Copos_testdata.sqlite3`. The three support-traversal `zischg_*`
variants apply Level 2 negative-graph reduction inside their support traversals. `adaptive_zischg_sponsel_copomatrix` instead
decomposes each COPOMATRIX projection child before adaptive restart and is retained under `models/legacy/`; none of these
models performs a one-time root split.

Small corpus matrices remain inline in SQLite. Large matrices are exact symmetric integer Matrix Market files under
`testdata/matrices/`; database rows reference them with `file:<relative-path>`, which the Python corpus runner resolves automatically.
