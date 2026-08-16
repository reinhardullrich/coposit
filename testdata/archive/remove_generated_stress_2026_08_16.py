#!/usr/bin/env python3
"""Remove the precheck-trivial project-generated sparse/dense matrix panel."""

from __future__ import annotations

import argparse
from pathlib import Path
import sqlite3


SOURCE_GUARDS = {
    93: (75, 10505, 13068, 902655, 10978721465, "Deterministic sparse small-integer stress matrices"),
    94: (75, 10595, 13113, 907380, 11088697190, "Deterministic dense randomized stress matrices"),
}


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=root / "testdata/copos_testdata.sqlite3")
    parser.add_argument("--diagnostics", type=Path, default=root / "experiments/diagnostics.sqlite3")
    arguments = parser.parse_args()

    database = arguments.database.resolve()
    diagnostics = arguments.diagnostics.resolve()
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    diagnostics_attached = diagnostics.is_file()
    if diagnostics_attached:
        connection.execute("ATTACH DATABASE ? AS diagnostics", (str(diagnostics),))

    try:
        rows = connection.execute(
            "SELECT matrix_id, source_id, dimension, matrix, is_copositive, is_strictly_copositive, "
            "smoke_set, core_and_stress_test, scale_set, timeout_5s_strict_set, n_le_100, n_gt_100_solved "
            "FROM matrices WHERE source_id IN (93, 94) ORDER BY matrix_id"
        ).fetchall()
        source_rows = dict(connection.execute("SELECT source_id, title FROM sources WHERE source_id IN (93, 94)"))
        if not rows and not source_rows:
            print("all 150 generated matrices and both generator sources are already absent")
            return

        actual_guards = {}
        for source_id in SOURCE_GUARDS:
            ids = [row[0] for row in rows if row[1] == source_id]
            actual_guards[source_id] = (
                len(ids), min(ids), max(ids), sum(ids), sum(matrix_id * matrix_id for matrix_id in ids), source_rows.get(source_id)
            )
        if actual_guards != SOURCE_GUARDS:
            raise RuntimeError(f"generated-matrix identity guard failed: {actual_guards}")
        if sum(row[4] == 1 and row[5] == 1 for row in rows) != 50:
            raise RuntimeError("expected exactly 50 strictly copositive generated matrices")
        if sum(row[4] == 1 and row[5] == 0 for row in rows) != 50:
            raise RuntimeError("expected exactly 50 boundary generated matrices")
        if sum(row[4] == 0 for row in rows) != 50:
            raise RuntimeError("expected exactly 50 non-copositive generated matrices")
        if any(row[6] or row[7] or not row[8] or row[11] for row in rows):
            raise RuntimeError("generated matrices have unexpected Smoke, Core/Stress, Scale, or N>100-solved membership")
        if sum(row[9] for row in rows) != 4 or sum(row[10] for row in rows) != 36:
            raise RuntimeError("generated matrices have unexpected Timeout or N<=100 membership")

        reference_counts = connection.execute(
            """SELECT
                   (SELECT count(*) FROM matrices, json_each(additional_source_ids) WHERE json_each.value IN (93, 94)),
                   (SELECT count(*) FROM matrices, json_each(references_solved)
                    WHERE json_extract(json_each.value, '$.source_id') IN (93, 94)),
                   (SELECT count(*) FROM matrices, json_each(references_unsolved)
                    WHERE json_extract(json_each.value, '$.source_id') IN (93, 94))"""
        ).fetchone()
        if any(reference_counts):
            raise RuntimeError(f"generator sources are still referenced outside source_id: {reference_counts}")

        external_files = []
        for storage in (row[3] for row in rows if row[3].startswith("file:")):
            relative = Path(storage[5:])
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"unsafe matrix path: {relative}")
            external_files.append(database.parent / relative)
        if len(external_files) != 21 or len(set(external_files)) != 21 or not all(path.is_file() for path in external_files):
            raise RuntimeError("expected exactly 21 distinct existing external matrix files")

        matrix_ids = [row[0] for row in rows]
        placeholders = ",".join("?" for _ in matrix_ids)
        diagnostics_results = 0
        preprocessing_results = 0
        connection.execute("BEGIN IMMEDIATE")
        try:
            if diagnostics_attached:
                diagnostics_results = connection.execute(
                    f"SELECT count(*) FROM diagnostics.results WHERE matrix_id IN ({placeholders})", matrix_ids
                ).fetchone()[0]
                preprocessing_results = connection.execute(
                    f"SELECT count(*) FROM diagnostics.preprocessing_results WHERE matrix_id IN ({placeholders})", matrix_ids
                ).fetchone()[0]
                connection.execute(f"DELETE FROM diagnostics.results WHERE matrix_id IN ({placeholders})", matrix_ids)
                connection.execute(f"DELETE FROM diagnostics.preprocessing_results WHERE matrix_id IN ({placeholders})", matrix_ids)
            connection.execute(f"DELETE FROM matrices WHERE matrix_id IN ({placeholders})", matrix_ids)
            connection.execute("DELETE FROM sources WHERE source_id IN (93, 94)")
            if connection.execute("SELECT count(*) FROM matrices WHERE source_id IN (93, 94)").fetchone()[0]:
                raise RuntimeError("generated matrices remain after deletion")
            if connection.execute("SELECT count(*) FROM sources WHERE source_id IN (93, 94)").fetchone()[0]:
                raise RuntimeError("generator sources remain after deletion")
            if list(connection.execute("PRAGMA foreign_key_check")):
                raise RuntimeError("foreign-key check failed")
            connection.commit()
        except BaseException:
            connection.rollback()
            raise

        for path in external_files:
            path.unlink()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("corpus integrity check failed")
        if diagnostics_attached and connection.execute("PRAGMA diagnostics.integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("diagnostics integrity check failed")
        print(
            f"removed_matrices={len(rows)} removed_files={len(external_files)} removed_sources=2 "
            f"removed_results={diagnostics_results} removed_preprocessing_results={preprocessing_results}"
        )
    finally:
        connection.close()


if __name__ == "__main__":
    main()
