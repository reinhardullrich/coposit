#!/usr/bin/env python3
"""Remove generated stress matrices above order 1,000 and add lower-order coverage."""

from __future__ import annotations

import argparse
from pathlib import Path
import sqlite3

import import_high_order_dense_randomized_stress_2026_08_09 as dense
import import_high_order_small_integer_stress_2026_08_09 as sparse


NEW_DIMENSIONS = {43, 60, 73, 81, 92, 103, 111, 124, 132, 145, 153, 161, 176, 187, 199}
FAMILY_PATTERN = "generated sparse %"
DENSE_FAMILY_PATTERN = "generated dense %"


def rows(generator, source_id: int):
    for dimension, matrix, strict, source, family, *_ in generator(tuple(sorted(NEW_DIMENSIONS))):
        yield dimension, matrix, strict, int(not family.endswith("/ not copositive")), source, source_id, family


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    parser.add_argument("--diagnostics", type=Path, default=Path(__file__).resolve().parents[2] / "experiments/diagnostics.sqlite3")
    arguments = parser.parse_args()

    database_directory = arguments.database.resolve().parent
    connection = sqlite3.connect(arguments.database)
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("ATTACH DATABASE ? AS diagnostics", (str(arguments.diagnostics.resolve()),))
    removed_files: list[Path] = []
    try:
        with connection:
            removed = connection.execute(
                "SELECT matrix_id, matrix FROM matrices WHERE (family LIKE ? OR family LIKE ?) AND dimension > 1000",
                (FAMILY_PATTERN, DENSE_FAMILY_PATTERN),
            ).fetchall()
            if len(removed) != 120:
                raise RuntimeError(f"expected exactly 120 generated rows above order 1,000, found {len(removed)}")
            removed_ids = [row[0] for row in removed]
            removed_files = [database_directory / row[1][5:] for row in removed if row[1].startswith("file:")]
            if len(removed_files) != len(removed) or not all(path.is_file() for path in removed_files):
                raise RuntimeError("every removed generated row must reference one existing matrix file")
            placeholders = ",".join("?" for _ in removed_ids)
            connection.execute(f"DELETE FROM diagnostics.results WHERE matrix_id IN ({placeholders})", removed_ids)
            connection.execute(f"DELETE FROM diagnostics.preprocessing_results WHERE matrix_id IN ({placeholders})", removed_ids)
            connection.execute(f"DELETE FROM matrices WHERE matrix_id IN ({placeholders})", removed_ids)

            next_id = connection.execute("SELECT COALESCE(MAX(matrix_id), 0) + 1 FROM matrices").fetchone()[0]
            generated = list(rows(sparse.generated_rows, 93)) + list(rows(dense.generated_rows, 94))
            if len(generated) != 90:
                raise RuntimeError(f"expected exactly 90 new generated rows, found {len(generated)}")
            for offset, row in enumerate(generated):
                dimension, matrix, strict, copositive, source, source_id, family = row
                duplicate = connection.execute(
                    "SELECT matrix_id FROM matrices WHERE dimension = ? AND matrix = ?", (dimension, matrix)
                ).fetchone()
                if duplicate:
                    raise RuntimeError(f"generated matrix duplicates existing ID {duplicate[0]}")
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, "
                    "source_id, family, scale_set) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1)",
                    (next_id + offset, dimension, matrix, strict, copositive, source, source_id, family),
                )

            connection.execute(
                "UPDATE sources SET title = ?, comment = ? WHERE source_id = 93",
                ("Deterministic sparse small-integer stress matrices",
                 "Exact local generator for the 75 sparse boundary strict and non-copositive stress matrices."),
            )
            connection.execute(
                "UPDATE sources SET title = ?, comment = ? WHERE source_id = 94",
                ("Deterministic dense randomized stress matrices",
                 "Exact local generator for the 75 dense boundary strict and non-copositive stress matrices."),
            )

        for path in removed_files:
            path.unlink()
        print(f"removed={len(removed_ids)} files={len(removed_files)} inserted={len(generated)}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
