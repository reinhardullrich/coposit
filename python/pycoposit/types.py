"""Public pycoposit data types and validation."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
import os
from typing import Any, Literal

Algorithm = Literal[
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "adaptive_dutour_danninger",
    "adaptive_dutour_copomatrix",
    "adaptive_sponsel_copomatrix",
    "adaptive_zischg_sponsel_copomatrix",
    "hadeler_1983",
    "dickinson_2019",
    "support_pruned_dickinson",
    "nullity_support_pruned_dickinson",
    "rhs_dickinson",
    "frank_wolfe_dickinson",
    "one_step_frank_wolfe_dickinson",
    "pairwise_frank_wolfe_dickinson",
    "support_polished_frank_wolfe_dickinson",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
    "frank_wolfe_sponsel",
    "fracessa",
    "zischg_hadeler",
    "zischg_dickinson",
    "zischg_fracessa",
]

CopositivityMode = Literal["copositive", "strictly_copositive", "both"]
COPOSITIVITY_MODES: tuple[CopositivityMode, ...] = ("copositive", "strictly_copositive", "both")
Preprocessing = Literal["none", "connected_components", "pre_checks", "both"]
PREPROCESSING_MODES: tuple[Preprocessing, ...] = ("none", "connected_components", "pre_checks", "both")

COMBINED_CLASSIFICATION_ALGORITHMS: tuple[Algorithm, ...] = (
    "danninger_1990",
    "hadeler_1983",
    "dickinson_2019",
)

ALGORITHMS: tuple[Algorithm, ...] = (
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "adaptive_dutour_danninger",
    "adaptive_dutour_copomatrix",
    "adaptive_sponsel_copomatrix",
    "adaptive_zischg_sponsel_copomatrix",
    "hadeler_1983",
    "dickinson_2019",
    "support_pruned_dickinson",
    "nullity_support_pruned_dickinson",
    "rhs_dickinson",
    "frank_wolfe_dickinson",
    "one_step_frank_wolfe_dickinson",
    "pairwise_frank_wolfe_dickinson",
    "support_polished_frank_wolfe_dickinson",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
    "frank_wolfe_sponsel",
    "fracessa",
    "zischg_hadeler",
    "zischg_dickinson",
    "zischg_fracessa",
)


def _validate_algorithm(algorithm: str) -> None:
    if not isinstance(algorithm, str):
        raise TypeError("algorithm must be a str")
    if algorithm not in ALGORITHMS:
        raise ValueError(f"algorithm must be one of: {', '.join(ALGORITHMS)}")


def _validate_mode(mode: str) -> None:
    if not isinstance(mode, str):
        raise TypeError("mode must be a str")
    if mode not in COPOSITIVITY_MODES:
        raise ValueError(f"mode must be one of: {', '.join(COPOSITIVITY_MODES)}")


def _validate_preprocessing(preprocessing: str) -> None:
    if not isinstance(preprocessing, str):
        raise TypeError("preprocessing must be a str")
    if preprocessing not in PREPROCESSING_MODES:
        raise ValueError(f"preprocessing must be one of: {', '.join(PREPROCESSING_MODES)}")


class StatusCode(IntEnum):
    """Status values returned by native matrix computation."""

    OK = 0
    PARSE_ERROR = 1
    EXEC_ERROR = 4
    TIMEOUT = 5
    NODE_LIMIT = 6
    INTERNAL_ERROR = 255


@dataclass(slots=True)
class Matrix:
    """One matrix submitted for analysis."""

    matrix_id: int
    matrix: str
    metadata: dict[str, Any] | None = None

    def __post_init__(self) -> None:
        if type(self.matrix_id) is not int:
            raise TypeError("Matrix.matrix_id must be an int")
        if not -(1 << 63) <= self.matrix_id < (1 << 63):
            raise ValueError("Matrix.matrix_id must fit in a signed 64-bit integer")
        if not isinstance(self.matrix, str):
            raise TypeError("Matrix.matrix must be a str")
        if self.metadata is not None and not isinstance(self.metadata, dict):
            raise TypeError("Matrix.metadata must be a dict or None")


@dataclass(slots=True)
class MPConfig:
    """Process scheduling for :func:`run_multiprocessing`."""

    workers: int = max(1, getattr(os, "process_cpu_count", os.cpu_count)() or 1)
    prefetch_per_worker: int = 128
    queue_maxsize: int = 4096
    start_method: Literal["spawn", "forkserver", "fork"] = "spawn"

    def __post_init__(self) -> None:
        if self.workers < 1:
            raise ValueError("MPConfig.workers must be >= 1")
        if self.prefetch_per_worker < 1:
            raise ValueError("MPConfig.prefetch_per_worker must be >= 1")
        if self.queue_maxsize < 1:
            raise ValueError("MPConfig.queue_maxsize must be >= 1")
