"""Adapt pycoposit objects to the sole low-level ``coposit`` interface."""

from __future__ import annotations

import ast
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import threading
import time
from typing import Callable

from .types import (
    Algorithm,
    CopositivityMode,
    Matrix,
    Preprocessing,
    StatusCode,
    _resolve_mode,
    _resolve_model_parameter,
    _validate_algorithm,
    _validate_preprocessing,
)

_execution_lock = threading.Lock()
_state_lock = threading.Lock()
_active_process: subprocess.Popen[bytes] | None = None
_timeout_requested = False
_diagnostic_text = ""
_certificate_distribution: list[list[int]] = []


def _binary_search_paths() -> tuple[Path, ...]:
    root = Path(__file__).resolve().parents[2]
    build = root / "cpp" / "build"
    return Path(__file__).resolve().parent, build, build / "Release", build / "RelWithDebInfo", build / "Debug"


def coposit_path() -> Path:
    """Return the configured or built ``coposit`` launcher."""

    configured = os.environ.get("COPOSIT")
    if configured:
        path = Path(configured).expanduser().resolve()
        if path.is_file():
            return path
        raise FileNotFoundError(f"COPOSIT does not name a file: {path}")
    for directory in _binary_search_paths():
        candidate = directory / ("coposit.exe" if os.name == "nt" else "coposit")
        if candidate.is_file():
            return candidate
    installed = shutil.which("coposit")
    if installed:
        return Path(installed).resolve()
    raise FileNotFoundError("Could not find coposit. Build it as described in README.md or set COPOSIT")


def model_companion_path(algorithm: Algorithm) -> Path:
    """Return the exact isolated model companion selected by the launcher."""

    _validate_algorithm(algorithm)
    launcher = coposit_path()
    suffix = ".exe" if os.name == "nt" else ""
    companion = launcher.with_name(f"coposit-{algorithm}{suffix}")
    if not companion.is_file():
        raise FileNotFoundError(f"Could not find model companion for {algorithm}: {companion}")
    return companion


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


def _parse_distribution(line: str) -> list[list[int]] | None:
    marker = "_counts=["
    start = line.find("certificate_")
    marker_at = line.find(marker, start)
    if start < 0 or marker_at < 0:
        return None
    end = line.find("]", marker_at + len(marker))
    if end < 0:
        return None
    try:
        parsed = ast.literal_eval("[" + line[marker_at + len(marker):end] + "]")
    except (SyntaxError, ValueError):
        return None
    if not isinstance(parsed, list):
        return None
    result: list[list[int]] = []
    for entry in parsed:
        if not isinstance(entry, tuple) or len(entry) not in (3, 4, 5) or not all(type(value) is int for value in entry):
            return None
        result.append(list(entry))
    return result


def _record_diagnostic_line(line: str) -> None:
    global _diagnostic_text, _certificate_distribution
    distribution = _parse_distribution(line)
    with _state_lock:
        _diagnostic_text += line + "\n"
        if distribution is not None:
            _certificate_distribution = distribution


def diagnostics_snapshot() -> dict:
    """Return the latest diagnostics emitted by the active analyzer call."""

    with _state_lock:
        return {
            "diagnostics": _diagnostic_text,
            "certificate_joint_distribution": [entry.copy() for entry in _certificate_distribution],
        }


def reset_timeout() -> None:
    global _timeout_requested, _diagnostic_text, _certificate_distribution
    _timeout_requested = False
    with _state_lock:
        _diagnostic_text = ""
        _certificate_distribution = []


def terminate_active_process() -> None:
    """Kill the current model companion when its Python worker is being replaced."""

    process = _active_process
    if process is not None:
        try:
            process.kill()
        except ProcessLookupError:
            pass


def install_timeout_handler(signal_number: int) -> None:
    """Install the reference runner's signal handler and forward expiry to the active analyzer."""

    def request_timeout(_signal_number, _frame) -> None:
        global _timeout_requested
        _timeout_requested = True
        process = _active_process
        if process is not None:
            try:
                process.send_signal(signal.SIGUSR1) if hasattr(signal, "SIGUSR1") else process.terminate()
            except ProcessLookupError:
                pass

    signal.signal(signal_number, request_timeout)


def _parse_machine_output(output: bytes) -> dict[str, object]:
    fields: dict[str, str] = {}
    for raw_line in output.decode("utf-8", errors="strict").splitlines():
        key, separator, value = raw_line.partition("=")
        if not separator or key in fields:
            raise ValueError("coposit returned malformed machine output")
        fields[key] = value
    required = {
        "coposit_result",
        "status",
        "is_copositive",
        "is_strictly_copositive",
        "elapsed_ns",
        "error_message_hex",
        "diagnostics_hex",
        "certificate_joint_distribution",
    }
    if fields.keys() != required or fields["coposit_result"] != "1":
        raise ValueError("coposit returned an incomplete machine result")

    def optional_bool(name: str) -> bool | None:
        value = int(fields[name])
        if value == -1:
            return None
        if value in (0, 1):
            return bool(value)
        raise ValueError(f"coposit returned invalid {name}")

    distribution = json.loads(fields["certificate_joint_distribution"])
    if not isinstance(distribution, list):
        raise ValueError("coposit returned an invalid certificate distribution")
    return {
        "status": int(fields["status"]),
        "is_copositive": optional_bool("is_copositive"),
        "is_strictly_copositive": optional_bool("is_strictly_copositive"),
        "elapsed_ns": int(fields["elapsed_ns"]),
        "error_message": bytes.fromhex(fields["error_message_hex"]).decode("utf-8"),
        "diagnostics": bytes.fromhex(fields["diagnostics_hex"]).decode("utf-8"),
        "certificate_joint_distribution": distribution,
    }


