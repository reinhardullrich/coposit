<p align="center">
  <img src="logo.png" width="600" alt="coposit logo" />
</p>

# Coposit

Coposit decides exact copositivity (CP, the non-strict predicate) and strict copositivity (SCP) for nonempty symmetric integer
matrices. The eight literature baselines and the selected Adaptive Sponsel–COPOMATRIX and Dickinson Final models support both
selectable modes; the other Coposit-created variants remain explicitly strict-only. Hadeler 1983, Dickinson 2019, Dickinson Final,
and Danninger 1990 can additionally classify both predicates in one traversal.

## Build And Test

Required: CMake 3.18 or newer, a C++17 compiler, Python 3.11 or newer, FLINT, MPFR, and GMP. CMake fetches pybind11 and GoogleTest.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

## Run

Input may use the compact FracESSA format, `dimension#values`. A full list contains the upper triangle in row-major order. A short
list of `floor(dimension/2)` values creates a zero-diagonal circular-symmetric matrix from its successive circular distances. Values
may be exact integers, decimals, scientific notation such as `2.5E15`, or fractions such as `-1/2`. A fraction has one optional sign
before its numerator and an unsigned nonzero integer denominator. Compact matrix text contains no whitespace.

```bash
cpp/build/coposit fast strict 2#1,0,1
cpp/build/coposit safe strict matrix.txt
cpp/build/coposit fast non-strict '2#1,-1,1'
cpp/build/coposit safe both '2#1,-1,1'
cpp/build/coposit safe strict --progress long-running-matrix.mtx
cpp/build/coposit fast strict --timeout 30 long-running-matrix.mtx
```

Quotes are optional shell syntax and are not part of the matrix. A positional argument beginning with one or more decimal digits
immediately followed by `#` is compact matrix text; every other positional argument is a file path. Omitting the argument or passing
`-` reads standard input.

The public command requires one search method:

| Method | Solver | Completion contract |
|---|---|---|
| `fast` | Adaptive Sponsel–COPOMATRIX | Every Boolean is exact, but the bounded traversal may stop unresolved |
| `safe` | Dickinson Final | Complete finite exact certificate enumeration when allowed to finish |

After the method, the public command requires `strict` or `non-strict`; `safe` additionally accepts `both`. No predicate has a hidden
default. Combined safe mode performs one Dickinson classification traversal and prints named `copositive` and
`strictly_copositive` results. The single-predicate commands print `true` or `false`. Both methods use the fused component/pre-check
pipeline: globally valid checks run during the root scan, negative-entry components are then visited, and Frank–Wolfe plus exact
definiteness are deferred to each component. Invalid input, a node limit, or another unresolved resource failure exits nonzero
instead of returning `false`.
The public command intentionally has no preprocessing option: both stages are always enabled. Explicit model and preprocessing
selection belongs to the analysis interfaces described below.

`--progress` writes one status line to standard error every second without changing the final standard-output result. The named
metric identifies the current preprocessing phase and work counter, or exact support-enumeration coverage, certified simplex
volume, recursive proof coverage, or traversal counters according to the model; it is deliberately not an ETA. See
[`docs/PROGRESS_REPORTING.md`](docs/PROGRESS_REPORTING.md) for the definitions and overhead boundary.

`--timeout SECONDS` applies a hard wall-clock limit to the complete command, including input parsing, connected components,
pre-checks, and the model. `SECONDS` must be positive and may contain a decimal fraction. At expiry the launcher terminates its
isolated companion, prints a timeout message on standard error, exits with status `124`, and prints no Boolean answer. Without this
option the launcher retains the direct no-watchdog execution path.

The same file or standard-input boundary accepts NIST Matrix Market `array` and `coordinate` matrices declared `symmetric`, with
`real`, `complex`, `integer`, or `pattern` fields. Other Matrix Market structures are rejected immediately; the stored lower triangle
is mirrored, so the parsed matrix is symmetric without a separate symmetry scan. Decimal and scientific values are converted exactly;
a complex matrix is usable by Coposit only when every imaginary part is zero. Slash fractions remain specific to the compact FracESSA
format. The parser clears one least common positive denominator and gives the algorithms only integers.
The exact-number, FracESSA, Matrix Market, and format-dispatch parsers live under `cpp/include/coposit/parsers/`.
Model entry points assume this parser-guaranteed nonempty square symmetric matrix and do not repeat the shape or symmetry scan.

