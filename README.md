<p align="center">
  <img src="logo.png?v=2" width="600" alt="coposit logo" />
</p>

# coposit

coposit decides whether a symmetric matrix is copositive, strictly copositive, or both. It uses exact arithmetic for every final
decision.

For a real symmetric matrix $A \in \mathbb{R}^{n \times n}$:

- **copositive (CP)** means $x^\top A x \geq 0$ for every $x \in \mathbb{R}^n$ with $x \geq 0$ componentwise;
- **strictly copositive (SCP)** means $x^\top A x > 0$ for every nonzero $x \in \mathbb{R}^n$ with $x \geq 0$ componentwise.

Strict copositivity is the stronger property. A matrix can therefore be copositive without being strictly copositive.

The ordinary command-line, Python, and C++ interfaces select the current production solver internally. Its identity is not part of
the public interface and may change between releases. The repository also keeps literature baselines and experimental models for
reproducible research.

## Install

### Command line

Download the package for Linux, macOS, or Windows from [GitHub Releases](https://github.com/reinhardullrich/coposit/releases) and
extract it. Run `coposit`; the other packaged files are private runtime dependencies and are not user interfaces.

On Windows, run `coposit.exe` instead of `./coposit` in the examples below.

### Python

Python 3.11 through 3.14 users can install the package from PyPI:

```bash
python -m pip install pycoposit
```

## Quick Start

The compact input below represents

```text
A = [[ 1, -1],
     [-1,  1]]
```

Run both checks:

```bash
./coposit '2#1,-1,1'
```

The result is

```text
copositive=true
strictly_copositive=false
```

Thus $x^\top A x$ is never negative when $x \geq 0$, but it is zero for at least one nonzero nonnegative vector.

To ask only one question:

```bash
./coposit --mode non-strict '2#1,-1,1'
./coposit --mode strict '2#1,-1,1'
```

These commands print one Boolean value and stop as soon as that predicate is decided.

## Input Formats

The command accepts a matrix directly, from a file, or from standard input:

```bash
./coposit '2#1,-1,1'
./coposit matrix.mtx
./coposit -
```

Two exact text formats are supported:

1. **Matrix Market:** symmetric `array` and `coordinate` matrices stored in a `.mtx` file. Integer, real, pattern, and complex fields
   with zero imaginary parts are accepted.
2. **Compact format:** `dimension#values`, followed by the upper triangle in row-major order. For example, `2#1,-1,1` stores
   $a_{11}=1$, $a_{12}=-1$, and $a_{22}=1$.

Compact values may be integers, decimals, scientific notation such as `2.5E15`, or fractions such as `-1/2`. A shorter
circular-symmetric form is also accepted: the common diagonal followed by one value for each circular distance. Compact matrix text
contains no spaces.

Decimal and fractional inputs are converted exactly. They are not rounded to binary floating point before solving.

## Timeouts And Diagnostics

Set a hard wall-clock limit with `--timeout`:

```bash
./coposit --timeout 30 matrix.mtx
```

A timeout is unresolved, not a negative answer. The command prints no Boolean result and exits with status `124`.

Use `--diagnostics` to see what a longer run is doing:

```bash
./coposit --diagnostics --timeout 30 matrix.mtx
```

Diagnostics are written to standard error about once per second. They describe the current phase and work counters; they are not an
estimate of the remaining time.

## Python Interface

`check()` runs the production solver and checks both properties by default:

```python
from pycoposit import check

result = check("2#1,-1,1")

print(result["is_copositive"])           # True
print(result["is_strictly_copositive"])  # False
```

It accepts compact matrix text, inline Matrix Market text, or a file path. To check only one property:

```python
cp = check("matrix.mtx", mode="copositive")
scp = check("matrix.mtx", mode="strictly_copositive")
```

The result is a dictionary containing the requested answers and basic execution metadata. A property that was not requested is
`None`. The batch, multiprocessing, and explicit-model research interfaces are documented in [python/README.md](python/README.md).

## Build From Source

Required: CMake 3.18 or newer, C and C++17 compilers, Python 3.11 or newer, FLINT, MPFR, and GMP.

For the production solver and command line only:

```bash
cmake -S cpp -B cpp/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCOPOSIT_BUILD_EXPERIMENTS=OFF \
  -DCOPOSIT_BUILD_TESTS=OFF \
  -DCOPOSIT_BUILD_PYTHON=OFF
cmake --build cpp/build-release --parallel
cpp/build-release/coposit '2#1,-1,1'
```

For the complete research build and test suite:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```

The complete build also fetches the dependencies used only by particular research models.

## C++ Interface

Add `cpp/` as a CMake subdirectory, link `coposit::coposit`, and include `<coposit/coposit.hpp>`:

```cmake
add_subdirectory(path/to/coposit/cpp coposit-build)
target_link_libraries(my_program PRIVATE coposit::coposit)
```

```cpp
#include <coposit/coposit.hpp>

const coposit::copositivity_result result = coposit::check(matrix);
```

The default checks both properties. Pass `coposit::copositivity_mode::copositive` or
`coposit::copositivity_mode::strictly_copositive` to request one. Direct C++ input must be a nonempty square symmetric
`coposit::matrix_integer`; the public function validates that boundary.

## Research Models

An ordinary user does not select a model. Production builds expose only `coposit`.

A complete source build exposes the retained baselines and experiments through the explicit `--model` research interface:

```bash
cpp/build/coposit --model hadeler_1983 --mode both matrix.mtx
cpp/build/coposit --model bundfuss_2008 --mode strict --preprocessing off --timeout 30 matrix.mtx
```

`--preprocessing off` is intended for controlled algorithm comparisons. Normal use should leave the exact preprocessing pipeline on.
See [the research CLI documentation](docs/ANALYSIS_CLI.md) for model availability, output contracts, and diagnostics.

## Further Documentation

- [Public documentation website](https://reinhardullrich.github.io/coposit/)
- [Python API and batch processing](python/README.md)
- [Research command-line interface](docs/ANALYSIS_CLI.md)
- [Diagnostics](docs/DIAGNOSTICS_REPORTING.md)
- [Release procedure](aidocs/RELEASING.md)
- [Technical documentation index](aidocs/INDEX.md)

Each maintained model has an `ALGORITHM.md` beside its implementation. These files explain the mathematics and decision flow of the
individual solver rather than ordinary installation and usage.

## Correctness And Failure Handling

Final classifications and certificates are accepted only after exact verification. Floating-point calculations may guide a search,
but they do not decide copositivity.

Invalid input, a timeout, a node limit, or another unresolved resource failure returns an error instead of silently reporting
`false`.

## License

coposit is free software licensed under [GPL-3.0-or-later](LICENSE). Prebuilt releases statically link third-party libraries; their
licenses and source locations are recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
