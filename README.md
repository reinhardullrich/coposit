<p align="center">
  <img src="logo.png" width="600" alt="coposit logo" />
</p>

# coposit

coposit experiments with exact copositivity (CP, the non-strict predicate) and strict copositivity (SCP) algorithms for nonempty
symmetric integer matrices. Every Dickinson-, Hadeler-, and FracESSA-based model supports selected CP or SCP checks and full CP/SCP
classification in one traversal. Danninger 1990 also supports combined classification. Other models expose only the modes they
implement explicitly.

## Build And Test

Required: CMake 3.18 or newer, C and C++17 compilers, Python 3.11 or newer, FLINT, MPFR, and GMP. CMake fetches GoogleTest,
CaDiCaL 2.2.1 for the SAT experiment, and clingo 5.8.2 with clasp 3.4.1 for the Clingo-SAT experiment.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

## Run

Input may use the compact FracESSA format, `dimension#values`. A full list contains the upper triangle in row-major order. A short
list of `floor(dimension/2)+1` values creates a circular-symmetric matrix from its common diagonal followed by its successive circular
distances. Values may be exact integers, decimals, scientific notation such as `2.5E15`, or fractions such as `-1/2`. A fraction has
one optional sign before its numerator and an unsigned nonzero integer denominator. Compact matrix text contains no whitespace.

```bash
cpp/build/coposit --model adaptive_sponsel_copomatrix --mode strict '2#1,0,1'
cpp/build/coposit --model dickinson_2019 --mode both matrix.txt
cpp/build/coposit --model hadeler_1983 --mode non-strict --preprocessing off '2#1,-1,1'
cpp/build/coposit --model dickinson_2019 --mode both --diagnostics --timeout 30 long-running-matrix.mtx
```

Quotes are optional shell syntax and are not part of the matrix. A positional argument beginning with one or more decimal digits
immediately followed by `#` is compact matrix text; every other positional argument is a file path. Omitting the argument or passing
`-` reads standard input.

`coposit` always requires an explicit model. For a combined-capable model, omitting `--mode` selects `both`; every other model
requires `strict` or `non-strict`. There are no `fast`, `safe`, or implicit model aliases during the experimentation phase. Combined
mode prints named `copositive` and `strictly_copositive` results; a single-predicate command prints `true` or `false`.

The complete preprocessing pipeline is on by default and can be bypassed with `--preprocessing off`. Globally valid checks run
during the root scan, negative-entry components are then visited, and Frank–Wolfe plus exact
definiteness and the structural matrix checks are deferred to each component. Exact Motzkin–Straus graph matrices use the
Open-MCS-derived exact maximum-clique search instead of maximal-Z enumeration. Danninger is then attempted when its best pivot creates at most
two children; an unresolved matrix
receives the corresponding COPOMATRIX attempt. Reduction children re-enter the same pipeline up to the internal maximum reduction
depth of two. Resolved connected components are discarded; the selected model receives only unresolved component matrices. An
inconclusive Danninger or COPOMATRIX attempt retains its unchanged parent component rather than its generated children. Invalid input,
a node limit, or another unresolved resource failure exits nonzero instead of returning `false`.

`--diagnostics` writes one status line to standard error every second without changing the final standard-output result. The named
metric identifies the current preprocessing phase and work counter, or exact support-enumeration coverage, certified simplex
volume, recursive proof coverage, or traversal counters according to the model; it is deliberately not an ETA. See
[`docs/DIAGNOSTICS_REPORTING.md`](docs/DIAGNOSTICS_REPORTING.md) for the definitions and overhead boundary.

`--timeout SECONDS` applies a hard wall-clock limit to the complete command, including input parsing, connected components,
pre-checks, and the model. `SECONDS` must be positive and may contain a decimal fraction. At expiry the launcher terminates its
isolated companion, prints a timeout message on standard error, exits with status `124`, and prints no Boolean answer. Without this
option the launcher retains the direct no-watchdog execution path.

The same file or standard-input boundary accepts NIST Matrix Market `array` and `coordinate` matrices declared `symmetric`, with
`real`, `complex`, `integer`, or `pattern` fields. Other Matrix Market structures are rejected immediately; the stored lower triangle
is mirrored, so the parsed matrix is symmetric without a separate symmetry scan. Decimal and scientific values are converted exactly;
a complex matrix is usable by coposit only when every imaginary part is zero. Slash fractions remain specific to the compact FracESSA
format. The parser clears one least common positive denominator and gives the algorithms only integers.
The exact-number, FracESSA, Matrix Market, and format-dispatch parsers live under `cpp/include/coposit/parsers/`.
Model entry points assume this parser-guaranteed nonempty square symmetric matrix and do not repeat the shape or symmetry scan.

For a worked, introductory explanation of Dickinson's certificate traversal, see
[`docs/DICKINSON_ALGORITHM_STEP_BY_STEP.md`](docs/DICKINSON_ALGORITHM_STEP_BY_STEP.md). Each model's authoritative technical
description remains its local `ALGORITHM.md`.

All models inheriting the Hadeler, Dickinson, or FracESSA support-system approach live under
`models/hadeler-based/<model-name>/`; [`models/hadeler-based/README.md`](models/hadeler-based/README.md) gives the compact inventory.
Other literature baselines live under `models/baselines/<model-name>/`, and other coposit-created models and comparisons, including
Adaptive Sponsel–COPOMATRIX, live under `models/experiments/<model-name>/`. No model directory lives directly under `models/`.
`coposit` dispatches an explicit model choice to an internal isolated companion. Python uses this same command rather than a second
native model interface. Model algorithm code is intentionally copied rather than shared so variants can diverge and interweave algorithms freely.
The `fracessa` model decides
whether the simplex minimum is positive from exact first-order candidate payoff signs for `-A`, stopping at the first nonnegative
payoff. Its supports use the shared packed representation with `ceil(n / 64)` machine words, so it has no fixed-width dimension limit.
The separate `fracessa_circular` experiment assumes a circular-symmetric input and applies FracESSA's direct bracelet generation,
complete rotation/reflection orbit pruning, and exact affine-multiplier reduction while classifying both CP and SCP.

## C++ Analysis Interface

Select any baseline or experiment and enable or bypass the complete preprocessing pipeline with `coposit`:

```bash
cpp/build/coposit \
  --model bundfuss_2008 \
  --mode strict \
  --timeout 30 \
  --preprocessing off \
  matrix.mtx
```

The fixed pipeline is on by default; its internal stages cannot be switched independently. Every baseline and experiment is selectable.
Models expose only their implemented predicate modes; every Dickinson-, Hadeler-, and FracESSA-based model plus Danninger 1990 supports
one-pass `--mode both` and defaults to it. The complete model list, preprocessing flow, output contract, and examples are in
[`docs/ANALYSIS_CLI.md`](docs/ANALYSIS_CLI.md).

## Python Analysis Interface

The source-tree package provides thin `coposit`, sequential, and bounded multiprocessing paths. The first argument selects a model:

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
