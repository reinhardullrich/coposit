"""pycoposit analysis data types and validation."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
import os
from typing import Literal

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
    "dense_bitset_dickinson",
    "interval_recursive_dickinson",
    "bdd_dickinson",
    "zdd_dickinson",
    "cbdd_dickinson",
    "cbdd_halfspace_dickinson",
    "upper_endpoint_cbdd_dickinson",
    "cbdd_dickinson_improved_1",
    "wide_certificate_cbdd_dickinson",
    "multithreaded_cbdd_dickinson",
    "ceiling_pruned_dickinson",
    "kernel_cone_dickinson",
    "affine_companion_dickinson",
    "layered_singular_lift_dickinson",
    "breadth_first_singular_lift_dickinson",
    "czdd_dickinson",
    "sat_dickinson",
    "sat_halfspace_dickinson",
    "sat_halfspace_rays_dickinson",
    "sat_halfspace_lp_dickinson",
    "sat_halfspace_milp_dickinson",
    "sat_halfspace_rays_lookahead_dickinson",
    "sat_halfspace_rays_wide_dickinson",
    "wide_certificate_sat_dickinson",
    "clingo_dickinson",
    "clingo_halfspace_dickinson",
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
Preprocessing = Literal["none", "both"]
PREPROCESSING_MODES: tuple[Preprocessing, ...] = ("none", "both")

COMBINED_CLASSIFICATION_ALGORITHMS: tuple[Algorithm, ...] = (
    "danninger_1990",
    "hadeler_1983",
    "dickinson_2019",
    "dense_bitset_dickinson",
    "interval_recursive_dickinson",
    "bdd_dickinson",
    "zdd_dickinson",
    "cbdd_dickinson",
    "cbdd_halfspace_dickinson",
    "upper_endpoint_cbdd_dickinson",
    "cbdd_dickinson_improved_1",
    "wide_certificate_cbdd_dickinson",
    "multithreaded_cbdd_dickinson",
    "ceiling_pruned_dickinson",
    "kernel_cone_dickinson",
    "affine_companion_dickinson",
    "layered_singular_lift_dickinson",
    "breadth_first_singular_lift_dickinson",
    "czdd_dickinson",
    "sat_dickinson",
    "sat_halfspace_dickinson",
    "sat_halfspace_rays_dickinson",
    "sat_halfspace_lp_dickinson",
    "sat_halfspace_milp_dickinson",
    "sat_halfspace_rays_lookahead_dickinson",
    "sat_halfspace_rays_wide_dickinson",
    "wide_certificate_sat_dickinson",
    "clingo_dickinson",
    "clingo_halfspace_dickinson",
    "support_pruned_dickinson",
    "nullity_support_pruned_dickinson",
    "rhs_dickinson",
    "frank_wolfe_dickinson",
    "one_step_frank_wolfe_dickinson",
    "pairwise_frank_wolfe_dickinson",
    "support_polished_frank_wolfe_dickinson",
    "fracessa",
    "zischg_hadeler",
    "zischg_dickinson",
    "zischg_fracessa",
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
    "dense_bitset_dickinson",
    "interval_recursive_dickinson",
    "bdd_dickinson",
    "zdd_dickinson",
    "cbdd_dickinson",
    "cbdd_halfspace_dickinson",
    "upper_endpoint_cbdd_dickinson",
    "cbdd_dickinson_improved_1",
    "wide_certificate_cbdd_dickinson",
    "multithreaded_cbdd_dickinson",
    "ceiling_pruned_dickinson",
    "kernel_cone_dickinson",
    "affine_companion_dickinson",
    "layered_singular_lift_dickinson",
    "breadth_first_singular_lift_dickinson",
    "czdd_dickinson",
    "sat_dickinson",
    "sat_halfspace_dickinson",
    "sat_halfspace_rays_dickinson",
    "sat_halfspace_lp_dickinson",
    "sat_halfspace_milp_dickinson",
    "sat_halfspace_rays_lookahead_dickinson",
    "sat_halfspace_rays_wide_dickinson",
    "wide_certificate_sat_dickinson",
    "clingo_dickinson",
    "clingo_halfspace_dickinson",
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

PARAMETERIZED_ALGORITHMS: tuple[Algorithm, ...] = (
    "sat_halfspace_rays_wide_dickinson",
    "wide_certificate_cbdd_dickinson",
    "wide_certificate_sat_dickinson",
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


def _resolve_mode(algorithm: Algorithm, mode: CopositivityMode | None) -> CopositivityMode:
    if mode is not None:
        _validate_mode(mode)
        return mode
    if algorithm in COMBINED_CLASSIFICATION_ALGORITHMS:
        return "both"
    raise ValueError(
        f"{algorithm} cannot classify both predicates in one traversal; run it once with --mode copositive and once with "
        "--mode strictly_copositive"
    )


def _validate_preprocessing(preprocessing: str) -> None:
    if not isinstance(preprocessing, str):
        raise TypeError("preprocessing must be a str")
    if preprocessing not in PREPROCESSING_MODES:
        raise ValueError(f"preprocessing must be one of: {', '.join(PREPROCESSING_MODES)}")


def _resolve_model_parameter(algorithm: Algorithm, model_parameter: str | None) -> str | None:
    if model_parameter is not None and not isinstance(model_parameter, str):
        raise TypeError("model_parameter must be a str or None")
    if algorithm in PARAMETERIZED_ALGORITHMS:
        if model_parameter is None:
            raise ValueError(f"{algorithm} requires model_parameter")
        value = model_parameter
        if not value.isascii() or not value.isdecimal() or not 0 <= int(value) <= 100:
            raise ValueError("wide-certificate model_parameter must be an integer percentage from 0 through 100")
        return str(int(value))
    if model_parameter is not None:
        raise ValueError(f"{algorithm} does not accept a model parameter")
    return None


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
    """Matrix text or path with an optional correlation ID."""

    matrix: str
    matrix_id: int | None = None

    def __post_init__(self) -> None:
        if not isinstance(self.matrix, str):
            raise TypeError("Matrix.matrix must be a str")
        if self.matrix_id is not None:
            if type(self.matrix_id) is not int:
                raise TypeError("Matrix.matrix_id must be an int or None")
            if not -(1 << 63) <= self.matrix_id < (1 << 63):
                raise ValueError("Matrix.matrix_id must fit in a signed 64-bit integer")


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
