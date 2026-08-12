#!/usr/bin/env python3
"""Bind every external corpus matrix to the SHA-256 of its exact file bytes."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sqlite3


MATRICES_SCHEMA = """CREATE TABLE matrices_with_file_hashes (
    matrix_id INTEGER PRIMARY KEY,
    dimension INTEGER NOT NULL CHECK(dimension > 0),
    matrix TEXT NOT NULL CHECK(length(matrix) > 0),
    file_sha256 TEXT CHECK(
        (matrix NOT LIKE 'file:%' AND file_sha256 IS NULL)
        OR (matrix LIKE 'file:%' AND file_sha256 IS NOT NULL
            AND length(file_sha256) = 64 AND file_sha256 NOT GLOB '*[^0-9a-f]*')
    ),
    is_strictly_copositive INTEGER NOT NULL CHECK(is_strictly_copositive IN (0, 1)),
    is_copositive INTEGER CHECK(is_copositive IS NULL OR is_copositive IN (0, 1)),
    source TEXT,
    family TEXT,
    smoke_set INTEGER NOT NULL DEFAULT 0 CHECK(smoke_set IN (0, 1)),
    representative_core INTEGER NOT NULL DEFAULT 0 CHECK(representative_core IN (0, 1)),
    stress_test INTEGER NOT NULL DEFAULT 0 CHECK(stress_test IN (0, 1)),
    scale_set INTEGER NOT NULL DEFAULT 0 CHECK(scale_set IN (0, 1)),
    CHECK(is_strictly_copositive = 0 OR is_copositive IS 1)
) STRICT"""


def _path_and_hash(database: Path, reference: str) -> tuple[Path, str]:
    relative_path = Path(reference.removeprefix("file:").strip())
    if not relative_path.parts or relative_path.is_absolute() or ".." in relative_path.parts:
        raise ValueError(f"unsafe matrix file reference: {reference}")
    path = (database.parent / relative_path).resolve()
    if not path.is_relative_to(database.parent):
        raise ValueError(f"matrix file reference escapes the database directory: {reference}")
    with path.open("rb") as source:
        return path, hashlib.file_digest(source, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    arguments = parser.parse_args()
    database_path = arguments.database.resolve()

    connection = sqlite3.connect(database_path)
    connection.execute("PRAGMA busy_timeout = 5000")
    try:
        columns = {row[1] for row in connection.execute("PRAGMA table_info(matrices)")}
        if "file_sha256" in columns:
            rows = connection.execute("SELECT matrix_id, matrix, file_sha256 FROM matrices WHERE matrix LIKE 'file:%'").fetchall()
            for matrix_id, reference, expected_sha256 in rows:
                _path, actual_sha256 = _path_and_hash(database_path, reference)
                if actual_sha256 != expected_sha256:
                    raise ValueError(f"matrix {matrix_id} file SHA-256 mismatch")
            print(f"already_migrated=1 external_files={len(rows)}")
            return

        connection.execute("PRAGMA foreign_keys = OFF")
        connection.execute("BEGIN IMMEDIATE")
        references = connection.execute(
            "SELECT matrix_id, matrix FROM matrices WHERE matrix LIKE 'file:%' ORDER BY matrix_id"
        ).fetchall()
        hashes = [(matrix_id, _path_and_hash(database_path, reference)[1]) for matrix_id, reference in references]

        connection.execute("CREATE TEMP TABLE matrix_file_hash(matrix_id INTEGER PRIMARY KEY, file_sha256 TEXT NOT NULL)")
        connection.executemany("INSERT INTO matrix_file_hash VALUES (?, ?)", hashes)
        connection.execute(MATRICES_SCHEMA)
        connection.execute(
            """INSERT INTO matrices_with_file_hashes (
                   matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive, source, family,
                   smoke_set, representative_core, stress_test, scale_set
               )
               SELECT m.matrix_id, m.dimension, m.matrix, h.file_sha256, m.is_strictly_copositive, m.is_copositive, m.source,
                      m.family, m.smoke_set, m.representative_core, m.stress_test, m.scale_set
               FROM matrices AS m LEFT JOIN matrix_file_hash AS h USING(matrix_id)"""
        )
        connection.execute("DROP TABLE matrices")
        connection.execute("ALTER TABLE matrices_with_file_hashes RENAME TO matrices")
        foreign_key_errors = connection.execute("PRAGMA foreign_key_check").fetchall()
        if foreign_key_errors:
            raise RuntimeError(f"foreign-key check failed: {foreign_key_errors[:5]}")
        connection.commit()
        connection.execute("VACUUM")
        print(f"migrated=1 external_files={len(hashes)}")
    except BaseException:
        connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == "__main__":
    main()
