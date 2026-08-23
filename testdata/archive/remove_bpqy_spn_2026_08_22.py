#!/usr/bin/env python3
"""Remove the 150 BPQY SPN Float64 materializations and their stored diagnostics."""

from __future__ import annotations

import argparse
from pathlib import Path
import sqlite3


MARKER = "import_batch=bpqy-julia185-float-materialization-2026-08-14;"
FAMILY = "BPQY SPN intended copositive-boundary generator"
EXPECTED = (150, 12874, 13023, 1942275, 25149829075, 75, 75)


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=root / "testdata/copos_testdata.sqlite3")
    parser.add_argument("--diagnostics", type=Path, default=root / "experiments/diagnostics.sqlite3")
    arguments = parser.parse_args()

    connection = sqlite3.connect(arguments.database)
    connection.execute("PRAGMA foreign_keys = ON")
    diagnostics_attached = arguments.diagnostics.is_file()
    if diagnostics_attached:
        connection.execute("ATTACH DATABASE ? AS diagnostics", (str(arguments.diagnostics),))

    condition = "family = ? AND instr(source, ?) > 0"
    parameters = (FAMILY, MARKER)
    try:
        guard = connection.execute(
            f"""SELECT count(*), min(matrix_id), max(matrix_id), sum(matrix_id), sum(matrix_id * matrix_id),
                       count(*) FILTER (WHERE dimension = 25), count(*) FILTER (WHERE dimension = 50)
                FROM matrices WHERE {condition}""",
            parameters,
        ).fetchone()
        if guard == (0, None, None, None, None, 0, 0):
            print("all 150 BPQY SPN matrices are already absent")
            return
        if guard != EXPECTED:
            raise RuntimeError(f"BPQY SPN identity guard failed: {guard}")

        matrix_ids = [row[0] for row in connection.execute(f"SELECT matrix_id FROM matrices WHERE {condition}", parameters)]
        placeholders = ",".join("?" for _ in matrix_ids)
        removed_results = 0
        removed_preprocessing = 0

        connection.execute("BEGIN IMMEDIATE")
        if diagnostics_attached:
            removed_results = connection.execute(
                f"SELECT count(*) FROM diagnostics.results WHERE matrix_id IN ({placeholders})", matrix_ids
            ).fetchone()[0]
            removed_preprocessing = connection.execute(
                f"SELECT count(*) FROM diagnostics.preprocessing_results WHERE matrix_id IN ({placeholders})", matrix_ids
            ).fetchone()[0]
            connection.execute(f"DELETE FROM diagnostics.results WHERE matrix_id IN ({placeholders})", matrix_ids)
            connection.execute(f"DELETE FROM diagnostics.preprocessing_results WHERE matrix_id IN ({placeholders})", matrix_ids)
        connection.execute(f"DELETE FROM matrices WHERE matrix_id IN ({placeholders})", matrix_ids)
        connection.commit()

        if connection.execute(f"SELECT count(*) FROM matrices WHERE {condition}", parameters).fetchone()[0] != 0:
            raise RuntimeError("BPQY SPN matrices remain after deletion")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise RuntimeError("corpus foreign-key check failed")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("corpus integrity check failed")
        if diagnostics_attached and connection.execute("PRAGMA diagnostics.integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("diagnostics integrity check failed")

        print(
            f"removed_matrices={len(matrix_ids)} removed_results={removed_results} "
            f"removed_preprocessing_results={removed_preprocessing}"
        )
    except BaseException:
        if connection.in_transaction:
            connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == "__main__":
    main()
