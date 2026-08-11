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


def _matrix_cli_string(matrix: Matrix) -> str:
    text = matrix.matrix.strip()
    if text.startswith("file:"):
        if not matrix.metadata or "dimension" not in matrix.metadata or "base_directory" not in matrix.metadata:
            raise ValueError("file references require Matrix.metadata['dimension'] and ['base_directory']")
        dimension = matrix.metadata["dimension"]
        if type(dimension) is not int or dimension < 1:
            raise TypeError("Matrix.metadata['dimension'] must be a positive int")
        reference = Path(text.removeprefix("file:").strip())
        if not reference.parts or reference.is_absolute() or ".." in reference.parts:
            raise ValueError("matrix file reference must be a safe relative path")
        base_directory = Path(matrix.metadata["base_directory"]).resolve()
        path = (base_directory / reference).resolve()
        if not path.is_relative_to(base_directory):
            raise ValueError("matrix file reference escapes its base directory")
        return _read_matrix_market(path, dimension)
    if "#" in text:
        return text
    if not matrix.metadata or "dimension" not in matrix.metadata:
        raise ValueError("Matrix.metadata['dimension'] is required when Matrix.matrix has no 'dimension#' prefix")
    dimension = matrix.metadata["dimension"]
    if type(dimension) is not int:
        raise TypeError("Matrix.metadata['dimension'] must be an int")
    return f"{dimension}#{text}"


def _read_matrix_market(path: Path, expected_dimension: int) -> str:
    """Read an exact symmetric integer Matrix Market array or coordinate file."""

    with path.open("r", encoding="ascii") as source:
        try:
            header = next(source).strip().lower().split()
        except StopIteration as error:
            raise ValueError(f"empty Matrix Market file: {path}") from error
        if (len(header) != 5 or header[:2] != ["%%matrixmarket", "matrix"] or header[2] not in ("array", "coordinate")
                or header[3:] != ["integer", "symmetric"]):
            raise ValueError(f"unsupported Matrix Market header in {path}")

        data = (
            (line_number, stripped.split())
            for line_number, line in enumerate(source, 2)
            if (stripped := line.strip()) and not stripped.startswith("%")
        )
        try:
            size_line, size = next(data)
        except StopIteration as error:
            raise ValueError(f"Matrix Market file has no size line: {path}") from error

        expected_size_fields = 2 if header[2] == "array" else 3
        if len(size) != expected_size_fields or any(not field.isascii() or not field.isdigit() for field in size):
            raise ValueError(f"invalid Matrix Market size at {path}:{size_line}")
        rows, columns = map(int, size[:2])
        if rows != expected_dimension or columns != expected_dimension:
            raise ValueError(
                f"Matrix Market size {rows}x{columns} does not match database dimension {expected_dimension} in {path}"
            )

        expected_values = expected_dimension * (expected_dimension + 1) // 2
        if header[2] == "array":
            values = []
            for line_number, fields in data:
                if len(fields) != 1 or not _is_exact_integer(fields[0]):
                    raise ValueError(f"invalid Matrix Market integer at {path}:{line_number}")
                values.append(fields[0])
            if len(values) != expected_values:
                raise ValueError(f"Matrix Market file {path} has {len(values)} values; expected {expected_values}")
        else:
            declared_entries = int(size[2])
            values = ["0"] * expected_values
            seen = bytearray(expected_values)
            actual_entries = 0
            for line_number, fields in data:
                if (len(fields) != 3 or any(not field.isascii() or not field.isdigit() for field in fields[:2])
                        or not _is_exact_integer(fields[2])):
                    raise ValueError(f"invalid Matrix Market coordinate at {path}:{line_number}")
                row, column = map(int, fields[:2])
                if not 1 <= column <= row <= expected_dimension:
                    raise ValueError(f"Matrix Market coordinate must lie in the lower triangle at {path}:{line_number}")
                zero_based_column = column - 1
                index = zero_based_column * expected_dimension - zero_based_column * (zero_based_column - 1) // 2 + row - column
                if seen[index]:
                    raise ValueError(f"duplicate Matrix Market coordinate at {path}:{line_number}")
                seen[index] = 1
                values[index] = fields[2]
                actual_entries += 1
            if actual_entries != declared_entries:
                raise ValueError(f"Matrix Market file {path} has {actual_entries} entries; expected {declared_entries}")

        return f"{expected_dimension}#{','.join(values)}"


def _is_exact_integer(token: str) -> bool:
    digits = token[1:] if token[:1] in ("+", "-") else token
    return bool(digits) and digits.isascii() and digits.isdigit()


def compute_matrix(
    algorithm: Algorithm,
    matrix: Matrix,
    mode: CopositivityMode = "strictly_copositive",
    preprocessing: Preprocessing = "none",
) -> dict:
    """Run one matrix through the selected native model."""

    _validate_algorithm(algorithm)
    _validate_mode(mode)
    _validate_preprocessing(preprocessing)
    native_result = load_native_module(algorithm).compute_matrix(_matrix_cli_string(matrix), mode, preprocessing)
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
        "metadata": matrix.metadata,
    }
