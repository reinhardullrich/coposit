# C++ Analysis Interface

`coposit-analyze` is the expert command for selecting one comparison model and controlling preprocessing. Its inventory is the
literature baselines, with `dickinson_final` replacing `dickinson_2019`, plus the selected `adaptive_sponsel_copomatrix`. Experimental
variants remain in the Python and reference-run interfaces. The normal `coposit` command exposes only the fixed `fast` and `safe`
methods.

## Basic Call

```bash
cpp/build/coposit-analyze --model bundfuss_2008 --mode strict '2#1,0,1'
```

The complete shape is:

```text
coposit-analyze --model MODEL --mode strict|non-strict|both [OPTIONS] [MATRIX|FILE|-]
```

`MATRIX`, `FILE`, and `-` follow the same rules as the normal command: compact `dimension#values`, a relative or absolute matrix-file
path, or standard input. `--progress` reports preprocessing and model progress on standard error.

`--timeout SECONDS` places a hard wall-clock limit on the entire command, including parsing, preprocessing, and model execution.
Seconds must be positive and may be fractional. A timeout prints no Boolean result and exits with status `124`.

Single-predicate calls print `true` or `false`. Combined calls print both named results:

```text
copositive=true
strictly_copositive=false
```

## Preprocessing Controls

Connected components, the pre-check stage, every individual pre-check, and principal submatrices through cardinality three are on by
default. They can be changed independently:

```bash
cpp/build/coposit-analyze \
  --model dickinson_final \
  --mode strict \
  --connected-components off \
  --pre-checks on \
  --pre-check frank-wolfe off \
  --pre-check positive-definiteness off \
  --principal-submatrices-up-to 2 \
  matrix.mtx
```

The switches are:

| Option | Values | Meaning |
|---|---|---|
| `--connected-components` | `on`, `off` | Enable or disable negative-entry connected-component decomposition |
| `--pre-checks` | `on`, `off` | Enable or disable the complete pre-check stage |
| `--pre-check NAME` | `on`, `off` | Enable or disable one named pre-check; the option may be repeated |
| `--principal-submatrices-up-to` | `1`, `2`, `3` | Highest cardinality checked when principal-submatrix checking is enabled |
| `--timeout` | positive seconds | Terminate the complete command at the wall-clock deadline |

The accepted pre-check names are:

- `small-dimension`
- `principal-submatrices`
- `nonnegative-off-diagonal`
- `negative-part-diagonal-dominance`
- `all-ones`
- `frank-wolfe`
- `positive-definiteness`

Turning the entire pre-check stage off leaves individual settings stored but inactive. Turning connected components off runs the
selected pre-checks on the original matrix instead of component matrices.

## Model Capabilities

All nine selectable models accept both `--mode strict` and `--mode non-strict`:

- `dutour_2018`
- `danninger_1990`
- `copomatrix_2011`
- `adaptive_sponsel_copomatrix`
- `hadeler_1983`
- `dickinson_final`
- `safi_2021`
- `bundfuss_2008`
- `sponsel_2012`

The following implement `--mode both` as one combined traversal:

- `danninger_1990`
- `hadeler_1983`
- `dickinson_final`

`coposit-analyze --help` lists the complete nine-model inventory. An excluded experimental model or unsupported combined mode is an
explicit error rather than a fallback or a second hidden run.

## Process Boundary

`coposit-analyze` is the only user-facing C++ model-selection command. It dispatches to an adjacent internal one-model companion so
the selected algorithm remains independently linked; no executable links several solver implementations or uses a runtime solver
registry. The internal companions are implementation details and are not the documented interface.
When no timeout is requested, the launcher directly replaces itself with that companion. A timed call alone keeps the launcher as a
small supervisor and starts one sleeping watchdog; therefore untimed analysis retains the previous execution cost.
