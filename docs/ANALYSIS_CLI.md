# `coposit` Analysis Interface

`coposit` is the command-line interface. Production builds select the current solver internally and do not accept `--model`. A
complete source research build adds `--model` for selecting any built literature baseline or experiment. Python calls this same
command; there is no separate native-extension model path.

## Basic Call

```bash
cpp/build/coposit --mode both '2#1,-1,1'
```

The complete shape is:

```text
coposit [--model MODEL] [--mode strict|non-strict|both] [OPTIONS] [MATRIX|FILE|-]
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

In a complete research build, omitting `--model` selects the current production solver without exposing its identity as part of the
interface. Omitting `--mode` selects `both` for a model with one-pass combined classification. A model without that capability
requires an explicit `strict` or `non-strict` mode and fails before reading the matrix if it is omitted. A selected predicate stops
once it is known; `both` continues until both predicates are known.

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
only strict copositivity. After Frank–Wolfe, one center-started heuristic KKT walk uses floating-point path selection but verifies
every proposed negative value and terminal KKT point exactly; a floating/exact KKT disagreement switches that walk to exact arithmetic.
Exact Motzkin–Straus graph matrices then take the complete maximum-clique classifier before matrix
factorization. Every other component first factorizes its original matrix. If that matrix has no positive off-diagonal entry, this
one factorization is already the complete Z-matrix decision. Otherwise its negative part is factorized, and maximal-Z is reached only
when both complete-matrix checks leave it useful.
Danninger and COPOMATRIX each run only when the selected pivot creates at most two order-reduced children. Every child repeats scan,
root checks, component splitting, and ordinary checks. Children may create grandchildren because the internal maximum reduction
depth is two; nodes at that depth create no further descendants and call no model. This depth is not a CLI option. If preprocessing
remains unresolved, the selected model receives only the unresolved connected-component matrices. Inconclusive Danninger and
COPOMATRIX descendants are discarded, so their unchanged parent component is retained. See
[`../aidocs/PREPROCESSING_PIPELINE_DESIGN.md`](../aidocs/PREPROCESSING_PIPELINE_DESIGN.md) for the complete flow.

## Model Capabilities

`coposit --help` lists models only in a complete research build. That build lists every baseline and experiment. Production builds
do not expose model selection. Every Dickinson-, Hadeler-, and FracESSA-based model plus
Danninger 1990 implements `--mode both` as one combined traversal and defaults to it when `--mode` is omitted. Other models require
an explicit implemented predicate mode. The parameterized wide-certificate models require an integer percentage from 0 through 100:

```bash
cpp/build/coposit --model wide_certificate_cbdd_dickinson --model-parameter 90 matrix.mtx
```

An unknown model, unsupported combined mode, missing mode, or missing required model parameter is an explicit error rather than a
fallback or a second hidden run.

## Process Boundary

`coposit` dispatches to an internal engine in a production build and to an isolated one-model companion in a complete research
build. This keeps algorithms independently linked; no executable links several solver implementations or uses a runtime solver
registry. The engine and research companions are implementation details, not user interfaces.
When no timeout is requested, the launcher directly replaces itself with that companion. A timed call alone keeps the launcher as a
small supervisor and starts one sleeping watchdog; therefore untimed analysis retains the previous execution cost.
