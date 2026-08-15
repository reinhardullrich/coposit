#!/usr/bin/env python3
"""Remove raw QP objectives whose negative diagonal makes copositivity trivial."""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path

from classify_obvious_literature_truth_2026_08_14 import EVIDENCE_PREFIX, matrix_entries


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
TARGETS = {
    59: (180, 11384, 11682, 2074750, 23915682590),
    61: (12, 11795, 11806, 141606, 1671021746),
    73: (598, 11819, 12509, 7270192, 88411556080),
}
TARGET_SQL = f"""
    source_id IN ({','.join(map(str, TARGETS))})
    AND source LIKE '%{EVIDENCE_PREFIX}e[%is a nonnegative witness with x^T A x=A[%<0%'
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        rows = list(connection.execute(
            f"""SELECT matrix_id, source_id, dimension, matrix,
                       smoke_set, representative_core, stress_test, scale_set, timeout_5s_strict_set
                FROM matrices WHERE {TARGET_SQL} ORDER BY matrix_id"""
        ))
        if not rows:
            print("all 790 trivial raw QP objectives are already absent")
            return

        actual = {}
        for source_id in TARGETS:
            ids = [row[0] for row in rows if row[1] == source_id]
            actual[source_id] = (len(ids), min(ids), max(ids), sum(ids), sum(matrix_id * matrix_id for matrix_id in ids))
        if actual != TARGETS:
            raise ValueError(f"target identity guard failed: {actual}")
        if any(storage.startswith("file:") for _, _, _, storage, *_ in rows):
            raise ValueError("target unexpectedly contains an external matrix payload")
        if any(any(row[4:]) for row in rows):
            raise ValueError("target unexpectedly belongs to a benchmark set")
        for matrix_id, _, n, storage, *_ in rows:
            if not any(i == j and value < 0 for i, j, value in matrix_entries(n, storage)):
                raise ValueError(f"matrix {matrix_id} no longer has a negative diagonal witness")

        target_ids = [(row[0],) for row in rows]
        for table in ("results", "preprocessing_results"):
            count = connection.execute(
                f"SELECT count(*) FROM {table} WHERE matrix_id IN (SELECT matrix_id FROM matrices WHERE {TARGET_SQL})"
            ).fetchone()[0]
            if count:
                raise ValueError(f"target unexpectedly has {count} dependent rows in {table}")

        print(f"validated={len(rows)} source_59={actual[59][0]} source_61={actual[61][0]} source_73={actual[73][0]}")
        if arguments.dry_run:
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany("DELETE FROM matrices WHERE matrix_id=?", target_ids)
        if connection.total_changes != len(rows):
            raise ValueError(f"expected {len(rows)} deletions, applied {connection.total_changes}")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")

    print(f"removed={len(rows)}")


if __name__ == "__main__":
    main()
