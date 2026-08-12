"""Python analysis interface for exact copositivity models and preprocessing experiments."""

from .core import compute_matrix
from .mp import run_multiprocessing
from .single import run
from .types import (
    ALGORITHMS,
    COMBINED_CLASSIFICATION_ALGORITHMS,
    COPOSITIVITY_MODES,
    PREPROCESSING_MODES,
    CopositivityMode,
    MPConfig,
    Matrix,
    Preprocessing,
    StatusCode,
)

__all__ = [
    "ALGORITHMS",
    "COMBINED_CLASSIFICATION_ALGORITHMS",
    "COPOSITIVITY_MODES",
    "PREPROCESSING_MODES",
    "CopositivityMode",
    "Preprocessing",
    "StatusCode",
    "Matrix",
    "MPConfig",
    "compute_matrix",
    "run",
    "run_multiprocessing",
]
