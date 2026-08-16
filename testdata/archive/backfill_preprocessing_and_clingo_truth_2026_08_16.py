#!/usr/bin/env python3
"""Backfill exact corpus truth from the current preprocessing and Clingo runs."""

from __future__ import annotations

import csv
import hashlib
import sqlite3
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "testdata" / "copos_testdata.sqlite3"
DIAGNOSTICS = ROOT / "experiments" / "diagnostics.sqlite3"
PREPROCESSING_CSV = ROOT / "experiments" / "preprocessing_depth_2026-08-15" / "results" / "depth_2_current_60s_merged.csv"
PREPROCESSING_SHA256 = "629d637251623ed98a9c5abccbbeeee3334a929a044306cc352ecb7b2abc6c4c"
CLINGO_SHA256 = "6064632cbc17c385a83cc36d343b9fecec7a3383ed6812bd684c91348f0a5ef3"
CLINGO_TIMEOUT_NS = 120_000_000_000
RECOVERED_TIMEOUTS = {
    12671: 120_042_813_696,
    12672: 120_030_678_689,
    12673: 120_057_214_745,
    12674: 120_066_935_667,
}


def preprocessing_truth() -> dict[int, tuple[int, int]]:
    if hashlib.sha256(PREPROCESSING_CSV.read_bytes()).hexdigest() != PREPROCESSING_SHA256:
        raise ValueError("preprocessing CSV hash does not match the documented merged run")
    with PREPROCESSING_CSV.open(newline="") as stream:
        rows = csv.DictReader(stream)
        result = {
            int(row["matrix_id"]): (int(row["is_copositive"]), int(row["is_strictly_copositive"]))
            for row in rows
            if row["status"] == "ok" and row["copositive_known"] == "1" and row["strict_known"] == "1"
        }
    if Counter(result.values()) != Counter({(0, 0): 978, (1, 0): 1025, (1, 1): 705}):
        raise ValueError("unexpected complete preprocessing classifications")
    return result


def clingo_truth() -> dict[int, tuple[int, int]]:
    with sqlite3.connect(DIAGNOSTICS) as connection:
        connection.execute("BEGIN IMMEDIATE")
        for matrix_id, elapsed_ns in RECOVERED_TIMEOUTS.items():
            row = connection.execute(
                """SELECT status FROM results
                   WHERE matrix_id=? AND model_id='clingo_sat_dickinson' AND mode='both' AND preprocessing='both'
                     AND binary_sha256=? AND timeout_ns=?""",
                (matrix_id, CLINGO_SHA256, CLINGO_TIMEOUT_NS),
            ).fetchone()
            if row is None or row[0] not in ("running", "timeout"):
                raise ValueError(f"matrix {matrix_id} has unexpected stored Clingo status: {row}")
            if row[0] == "running":
                connection.execute(
                    """UPDATE results SET status='timeout',elapsed_ns=?,message=?
                       WHERE matrix_id=? AND model_id='clingo_sat_dickinson' AND mode='both' AND preprocessing='both'
                         AND binary_sha256=? AND timeout_ns=?""",
                    (
                        elapsed_ns,
                        "Final timeout recovered from the completed dispatcher output; diagnostics are the last persisted snapshot.",
                        matrix_id,
                        CLINGO_SHA256,
                        CLINGO_TIMEOUT_NS,
                    ),
                )
        counts = Counter(dict(connection.execute(
            """SELECT status,count(*) FROM results
               WHERE model_id='clingo_sat_dickinson' AND mode='both' AND preprocessing='both'
                 AND binary_sha256=? AND timeout_ns=? GROUP BY status""",
            (CLINGO_SHA256, CLINGO_TIMEOUT_NS),
        )))
        if counts != Counter({"ok": 71, "timeout": 164}):
            raise ValueError(f"unexpected final Clingo counts: {counts}")
        result = {
            matrix_id: (copositive, strict)
            for matrix_id, copositive, strict in connection.execute(
                """SELECT matrix_id,is_copositive,is_strictly_copositive FROM results
                   WHERE model_id='clingo_sat_dickinson' AND mode='both' AND preprocessing='both'
                     AND binary_sha256=? AND timeout_ns=? AND status='ok'""",
                (CLINGO_SHA256, CLINGO_TIMEOUT_NS),
            )
        }
        if Counter(result.values()) != Counter({(0, 0): 46, (1, 1): 25}):
            raise ValueError("unexpected Clingo classifications")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("diagnostics database integrity check failed")
        connection.commit()
        return result


def apply_truth(preprocessing: dict[int, tuple[int, int]], clingo: dict[int, tuple[int, int]]) -> None:
    overlap = preprocessing.keys() & clingo.keys()
    if overlap:
        raise ValueError(f"preprocessing and Clingo truth overlap: {sorted(overlap)}")
    with sqlite3.connect(CORPUS) as connection:
        connection.execute("BEGIN IMMEDIATE")
        flags = {
            matrix_id: solved
            for matrix_id, solved in connection.execute(
                f"SELECT matrix_id,preprocessing_solved FROM matrices WHERE matrix_id IN ({','.join('?' for _ in preprocessing)})",
                tuple(preprocessing),
            )
        }
        if set(flags) != set(preprocessing) or any(not solved for solved in flags.values()):
            raise ValueError("complete preprocessing evidence does not match preprocessing_solved flags")

        pending = Counter()
        for evidence, values in (("preprocessing", preprocessing), ("clingo", clingo)):
            for matrix_id, (copositive, strict) in values.items():
                old = connection.execute(
                    "SELECT is_copositive,is_strictly_copositive FROM matrices WHERE matrix_id=?", (matrix_id,)
                ).fetchone()
                if old is None:
                    raise ValueError(f"matrix {matrix_id} is absent from the corpus")
                if (old[0] is not None and old[0] != copositive) or (old[1] is not None and old[1] != strict):
                    raise ValueError(f"matrix {matrix_id} contradicts {evidence} truth: stored={old}, new={(copositive, strict)}")
                if old != (copositive, strict):
                    connection.execute(
                        "UPDATE matrices SET is_copositive=?,is_strictly_copositive=? WHERE matrix_id=?",
                        (copositive, strict, matrix_id),
                    )
                    pending[evidence] += 1
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("corpus database integrity check failed")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("corpus database foreign-key check failed")
        connection.commit()
        print(f"preprocessing_truth_updates={pending['preprocessing']}")
        print(f"clingo_truth_updates={pending['clingo']}")


if __name__ == "__main__":
    apply_truth(preprocessing_truth(), clingo_truth())