For a worked, introductory explanation of Dickinson's certificate traversal, see
[`docs/DICKINSON_ALGORITHM_STEP_BY_STEP.md`](docs/DICKINSON_ALGORITHM_STEP_BY_STEP.md). Each model's authoritative technical
description remains its local `ALGORITHM.md`.

The selected Adaptive Sponsel–COPOMATRIX and Dickinson Final sources live directly under `models/`. Literature baselines live under
`models/baselines/<model-name>/`; every other Coposit-created model and comparison lives under
`models/experiments/<model-name>/`. The user-facing `coposit` launcher dispatches `fast` and `safe` to separate one-model companion
binaries. `coposit-analyze` is the sole C++ analysis command and dispatches an explicit model choice to an internal isolated
companion. The Python package remains the batch-analysis interface. Model algorithm code is intentionally copied rather than shared
so variants can diverge and interweave algorithms freely. `dickinson_final` is the selected independent copy of the `dickinson_2019`
baseline used by the public `safe` method. The `fracessa` model decides
whether the simplex minimum is positive from exact first-order candidate payoff signs for `-A`, stopping at the first nonnegative
payoff. Its supports use the shared packed representation with `ceil(n / 64)` machine words, so it has no fixed-width dimension limit.

## C++ Analysis Interface

Select one comparison model and independently tune preprocessing with `coposit-analyze`:

```bash
cpp/build/coposit-analyze \
  --model bundfuss_2008 \
  --mode strict \
  --timeout 30 \
  --connected-components off \
  --pre-checks on \
  --pre-check frank-wolfe off \
  --principal-submatrices-up-to 2 \
  matrix.mtx
```

All preprocessing is on by default. Every selectable model supports strict and non-strict mode; Danninger, Hadeler, and Dickinson
Final additionally support one-pass `--mode both`. The inventory comprises the literature baselines except superseded Dickinson 2019,
plus Dickinson Final and Adaptive Sponsel–COPOMATRIX. The complete model list, individual pre-check names, output contract, and
examples are in [`docs/ANALYSIS_CLI.md`](docs/ANALYSIS_CLI.md).

## Python Analysis Interface

The source-tree package mirrors FracESSA's thin native, sequential, and bounded multiprocessing paths. The first argument selects a
maintained algorithm:

```python
from pycoposit import MPConfig, Matrix, run, run_multiprocessing

matrix = Matrix("2#1,0,1")
single_result = run("hadeler_1983", matrix)
cp_result = run("hadeler_1983", Matrix("2#1,-1,1"), mode="copositive")
classification = run("hadeler_1983", Matrix("2#1,-1,1"), mode="both")
assert classification["is_copositive"] is True
assert classification["is_strictly_copositive"] is False

if __name__ == "__main__":
    results = list(run_multiprocessing("hadeler_1983", [matrix], MPConfig(workers=2)))
```

After the CMake build, run source-tree scripts with `PYTHONPATH=python`. Package builds use `pyproject.toml`. See
`python/README.md` for the result contract and all algorithm identifiers. A Python `Matrix` accepts compact or inline Matrix Market
text directly, or an ordinary relative or absolute file path that C++ opens and parses. Its matrix input is required and `matrix_id`
is optional; there is no metadata or base-directory argument.

Reference runs use a cooperative per-matrix timeout and explicit CPU IDs:

```bash
PYTHONPATH=python python3 python/run_results.py hadeler_1983 \
    --timeout-seconds 5 --dimension-from 2 --dimension-to 20 \
    --parent-cpu 3 --cpus 4 5 6 7
```

Completed, timed-out, and failed runs are stored in `testdata/copos_testdata.sqlite3`. The three support-traversal `zischg_*`
variants apply Level 2 negative-graph reduction inside their support traversals. `adaptive_zischg_sponsel_copomatrix` instead
decomposes each COPOMATRIX projection child before adaptive restart and is retained under `models/experiments/`; none of these
models performs a one-time root split.

Small corpus matrices remain inline in SQLite. Large matrices are exact symmetric integer Matrix Market files under
`testdata/matrices/`; each uses whichever standard symmetric array or coordinate representation is smaller. Database rows reference
them with `file:<relative-path>`, which the Python corpus runner resolves automatically.
