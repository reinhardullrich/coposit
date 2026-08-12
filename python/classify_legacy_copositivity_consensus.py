#!/usr/bin/env python3
"""Fill unknown non-strict-copositivity truth from unanimous completed baselines."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
from pathlib import Path
import signal
import sqlite3
import time

from pycoposit import Matrix, StatusCode, compute_matrix
from pycoposit.core import load_native_module


MODELS = (
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "hadeler_1983",
    "dickinson_2019",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
)
MINIMUM_COMPLETED_MODELS = 4
ABSTENTION_STATUSES = {StatusCode.TIMEOUT, StatusCode.NODE_LIMIT}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compute_with_timeout(model: str, matrix: Matrix, timeout_seconds: float) -> dict:
    module = load_native_module(model)
    module._install_timeout_handler(signal.SIGALRM)
    signal.setitimer(signal.ITIMER_REAL, timeout_seconds)
    try:
        return compute_matrix(model, matrix, "copositive")
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--database",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "testdata" / "copos_testdata.sqlite3",
    )
    parser.add_argument("--timeout-seconds", type=float, default=5.0)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--no-write", action="store_true")
    args = parser.parse_args()
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    if args.limit is not None and args.limit <= 0:
        parser.error("--limit must be positive")

    for model in MODELS:
        binary = Path(load_native_module(model).__file__)
        print(f"model={model} sha256={sha256(binary)}", flush=True)

    database = sqlite3.connect(args.database)
    database.execute("PRAGMA busy_timeout = 5000")
    try:
        integrity = database.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise RuntimeError(f"database integrity check failed: {integrity}")
        rows = database.execute(
            "SELECT matrix_id, dimension, matrix FROM matrices WHERE is_copositive IS NULL ORDER BY matrix_id"
        ).fetchall()
        if args.limit is not None:
            rows = rows[: args.limit]
        print(
            f"unknown_selected={len(rows)} timeout_seconds={args.timeout_seconds:g} write={not args.no_write}",
            flush=True,
        )

        started = time.monotonic()
        classifications: Counter[bool] = Counter()
        model_elapsed_ns: Counter[str] = Counter()
        abstentions: Counter[str] = Counter()
        database_directory = args.database.resolve().parent
        for index, (matrix_id, dimension, values) in enumerate(rows, 1):
            matrix_source = str(database_directory / values[5:]) if values.startswith("file:") else f"{dimension}#{values}"
            matrix = Matrix(matrix_source, matrix_id=matrix_id)
            results = {model: compute_with_timeout(model, matrix, args.timeout_seconds) for model in MODELS}
            errors = {
                model: result
                for model, result in results.items()
                if result["status"] not in ABSTENTION_STATUSES
                and (result["status"] != StatusCode.OK or type(result["is_copositive"]) is not bool)
            }
            if errors:
                for model, result in errors.items():
                    print(
                        f"ERROR matrix_id={matrix_id} n={dimension} model={model} status={result['status']} "
                        f"error={result['error_message']!r}",
                        flush=True,
                    )
                return 3

            completed = {model: result for model, result in results.items() if result["status"] == StatusCode.OK}
            if len(completed) < MINIMUM_COMPLETED_MODELS:
                statuses = {model: result["status"] for model, result in results.items()}
                print(
                    f"INSUFFICIENT_RESULTS matrix_id={matrix_id} n={dimension} completed={len(completed)} "
                    f"minimum={MINIMUM_COMPLETED_MODELS} statuses={statuses}",
                    flush=True,
                )
                return 4

            outcomes = {model: result["is_copositive"] for model, result in completed.items()}
            if len(set(outcomes.values())) != 1:
                print(f"DISAGREEMENT matrix_id={matrix_id} n={dimension} outcomes={outcomes}", flush=True)
                return 2

            classification = next(iter(outcomes.values()))
            classifications[classification] += 1
            for model, result in results.items():
                model_elapsed_ns[model] += result["elapsed_ns"]
                if result["status"] in ABSTENTION_STATUSES:
                    abstentions[model] += 1
            if not args.no_write:
                updated = database.execute(
                    "UPDATE matrices SET is_copositive = ? WHERE matrix_id = ? AND is_copositive IS NULL",
                    (classification, matrix_id),
                ).rowcount
                if updated != 1:
                    raise RuntimeError(f"matrix {matrix_id} changed concurrently")
                database.commit()
            print(
                f"[{index}/{len(rows)}] matrix_id={matrix_id} n={dimension} is_copositive={int(classification)} "
                f"completed={len(completed)} abstained={len(results) - len(completed)}",
                flush=True,
            )

        remaining = database.execute("SELECT count(*) FROM matrices WHERE is_copositive IS NULL").fetchone()[0]
        print(
            f"complete classified_true={classifications[True]} classified_false={classifications[False]} "
            f"remaining_unknown={remaining} wall_seconds={time.monotonic() - started:.3f}",
            flush=True,
        )
        for model in MODELS:
            print(
                f"model={model} elapsed_seconds={model_elapsed_ns[model] / 1e9:.9f} abstentions={abstentions[model]}",
                flush=True,
            )
        return 0
    finally:
        database.close()


if __name__ == "__main__":
    raise SystemExit(main())
