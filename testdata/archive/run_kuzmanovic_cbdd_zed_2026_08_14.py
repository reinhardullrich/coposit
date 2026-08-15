#!/usr/bin/env python3
"""Run ordinary CBDD-Zed Dickinson directly into the Kuzmanovic staging table."""

from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
from multiprocessing import get_context
from multiprocessing.connection import wait as wait_for_connections
import os
from pathlib import Path
import signal
import sqlite3
from time import monotonic

from pycoposit import Matrix
from pycoposit.core import load_native_module
from run_results import (
    STARTUP_TIMEOUT_SECONDS,
    TIMEOUT_GRACE_SECONDS,
    TIMEOUT_SIGNAL,
    _decode_result,
    _discard_worker,
    _receive,
    _sha256,
    _shutdown_worker,
    _spawn_worker,
)


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
MODEL = "cbdd_zed_dickinson"
MODE = "copositive"
PREPROCESSING = "none"
TIMEOUT_SECONDS = 10.0
TIMEOUT_NS = 10_000_000_000
PARENT_CPU = 3
WORKER_CPUS = (4, 5, 6, 7)
COMMIT_EVERY = 100

COLUMNS = {
    "cbdd_zed_status": "TEXT CHECK(cbdd_zed_status IS NULL OR cbdd_zed_status IN ('ok','parse_error','timeout','node_limit','error'))",
    "cbdd_zed_is_copositive": "INTEGER CHECK(cbdd_zed_is_copositive IS NULL OR cbdd_zed_is_copositive IN (0,1))",
    "cbdd_zed_matches_published_label":
        "INTEGER CHECK(cbdd_zed_matches_published_label IS NULL OR cbdd_zed_matches_published_label IN (0,1))",
    "cbdd_zed_elapsed_ns": "INTEGER CHECK(cbdd_zed_elapsed_ns IS NULL OR cbdd_zed_elapsed_ns >= 0)",
    "cbdd_zed_timeout_ns": "INTEGER CHECK(cbdd_zed_timeout_ns IS NULL OR cbdd_zed_timeout_ns > 0)",
    "cbdd_zed_binary_sha256": "TEXT CHECK(cbdd_zed_binary_sha256 IS NULL OR (length(cbdd_zed_binary_sha256)=64 "
                                "AND cbdd_zed_binary_sha256 NOT GLOB '*[^0-9a-f]*'))",
    "cbdd_zed_recorded_at": "TEXT",
    "cbdd_zed_message": "TEXT",
}


def ensure_columns(connection: sqlite3.Connection) -> None:
    existing = {row[1] for row in connection.execute("PRAGMA table_info(kuzmanovic_test_matrices)")}
    for name, declaration in COLUMNS.items():
        if name not in existing:
            connection.execute(f"ALTER TABLE kuzmanovic_test_matrices ADD COLUMN {name} {declaration}")
    connection.commit()


def published_match(label: str, status: str, value: int | None) -> int | None:
    if status != "ok" or label == "no_answer":
        return None
    return int(value == (label == "copositive"))


