#!/usr/bin/env python3
"""Run one model over selected corpus matrices on explicit CPUs and store reference results."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from multiprocessing import get_context
from multiprocessing.connection import Connection, wait as wait_for_connections
import os
from pathlib import Path
from queue import Empty, Full, Queue
import signal
import sqlite3
from threading import Event, Lock, Thread
from time import monotonic

from pycoposit import (
    ALGORITHMS,
    COPOSITIVITY_MODES,
    PREPROCESSING_MODES,
    Matrix,
    StatusCode,
    compute_matrix,
)
from pycoposit.core import diagnostics_snapshot, install_timeout_handler, model_companion_path, reset_timeout, terminate_active_process
from pycoposit.types import _resolve_mode as _resolve_api_mode, _resolve_model_parameter

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATABASE = REPOSITORY_ROOT / "testdata" / "copos_testdata.sqlite3"
DEFAULT_RESULTS_DATABASE = REPOSITORY_ROOT / "experiments" / "diagnostics.sqlite3"
DIAGNOSTICS_SCHEMA = REPOSITORY_ROOT / "testdata" / "diagnostics_schema.sql"
STARTUP_TIMEOUT_SECONDS = 30.0
TIMEOUT_GRACE_SECONDS = 1.0
TIMEOUT_SIGNAL = signal.SIGUSR1
DETAILED_DIAGNOSTICS_LIMIT = 100
DIAGNOSTICS_INTERVAL_SECONDS = 1.0
DATABASE_QUEUE_PER_WORKER = 2
MATRIX_SETS = (
    "smoke_set", "core_and_stress_test", "n_le_100", "n_gt_100_solved", "bpqy_benchmark", "bpqy_quick_test",
    "references_unsolved",
)


def _resolve_mode(model: str, requested_mode: str | None) -> str:
    return _resolve_api_mode(model, requested_mode)


def _result_model_id(model: str, model_parameter: str | None) -> str:
    return model if model_parameter is None else f"{model}@{model_parameter}"


RESULT_UPSERT_SQL = """INSERT INTO results (
       matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
       is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, diagnostics, certificate_joint_distribution, message
   ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
   ON CONFLICT(matrix_id, model_id, mode, preprocessing, binary_sha256) DO UPDATE SET
       status = excluded.status,
       is_copositive = excluded.is_copositive,
       is_strictly_copositive = excluded.is_strictly_copositive,
       elapsed_ns = excluded.elapsed_ns,
       timeout_ns = excluded.timeout_ns,
       recorded_at = excluded.recorded_at,
       diagnostics = excluded.diagnostics,
       certificate_joint_distribution = excluded.certificate_joint_distribution,
       message = excluded.message"""


def _initialize_results_database(database: Path) -> None:
    database.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(database) as connection:
        if not connection.execute("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'results'").fetchone():
            connection.executescript(DIAGNOSTICS_SCHEMA.read_text())


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _database_writer(results_database: Path, corpus_database: Path, parent_cpu: int, writes: Queue, errors: list[BaseException]) -> None:
    connection = None
    try:
        os.sched_setaffinity(0, {parent_cpu})
        connection = sqlite3.connect(results_database)
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 5000")
        separate_databases = results_database != corpus_database
        if separate_databases:
            connection.execute("ATTACH DATABASE ? AS corpus", (str(corpus_database),))
        matrix_schema = "corpus" if separate_databases else "main"
        matrix_table = f"{matrix_schema}.matrices"
        matrix_columns = {row[1] for row in connection.execute(f"PRAGMA {matrix_schema}.table_info(matrices)")}
        cache_enabled = {"fastest_elapsed_ns", "fastest_result_ref"} <= matrix_columns
        while True:
            values = writes.get()
            if values is None:
                return
            batch = [values]
            stop_after_commit = False
            for _ in range(writes.maxsize - 1):
                try:
                    values = writes.get_nowait()
                except Empty:
                    break
                if values is None:
                    stop_after_commit = True
                    break
                batch.append(values)
            connection.executemany(RESULT_UPSERT_SQL, batch)
            if cache_enabled:
                matrix_ids = sorted({values[0] for values in batch})
                placeholders = ",".join("?" for _ in matrix_ids)
                connection.execute(
                    f"""UPDATE {matrix_table} AS m
                        SET (fastest_elapsed_ns, fastest_result_ref) = (
                            SELECT r.elapsed_ns, json_object(
                                'model_id', r.model_id, 'mode', r.mode, 'preprocessing', r.preprocessing,
                                'binary_sha256', r.binary_sha256
                            )
                            FROM main.results AS r
                            WHERE r.matrix_id = m.matrix_id
                              AND r.status = 'ok'
                              AND r.mode = 'both'
                              AND (r.is_copositive IS NULL OR m.is_copositive IS NULL OR r.is_copositive = m.is_copositive)
                              AND (r.is_strictly_copositive IS NULL OR m.is_strictly_copositive IS NULL
                                   OR r.is_strictly_copositive = m.is_strictly_copositive)
                            ORDER BY r.elapsed_ns, r.model_id, r.mode, r.preprocessing, r.binary_sha256
                            LIMIT 1
                        )
                        WHERE m.matrix_id IN ({placeholders})""",
                    matrix_ids,
                )
            connection.commit()
            if stop_after_commit:
                return
    except BaseException as error:
        errors.append(error)
    finally:
        if connection is not None:
            connection.close()


def _compute_worker(
    model_id: str,
    mode: str,
    preprocessing: str,
    model_parameter: str | None,
    collect_diagnostics: bool,
    cpu_id: int,
    messages: Connection,
) -> None:
    try:
        send_lock = Lock()

        def send(message) -> None:
            with send_lock:
                messages.send(message)

        def terminate_worker(signal_number, _frame) -> None:
            terminate_active_process()
            raise SystemExit(128 + signal_number)

        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, terminate_worker)
        os.sched_setaffinity(0, {cpu_id})
        install_timeout_handler(TIMEOUT_SIGNAL)
        send(("ready", os.getpid()))

        while True:
            try:
                matrix = messages.recv()
            except EOFError:
                return
            if matrix is None:
                return

            reset_timeout()
            send(("started", matrix.matrix_id))
            monitor_stop = Event() if collect_diagnostics else None

            def publish_diagnostics() -> None:
                assert monitor_stop is not None
                while not monitor_stop.wait(DIAGNOSTICS_INTERVAL_SECONDS):
                    try:
                        send(("diagnostics", matrix.matrix_id, diagnostics_snapshot()))
                    except BaseException:
                        return

            monitor = Thread(target=publish_diagnostics, name="diagnostics", daemon=True) if collect_diagnostics else None
            if monitor is not None:
                monitor.start()
            try:
                result = compute_matrix(
                    model_id,
                    matrix,
                    mode,
                    preprocessing,
                    collect_certificate_joint_distribution=collect_diagnostics,
                    model_parameter=model_parameter,
                    _stream_diagnostics=collect_diagnostics,
                )
            except BaseException as error:
                send(("error", f"{type(error).__name__}: {error}"))
                return
            finally:
                if monitor is not None:
                    monitor_stop.set()
                    monitor.join()
            send(("result", result))
    except BaseException as error:  # the parent must see setup and worker failures
        try:
            messages.send(("error", f"{type(error).__name__}: {error}"))
        except (BrokenPipeError, EOFError, OSError):
            pass
    finally:
        messages.close()


def _spawn_worker(
    context, model_id: str, mode: str, preprocessing: str, model_parameter: str | None, collect_diagnostics: bool, cpu_id: int
):
    parent_messages, child_messages = context.Pipe()
    process = context.Process(
        target=_compute_worker,
        args=(model_id, mode, preprocessing, model_parameter, collect_diagnostics, cpu_id, child_messages),
    )
    try:
        process.start()
    except BaseException:
        parent_messages.close()
        child_messages.close()
        raise
    child_messages.close()
    return {
        "cpu_id": cpu_id,
        "process": process,
        "messages": parent_messages,
        "state": "starting",
        "deadline": monotonic() + STARTUP_TIMEOUT_SECONDS,
        "started_at": None,
        "last_diagnostics": None,
        "last_distribution": None,
        "row": None,
    }


def _terminate(process) -> None:
    process.terminate()
    process.join(timeout=1.0)
    if process.is_alive():
        process.kill()
        process.join()


def _discard_worker(worker) -> None:
    if worker["process"].is_alive():
        _terminate(worker["process"])
    else:
        worker["process"].join(timeout=0)
    worker["messages"].close()


def _shutdown_worker(worker) -> None:
    if worker["process"].is_alive():
        try:
            worker["messages"].send(None)
        except (BrokenPipeError, EOFError, OSError):
            pass
        worker["process"].join(timeout=1.0)
    if worker["process"].is_alive():
        _terminate(worker["process"])
    worker["messages"].close()


def _receive(worker):
    try:
        return worker["messages"].recv()
    except EOFError:
        worker["process"].join(timeout=0)
        return "error", f"worker exited with code {worker['process'].exitcode}"


def _decode_result(
    result,
    mode: str,
    timeout_seconds: float,
    collect_diagnostics: bool = True,
) -> tuple[str, int | None, int | None, int | None, str | None, str | None, str | None]:
    diagnostics = distribution = None
    if collect_diagnostics:
        try:
            diagnostics, distribution = _encode_diagnostics(result)
        except ValueError as error:
            return "error", None, None, None, None, None, str(error)

    status = int(result["status"])
    if status == int(StatusCode.PARSE_ERROR):
        return "parse_error", None, None, None, diagnostics, distribution, result["error_message"] or "invalid matrix input"
    if status == int(StatusCode.TIMEOUT):
        return "timeout", None, None, int(result["elapsed_ns"]), diagnostics, distribution, f"exceeded {timeout_seconds:g} seconds"
    if status == int(StatusCode.NODE_LIMIT):
        return (
            "node_limit", None, None, int(result["elapsed_ns"]), diagnostics, distribution,
            result["error_message"] or "open-node limit reached",
        )
    if status != int(StatusCode.OK):
        return "error", None, None, None, diagnostics, distribution, result["error_message"] or f"native status {status}"

    if result["mode"] != mode:
        return (
            "error", None, None, None, diagnostics, distribution,
            f"native result mode {result['mode']!r} does not match requested mode {mode!r}",
        )
    copositive = result["is_copositive"]
    strictly_copositive = result["is_strictly_copositive"]
    elapsed_ns = int(result["elapsed_ns"])
    valid_payload = (
        (mode == "copositive" and type(copositive) is bool and strictly_copositive is None)
        or (mode == "strictly_copositive" and copositive is None and type(strictly_copositive) is bool)
        or (mode == "both" and type(copositive) is bool and type(strictly_copositive) is bool)
    )
    if not valid_payload or elapsed_ns < 0:
        return "error", None, None, None, diagnostics, distribution, "native result has an invalid classification or elapsed time"
    return (
        "ok",
        int(copositive) if copositive is not None else None,
        int(strictly_copositive) if strictly_copositive is not None else None,
        elapsed_ns,
        diagnostics,
        distribution,
        None,
    )


def _encode_diagnostics(result) -> tuple[str, str]:
    values = result.get("certificate_joint_distribution")
    valid = type(values) is list and all(
        type(entry) in (list, tuple)
        and len(entry) in (3, 4, 5)
        and all(type(value) is int and value >= 0 for value in entry)
        and entry[-1] > 0
        and (len(entry) != 4 or entry[1] <= entry[2])
        and (len(entry) != 5 or entry[0] <= entry[1] <= entry[2] and entry[3] <= entry[1])
        for entry in values
    )
    if not valid:
        raise ValueError("native result has an invalid certificate joint distribution")
    diagnostics = result.get("diagnostics")
    if type(diagnostics) is not str:
        raise ValueError("native result has invalid diagnostics")
    return diagnostics, json.dumps(values, separators=(",", ":"))


def _validate_arguments(arguments) -> None:
    if arguments.mode not in COPOSITIVITY_MODES:
        raise ValueError(f"mode must be one of: {', '.join(COPOSITIVITY_MODES)}")
    if arguments.preprocessing not in PREPROCESSING_MODES:
        raise ValueError(f"preprocessing must be one of: {', '.join(PREPROCESSING_MODES)}")
    dense_limit_selected = arguments.dense_bitset_max_n is not None or arguments.dense_bitset_max_gib is not None
    if dense_limit_selected and arguments.model != "dense_bitset_dickinson":
        raise ValueError("dense-bitset limits apply only to dense_bitset_dickinson")
    if arguments.dense_bitset_max_n is not None and arguments.dense_bitset_max_n < 1:
        raise ValueError("dense-bitset-max-n must be positive")
    if arguments.dense_bitset_max_gib is not None and arguments.dense_bitset_max_gib < 1:
        raise ValueError("dense-bitset-max-gib must be positive")
    if not math.isfinite(arguments.timeout_seconds) or arguments.timeout_seconds <= 0:
        raise ValueError("timeout must be a finite positive number")
    if round(arguments.timeout_seconds * 1_000_000_000) < 1:
        raise ValueError("timeout must be at least one nanosecond")
    if arguments.dimension_from < 1:
        raise ValueError("dimension-from must be positive")
    if arguments.dimension_to is not None and arguments.dimension_to < arguments.dimension_from:
        raise ValueError("dimension-to must be at least dimension-from")
    if arguments.matrix_id_from < 1:
        raise ValueError("matrix-id-from must be positive")
    if arguments.matrix_id_to is not None and arguments.matrix_id_to < arguments.matrix_id_from:
        raise ValueError("matrix-id-to must be at least matrix-id-from")
    if arguments.matrix_ids is not None and any(matrix_id < 1 for matrix_id in arguments.matrix_ids):
        raise ValueError("matrix-ids must be positive")
    if len(arguments.cpus) != len(set(arguments.cpus)):
        raise ValueError("CPU IDs must not contain duplicates")
    if arguments.parent_cpu in arguments.cpus:
        raise ValueError("parent CPU must not also be a worker CPU")
    if not hasattr(os, "sched_getaffinity") or not hasattr(os, "sched_setaffinity"):
        raise RuntimeError("CPU affinity requires Linux os.sched_setaffinity()")
    unavailable = sorted(({arguments.parent_cpu} | set(arguments.cpus)) - set(os.sched_getaffinity(0)))
    if unavailable:
        raise ValueError(f"CPUs {unavailable} are unavailable; choose from {sorted(os.sched_getaffinity(0))}")
    if not arguments.database.is_file():
        raise ValueError(f"database does not exist: {arguments.database}")
    if not arguments.results_database.is_file():
        raise ValueError(f"results database does not exist: {arguments.results_database}")


def run(arguments) -> bool:
    """Run selected rows and return whether Ctrl-C requested an orderly drain."""

    _initialize_results_database(arguments.results_database)
    _validate_arguments(arguments)
    if arguments.dense_bitset_max_n is not None:
        os.environ["COPOSIT_DENSE_BITSET_MAX_N"] = str(arguments.dense_bitset_max_n)
        os.environ.pop("COPOSIT_DENSE_BITSET_MAX_GIB", None)
    elif arguments.dense_bitset_max_gib is not None:
        os.environ["COPOSIT_DENSE_BITSET_MAX_GIB"] = str(arguments.dense_bitset_max_gib)
        os.environ.pop("COPOSIT_DENSE_BITSET_MAX_N", None)
    os.sched_setaffinity(0, {arguments.parent_cpu})
    result_model_id = _result_model_id(arguments.model, arguments.model_parameter)
    binary = model_companion_path(arguments.model)
    binary_sha256 = _sha256(binary)
    timeout_ns = round(arguments.timeout_seconds * 1_000_000_000)

    connection = sqlite3.connect(arguments.database)
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA busy_timeout = 5000")
    try:
        separate_results_database = arguments.results_database != arguments.database
        if separate_results_database:
            connection.execute("ATTACH DATABASE ? AS diagnostics", (str(arguments.results_database),))
        result_table = "diagnostics.results" if separate_results_database else "results"

        where = "m.dimension >= ? AND (? IS NULL OR m.dimension <= ?) AND m.matrix_id >= ? AND (? IS NULL OR m.matrix_id <= ?)"
        query_values: list[object] = [
            arguments.dimension_from,
            arguments.dimension_to,
            arguments.dimension_to,
            arguments.matrix_id_from,
            arguments.matrix_id_to,
            arguments.matrix_id_to,
        ]
        if arguments.matrix_set:
            selections = (
                "json_array_length(m.references_unsolved) > 0" if matrix_set == "references_unsolved" else f"m.{matrix_set} = 1"
                for matrix_set in arguments.matrix_set
            )
            where += " AND m.preprocessing_solved = 0 AND (" + " OR ".join(selections) + ")"
        if arguments.matrix_ids:
            matrix_ids = sorted(set(arguments.matrix_ids))
            where += f" AND m.matrix_id IN ({','.join('?' for _ in matrix_ids)})"
            query_values.extend(matrix_ids)
        if arguments.without_results:
            where += f" AND NOT EXISTS (SELECT 1 FROM {result_table} r WHERE r.matrix_id = m.matrix_id)"

        database_directory = arguments.database.resolve().parent

        if arguments.retry_timeouts:
            where += (
                f" AND EXISTS (SELECT 1 FROM {result_table} r WHERE r.matrix_id = m.matrix_id AND r.model_id = ? "
                "AND r.mode = ? AND r.preprocessing = ? AND r.binary_sha256 = ? AND r.status = 'timeout')"
            )
            query_values.extend((result_model_id, arguments.mode, arguments.preprocessing, binary_sha256))
        elif not arguments.rerun:
            where += (
                f" AND NOT EXISTS (SELECT 1 FROM {result_table} r WHERE r.matrix_id = m.matrix_id AND r.model_id = ? "
                "AND r.mode = ? AND r.preprocessing = ? AND r.binary_sha256 = ? AND r.status <> 'running')"
            )
            query_values.extend((result_model_id, arguments.mode, arguments.preprocessing, binary_sha256))

        matrix_ids = [row[0] for row in connection.execute(
            f"SELECT matrix_id FROM matrices m WHERE {where} ORDER BY dimension, matrix_id", query_values
        )]
        total = len(matrix_ids)
        print(
            f"model={arguments.model} model_parameter={arguments.model_parameter or 'none'} "
            f"mode={arguments.mode} preprocessing={arguments.preprocessing} "
            f"binary_sha256={binary_sha256 or 'none'} matrices={total} "
            f"timeout_seconds={arguments.timeout_seconds:g} "
            f"dimensions={arguments.dimension_from}..{arguments.dimension_to or 'max'} parent_cpu={arguments.parent_cpu} "
            f"matrix_ids={arguments.matrix_id_from}..{arguments.matrix_id_to or 'max'} "
            f"explicit_matrix_ids={len(set(arguments.matrix_ids)) if arguments.matrix_ids else 'all'} "
            f"matrix_sets={','.join(arguments.matrix_set) if arguments.matrix_set else 'all'} "
            f"without_results={'yes' if arguments.without_results else 'no'} "
            f"diagnostics={'no' if arguments.without_diagnostics else 'yes'} "
            f"dense_bitset_max_n={arguments.dense_bitset_max_n or 'default'} "
            f"dense_bitset_max_gib={arguments.dense_bitset_max_gib or 'default'} "
            f"results_database={arguments.results_database} "
            f"cpus={','.join(map(str, arguments.cpus))}",
            flush=True,
        )
        if total == 0:
            return False

        context = get_context("spawn")
        started = monotonic()
        matrix_id_iterator = iter(matrix_ids)

        def load_next_row():
            matrix_id = next(matrix_id_iterator, None)
            if matrix_id is None:
                return None
            return connection.execute(
                "SELECT matrix_id, dimension, matrix, is_copositive, is_strictly_copositive "
                "FROM matrices WHERE matrix_id = ?",
                (matrix_id,),
            ).fetchone()

        next_row = load_next_row()
        worker_count = min(len(arguments.cpus), total)
        database_writes = Queue(maxsize=max(1, worker_count * DATABASE_QUEUE_PER_WORKER))
        database_errors: list[BaseException] = []
        database_writer = Thread(
            target=_database_writer,
            args=(arguments.results_database, arguments.database, arguments.parent_cpu, database_writes, database_errors),
            name="sqlite-writer",
            daemon=True,
        )
        database_writer.start()
        workers = {
            cpu_id: _spawn_worker(
                context,
                arguments.model,
                arguments.mode,
                arguments.preprocessing,
                arguments.model_parameter,
                not arguments.without_diagnostics,
                cpu_id,
            )
            for cpu_id in arguments.cpus[:worker_count]
        }
        completed_count = 0
        last_diagnostics_at = started
        interrupted = False
        interrupt_requested = False

        def request_interrupt(_signal_number, _frame) -> None:
            nonlocal interrupt_requested
            interrupt_requested = True

        def queue_database_write(values) -> None:
            while True:
                if database_errors:
                    raise RuntimeError(f"database writer failed: {database_errors[0]}")
                try:
                    database_writes.put(values, timeout=0.1)
                    return
                except Full:
                    continue

        def record_diagnostics(worker, payload) -> None:
            if worker["row"] is None or worker["started_at"] is None:
                return
            diagnostics, distribution = _encode_diagnostics(payload)
            worker["last_diagnostics"] = diagnostics
            worker["last_distribution"] = distribution
            matrix_id = worker["row"][0]
            queue_database_write((
                matrix_id,
                result_model_id,
                arguments.mode,
                arguments.preprocessing,
                binary_sha256,
                "running",
                None,
                None,
                round((monotonic() - worker["started_at"]) * 1_000_000_000),
                timeout_ns,
                datetime.now(tz=timezone.utc).isoformat(timespec="seconds"),
                diagnostics,
                distribution,
                None,
            ))

        def record(
            worker,
            status: str,
            copositive: int | None,
            strictly_copositive: int | None,
            elapsed_ns: int | None,
            diagnostics: str | None,
            certificate_joint_distribution: str | None,
            message: str | None,
        ) -> None:
            nonlocal completed_count, last_diagnostics_at
            if status == "error" and elapsed_ns is None and worker["started_at"] is not None:
                elapsed_ns = round((monotonic() - worker["started_at"]) * 1_000_000_000)
            matrix_id, dimension, expected_copositive, expected_strictly_copositive = worker["row"]
            recorded_at = datetime.now(tz=timezone.utc).isoformat(timespec="seconds")
            queue_database_write((
                matrix_id,
                result_model_id,
                arguments.mode,
                arguments.preprocessing,
                binary_sha256,
                status,
                copositive,
                strictly_copositive,
                elapsed_ns,
                timeout_ns,
                recorded_at,
                diagnostics,
                certificate_joint_distribution,
                message,
            ))
            completed_count += 1
            expected = {
                "copositive": (expected_copositive, None),
                "strictly_copositive": (None, expected_strictly_copositive),
                "both": (expected_copositive, expected_strictly_copositive),
            }[arguments.mode]
            actual = (copositive, strictly_copositive)
            requested = {"copositive": (0,), "strictly_copositive": (1,), "both": (0, 1)}[arguments.mode]
            if status != "ok":
                comparison = status
            elif any(expected[index] is not None and actual[index] != expected[index] for index in requested):
                comparison = "MISMATCH"
            else:
                comparison = "match" if all(expected[index] is not None for index in requested) else "unverified"
            now = monotonic()
            if (total <= DETAILED_DIAGNOSTICS_LIMIT or completed_count == total or comparison == "MISMATCH"
                    or comparison == "unverified"
                    or status in ("parse_error", "error")
                    or now - last_diagnostics_at >= DIAGNOSTICS_INTERVAL_SECONDS):
                print(
                    f"[{completed_count}/{total}] matrix={matrix_id} dimension={dimension} cpu={worker['cpu_id']} "
                    f"pid={worker['process'].pid} status={status} result={actual} expected={expected} "
                    f"comparison={comparison} elapsed_ns={elapsed_ns} certificate_bins="
                    f"{len(json.loads(certificate_joint_distribution)) if certificate_joint_distribution is not None else 'none'} "
                    f"diagnostics_bytes={len(diagnostics.encode()) if diagnostics is not None else 'none'}",
                    flush=True,
                )
                last_diagnostics_at = now
            worker["row"] = None
            worker["started_at"] = None
            worker["last_diagnostics"] = None
            worker["last_distribution"] = None

        def remove_worker(cpu_id: int, restart: bool) -> None:
            worker = workers.pop(cpu_id)
            _discard_worker(worker)
            if restart and not interrupted and next_row is not None:
                workers[cpu_id] = _spawn_worker(
                    context,
                    arguments.model,
                    arguments.mode,
                    arguments.preprocessing,
                    arguments.model_parameter,
                    not arguments.without_diagnostics,
                    cpu_id,
                )

        previous_interrupt_handler = signal.signal(signal.SIGINT, request_interrupt)
        try:
            while workers:
                if database_errors:
                    raise RuntimeError(f"database writer failed: {database_errors[0]}")
                if interrupt_requested and not interrupted:
                    interrupted = True
                    next_row = None
                    print("interrupt received; waiting for active matrices and assigning no new work", flush=True)

                deadlines = [worker["deadline"] for worker in workers.values() if worker["deadline"] is not None]
                wait_seconds = None if not deadlines else max(0.0, min(deadlines) - monotonic())
                ready = set(wait_for_connections([worker["messages"] for worker in workers.values()], timeout=wait_seconds))
                if interrupt_requested and not interrupted:
                    interrupted = True
                    next_row = None
                    print("interrupt received; waiting for active matrices and assigning no new work", flush=True)

                for cpu_id, worker in list(workers.items()):
                    if worker["messages"] not in ready:
                        continue
                    message = _receive(worker)
                    state = worker["state"]

                    if state == "starting":
                        if message[0] != "ready":
                            raise RuntimeError(f"worker on CPU {cpu_id} failed to start: {message[1]}")
                        worker["state"] = "idle"
                        worker["deadline"] = None
                        continue

                    if state == "assigned" and message[0] == "started":
                        if int(message[1]) != worker["row"][0]:
                            raise RuntimeError(f"worker on CPU {cpu_id} started the wrong matrix")
                        worker["state"] = "running"
                        worker["started_at"] = monotonic()
                        worker["last_diagnostics"] = None
                        worker["last_distribution"] = None
                        worker["deadline"] = monotonic() + arguments.timeout_seconds
                        continue

                    if state in ("running", "stopping") and message[0] == "diagnostics":
                        if int(message[1]) != worker["row"][0]:
                            raise RuntimeError(f"worker on CPU {cpu_id} reported diagnostics for the wrong matrix")
                        record_diagnostics(worker, message[2])
                        continue

                    if state in ("running", "stopping") and message[0] == "result":
                        decoded = _decode_result(
                            message[1],
                            arguments.mode,
                            arguments.timeout_seconds,
                            not arguments.without_diagnostics,
                        )
                        record(worker, *decoded)
                        if decoded[0] == "error":
                            remove_worker(cpu_id, restart=True)
                        else:
                            worker["state"] = "idle"
                            worker["deadline"] = None
                        continue

                    error_message = str(message[1]) if message[0] == "error" else f"unexpected worker message {message[0]!r}"
                    if worker["row"] is not None:
                        record(
                            worker, "error", None, None, None,
                            worker["last_diagnostics"], worker["last_distribution"], error_message,
                        )
                    remove_worker(cpu_id, restart=True)

                now = monotonic()
                for cpu_id, worker in list(workers.items()):
                    deadline = worker["deadline"]
                    if deadline is None or now < deadline:
                        continue
                    if worker["state"] == "running":
                        try:
                            os.kill(worker["process"].pid, TIMEOUT_SIGNAL)
                        except ProcessLookupError:
                            record(
                                worker, "error", None, None, None,
                                worker["last_diagnostics"], worker["last_distribution"],
                                "worker exited before the timeout signal",
                            )
                            remove_worker(cpu_id, restart=True)
                        else:
                            worker["state"] = "stopping"
                            worker["deadline"] = monotonic() + TIMEOUT_GRACE_SECONDS
                        continue
                    if worker["state"] == "stopping":
                        record(
                            worker,
                            "timeout",
                            None,
                            None,
                            round((monotonic() - worker["started_at"]) * 1_000_000_000),
                            worker["last_diagnostics"],
                            worker["last_distribution"],
                            f"exceeded {arguments.timeout_seconds:g} seconds and did not stop within {TIMEOUT_GRACE_SECONDS:g} second",
                        )
                        remove_worker(cpu_id, restart=True)
                        continue
                    if worker["state"] == "starting":
                        raise RuntimeError(f"worker on CPU {cpu_id} startup exceeded {STARTUP_TIMEOUT_SECONDS:g} seconds")

                    record(
                        worker, "error", None, None, None,
                        worker["last_diagnostics"], worker["last_distribution"],
                        "worker did not acknowledge the matrix within 30 seconds",
                    )
                    remove_worker(cpu_id, restart=True)

                for cpu_id, worker in list(workers.items()):
                    if worker["state"] != "idle":
                        continue
                    if interrupted or next_row is None:
                        workers.pop(cpu_id)
                        _shutdown_worker(worker)
                        continue

                    matrix_id, dimension, values, expected_copositive, expected_strictly_copositive = next_row
                    next_row = load_next_row()
                    worker["row"] = (matrix_id, dimension, expected_copositive, expected_strictly_copositive)
                    worker["state"] = "assigned"
                    worker["deadline"] = monotonic() + STARTUP_TIMEOUT_SECONDS
                    matrix_source = str(database_directory / values[5:]) if values.startswith("file:") else f"{dimension}#{values}"
                    worker["messages"].send(Matrix(matrix_source, matrix_id=matrix_id))
        finally:
            try:
                for worker in workers.values():
                    _shutdown_worker(worker)
            finally:
                signal.signal(signal.SIGINT, previous_interrupt_handler)

        queue_database_write(None)
        database_writer.join()
        if database_errors:
            raise RuntimeError(f"database writer failed: {database_errors[0]}")
        print(f"completed={completed_count} wall_seconds={monotonic() - started:.3f} interrupted={int(interrupted)}", flush=True)
        return interrupted
    finally:
        if "database_writer" in locals() and database_writer.is_alive():
            while database_writer.is_alive():
                try:
                    database_writes.put(None, timeout=0.1)
                    break
                except Full:
                    continue
            database_writer.join()
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", choices=ALGORITHMS)
    parser.add_argument(
        "--mode",
        choices=COPOSITIVITY_MODES,
        help="predicate to test; omitted selects 'both' for combined-classification models and rejects other models",
    )
    parser.add_argument("--timeout-seconds", type=float, required=True)
    parser.add_argument("--dimension-from", type=int, default=1)
    parser.add_argument("--dimension-to", type=int)
    parser.add_argument("--matrix-id-from", type=int, default=1)
    parser.add_argument("--matrix-id-to", type=int)
    parser.add_argument("--matrix-ids", type=int, nargs="+", metavar="ID")
    parser.add_argument("--matrix-set", choices=MATRIX_SETS, nargs="+", help="run the union of the selected corpus selectors")
    parser.add_argument("--without-results", action="store_true", help="select only matrices with no result rows")
    parser.add_argument("--without-diagnostics", action="store_true", help="do not collect or store runtime diagnostics")
    parser.add_argument("--parent-cpu", type=int, required=True, metavar="ID")
    parser.add_argument("--cpus", type=int, nargs="+", required=True, metavar="ID")
    parser.add_argument("--database", type=Path, default=DEFAULT_DATABASE, help="matrix corpus database")
    parser.add_argument(
        "--results-database",
        type=Path,
        help="benchmark/diagnostics database (defaults to the ignored experiments database for the maintained corpus)",
    )
    parser.add_argument("--preprocessing", choices=PREPROCESSING_MODES, default="both")
    parser.add_argument(
        "--model-parameter",
        help="model-specific value; wide-certificate models use a percentage and xxx_two uses alternating or ascending",
    )
    dense_limit = parser.add_mutually_exclusive_group()
    dense_limit.add_argument("--dense-bitset-max-n", type=int, metavar="N")
    dense_limit.add_argument("--dense-bitset-max-gib", type=int, metavar="GIB")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--rerun", action="store_true", help="replace every selected row for this model, preprocessing, and binary")
    selection.add_argument(
        "--retry-timeouts",
        action="store_true",
        help="replace only selected timeout rows for this model, preprocessing, and binary",
    )
    arguments = parser.parse_args()
    arguments.database = arguments.database.resolve()
    arguments.results_database = (
        arguments.results_database.resolve()
        if arguments.results_database is not None
        else (DEFAULT_RESULTS_DATABASE if arguments.database == DEFAULT_DATABASE else arguments.database).resolve()
    )
    try:
        arguments.mode = _resolve_mode(arguments.model, arguments.mode)
        arguments.model_parameter = _resolve_model_parameter(arguments.model, arguments.model_parameter)
        interrupted = run(arguments)
    except (RuntimeError, ValueError) as error:
        parser.error(str(error))
    if interrupted:
        raise SystemExit(130)


if __name__ == "__main__":
    main()
