# `coposit` Analysis Interface

`coposit` is the sole low-level model-execution command. It selects any literature baseline or experiment and enables or bypasses
preprocessing. Python calls this same command; there is no separate native-extension model path and there are no `fast`, `safe`, or
implicit model aliases.

## Basic Call

```bash
cpp/build/coposit --model bundfuss_2008 --mode strict '2#1,0,1'
```

The complete shape is:

```text
coposit --model MODEL [--mode strict|non-strict|both] [OPTIONS] [MATRIX|FILE|-]
```

`MATRIX`, `FILE`, and `-` select compact `dimension#values`, a relative or absolute matrix-file
path, or standard input. `--diagnostics` reports preprocessing and model diagnostics on standard error.

`--timeout SECONDS` places a hard wall-clock limit on the entire command, including parsing, preprocessing, and model execution.
Seconds must be positive and may be fractional. A timeout prints no Boolean result and exits with status `124`.

Single-predicate calls print `true` or `false`. Combined calls print both named results:

```text
copositive=true
strictly_copositive=false
```

Omitting `--mode` selects `both` for a model with one-pass combined classification. A model without that capability requires an
explicit `strict` or `non-strict` mode and fails before reading the matrix if it is omitted.

## Preprocessing Controls

The complete fixed preprocessing pipeline is on by default. It has one switch:

```bash
cpp/build/coposit \
  --model dickinson_2019 \
  --mode strict \
  --preprocessing off \
  matrix.mtx
```

| Option | Values | Meaning |
|---|---|---|
| `--preprocessing` | `on`, `off` | Run the complete fixed pipeline or call the selected model directly |
| `--model-parameter` | model-specific string | Configure a parameterized model; other models reject it |
| `--timeout` | positive seconds | Terminate the complete command at the wall-clock deadline |

When enabled, the pipeline always performs its root checks, negative-entry component split, ordinary component checks, bounded
Danninger reduction, and bounded COPOMATRIX reduction in that order. The `z-matrix` check examines maximal principal blocks
with nonpositive off-diagonal entries. An indefinite block rejects both modes, while a singular positive-semidefinite block rejects
only strict copositivity; Motzkin–Straus graph matrices bypass this check because maximal-clique enumeration is unproductive there.
Danninger and COPOMATRIX each run only when the selected pivot creates at most two order-reduced children. Every child repeats scan,
root checks, component splitting, and ordinary checks. Children may create grandchildren because the internal maximum reduction
depth is two; nodes at that depth create no further descendants and call no model. This depth is not a CLI option. If preprocessing
remains unresolved, the selected model receives only the unresolved connected-component matrices. Inconclusive Danninger and
COPOMATRIX descendants are discarded, so their unchanged parent component is retained. See
[`../aidocs/PREPROCESSING_PIPELINE_DESIGN.md`](../aidocs/PREPROCESSING_PIPELINE_DESIGN.md) for the complete flow.

## Model Capabilities

`coposit --help` lists every currently built baseline and experiment. Every Dickinson-, Hadeler-, and FracESSA-based model plus
Danninger 1990 implements `--mode both` as one combined traversal and defaults to it when `--mode` is omitted. Other models require
an explicit implemented predicate mode. The parameterized wide-certificate models require an integer percentage from 0 through 100:

```bash
cpp/build/coposit --model wide_certificate_cbdd_dickinson --model-parameter 90 matrix.mtx
```

An unknown model, unsupported combined mode, missing mode, or missing required model parameter is an explicit error rather than a
fallback or a second hidden run.

## Process Boundary

`coposit` dispatches to an adjacent internal one-model companion so
the selected algorithm remains independently linked; no executable links several solver implementations or uses a runtime solver
registry. The internal companions are implementation details and are not the documented interface.
When no timeout is requested, the launcher directly replaces itself with that companion. A timed call alone keeps the launcher as a
small supervisor and starts one sleeping watchdog; therefore untimed analysis retains the previous execution cost.
