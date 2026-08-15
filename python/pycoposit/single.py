"""Run pycoposit matrices sequentially."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

from .core import compute_matrix
from .types import Algorithm, CopositivityMode, Matrix, Preprocessing, _validate_algorithm, _validate_mode, _validate_preprocessing


def _run_matrices(
    algorithm: Algorithm, matrices: Iterable[Matrix], mode: CopositivityMode, preprocessing: Preprocessing, progress: bool
) -> Iterator[dict]:
    for matrix in matrices:
        yield compute_matrix(algorithm, matrix, mode, preprocessing, progress)


def run(
    algorithm: Algorithm,
    matrices: Matrix | Iterable[Matrix],
    mode: CopositivityMode = "strictly_copositive",
    preprocessing: Preprocessing = "both",
    progress: bool = False,
) -> dict | Iterator[dict]:
    """Run one matrix immediately or lazily yield an iterable in input order."""

    _validate_algorithm(algorithm)
    _validate_mode(mode)
    _validate_preprocessing(preprocessing)
    if type(progress) is not bool:
        raise TypeError("progress must be a bool")
    if isinstance(matrices, Matrix):
        return compute_matrix(algorithm, matrices, mode, preprocessing, progress)
    return _run_matrices(algorithm, matrices, mode, preprocessing, progress)
