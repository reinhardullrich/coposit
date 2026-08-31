"""Run pycoposit matrices sequentially."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

from .core import compute_matrix
from .types import (
    Algorithm,
    CopositivityMode,
    INCUMBENT_MODEL,
    Matrix,
    Preprocessing,
    _resolve_mode,
    _resolve_model_parameter,
    _validate_algorithm,
    _validate_preprocessing,
)


def check(
    matrix: Matrix | str,
    mode: CopositivityMode = "both",
) -> dict:
    """Check one matrix with the current incumbent model."""

    if isinstance(matrix, str):
        matrix = Matrix(matrix)
    elif not isinstance(matrix, Matrix):
        raise TypeError("matrix must be a Matrix or str")
    return compute_matrix(INCUMBENT_MODEL, matrix, mode)


def _run_matrices(
    algorithm: Algorithm, matrices: Iterable[Matrix], mode: CopositivityMode, preprocessing: Preprocessing, diagnostics: bool,
    model_parameter: str | None,
) -> Iterator[dict]:
    for matrix in matrices:
        yield compute_matrix(algorithm, matrix, mode, preprocessing, diagnostics, model_parameter=model_parameter)


def run(
    algorithm: Algorithm,
    matrices: Matrix | Iterable[Matrix],
    mode: CopositivityMode | None = None,
    preprocessing: Preprocessing = "both",
    diagnostics: bool = False,
    model_parameter: str | None = None,
) -> dict | Iterator[dict]:
    """Run one matrix immediately or lazily yield an iterable in input order."""

    _validate_algorithm(algorithm)
    mode = _resolve_mode(algorithm, mode)
    model_parameter = _resolve_model_parameter(algorithm, model_parameter)
    _validate_preprocessing(preprocessing)
    if type(diagnostics) is not bool:
        raise TypeError("diagnostics must be a bool")
    if isinstance(matrices, Matrix):
        return compute_matrix(algorithm, matrices, mode, preprocessing, diagnostics, model_parameter=model_parameter)
    return _run_matrices(algorithm, matrices, mode, preprocessing, diagnostics, model_parameter)
