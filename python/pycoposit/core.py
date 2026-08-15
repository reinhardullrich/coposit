"""Adapt pycoposit objects to the model-specific native modules."""

from __future__ import annotations

from importlib import import_module
from pathlib import Path
import sys
import threading

from .types import Algorithm, CopositivityMode, Matrix, Preprocessing, _validate_algorithm, _validate_mode, _validate_preprocessing

_native_modules: dict[str, object] = {}
_native_lock = threading.Lock()


def _native_search_paths() -> tuple[Path, ...]:
    root = Path(__file__).resolve().parents[2]
    build = root / "cpp" / "build"
    return build, build / "Release", build / "RelWithDebInfo", build / "Debug"


def load_native_module(algorithm: Algorithm):
    """Load and cache the one-model native extension for ``algorithm``."""

    _validate_algorithm(algorithm)
    if algorithm in _native_modules:
        return _native_modules[algorithm]

    module_name = f"coposit_{algorithm}_core"
    with _native_lock:
        if algorithm in _native_modules:
            return _native_modules[algorithm]

        try:
            module = import_module(module_name)
        except ModuleNotFoundError:
            for path in _native_search_paths():
                if path.exists() and str(path) not in sys.path:
                    sys.path.insert(0, str(path))
                try:
                    module = import_module(module_name)
                    break
                except ModuleNotFoundError:
                    continue
            else:
                raise ModuleNotFoundError(f"Could not import '{module_name}'. Build the Python modules as described in README.md")

        _native_modules[algorithm] = module
        return module


def _is_compact_matrix_text(text: str) -> bool:
    index = 0
    while index < len(text) and "0" <= text[index] <= "9":
        index += 1
    return index > 0 and index < len(text) and text[index] == "#"


def matrix_parser_source(matrix: Matrix) -> tuple[str, bool]:
    """Return one Matrix as parser text or a direct filename."""

    text = matrix.matrix
    if _is_compact_matrix_text(text) or text.startswith("%%MatrixMarket"):
        return text, False
    return text, True


def compute_matrix(
    algorithm: Algorithm,
    matrix: Matrix,
    mode: CopositivityMode = "strictly_copositive",
    preprocessing: Preprocessing = "both",
    progress: bool = False,
    collect_certificate_joint_distribution: bool = False,
) -> dict:
    """Run one matrix through the selected native model."""

    _validate_algorithm(algorithm)
    _validate_mode(mode)
    _validate_preprocessing(preprocessing)
    if type(progress) is not bool:
        raise TypeError("progress must be a bool")
    if type(collect_certificate_joint_distribution) is not bool:
        raise TypeError("collect_certificate_joint_distribution must be a bool")
    matrix_source, is_file = matrix_parser_source(matrix)
    native_module = load_native_module(algorithm)
    native_compute = native_module.compute_matrix_file if is_file else native_module.compute_matrix
    native_arguments = (matrix_source, mode, preprocessing, progress)
    native_result = (
        native_compute(*native_arguments, True)
        if collect_certificate_joint_distribution
        else native_compute(*native_arguments)
    )
    return {
        "algorithm": algorithm,
        "mode": mode,
        "preprocessing": preprocessing,
        "matrix_id": matrix.matrix_id,
        "status": native_result["status"],
        "is_copositive": native_result["is_copositive"],
        "is_strictly_copositive": native_result["is_strictly_copositive"],
        "elapsed_ns": native_result["elapsed_ns"],
        "error_message": native_result["error_message"],
        "diagnostics": native_result.get("diagnostics"),
        "certificate_joint_distribution": native_result.get("certificate_joint_distribution"),
    }
