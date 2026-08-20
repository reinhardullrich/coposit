#!/usr/bin/env python3
"""Copy completed exact SAT-Halfspace-Rays BPQY classifications into corpus truth."""

from __future__ import annotations

import sqlite3
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
CORPUS = PROJECT / "testdata/copos_testdata.sqlite3"
DIAGNOSTICS = PROJECT / "experiments/diagnostics.sqlite3"
MARKER = "import_batch=bpqy-julia185-float-extension-2026-08-19;%"
MODEL = "sat_halfspace_rays_dickinson"
BINARY = "485b736c5a747fb998f85c7272099b5e657e64645c774c2b540f56d421affb16"


def main() -> None:
    with sqlite3.connect(CORPUS) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("ATTACH DATABASE ? AS diagnostics", (str(DIAGNOSTICS),))
        key = (MODEL, BINARY)
        result_filter = "r.model_id=? AND r.mode='both' AND r.preprocessing='both' AND r.binary_sha256=?"
        rows = connection.execute(
            f"""SELECT r.status, r.is_copositive, r.is_strictly_copositive, count(*)
                FROM diagnostics.results r JOIN matrices m USING(matrix_id)
                WHERE m.source LIKE ? AND {result_filter}
                GROUP BY r.status, r.is_copositive, r.is_strictly_copositive""",
            (MARKER, *key),
        ).fetchall()
        if sum(row[3] for row in rows) != 750 or any(row[0] not in {"ok", "timeout"} for row in rows):
            raise ValueError(f"unexpected BPQY result coverage: {rows}")
        if any(row[0] == "ok" and (row[1] not in {0, 1} or row[2] not in {0, 1}) for row in rows):
            raise ValueError(f"incomplete successful BPQY classification: {rows}")
        conflicts = connection.execute(
            f"""SELECT count(*) FROM diagnostics.results r JOIN matrices m USING(matrix_id)
                WHERE m.source LIKE ? AND {result_filter} AND r.status='ok'
                  AND ((m.is_copositive IS NOT NULL AND m.is_copositive<>r.is_copositive)
                    OR (m.is_strictly_copositive IS NOT NULL AND m.is_strictly_copositive<>r.is_strictly_copositive))""",
            (MARKER, *key),
        ).fetchone()[0]
        if conflicts:
            raise ValueError(f"{conflicts} completed classifications conflict with corpus truth")

        connection.execute("BEGIN IMMEDIATE")
        connection.execute(
            f"""UPDATE matrices AS m
                SET is_copositive=(
                        SELECT r.is_copositive FROM diagnostics.results r WHERE r.matrix_id=m.matrix_id AND {result_filter}
                    ),
                    is_strictly_copositive=(
                        SELECT r.is_strictly_copositive FROM diagnostics.results r WHERE r.matrix_id=m.matrix_id AND {result_filter}
                    )
                WHERE m.source LIKE ? AND m.is_copositive IS NULL AND m.is_strictly_copositive IS NULL
                  AND EXISTS (
                      SELECT 1 FROM diagnostics.results r WHERE r.matrix_id=m.matrix_id AND {result_filter} AND r.status='ok'
                  )""",
            (*key, *key, MARKER, *key),
        )
        changed = connection.execute("SELECT changes()").fetchone()[0]
        classified, unknown = connection.execute(
            """SELECT sum(is_copositive IS NOT NULL AND is_strictly_copositive IS NOT NULL),
                      sum(is_copositive IS NULL AND is_strictly_copositive IS NULL)
               FROM matrices WHERE source LIKE ?""",
            (MARKER,),
        ).fetchone()
        cache_mismatches = connection.execute(
            f"""SELECT count(*) FROM diagnostics.results r JOIN matrices m USING(matrix_id)
                WHERE m.source LIKE ? AND {result_filter} AND r.status='ok'
                  AND (m.fastest_elapsed_ns IS NOT r.elapsed_ns
                    OR json_extract(m.fastest_result_ref,'$.model_id') IS NOT r.model_id
                    OR json_extract(m.fastest_result_ref,'$.binary_sha256') IS NOT r.binary_sha256)""",
            (MARKER, *key),
        ).fetchone()[0]
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        if (classified, unknown, cache_mismatches, integrity) != (404, 346, 0, "ok"):
            raise ValueError(
                f"post-classification verification failed: classified={classified} unknown={unknown} "
                f"cache_mismatches={cache_mismatches} integrity={integrity}"
            )
    print(f"changed={changed} classified={classified} unknown={unknown} cache_mismatches={cache_mismatches} integrity={integrity}")


if __name__ == "__main__":
    main()