def _execute_coposit(
    command: list[str], standard_input: bytes | None, *, mirror_diagnostics: bool, stream_diagnostics: bool
) -> dict[str, object]:
    global _active_process, _diagnostic_text, _certificate_distribution
    reset_timeout()
    started = time.monotonic_ns()
    process = subprocess.Popen(
        command,
        stdin=subprocess.PIPE if standard_input is not None else subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    _active_process = process
    stdout_parts: list[bytes] = []

    def read_stdout() -> None:
        assert process.stdout is not None
        stdout_parts.append(process.stdout.read())

    def read_stderr() -> None:
        assert process.stderr is not None
        while raw_line := process.stderr.readline():
            line = raw_line.decode("utf-8", errors="replace").rstrip("\r\n")
            if line:
                _record_diagnostic_line(line)
            if mirror_diagnostics:
                sys.stderr.buffer.write(raw_line)
                sys.stderr.buffer.flush()

    stdout_thread = threading.Thread(target=read_stdout, name="coposit-stdout", daemon=True)
    stderr_thread = threading.Thread(target=read_stderr, name="coposit-stderr", daemon=True)
    stdout_thread.start()
    stderr_thread.start()
    try:
        if standard_input is not None:
            assert process.stdin is not None
            try:
                process.stdin.write(standard_input)
                process.stdin.close()
            except BrokenPipeError:
                pass  # The companion can reject options before reading matrix input.
        return_code = process.wait()
    finally:
        _active_process = None
        stdout_thread.join()
        stderr_thread.join()
        for stream in (process.stdin, process.stdout, process.stderr):
            if stream is not None:
                try:
                    stream.close()
                except BrokenPipeError:
                    pass

    elapsed_ns = time.monotonic_ns() - started
    if _timeout_requested:
        if return_code == 0 and stdout_parts:
            parsed = _parse_machine_output(b"".join(stdout_parts))
            if int(parsed["status"]) == int(StatusCode.TIMEOUT):
                with _state_lock:
                    _diagnostic_text = str(parsed["diagnostics"])
                    _certificate_distribution = [list(entry) for entry in parsed["certificate_joint_distribution"]]
                return parsed
        return {
            "status": int(StatusCode.TIMEOUT),
            "is_copositive": None,
            "is_strictly_copositive": None,
            "elapsed_ns": elapsed_ns,
            "error_message": "",
            **diagnostics_snapshot(),
        }
    if return_code != 0:
        return {
            "status": int(StatusCode.INTERNAL_ERROR),
            "is_copositive": None,
            "is_strictly_copositive": None,
            "elapsed_ns": elapsed_ns,
            "error_message": f"coposit exited with status {return_code}",
            **diagnostics_snapshot(),
        }
    parsed = _parse_machine_output(b"".join(stdout_parts))
    if stream_diagnostics:
        with _state_lock:
            _diagnostic_text = str(parsed["diagnostics"])
            _certificate_distribution = [list(entry) for entry in parsed["certificate_joint_distribution"]]
    return parsed


def compute_matrix(
    algorithm: Algorithm,
    matrix: Matrix,
    mode: CopositivityMode | None = None,
    preprocessing: Preprocessing = "both",
    diagnostics: bool = False,
    collect_certificate_joint_distribution: bool = False,
    model_parameter: str | None = None,
    *,
    _stream_diagnostics: bool = False,
) -> dict:
    """Run one matrix through ``coposit``."""

    _validate_algorithm(algorithm)
    mode = _resolve_mode(algorithm, mode)
    model_parameter = _resolve_model_parameter(algorithm, model_parameter)
    _validate_preprocessing(preprocessing)
    if type(diagnostics) is not bool:
        raise TypeError("diagnostics must be a bool")
    if type(collect_certificate_joint_distribution) is not bool:
        raise TypeError("collect_certificate_joint_distribution must be a bool")

    command = [
        str(coposit_path()),
        "--model",
        algorithm,
        "--mode",
        {"copositive": "non-strict", "strictly_copositive": "strict", "both": "both"}[mode],
        "--preprocessing",
        "on" if preprocessing == "both" else "off",
        "--machine",
    ]
    collect = collect_certificate_joint_distribution or diagnostics or _stream_diagnostics
    if collect:
        command.append("--collect-diagnostics")
    if diagnostics or _stream_diagnostics:
        command.append("--diagnostics")
    if model_parameter is not None:
        command.extend(("--model-parameter", model_parameter))

    matrix_source, is_file = matrix_parser_source(matrix)
    standard_input = None if is_file else matrix_source.encode()
    command.append(matrix_source if is_file else "-")
    with _execution_lock:
        native_result = _execute_coposit(
            command, standard_input, mirror_diagnostics=diagnostics, stream_diagnostics=_stream_diagnostics
        )
    return {
        "algorithm": algorithm,
        "mode": mode,
        "preprocessing": preprocessing,
        "model_parameter": model_parameter,
        "matrix_id": matrix.matrix_id,
        **native_result,
    }
