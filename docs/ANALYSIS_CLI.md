# C++ Analysis Interface

`coposit-analyze` is the expert command for selecting one comparison model and enabling or bypassing preprocessing. Its inventory is the
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

The complete fixed preprocessing pipeline is on by default. It has one switch:

```bash
cpp/build/coposit-analyze \
  --model dickinson_final \
  --mode strict \
  --preprocessing off \
  matrix.mtx
```

| Option | Values | Meaning |
|---|---|---|
| `--preprocessing` | `on`, `off` | Run the complete fixed pipeline or call the selected model directly |
| `--timeout` | positive seconds | Terminate the complete command at the wall-clock deadline |

When enabled, the pipeline always performs its root checks, negative-entry component split, ordinary component checks, bounded
Danninger reduction, and bounded COPOMATRIX reduction in that order. The `z-matrix` check examines maximal principal blocks
with nonpositive off-diagonal entries. An indefinite block rejects both modes, while a singular positive-semidefinite block rejects
only strict copositivity; Motzkin–Straus graph matrices bypass this check because maximal-clique enumeration is unproductive there.
Danninger and COPOMATRIX each run only when the selected pivot creates at most two order-reduced children. Every child repeats scan,
root checks, component splitting, and ordinary checks. Children may create grandchildren because the internal maximum reduction
depth is two; nodes at that depth create no further descendants and call no model. This depth is not a CLI option. If preprocessing
remains unresolved, the selected model receives the unchanged original matrix. See
[`../aidocs/PREPROCESSING_PIPELINE_DESIGN.md`](../aidocs/PREPROCESSING_PIPELINE_DESIGN.md) for the complete flow.

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