def run() -> None:
    os.sched_setaffinity(0, {PARENT_CPU})
    module = load_native_module(MODEL)
    binary_sha256 = _sha256(Path(module.__file__).resolve())
    connection = sqlite3.connect(DATABASE)
    connection.execute("PRAGMA foreign_keys=ON")
    connection.execute("PRAGMA busy_timeout=5000")
    ensure_columns(connection)

    examples = [row[0] for row in connection.execute(
        """SELECT published_example FROM kuzmanovic_test_matrices
           WHERE cbdd_zed_binary_sha256 IS NOT ? OR cbdd_zed_timeout_ns IS NOT ?
           ORDER BY dimension,published_example""",
        (binary_sha256, TIMEOUT_NS),
    )]
    total = len(examples)
    print(
        f"model={MODEL} mode={MODE} preprocessing={PREPROCESSING} binary_sha256={binary_sha256} "
        f"matrices={total} timeout_seconds=10 parent_cpu={PARENT_CPU} cpus={','.join(map(str, WORKER_CPUS))}",
        flush=True,
    )
    if not examples:
        connection.close()
        return

    iterator = iter(examples)

    def next_row():
        example = next(iterator, None)
        if example is None:
            return None
        return connection.execute(
            "SELECT published_example,dimension,matrix,published_label FROM kuzmanovic_test_matrices WHERE published_example=?",
            (example,),
        ).fetchone()

    pending = next_row()
    context = get_context("spawn")
    workers = {cpu: _spawn_worker(context, MODEL, MODE, PREPROCESSING, cpu) for cpu in WORKER_CPUS[:min(len(WORKER_CPUS), total)]}
    completed = 0
    uncommitted = 0
    counts = Counter()
    started = monotonic()
    last_report = started

    def record(worker, status, is_copositive, _strict, elapsed_ns, message):
        nonlocal completed, uncommitted, last_report
        example, dimension, label = worker["row"]
        match = published_match(label, status, is_copositive)
        connection.execute(
            """UPDATE kuzmanovic_test_matrices SET
                   cbdd_zed_status=?, cbdd_zed_is_copositive=?, cbdd_zed_matches_published_label=?, cbdd_zed_elapsed_ns=?,
                   cbdd_zed_timeout_ns=?, cbdd_zed_binary_sha256=?, cbdd_zed_recorded_at=?, cbdd_zed_message=?
               WHERE published_example=?""",
            (
                status, is_copositive, match, elapsed_ns, TIMEOUT_NS, binary_sha256,
                datetime.now(tz=timezone.utc).isoformat(timespec="seconds"), message, example,
            ),
        )
        completed += 1
        uncommitted += 1
        counts[(status, is_copositive, match)] += 1
        if uncommitted >= COMMIT_EVERY:
            connection.commit()
            uncommitted = 0
        now = monotonic()
        if match == 0 or status in ("parse_error", "error") or now - last_report >= 1.0 or completed == total:
            print(
                f"[{completed}/{total}] example={example} dimension={dimension} label={label} status={status} "
                f"is_copositive={is_copositive} matches_published={match} elapsed_ns={elapsed_ns}",
                flush=True,
            )
            last_report = now
        worker["row"] = None

    try:
        while workers:
            deadlines = [worker["deadline"] for worker in workers.values() if worker["deadline"] is not None]
            timeout = None if not deadlines else max(0.0, min(deadlines) - monotonic())
            ready = set(wait_for_connections([worker["messages"] for worker in workers.values()], timeout=timeout))

            for cpu, worker in list(workers.items()):
                if worker["messages"] not in ready:
                    continue
                message = _receive(worker)
                if worker["state"] == "starting" and message[0] == "ready":
                    worker["state"] = "idle"
                    worker["deadline"] = None
                elif worker["state"] == "assigned" and message[0] == "started":
                    if int(message[1]) != worker["row"][0]:
                        raise RuntimeError(f"worker on CPU {cpu} started the wrong matrix")
                    worker["state"] = "running"
                    worker["deadline"] = monotonic() + TIMEOUT_SECONDS
                elif worker["state"] in ("running", "stopping") and message[0] == "result":
                    record(worker, *_decode_result(message[1], MODE, TIMEOUT_SECONDS))
                    worker["state"] = "idle"
                    worker["deadline"] = None
                else:
                    error = str(message[1]) if message[0] == "error" else f"unexpected worker message {message[0]!r}"
                    if worker["row"] is not None:
                        record(worker, "error", None, None, None, error)
                    _discard_worker(worker)
                    workers.pop(cpu)
                    if pending is not None:
                        workers[cpu] = _spawn_worker(context, MODEL, MODE, PREPROCESSING, cpu)

            now = monotonic()
            for cpu, worker in list(workers.items()):
                if worker["deadline"] is None or now < worker["deadline"]:
                    continue
                if worker["state"] == "running":
                    try:
                        os.kill(worker["process"].pid, TIMEOUT_SIGNAL)
                    except ProcessLookupError:
                        record(worker, "error", None, None, None, "worker exited before timeout signal")
                        _discard_worker(worker)
                        workers.pop(cpu)
                    else:
                        worker["state"] = "stopping"
                        worker["deadline"] = monotonic() + TIMEOUT_GRACE_SECONDS
                elif worker["state"] == "stopping":
                    record(worker, "timeout", None, None, None, "did not stop within one-second timeout grace")
                    _discard_worker(worker)
                    workers.pop(cpu)
                    if pending is not None:
                        workers[cpu] = _spawn_worker(context, MODEL, MODE, PREPROCESSING, cpu)
                else:
                    raise RuntimeError(f"worker on CPU {cpu} did not start within {STARTUP_TIMEOUT_SECONDS:g} seconds")

            for cpu, worker in list(workers.items()):
                if worker["state"] != "idle":
                    continue
                if pending is None:
                    workers.pop(cpu)
                    _shutdown_worker(worker)
                    continue
                example, dimension, values, label = pending
                pending = next_row()
                worker["row"] = (example, dimension, label)
                worker["state"] = "assigned"
                worker["deadline"] = monotonic() + STARTUP_TIMEOUT_SECONDS
                worker["messages"].send(Matrix(f"{dimension}#{values}", matrix_id=example))
    finally:
        for worker in workers.values():
            _shutdown_worker(worker)
        connection.commit()
        connection.close()

    print(f"completed={completed} wall_seconds={monotonic() - started:.3f} counts={dict(counts)}", flush=True)


if __name__ == "__main__":
    run()
